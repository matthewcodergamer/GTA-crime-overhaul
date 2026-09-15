#include "ClerkRecognitionDirector.h"

#include "Foundation.h"
#include "witness/WitnessDomain.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

namespace gco::identity {
namespace {

float distance(const platform::Vec3& a, const platform::Vec3& b) noexcept {
    return std::sqrt(platform::distanceSquared(a, b));
}

crime::CrimeLocation crimeLocation(const platform::Vec3& position, const std::string& tag) {
    return crime::CrimeLocation{position.x, position.y, position.z, tag};
}

std::string levelTopic(const RecognitionLevel level) {
    return level == RecognitionLevel::Recognized
        ? "repeat_robber.recognized"
        : "repeat_robber.suspicious";
}

} // namespace

ClerkRecognitionDirector::ClerkRecognitionDirector(
    platform::PlatformServices& platform,
    IdentitySystem& identitySystem,
    robbery::PrototypeStoreRuntime& storeRuntime,
    crime::CrimeDirector& crimeDirector,
    EventBus& events,
    ClerkRecognitionTuning tuning)
    : platform_(platform),
      identitySystem_(identitySystem),
      storeRuntime_(storeRuntime),
      crimeDirector_(crimeDirector),
      events_(events),
      tuning_(std::move(tuning)) {}

ClerkRecognitionDirector::~ClerkRecognitionDirector() {
    shutdown();
}

void ClerkRecognitionDirector::initialize() {
    if (initialized_) return;
    robberyStartedSub_ = events_.subscribe("business.robbery_started", [this](const RuntimeEvent& event) {
        onRobberyStarted(event);
    });
    robberyFinishedSub_ = events_.subscribe("business.robbery_finished", [this](const RuntimeEvent& event) {
        onRobberyFinished(event);
    });
    reportCompletedSub_ = events_.subscribe("witness.report_completed", [this](const RuntimeEvent& event) {
        onReportCompleted(event);
    });
    clerkReplacedSub_ = events_.subscribe("business.clerk_replaced", [this](const RuntimeEvent& event) {
        onClerkReplacedOrKilled(event);
    });
    clerkKilledSub_ = events_.subscribe("business.clerk_killed", [this](const RuntimeEvent& event) {
        onClerkReplacedOrKilled(event);
    });
    initialized_ = true;
}

void ClerkRecognitionDirector::shutdown() {
    if (!initialized_) return;
    if (robberyStartedSub_) events_.unsubscribe(robberyStartedSub_);
    if (robberyFinishedSub_) events_.unsubscribe(robberyFinishedSub_);
    if (reportCompletedSub_) events_.unsubscribe(reportCompletedSub_);
    if (clerkReplacedSub_) events_.unsubscribe(clerkReplacedSub_);
    if (clerkKilledSub_) events_.unsubscribe(clerkKilledSub_);
    robberyStartedSub_ = robberyFinishedSub_ = reportCompletedSub_ = clerkReplacedSub_ = clerkKilledSub_ = 0;
    observation_ = {};
    accumulatedViewMs_ = 0;
    lastViewSampleMs_ = 0;
    initialized_ = false;
}

void ClerkRecognitionDirector::onRobberyStarted(const RuntimeEvent& event) {
    if (!storeRuntime_.initialized() || event.subjectId != storeRuntime_.model().persistent().businessId) return;
    const auto caseId = payloadId(event.payload, "caseId");
    const auto crimeId = payloadId(event.payload, "crimeId");
    const auto clerkId = payloadId(event.payload, "clerkId");
    if (!caseId || !crimeId || !clerkId) return;

    observation_ = {};
    observation_.businessId = event.subjectId;
    observation_.caseId = *caseId;
    observation_.crimeId = *crimeId;
    observation_.clerkId = *clerkId;
    observation_.active = true;
    accumulatedViewMs_ = 0;
    lastViewSampleMs_ = 0;
}

void ClerkRecognitionDirector::onRobberyFinished(const RuntimeEvent& event) {
    if (observation_.businessId == 0 || event.subjectId != observation_.businessId) return;
    const auto caseId = payloadId(event.payload, "caseId");
    if (caseId && *caseId != observation_.caseId) return;
    observation_.active = false;
    accumulatedViewMs_ = 0;
    lastViewSampleMs_ = 0;
}

void ClerkRecognitionDirector::onReportCompleted(const RuntimeEvent& event) {
    // Ambient Stage-4 witnesses publish caseId as subject. Only the persistent store clerk publishes businessId.
    if (observation_.businessId == 0 || event.subjectId != observation_.businessId) return;
    if (event.payload.find("source=clerk_or_alarm") == std::string::npos) return;
    persistReportedClerkEvidence(lastViewSampleMs_ != 0 ? lastViewSampleMs_ : observation_.faceObservedAtMs);
}

void ClerkRecognitionDirector::onClerkReplacedOrKilled(const RuntimeEvent& event) {
    if (!storeRuntime_.initialized() || event.subjectId != storeRuntime_.model().persistent().businessId) return;
    observation_ = {};
    accumulatedViewMs_ = 0;
    lastViewSampleMs_ = 0;
    lastRecognition_ = {};
}

void ClerkRecognitionDirector::tickFiveHz(const std::uint64_t nowMs, const bool gameplayAllowed) {
    if (!initialized_ || !gameplayAllowed || !storeRuntime_.initialized() || !storeRuntime_.targetReady()
        || !storeRuntime_.detailedActive()) {
        accumulatedViewMs_ = 0;
        lastViewSampleMs_ = 0;
        return;
    }

    if (observation_.active) sampleRobberyObservation(nowMs);
    else if (nowMs >= nextRecognitionCheckMs_) {
        nextRecognitionCheckMs_ = nowMs + tuning_.recognitionCheckMs;
        evaluateRepeatRecognition(nowMs);
    }
}

bool ClerkRecognitionDirector::visibleSample(
    const platform::PedHandle clerk,
    const platform::PedHandle player,
    const platform::PedSnapshot& clerkSnapshot,
    const platform::PedSnapshot& playerSnapshot,
    const std::uint64_t nowMs,
    float& confidence,
    float& faceQuality) {

    confidence = 0.0f;
    faceQuality = 0.0f;
    const float d = distance(clerkSnapshot.position, playerSnapshot.position);
    if (d > tuning_.observationRadius) {
        accumulatedViewMs_ = 0;
        lastViewSampleMs_ = nowMs;
        return false;
    }

    const float fov = witness::angularFrontQuality(
        clerkSnapshot.position,
        clerkSnapshot.heading,
        playerSnapshot.position,
        tuning_.halfFovDegrees);
    if (fov <= 0.0f || !platform_.world.hasLineOfSight(
            clerk,
            player,
            platform::LineOfSightProfile::DefaultVisibility)) {
        accumulatedViewMs_ = 0;
        lastViewSampleMs_ = nowMs;
        return false;
    }

    const std::uint64_t delta = lastViewSampleMs_ == 0
        ? 200
        : std::min<std::uint64_t>(1000, nowMs >= lastViewSampleMs_ ? nowMs - lastViewSampleMs_ : 0);
    lastViewSampleMs_ = nowMs;
    accumulatedViewMs_ += delta;
    confidence = witness::visualConfidence(
        d,
        tuning_.observationRadius,
        fov,
        perception_.ambientVisibilityFactor(),
        true,
        accumulatedViewMs_);
    faceQuality = witness::angularFrontQuality(
        playerSnapshot.position,
        playerSnapshot.heading,
        clerkSnapshot.position,
        tuning_.faceHalfAngleDegrees);
    return confidence > 0.10f;
}

void ClerkRecognitionDirector::sampleRobberyObservation(const std::uint64_t nowMs) {
    const auto clerk = storeRuntime_.boundClerkPed();
    const auto player = platform_.world.playerPed();
    if (clerk == 0 || !platform_.world.pedExists(clerk)) return;
    const auto clerkSnapshot = platform_.world.snapshotPed(clerk);
    const auto playerSnapshot = platform_.world.snapshotPed(player);
    if (!clerkSnapshot || !playerSnapshot || !clerkSnapshot->alive || !playerSnapshot->alive) return;

    float confidence = 0.0f;
    float faceQuality = 0.0f;
    if (!visibleSample(clerk, player, *clerkSnapshot, *playerSnapshot, nowMs, confidence, faceQuality)) return;

    const IdentitySnapshot current = identitySystem_.snapshot(*playerSnapshot);
    auto memory = storeRuntime_.clerkRecognitionMemory();
    bool memoryChanged = false;

    if (confidence >= 0.20f) {
        observation_.outfitObserved = true;
        if (confidence >= observation_.outfitConfidence) {
            observation_.outfitKey = current.outfit.stableKey;
            observation_.outfitConfidence = confidence;
            observation_.outfitObservedAtMs = nowMs;
        }
        memoryChanged = identitySystem_.observeOutfit(memory, current, confidence, nowMs) || memoryChanged;
    }

    if (current.mask.blocksNewFaceCapture && confidence >= 0.25f) {
        observation_.maskObserved = true;
        observation_.maskCoverage = current.mask.coverage;
        if (confidence >= observation_.maskConfidence) {
            observation_.maskConfidence = confidence;
            observation_.maskObservedAtMs = nowMs;
        }
    }

    const float faceConfidence = witness::clamp01(confidence * faceQuality);
    if (current.faceCaptureAllowed
        && current.character != CharacterIdentity::Unknown
        && accumulatedViewMs_ >= tuning_.faceMinimumViewMs
        && faceConfidence >= 0.38f) {
        if (!observation_.faceObserved || faceConfidence > observation_.faceConfidence) {
            observation_.faceObserved = true;
            observation_.faceIdentity = current.character;
            observation_.faceConfidence = faceConfidence;
            observation_.faceObservedAtMs = nowMs;
        }
        memoryChanged = identitySystem_.observeFace(memory, current, faceConfidence, nowMs) || memoryChanged;
    }

    if (memoryChanged) storeRuntime_.updateClerkRecognition(std::move(memory));
}

void ClerkRecognitionDirector::evaluateRepeatRecognition(const std::uint64_t nowMs) {
    const auto clerk = storeRuntime_.boundClerkPed();
    const auto player = platform_.world.playerPed();
    if (clerk == 0 || !platform_.world.pedExists(clerk)) return;
    const auto clerkSnapshot = platform_.world.snapshotPed(clerk);
    const auto playerSnapshot = platform_.world.snapshotPed(player);
    if (!clerkSnapshot || !playerSnapshot || !clerkSnapshot->alive || !playerSnapshot->alive) return;
    if (!storeRuntime_.target().businessVolume.contains(playerSnapshot->position)) return;

    float confidence = 0.0f;
    float faceQuality = 0.0f;
    if (!visibleSample(clerk, player, *clerkSnapshot, *playerSnapshot, nowMs, confidence, faceQuality)) {
        lastRecognition_ = {};
        return;
    }

    const IdentitySnapshot current = identitySystem_.snapshot(*playerSnapshot);
    lastRecognition_ = identitySystem_.evaluateRecognition(
        storeRuntime_.clerkRecognitionMemory(),
        current,
        storeRuntime_.model().persistent().clerk.id ^ nowMs / std::max<std::uint64_t>(1, tuning_.recognitionCheckMs));

    if (lastRecognition_.level == RecognitionLevel::None || nowMs < nextRecognitionReactionMs_) return;
    nextRecognitionReactionMs_ = nowMs + tuning_.recognitionReactionCooldownMs;

    events_.publish(RuntimeEvent{
        levelTopic(lastRecognition_.level),
        storeRuntime_.model().persistent().businessId,
        "clerkId=" + std::to_string(storeRuntime_.model().persistent().clerk.id)
            + ";level=" + std::string(recognitionLevelName(lastRecognition_.level))
            + ";behavior=" + std::string(recognitionBehaviorName(lastRecognition_.behavior))
            + ";faceMatch=" + std::to_string(lastRecognition_.faceMatch)
            + ";clothingMatch=" + std::to_string(lastRecognition_.clothingMatch)});

    auto memory = storeRuntime_.clerkRecognitionMemory();
    ++memory.recognitionCount;
    storeRuntime_.updateClerkRecognition(std::move(memory));
    applyRecognitionBehavior(lastRecognition_, nowMs);
}

void ClerkRecognitionDirector::applyRecognitionBehavior(
    const RecognitionResult& result,
    const std::uint64_t nowMs) {

    (void)nowMs;
    const auto clerk = storeRuntime_.boundClerkPed();
    const auto player = platform_.world.playerPed();
    const auto clerkSnapshot = platform_.world.snapshotPed(clerk);
    const auto playerSnapshot = platform_.world.snapshotPed(player);
    if (!clerkSnapshot || !playerSnapshot) return;

    switch (result.behavior) {
    case RecognitionBehavior::None:
        break;
    case RecognitionBehavior::Notice:
        pedBehavior_.turnToCoord(clerk, playerSnapshot->position, 650);
        break;
    case RecognitionBehavior::StarePause:
        pedBehavior_.turnToCoord(clerk, playerSnapshot->position, 1400);
        break;
    case RecognitionBehavior::BackAway: {
        const float dx = clerkSnapshot->position.x - playerSnapshot->position.x;
        const float dy = clerkSnapshot->position.y - playerSnapshot->position.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length > 0.05f) {
            platform::Vec3 target = clerkSnapshot->position;
            target.x += dx / length * 1.3f;
            target.y += dy / length * 1.3f;
            pedBehavior_.goStraightTo(clerk, target, 0.75f, clerkSnapshot->heading);
        } else {
            pedBehavior_.turnToCoord(clerk, playerSnapshot->position, 1000);
        }
        break;
    }
    case RecognitionBehavior::SilentAlarmPossible:
        pedBehavior_.turnToCoord(clerk, playerSnapshot->position, 800);
        events_.publish(RuntimeEvent{
            "clerk.recognition_silent_alarm_possible",
            storeRuntime_.model().persistent().businessId,
            "clerkId=" + std::to_string(storeRuntime_.model().persistent().clerk.id)});
        break;
    }
}

