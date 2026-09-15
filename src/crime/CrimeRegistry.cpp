#include "CrimeRegistry.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace gco::crime {
namespace {

bool confidenceValid(const float value) noexcept {
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

std::uint8_t knowledgeRank(const SuspectKnowledge value) noexcept {
    return static_cast<std::uint8_t>(value);
}

} // namespace

CrimeRecordResult CrimeRegistry::recordCrime(
    const CrimeOccurrence& occurrence,
    LogicalIdGenerator& ids) {

    CrimeRecordResult result{};
    CaseFile* file = nullptr;

    if (!occurrence.incidentKey.empty()) {
        const auto found = incidentToCase_.find(occurrence.incidentKey);
        if (found != incidentToCase_.end()) {
            file = findCaseMutable(found->second);
            if (file != nullptr && !isTerminal(file->state)) {
                result.mergedIntoExistingCase = true;
            } else {
                file = nullptr;
                incidentToCase_.erase(found);
            }
        }
    }

    if (file == nullptr) {
        const LogicalId caseId = ids.next(LogicalIdDomain::Case);
        if (caseId == 0) {
            return result;
        }

        CaseFile created{};
        created.id = caseId;
        created.incidentKey = occurrence.incidentKey;
        created.state = CaseState::Unobserved;
        created.severity = baseSeverity(occurrence.type);
        created.createdAtMs = occurrence.occurredAtMs;
        created.updatedAtMs = occurrence.occurredAtMs;

        auto [inserted, ok] = cases_.emplace(caseId, std::move(created));
        if (!ok) {
            return result;
        }
        file = &inserted->second;
        if (!file->incidentKey.empty()) {
            incidentToCase_[file->incidentKey] = caseId;
        }
    }

    const LogicalId crimeId = ids.next(LogicalIdDomain::Crime);
    if (crimeId == 0) {
        return {};
    }

    CrimeEvent event{};
    event.id = crimeId;
    event.caseId = file->id;
    event.type = occurrence.type;
    event.severity = baseSeverity(occurrence.type);
    event.location = occurrence.location;
    event.businessId = occurrence.businessId;
    event.occurredAtMs = occurrence.occurredAtMs;

    if (!crimes_.emplace(crimeId, event).second) {
        return {};
    }

    file->crimeIds.push_back(crimeId);
    file->severity = maxSeverity(file->severity, event.severity);
    file->updatedAtMs = std::max(file->updatedAtMs, occurrence.occurredAtMs);

    result.crimeId = crimeId;
    result.caseId = file->id;
    return result;
}

bool CrimeRegistry::escalateCrime(
    const LogicalId crimeId,
    const CrimeSeverity severity,
    const std::uint64_t nowMs) {

    const auto found = crimes_.find(crimeId);
    if (found == crimes_.end()) {
        return false;
    }
    if (static_cast<std::uint8_t>(severity) < static_cast<std::uint8_t>(found->second.severity)) {
        return false;
    }

    found->second.severity = severity;
    CaseFile* file = findCaseMutable(found->second.caseId);
    if (file == nullptr || isTerminal(file->state)) {
        return false;
    }
    refreshCaseSeverity(*file);
    file->updatedAtMs = std::max(file->updatedAtMs, nowMs);
    return true;
}

EvidenceAddResult CrimeRegistry::addEvidence(
    const LogicalId caseId,
    EvidenceRecord evidence) {

    CaseFile* file = findCaseMutable(caseId);
    if (file == nullptr) {
        return EvidenceAddResult::CaseMissing;
    }
    if (!confidenceValid(evidence.confidence) || isTerminal(file->state)) {
        return EvidenceAddResult::Invalid;
    }
    if (evidenceDuplicate(*file, evidence)) {
        return EvidenceAddResult::Duplicate;
    }

    if (evidence.dedupKey.empty()) {
        evidence.dedupKey = evidenceFingerprint(evidence);
    }
    const std::uint64_t observedAt = evidence.observedAtMs;
    file->evidence.push_back(std::move(evidence));
    file->lastEvidenceAtMs = std::max(file->lastEvidenceAtMs, observedAt);
    file->updatedAtMs = std::max(file->updatedAtMs, observedAt);

    if (file->state == CaseState::Unobserved) {
        file->state = CaseState::Observed;
    }
    return EvidenceAddResult::Added;
}

bool CrimeRegistry::transitionCase(
    const LogicalId caseId,
    const CaseState nextState,
    const std::uint64_t nowMs) {

    CaseFile* file = findCaseMutable(caseId);
    if (file == nullptr || !canTransition(file->state, nextState)) {
        return false;
    }
    file->state = nextState;
    file->updatedAtMs = std::max(file->updatedAtMs, nowMs);
    if (nextState == CaseState::Resolved && file->resolution == CaseResolution::None) {
        file->resolution = CaseResolution::AdministrativelyResolved;
    }
    if (nextState == CaseState::Resolved && !file->incidentKey.empty()) {
        const auto found = incidentToCase_.find(file->incidentKey);
        if (found != incidentToCase_.end() && found->second == caseId) {
            incidentToCase_.erase(found);
        }
    }
    return true;
}

bool CrimeRegistry::setSuspectKnowledge(
    const LogicalId caseId,
    const SuspectKnowledge knowledge,
    const float identityConfidence,
    const std::uint64_t nowMs) {

    CaseFile* file = findCaseMutable(caseId);
    if (file == nullptr || isTerminal(file->state) || !confidenceValid(identityConfidence)) {
        return false;
    }
    if (knowledgeRank(knowledge) < knowledgeRank(file->suspectKnowledge)) {
        return false;
    }
    file->suspectKnowledge = knowledge;
    file->identityConfidence = std::max(file->identityConfidence, identityConfidence);
    file->updatedAtMs = std::max(file->updatedAtMs, nowMs);
    return true;
}

bool CrimeRegistry::setImmediateResponse(
    const LogicalId caseId,
    const ImmediateResponseState& immediate,
    const std::uint64_t nowMs) {

    CaseFile* file = findCaseMutable(caseId);
    if (file == nullptr || isTerminal(file->state)) {
        return false;
    }
    file->immediate = immediate;
    file->immediate.lastUpdatedAtMs = std::max(file->immediate.lastUpdatedAtMs, nowMs);
    file->updatedAtMs = std::max(file->updatedAtMs, nowMs);
    return true;
}

bool CrimeRegistry::setBoloOrWarrant(
    const LogicalId caseId,
    const bool personWarrant,
    const bool vehicleBolo,
    const std::uint64_t nowMs) {

    CaseFile* file = findCaseMutable(caseId);
    if (file == nullptr || isTerminal(file->state)) {
        return false;
    }
    file->activePersonWarrant = personWarrant;
    file->activeVehicleBolo = vehicleBolo;
    file->updatedAtMs = std::max(file->updatedAtMs, nowMs);
    return true;
}

bool CrimeRegistry::resolveCase(
    const LogicalId caseId,
    const CaseResolution resolution,
    const std::uint64_t nowMs) {

    if (resolution == CaseResolution::None) {
        return false;
    }
    CaseFile* file = findCaseMutable(caseId);
    if (file == nullptr || isTerminal(file->state)) {
        return false;
    }
    if (!canTransition(file->state, CaseState::Resolved)) {
        return false;
    }
    file->resolution = resolution;
    file->activePersonWarrant = false;
    file->activeVehicleBolo = false;
    file->immediate = {};
    return transitionCase(caseId, CaseState::Resolved, nowMs);
}

std::size_t CrimeRegistry::applyDecay(
    const std::uint64_t nowMs,
    const CaseDecayPolicy& policy) {

    std::size_t resolved = 0;
    for (auto& [caseId, file] : cases_) {
        (void)caseId;
        if (isTerminal(file.state) || isMajorCase(file.severity)
            || file.activePersonWarrant || file.activeVehicleBolo) {
            continue;
        }

        float strongest = 0.0f;
        for (const auto& evidence : file.evidence) {
            strongest = std::max(strongest, evidence.confidence);
        }
        if (strongest > policy.weakEvidenceThreshold) {
            continue;
        }

        const std::uint64_t reference = std::max(file.updatedAtMs, file.lastEvidenceAtMs);
        if (nowMs < reference) {
            continue;
        }
        const std::uint64_t age = nowMs - reference;
        const std::uint64_t threshold = file.severity == CrimeSeverity::Minor
            ? policy.minorWeakCaseExpiryMs
            : policy.moderateWeakCaseExpiryMs;
        if (age < threshold) {
            continue;
        }

        if (resolveCase(file.id, CaseResolution::ExpiredWeakEvidence, nowMs)) {
            ++resolved;
        }
    }
    return resolved;
}

float CrimeRegistry::aggregateConfidence(
    const LogicalId caseId,
    const EvidenceKind kind) const noexcept {

    const CaseFile* file = findCase(caseId);
    if (file == nullptr) {
        return 0.0f;
    }

    std::map<std::string, float> independent;
    std::size_t anonymousIndex = 0;
    for (const auto& evidence : file->evidence) {
        if (evidence.kind != kind || !confidenceValid(evidence.confidence)) {
            continue;
        }
        const std::string key = evidence.independenceKey.empty()
            ? "__independent_" + std::to_string(anonymousIndex++) + '_' + evidence.dedupKey
            : evidence.independenceKey;
        auto [it, inserted] = independent.emplace(key, evidence.confidence);
        if (!inserted) {
            it->second = std::max(it->second, evidence.confidence);
        }
    }

    double remainingUncertainty = 1.0;
    for (const auto& [key, confidence] : independent) {
        (void)key;
        remainingUncertainty *= 1.0 - static_cast<double>(confidence);
    }
    return static_cast<float>(std::clamp(1.0 - remainingUncertainty, 0.0, 1.0));
}

const CaseFile* CrimeRegistry::findCase(const LogicalId caseId) const noexcept {
    const auto found = cases_.find(caseId);
    return found == cases_.end() ? nullptr : &found->second;
}

CaseFile* CrimeRegistry::findCaseMutable(const LogicalId caseId) noexcept {
    const auto found = cases_.find(caseId);
    return found == cases_.end() ? nullptr : &found->second;
}

const CrimeEvent* CrimeRegistry::findCrime(const LogicalId crimeId) const noexcept {
    const auto found = crimes_.find(crimeId);
    return found == crimes_.end() ? nullptr : &found->second;
}

const CaseFile* CrimeRegistry::findByIncidentKey(const std::string_view incidentKey) const noexcept {
    if (incidentKey.empty()) {
        return nullptr;
    }
    const auto found = incidentToCase_.find(std::string(incidentKey));
    return found == incidentToCase_.end() ? nullptr : findCase(found->second);
}

std::vector<const CaseFile*> CrimeRegistry::cases() const {
    std::vector<const CaseFile*> result;
    result.reserve(cases_.size());
    for (const auto& [id, file] : cases_) {
        (void)id;
        result.push_back(&file);
    }
    std::sort(result.begin(), result.end(), [](const CaseFile* left, const CaseFile* right) {
        return left->id < right->id;
    });
    return result;
}

std::vector<const CrimeEvent*> CrimeRegistry::crimesForCase(const LogicalId caseId) const {
    std::vector<const CrimeEvent*> result;
    const CaseFile* file = findCase(caseId);
    if (file == nullptr) {
        return result;
    }
    result.reserve(file->crimeIds.size());
    for (const LogicalId crimeId : file->crimeIds) {
        if (const CrimeEvent* event = findCrime(crimeId); event != nullptr) {
            result.push_back(event);
        }
    }
    return result;
}

void CrimeRegistry::clear() {
    cases_.clear();
    crimes_.clear();
    incidentToCase_.clear();
}

bool CrimeRegistry::importCase(
    CaseFile file,
    std::vector<CrimeEvent> crimes,
    std::string* reason) {

    const auto reject = [reason](const std::string& message) {
        if (reason != nullptr) {
            *reason = message;
        }
        return false;
    };

    if (file.id == 0 || logicalIdDomain(file.id) != LogicalIdDomain::Case) {
        return reject("case has invalid logical ID domain");
    }
    if (cases_.contains(file.id)) {
        return reject("duplicate case ID");
    }
    if (file.identityConfidence < 0.0f || file.identityConfidence > 1.0f
        || !std::isfinite(file.identityConfidence)) {
        return reject("case has invalid identity confidence");
    }

    std::set<LogicalId> expected(file.crimeIds.begin(), file.crimeIds.end());
    if (expected.size() != file.crimeIds.size() || expected.size() != crimes.size()) {
        return reject("case crime ID list is duplicate or does not match persisted crime records");
    }
    for (const auto& evidence : file.evidence) {
        if (!confidenceValid(evidence.confidence)) {
            return reject("case contains invalid evidence confidence");
        }
    }
    for (const auto& event : crimes) {
        if (event.id == 0 || logicalIdDomain(event.id) != LogicalIdDomain::Crime
            || event.caseId != file.id || !expected.contains(event.id)
            || crimes_.contains(event.id)) {
            return reject("case contains invalid or duplicate crime record");
        }
    }

    const LogicalId caseId = file.id;
    const std::string incidentKey = file.incidentKey;
    for (auto& event : crimes) {
        crimes_.emplace(event.id, std::move(event));
    }
    cases_.emplace(caseId, std::move(file));
    if (!incidentKey.empty()) {
        const CaseFile* imported = findCase(caseId);
        if (imported != nullptr && !isTerminal(imported->state)) {
            const auto [it, inserted] = incidentToCase_.emplace(incidentKey, caseId);
            if (!inserted && it->second != caseId) {
                clear();
                return reject("two active cases share one incident key");
            }
        }
    }
    return true;
}

bool CrimeRegistry::evidenceDuplicate(
    const CaseFile& file,
    const EvidenceRecord& evidence) const {

    const std::string fingerprint = evidenceFingerprint(evidence);
    return std::any_of(file.evidence.begin(), file.evidence.end(), [&](const EvidenceRecord& existing) {
        return evidenceFingerprint(existing) == fingerprint;
    });
}

void CrimeRegistry::refreshCaseSeverity(CaseFile& file) {
    CrimeSeverity severity = CrimeSeverity::Minor;
    for (const LogicalId crimeId : file.crimeIds) {
        if (const CrimeEvent* event = findCrime(crimeId); event != nullptr) {
            severity = maxSeverity(severity, event->severity);
        }
    }
    file.severity = severity;
}

} // namespace gco::crime
