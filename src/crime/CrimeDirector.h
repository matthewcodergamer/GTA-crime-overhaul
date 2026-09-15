#pragma once

#include "CrimeRegistry.h"

namespace gco::crime {

class CrimeDirector final {
public:
    CrimeDirector(CrimeRegistry& registry, LogicalIdGenerator& ids, EventBus& events)
        : registry_(registry), ids_(ids), events_(events) {}

    CrimeRecordResult recordCrime(const CrimeOccurrence& occurrence);
    bool escalateCrime(LogicalId crimeId, CrimeSeverity severity, std::uint64_t nowMs);
    EvidenceAddResult addEvidence(LogicalId caseId, EvidenceRecord evidence);

    bool beginReporting(LogicalId caseId, std::uint64_t nowMs);
    bool markReported(LogicalId caseId, std::uint64_t nowMs);
    bool beginInvestigation(LogicalId caseId, std::uint64_t nowMs);
    bool markUnknownSuspect(LogicalId caseId, float identityConfidence, std::uint64_t nowMs);
    bool markIdentifiedSuspect(LogicalId caseId, float identityConfidence, std::uint64_t nowMs);
    bool issueBoloOrWarrant(
        LogicalId caseId,
        bool personWarrant,
        bool vehicleBolo,
        std::uint64_t nowMs);
    bool beginPursuitOrSearch(LogicalId caseId, std::uint64_t nowMs);
    bool markDormant(LogicalId caseId, std::uint64_t nowMs);
    bool resolve(LogicalId caseId, CaseResolution resolution, std::uint64_t nowMs);
    bool setImmediateResponse(
        LogicalId caseId,
        const ImmediateResponseState& immediate,
        std::uint64_t nowMs);
    std::size_t applyDecay(std::uint64_t nowMs, const CaseDecayPolicy& policy = {});

private:
    bool transitionAndPublish(LogicalId caseId, CaseState nextState, std::uint64_t nowMs);
    void publishCaseEvent(std::string topic, LogicalId caseId, std::string payload = {});

    CrimeRegistry& registry_;
    LogicalIdGenerator& ids_;
    EventBus& events_;
};

} // namespace gco::crime