void ClerkRecognitionDirector::persistReportedClerkEvidence(const std::uint64_t nowMs) {
    if (observation_.caseId == 0 || observation_.clerkId == 0) return;
    const auto effectiveNow = nowMs != 0 ? nowMs : observation_.outfitObservedAtMs;

    if (observation_.faceObserved) {
        addCaseEvidence(
            crime::EvidenceKind::Face,
            observation_.faceConfidence,
            observation_.faceObservedAtMs,
            "clerk_saw_uncovered_face");
        addCaseEvidence(
            crime::EvidenceKind::Identity,
            observation_.faceConfidence,
            observation_.faceObservedAtMs,
            "character_identity=" + std::string(characterIdentityName(observation_.faceIdentity)));
    }
    if (observation_.maskObserved) {
        addCaseEvidence(
            crime::EvidenceKind::Mask,
            observation_.maskConfidence,
            observation_.maskObservedAtMs,
            "approved_mask_coverage=" + std::string(maskCoverageName(observation_.maskCoverage)));
    }
    if (observation_.outfitObserved) {
        addCaseEvidence(
            crime::EvidenceKind::Clothing,
            observation_.outfitConfidence,
            observation_.outfitObservedAtMs != 0 ? observation_.outfitObservedAtMs : effectiveNow,
            "outfit=" + observation_.outfitKey);
    }
}

