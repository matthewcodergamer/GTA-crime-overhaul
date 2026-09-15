#include "CrimeDirector.h"

#include <sstream>

namespace gco::crime {

CrimeRecordResult CrimeDirector::recordCrime(const CrimeOccurrence& occurrence) {
    const CrimeRecordResult result = registry_.recordCrime(occurrence, ids_);
    if (result.crimeId == 0 || result.caseId == 0) {
        return result;
    }

    std::ostringstream payload;
    payload << "crimeId=" << result.crimeId
            << ";type=" << crimeTypeName(occurrence.type)
            << ";merged=" << (result.mergedIntoExistingCase ? "true" : "false");
    publishCaseEvent("crime.created", result.caseId, payload.str());
    return result;
}

bool CrimeDirector::escalateCrime(
    const LogicalId crimeId,
    const CrimeSeverity severity,
    const std::uint64_t nowMs) {

    const CrimeEvent* before = registry_.findCrime(crimeId);
    if (before == nullptr) {
        return false;
    }
    const LogicalId caseId = before->caseId;
    if (!registry_.escalateCrime(crimeId, severity, nowMs)) {
        return false;
    }
    publishCaseEvent(
        "crime.escalated",
        caseId,
        "crimeId=" + std::to_string(crimeId) + ";severity=" + std::string(crimeSeverityName(severity)));
    return true;
}

EvidenceAddResult CrimeDirector::addEvidence(const LogicalId caseId, EvidenceRecord evidence) {
    const EvidenceKind kind = evidence.kind;
    const EvidenceAddResult result = registry_.addEvidence(caseId, std::move(evidence));
    if (result == EvidenceAddResult::Added) {
        publishCaseEvent("case.evidence_added", caseId, std::string(evidenceKindName(kind)));
    }
    return result;
}

bool CrimeDirector::beginReporting(const LogicalId caseId, const std::uint64_t nowMs) {
    return transitionAndPublish(caseId, CaseState::Reporting, nowMs);
}

bool CrimeDirector::markReported(const LogicalId caseId, const std::uint64_t nowMs) {
    return transitionAndPublish(caseId, CaseState::Reported, nowMs);
}

bool CrimeDirector::beginInvestigation(const LogicalId caseId, const std::uint64_t nowMs) {
    return transitionAndPublish(caseId, CaseState::Investigating, nowMs);
}

bool CrimeDirector::markUnknownSuspect(
    const LogicalId caseId,
    const float identityConfidence,
    const std::uint64_t nowMs) {

    if (!registry_.setSuspectKnowledge(caseId, SuspectKnowledge::DescriptionOnly, identityConfidence, nowMs)) {
        return false;
    }
    return transitionAndPublish(caseId, CaseState::UnknownSuspect, nowMs);
}

bool CrimeDirector::markIdentifiedSuspect(
    const LogicalId caseId,
    const float identityConfidence,
    const std::uint64_t nowMs) {

    if (!registry_.setSuspectKnowledge(caseId, SuspectKnowledge::Identified, identityConfidence, nowMs)) {
        return false;
    }
    if (!transitionAndPublish(caseId, CaseState::IdentifiedSuspect, nowMs)) {
        return false;
    }
    publishCaseEvent("case.suspect_identified", caseId, "confidence=" + std::to_string(identityConfidence));
    return true;
}

bool CrimeDirector::issueBoloOrWarrant(
    const LogicalId caseId,
    const bool personWarrant,
    const bool vehicleBolo,
    const std::uint64_t nowMs) {

    if (!personWarrant && !vehicleBolo) {
        return false;
    }
    if (!registry_.setBoloOrWarrant(caseId, personWarrant, vehicleBolo, nowMs)) {
        return false;
    }
    if (!transitionAndPublish(caseId, CaseState::BoloOrWarrant, nowMs)) {
        return false;
    }
    if (personWarrant) {
        publishCaseEvent("case.person_warrant_issued", caseId);
    }
    if (vehicleBolo) {
        publishCaseEvent("case.vehicle_bolo_issued", caseId);
    }
    return true;
}

bool CrimeDirector::beginPursuitOrSearch(const LogicalId caseId, const std::uint64_t nowMs) {
    return transitionAndPublish(caseId, CaseState::PursuitOrSearch, nowMs);
}

bool CrimeDirector::markDormant(const LogicalId caseId, const std::uint64_t nowMs) {
    return transitionAndPublish(caseId, CaseState::Dormant, nowMs);
}

bool CrimeDirector::resolve(
    const LogicalId caseId,
    const CaseResolution resolution,
    const std::uint64_t nowMs) {

    if (!registry_.resolveCase(caseId, resolution, nowMs)) {
        return false;
    }
    publishCaseEvent("case.resolved", caseId, std::string(caseResolutionName(resolution)));
    return true;
}

bool CrimeDirector::setImmediateResponse(
    const LogicalId caseId,
    const ImmediateResponseState& immediate,
    const std::uint64_t nowMs) {

    if (!registry_.setImmediateResponse(caseId, immediate, nowMs)) {
        return false;
    }
    std::ostringstream payload;
    payload << "active=" << (immediate.active ? "true" : "false")
            << ";reportPending=" << (immediate.reportPending ? "true" : "false")
            << ";pursuitActive=" << (immediate.pursuitActive ? "true" : "false")
            << ";tacticalLevel=" << static_cast<unsigned>(immediate.tacticalLevel);
    publishCaseEvent("case.immediate_response_changed", caseId, payload.str());
    return true;
}

std::size_t CrimeDirector::applyDecay(
    const std::uint64_t nowMs,
    const CaseDecayPolicy& policy) {

    const std::size_t resolved = registry_.applyDecay(nowMs, policy);
    if (resolved > 0) {
        events_.publish(RuntimeEvent{"case.decay_housekeeping", 0, "resolved=" + std::to_string(resolved)});
    }
    return resolved;
}

bool CrimeDirector::transitionAndPublish(
    const LogicalId caseId,
    const CaseState nextState,
    const std::uint64_t nowMs) {

    const CaseFile* before = registry_.findCase(caseId);
    if (before == nullptr) {
        return false;
    }
    const CaseState previous = before->state;
    if (!registry_.transitionCase(caseId, nextState, nowMs)) {
        return false;
    }
    publishCaseEvent(
        "case.state_changed",
        caseId,
        std::string(caseStateName(previous)) + "->" + std::string(caseStateName(nextState)));
    return true;
}

void CrimeDirector::publishCaseEvent(
    std::string topic,
    const LogicalId caseId,
    std::string payload) {
    events_.publish(RuntimeEvent{std::move(topic), caseId, std::move(payload)});
}

} // namespace gco::crime
