#include "InvestigationDirector.h"

#include "Foundation.h"

#include <algorithm>
#include <sstream>

namespace gco::investigation {
namespace {

std::uint32_t stableSeed(const LogicalId caseId, const std::string_view sourceKey) noexcept {
    std::uint32_t hash = static_cast<std::uint32_t>(caseId ^ (caseId >> 32U));
    for (const unsigned char ch : sourceKey) {
        hash ^= ch;
        hash *= 16777619u;
    }
    return hash;
}

} // namespace

InvestigationDirector::InvestigationDirector(
    crime::CrimeRegistry& registry,
    crime::CrimeDirector& crimeDirector,
    witness::WitnessDirector& witnessDirector,
    DispatchDirector& dispatchDirector,
    platform::IPoliceInvestigationAdapter& adapter,
    EventBus& events,
    InvestigationTuning tuning)
    : registry_(registry),
      crimeDirector_(crimeDirector),
      witnessDirector_(witnessDirector),
      dispatchDirector_(dispatchDirector),
      adapter_(adapter),
      events_(events),
      tuning_(tuning) {}

InvestigationDirector::~InvestigationDirector() {
    shutdown();
}

void InvestigationDirector::initialize() {
    if (initialized_) return;
    witnessReportedSub_ = events_.subscribe("witness.report_completed", [this](const RuntimeEvent& event) {
        onWitnessReported(event);
    });
    initialized_ = true;
}

void InvestigationDirector::shutdown() {
    if (!initialized_) return;
    abortActiveInterview("runtime shutdown");
    if (witnessReportedSub_ != 0) events_.unsubscribe(witnessReportedSub_);
    witnessReportedSub_ = 0;
    knownWitnesses_.clear();
    interviewed_.clear();
    initialized_ = false;
}

void InvestigationDirector::tickFiveHz(const std::uint64_t nowMs, const bool gameplayAllowed) {
    if (!initialized_) return;
    if (!gameplayAllowed) {
        abortActiveInterview("mission compatibility gate");
        return;
    }

    if (active_) {
        if (!dispatchDirector_.highDetailArrived(active_->caseId)) {
            abortActiveInterview("scene abstracted or officer unavailable");
            return;
        }
        tickActiveInterview(nowMs);
        return;
    }

    for (const auto* scene : dispatchDirector_.scenes()) {
        if (scene == nullptr || scene->detailLevel != SceneDetailLevel::HighDetail
            || !scene->logicalArrivalRecorded || nowMs >= scene->expiresAtMs) {
            continue;
        }
        cacheLiveWitnesses(scene->caseId);
        if (tryStartInterview(scene->caseId, nowMs)) return;
    }
}

std::string InvestigationDirector::debugSummary() const {
    std::ostringstream out;
    out << "InvestigationDirector: knownWitnesses=" << knownWitnesses_.size()
        << ";interviewed=" << interviewed_.size()
        << ";active=" << (active_.has_value() ? "true" : "false");
    if (active_) {
        out << ";case=" << active_->caseId
            << ";source=" << active_->sourceKey
            << ";turn=" << active_->turnIndex << '/' << active_->plan.size();
    }
    return out.str();
}

void InvestigationDirector::onWitnessReported(const RuntimeEvent& event) {
    if (event.subjectId == 0 || event.payload.empty()) return;
    cacheLiveWitnesses(event.subjectId);
}

void InvestigationDirector::cacheLiveWitnesses(const LogicalId caseId) {
    for (const auto& candidate : witnessDirector_.interviewCandidates(caseId)) {
        if (!candidate.alive || candidate.ped == 0 || candidate.sourceKey.empty()) continue;
        const auto exists = std::find_if(knownWitnesses_.begin(), knownWitnesses_.end(), [&](const KnownWitness& known) {
            return known.caseId == caseId && known.sourceKey == candidate.sourceKey;
        });
        if (exists == knownWitnesses_.end()) {
            knownWitnesses_.push_back(KnownWitness{caseId, candidate.ped, candidate.sourceKey});
        } else {
            exists->ped = candidate.ped;
        }
    }
}

bool InvestigationDirector::tryStartInterview(const LogicalId caseId, const std::uint64_t nowMs) {
    const auto* file = registry_.findCase(caseId);
    if (file == nullptr) return false;

    std::size_t completedForCase = 0;
    for (const auto& key : interviewed_) {
        if (key.rfind(std::to_string(caseId) + ":", 0) == 0) ++completedForCase;
    }
    if (completedForCase >= tuning_.maxWitnessesPerCase) return false;

    for (const auto& known : knownWitnesses_) {
        if (known.caseId != caseId || known.ped == 0) continue;
        const std::string key = interviewKey(caseId, known.sourceKey);
        if (interviewed_.contains(key)) continue;

        const auto summary = deriveInterviewSummary(*file, known.sourceKey);
        if (!summary.hasReport) continue;

        dialogue::InterviewOptions options{};
        options.maxQuestionPairs = 4;
        options.variationSeed = stableSeed(caseId, known.sourceKey);
        auto plan = dialogue::InvestigationDialogueComposer::compose(summary.dialogueFacts, options);
        if (plan.empty()) {
            interviewed_.insert(key);
            continue;
        }

        if (!adapter_.beginInterview(caseId, known.ped, PresentationStyle::Clipboard)) continue;

        ActiveInterview active{};
        active.caseId = caseId;
        active.witnessPed = known.ped;
        active.sourceKey = known.sourceKey;
        active.summary = summary;
        active.plan = std::move(plan);
        active.turnIndex = 0;
        active.nextTurnAtMs = nowMs;
        active_ = std::move(active);
        events_.publish(RuntimeEvent{
            "investigation.interview_started",
            caseId,
            "source=" + known.sourceKey});
        return true;
    }
    return false;
}

void InvestigationDirector::tickActiveInterview(const std::uint64_t nowMs) {
    if (!active_ || nowMs < active_->nextTurnAtMs) return;
    if (active_->turnIndex >= active_->plan.turns.size()) {
        completeActiveInterview(nowMs);
        return;
    }

    const auto& turn = active_->plan.turns[active_->turnIndex];
    adapter_.presentInterviewTurn(
        active_->caseId,
        active_->witnessPed,
        turn.speaker,
        presentationFor(turn),
        turn.text);
    ++active_->turnIndex;
    active_->nextTurnAtMs = nowMs + tuning_.turnDelayMs;
}

void InvestigationDirector::abortActiveInterview(const char* reason) {
    if (!active_) return;
    adapter_.endInterview(active_->caseId, active_->witnessPed);
    events_.publish(RuntimeEvent{
        "investigation.interview_interrupted",
        active_->caseId,
        std::string("source=") + active_->sourceKey + ";reason=" + reason});
    active_.reset();
}

void InvestigationDirector::completeActiveInterview(const std::uint64_t nowMs) {
    if (!active_) return;
    const LogicalId caseId = active_->caseId;
    const auto sourceKey = active_->sourceKey;
    const auto witnessPed = active_->witnessPed;
    const auto summary = active_->summary;

    adapter_.endInterview(caseId, witnessPed);
    commitInterviewFacts(caseId, summary, nowMs);
    interviewed_.insert(interviewKey(caseId, sourceKey));
    events_.publish(RuntimeEvent{
        "investigation.interview_completed",
        caseId,
        "source=" + sourceKey + ";facts=" + std::to_string(summary.facts.size())});
    active_.reset();
}

void InvestigationDirector::commitInterviewFacts(
    const LogicalId caseId,
    const InterviewSourceSummary& summary,
    const std::uint64_t nowMs) {

    float bestVehicle = 0.0f;
    float bestPlate = 0.0f;
    for (const auto& fact : summary.facts) {
        crime::EvidenceRecord evidence{};
        evidence.source = crime::EvidenceSource::Officer;
        evidence.kind = fact.evidenceKind;
        evidence.confidence = fact.confidence;
        evidence.observedAtMs = nowMs;
        // Preserve the original witness source family. Interviewing a witness must not create a
        // second statistically independent source and inflate certainty.
        evidence.independenceKey = fact.sourceKey;
        evidence.dedupKey = "interview:" + std::to_string(caseId) + ':' + fact.sourceKey + ':'
            + interviewSemanticName(fact.semantic);
        evidence.snapshot.descriptor = "interview_semantic=" + interviewSemanticName(fact.semantic)
            + ";reported=" + fact.descriptor;
        evidence.snapshot.location = fact.location;

        if (crimeDirector_.addEvidence(caseId, std::move(evidence)) == crime::EvidenceAddResult::Added) {
            casePersistenceDirty_ = true;
        }
        if (fact.semantic == InterviewSemanticEvent::VehicleSeen) bestVehicle = std::max(bestVehicle, fact.confidence);
        if (fact.semantic == InterviewSemanticEvent::PlateSeen) bestPlate = std::max(bestPlate, fact.confidence);
    }

    const auto* file = registry_.findCase(caseId);
    if (file != nullptr && !file->activeVehicleBolo
        && (bestVehicle >= tuning_.vehicleBoloMinimumConfidence
            || bestPlate >= tuning_.plateBoloMinimumConfidence)) {
        if (crimeDirector_.issueBoloOrWarrant(caseId, false, true, nowMs)) {
            casePersistenceDirty_ = true;
        }
    }
}

PresentationStyle InvestigationDirector::presentationFor(const dialogue::ConversationTurn& turn) noexcept {
    if (turn.speaker == dialogue::InterviewSpeaker::Witness) {
        return turn.topic == dialogue::InterviewTopic::Opening
            ? PresentationStyle::FaceWitness
            : PresentationStyle::Phone;
    }
    switch (turn.topic) {
    case dialogue::InterviewTopic::Opening:
    case dialogue::InterviewTopic::Closing:
        return PresentationStyle::Radio;
    case dialogue::InterviewTopic::Summary:
        return PresentationStyle::Clipboard;
    default:
        return PresentationStyle::Clipboard;
    }
}

std::string InvestigationDirector::interviewKey(
    const LogicalId caseId,
    const std::string_view sourceKey) {
    return std::to_string(caseId) + ':' + std::string(sourceKey);
}

} // namespace gco::investigation