void ClerkRecognitionDirector::addCaseEvidence(
    const crime::EvidenceKind kind,
    const float confidence,
    const std::uint64_t observedAtMs,
    std::string descriptor) {

    crime::EvidenceRecord evidence{};
    evidence.source = crime::EvidenceSource::Witness;
    evidence.kind = kind;
    evidence.confidence = witness::clamp01(confidence);
    evidence.observedAtMs = observedAtMs;
    evidence.independenceKey = "clerk:" + std::to_string(observation_.clerkId);
    evidence.dedupKey = "clerk:" + std::to_string(observation_.clerkId)
        + ":case:" + std::to_string(observation_.caseId)
        + ":" + std::string(crime::evidenceKindName(kind));
    evidence.snapshot.descriptor = std::move(descriptor);
    evidence.snapshot.location = crimeLocation(storeRuntime_.target().activationCenter(), storeRuntime_.target().id);
    evidence.snapshot.sourceLogicalId = observation_.clerkId;

    if (crimeDirector_.addEvidence(observation_.caseId, std::move(evidence)) == crime::EvidenceAddResult::Added) {
        casePersistenceDirty_ = true;
    }
}

std::optional<LogicalId> ClerkRecognitionDirector::payloadId(
    const std::string_view payload,
    const std::string_view key) {

    const std::string prefix = std::string(key) + "=";
    const std::size_t start = payload.find(prefix);
    if (start == std::string_view::npos) return std::nullopt;
    const std::size_t valueStart = start + prefix.size();
    const std::size_t end = payload.find(';', valueStart);
    const std::string text(payload.substr(
        valueStart,
        end == std::string_view::npos ? payload.size() - valueStart : end - valueStart));
    try {
        const auto value = static_cast<LogicalId>(std::stoull(text));
        return value == 0 ? std::nullopt : std::optional<LogicalId>{value};
    } catch (...) {
        return std::nullopt;
    }
}

std::string ClerkRecognitionDirector::debugSummary() const {
    const auto& memory = storeRuntime_.clerkRecognitionMemory();
    std::ostringstream out;
    out << "ClerkRecognition: clerkId=" << storeRuntime_.model().persistent().clerk.id
        << ";robberyActive=" << (observation_.active ? "true" : "false")
        << ";lastLevel=" << recognitionLevelName(lastRecognition_.level)
        << ";lastBehavior=" << recognitionBehaviorName(lastRecognition_.behavior)
        << ";faceMichael=" << identitySystem_.rememberedFaceConfidence(memory, CharacterIdentity::Michael)
        << ";faceFranklin=" << identitySystem_.rememberedFaceConfidence(memory, CharacterIdentity::Franklin)
        << ";faceTrevor=" << identitySystem_.rememberedFaceConfidence(memory, CharacterIdentity::Trevor)
        << ";outfitConfidence=" << memory.clothingConfidence
        << ";recognitionCount=" << memory.recognitionCount;
    return out.str();
}

} // namespace gco::identity
