#pragma once

#include "crime/CrimeRegistry.h"
#include "dialogue/InvestigationDialogue.h"
#include "platform/PlatformTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gco::investigation {

enum class PoliceResponseType : std::uint8_t {
    None,
    Patrol,
    MultiUnit,
    Armed,
    Critical
};

enum class SceneDetailLevel : std::uint8_t {
    Abstract,
    HighDetail
};

enum class SceneTaskKind : std::uint8_t {
    Approach,
    Perimeter,
    CheckInterior,
    CheckBody,
    CheckAbandonedVehicle,
    CheckExit,
    InterviewWitness,
    RadioUpdate,
    Idle
};

enum class PresentationStyle : std::uint8_t {
    Idle,
    Clipboard,
    Phone,
    Radio,
    FaceWitness
};

enum class InterviewSemanticEvent : std::uint8_t {
    FaceSeen,
    FaceNotSeen,
    MaskSeen,
    ClothingSeen,
    WeaponSeen,
    VehicleSeen,
    VehicleNotSeen,
    PlateSeen,
    PlateNotSeen,
    DirectionKnown,
    ShotsReported,
    InjuryOrBodySeen
};

struct DispatchFacts final {
    crime::CrimeSeverity severity = crime::CrimeSeverity::Minor;
    bool armed = false;
    bool shotsFired = false;
    bool injuries = false;
    bool death = false;
    bool officerAttack = false;
};

struct SceneGeometry final {
    crime::CrimeLocation center{};
    std::vector<crime::CrimeLocation> interiorPoints;
    std::vector<crime::CrimeLocation> exitPoints;
};

struct SceneTask final {
    SceneTaskKind kind = SceneTaskKind::Idle;
    crime::CrimeLocation target{};
    PresentationStyle presentation = PresentationStyle::Idle;
    std::string detail;
};

struct DispatchPlan final {
    LogicalId caseId = 0;
    std::optional<LogicalId> businessId;
    PoliceResponseType response = PoliceResponseType::None;
    std::uint8_t tacticalLevel = 0;
    std::uint8_t officerCount = 0;
    std::uint64_t abstractArrivalDelayMs = 0;
    SceneGeometry geometry{};
    std::vector<SceneTask> tasks;
};

struct SceneRecord final {
    LogicalId caseId = 0;
    std::optional<LogicalId> businessId;
    PoliceResponseType response = PoliceResponseType::None;
    SceneDetailLevel detailLevel = SceneDetailLevel::Abstract;
    SceneGeometry geometry{};
    std::vector<SceneTask> tasks;
    std::uint64_t dispatchedAtMs = 0;
    std::uint64_t expectedArrivalAtMs = 0;
    std::uint64_t arrivedAtMs = 0;
    std::uint64_t lastHighDetailAtMs = 0;
    std::uint64_t expiresAtMs = 0;
    std::uint64_t businessRecoveryAtMs = 0;
    bool logicalArrivalRecorded = false;
    bool investigationStarted = false;
    bool recoveryEventPublished = false;
};

struct InterviewFact final {
    InterviewSemanticEvent semantic = InterviewSemanticEvent::FaceNotSeen;
    crime::EvidenceKind evidenceKind = crime::EvidenceKind::CrimeObserved;
    float confidence = 0.0f;
    std::uint64_t observedAtMs = 0;
    std::string sourceKey;
    std::string descriptor;
    crime::CrimeLocation location{};
};

struct InterviewSourceSummary final {
    std::string sourceKey;
    bool hasReport = false;
    std::vector<InterviewFact> facts;
    dialogue::WitnessStatementFacts dialogueFacts{};
};

[[nodiscard]] DispatchFacts deriveDispatchFacts(
    const crime::CaseFile& file,
    const std::vector<const crime::CrimeEvent*>& crimes) noexcept;
[[nodiscard]] DispatchPlan makeDispatchPlan(
    const crime::CaseFile& file,
    const std::vector<const crime::CrimeEvent*>& crimes,
    SceneGeometry geometry);
[[nodiscard]] InterviewSourceSummary deriveInterviewSummary(
    const crime::CaseFile& file,
    std::string_view sourceKey);
[[nodiscard]] std::vector<std::string> reportingWitnessSourceKeys(const crime::CaseFile& file);
[[nodiscard]] bool isWitnessEvidence(const crime::EvidenceRecord& evidence) noexcept;
[[nodiscard]] std::string interviewSemanticName(InterviewSemanticEvent value);
[[nodiscard]] std::string_view policeResponseTypeName(PoliceResponseType value) noexcept;
[[nodiscard]] std::string_view sceneDetailLevelName(SceneDetailLevel value) noexcept;
[[nodiscard]] std::string_view sceneTaskKindName(SceneTaskKind value) noexcept;

} // namespace gco::investigation
