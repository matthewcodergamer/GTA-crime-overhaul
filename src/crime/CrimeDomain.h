#pragma once

#include "CoreServices.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gco::crime {

enum class CrimeType : std::uint8_t {
    Robbery,
    ArmedRobbery,
    Assault,
    Homicide,
    OfficerAssault,
    VehicleTheft,
    PropertyDamage,
    Count
};

enum class CrimeSeverity : std::uint8_t {
    Minor = 0,
    Moderate = 1,
    Serious = 2,
    Major = 3,
    Critical = 4
};

enum class CaseState : std::uint8_t {
    Unobserved,
    Observed,
    Reporting,
    Reported,
    Investigating,
    UnknownSuspect,
    IdentifiedSuspect,
    BoloOrWarrant,
    PursuitOrSearch,
    Dormant,
    Resolved
};

enum class SuspectKnowledge : std::uint8_t {
    Unknown,
    DescriptionOnly,
    ProbableIdentity,
    Identified
};

enum class CaseResolution : std::uint8_t {
    None,
    ExpiredWeakEvidence,
    Arrested,
    AdministrativelyResolved
};

enum class EvidenceSource : std::uint8_t {
    Witness,
    Camera,
    Officer,
    Alarm,
    Vehicle,
    SceneDiscovery,
    SyntheticDebug
};

enum class EvidenceKind : std::uint8_t {
    CrimeObserved,
    Face,
    Mask,
    Clothing,
    Weapon,
    Vehicle,
    Plate,
    Direction,
    Location,
    Identity,
    Injury,
    Body,
    PropertyDamage
};

struct CrimeLocation final {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    std::string zoneTag;
};

struct ImmediateResponseState final {
    bool active = false;
    bool reportPending = false;
    bool pursuitActive = false;
    std::uint8_t tacticalLevel = 0;
    std::uint64_t lastUpdatedAtMs = 0;
};

struct CrimeEvent final {
    LogicalId id = 0;
    LogicalId caseId = 0;
    CrimeType type = CrimeType::Robbery;
    CrimeSeverity severity = CrimeSeverity::Minor;
    CrimeLocation location{};
    std::optional<LogicalId> businessId;
    std::uint64_t occurredAtMs = 0;
};

struct EvidenceSnapshot final {
    // Historical presentation/value data only. Runtime GTA handles are forbidden here.
    std::string descriptor;
    CrimeLocation location{};
    std::optional<LogicalId> sourceLogicalId;
    std::optional<LogicalId> relatedVehicleId;
};

struct EvidenceRecord final {
    EvidenceSource source = EvidenceSource::Witness;
    EvidenceKind kind = EvidenceKind::CrimeObserved;
    float confidence = 0.0f;
    std::uint64_t observedAtMs = 0;
    // Records with the same non-empty independenceKey are treated as one source family when
    // confidence is fused, preventing repeated copies of one report from multiplying certainty.
    std::string independenceKey;
    // Exact duplicate suppression key. Empty means derive one from stable record fields.
    std::string dedupKey;
    EvidenceSnapshot snapshot{};
};

struct CaseFile final {
    static constexpr std::uint32_t ModelVersion = 1;

    LogicalId id = 0;
    std::string incidentKey;
    std::vector<LogicalId> crimeIds;
    CaseState state = CaseState::Unobserved;
    CrimeSeverity severity = CrimeSeverity::Minor;
    SuspectKnowledge suspectKnowledge = SuspectKnowledge::Unknown;
    float identityConfidence = 0.0f;
    bool activePersonWarrant = false;
    bool activeVehicleBolo = false;
    ImmediateResponseState immediate{};
    std::vector<EvidenceRecord> evidence;
    std::uint64_t createdAtMs = 0;
    std::uint64_t updatedAtMs = 0;
    std::uint64_t lastEvidenceAtMs = 0;
    CaseResolution resolution = CaseResolution::None;
};

struct CrimeOccurrence final {
    CrimeType type = CrimeType::Robbery;
    CrimeLocation location{};
    std::optional<LogicalId> businessId;
    std::uint64_t occurredAtMs = 0;
    // A caller-owned stable incident/session key. Non-empty matching keys merge crimes into the
    // same unresolved case. We deliberately do not guess case identity from proximity alone.
    std::string incidentKey;
};

struct CrimeRecordResult final {
    LogicalId crimeId = 0;
    LogicalId caseId = 0;
    bool mergedIntoExistingCase = false;
};

struct CaseDecayPolicy final {
    std::uint64_t minorWeakCaseExpiryMs = 24ull * 60ull * 60ull * 1000ull;
    std::uint64_t moderateWeakCaseExpiryMs = 72ull * 60ull * 60ull * 1000ull;
    float weakEvidenceThreshold = 0.35f;
};

[[nodiscard]] CrimeSeverity baseSeverity(CrimeType type) noexcept;
[[nodiscard]] CrimeSeverity maxSeverity(CrimeSeverity left, CrimeSeverity right) noexcept;
[[nodiscard]] bool isMajorCase(CrimeSeverity severity) noexcept;
[[nodiscard]] bool canTransition(CaseState from, CaseState to) noexcept;
[[nodiscard]] bool isTerminal(CaseState state) noexcept;
[[nodiscard]] std::string evidenceFingerprint(const EvidenceRecord& evidence);

[[nodiscard]] std::string_view crimeTypeName(CrimeType value) noexcept;
[[nodiscard]] std::string_view crimeSeverityName(CrimeSeverity value) noexcept;
[[nodiscard]] std::string_view caseStateName(CaseState value) noexcept;
[[nodiscard]] std::string_view suspectKnowledgeName(SuspectKnowledge value) noexcept;
[[nodiscard]] std::string_view caseResolutionName(CaseResolution value) noexcept;
[[nodiscard]] std::string_view evidenceSourceName(EvidenceSource value) noexcept;
[[nodiscard]] std::string_view evidenceKindName(EvidenceKind value) noexcept;

[[nodiscard]] std::optional<CrimeType> crimeTypeFromName(std::string_view value) noexcept;
[[nodiscard]] std::optional<CrimeSeverity> crimeSeverityFromName(std::string_view value) noexcept;
[[nodiscard]] std::optional<CaseState> caseStateFromName(std::string_view value) noexcept;
[[nodiscard]] std::optional<SuspectKnowledge> suspectKnowledgeFromName(std::string_view value) noexcept;
[[nodiscard]] std::optional<CaseResolution> caseResolutionFromName(std::string_view value) noexcept;
[[nodiscard]] std::optional<EvidenceSource> evidenceSourceFromName(std::string_view value) noexcept;
[[nodiscard]] std::optional<EvidenceKind> evidenceKindFromName(std::string_view value) noexcept;

} // namespace gco::crime
