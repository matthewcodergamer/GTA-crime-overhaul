#pragma once

#include "CrimeDomain.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gco::crime {

enum class EvidenceAddResult : std::uint8_t {
    Added,
    Duplicate,
    CaseMissing,
    Invalid
};

class CrimeRegistry final {
public:
    CrimeRecordResult recordCrime(const CrimeOccurrence& occurrence, LogicalIdGenerator& ids);
    bool escalateCrime(LogicalId crimeId, CrimeSeverity severity, std::uint64_t nowMs);

    EvidenceAddResult addEvidence(LogicalId caseId, EvidenceRecord evidence);
    bool transitionCase(LogicalId caseId, CaseState nextState, std::uint64_t nowMs);
    bool setSuspectKnowledge(
        LogicalId caseId,
        SuspectKnowledge knowledge,
        float identityConfidence,
        std::uint64_t nowMs);
    bool setImmediateResponse(
        LogicalId caseId,
        const ImmediateResponseState& immediate,
        std::uint64_t nowMs);
    bool setBoloOrWarrant(
        LogicalId caseId,
        bool personWarrant,
        bool vehicleBolo,
        std::uint64_t nowMs);
    bool resolveCase(LogicalId caseId, CaseResolution resolution, std::uint64_t nowMs);

    std::size_t applyDecay(std::uint64_t nowMs, const CaseDecayPolicy& policy = {});

    [[nodiscard]] float aggregateConfidence(LogicalId caseId, EvidenceKind kind) const noexcept;
    [[nodiscard]] const CaseFile* findCase(LogicalId caseId) const noexcept;
    [[nodiscard]] CaseFile* findCaseMutable(LogicalId caseId) noexcept;
    [[nodiscard]] const CrimeEvent* findCrime(LogicalId crimeId) const noexcept;
    [[nodiscard]] const CaseFile* findByIncidentKey(std::string_view incidentKey) const noexcept;

    [[nodiscard]] std::vector<const CaseFile*> cases() const;
    [[nodiscard]] std::vector<const CrimeEvent*> crimesForCase(LogicalId caseId) const;
    [[nodiscard]] std::size_t caseCount() const noexcept { return cases_.size(); }
    [[nodiscard]] std::size_t crimeCount() const noexcept { return crimes_.size(); }

    void clear();

    // Persistence import path. It validates ownership/domain invariants and never accepts raw GTA handles.
    bool importCase(CaseFile file, std::vector<CrimeEvent> crimes, std::string* reason = nullptr);

private:
    [[nodiscard]] bool evidenceDuplicate(const CaseFile& file, const EvidenceRecord& evidence) const;
    void refreshCaseSeverity(CaseFile& file);

    std::unordered_map<LogicalId, CaseFile> cases_;
    std::unordered_map<LogicalId, CrimeEvent> crimes_;
    std::unordered_map<std::string, LogicalId> incidentToCase_;
};

} // namespace gco::crime
