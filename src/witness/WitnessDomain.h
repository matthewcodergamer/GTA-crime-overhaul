#pragma once

#include "CoreServices.h"
#include "platform/PlatformTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gco::witness {

enum class WeaponClass : std::uint8_t {
    Unknown,
    Unarmed,
    Melee,
    Handgun,
    Smg,
    Shotgun,
    Rifle,
    MachineGun,
    Sniper,
    Heavy,
    Thrown
};

enum class FaceCoverKnowledge : std::uint8_t {
    Unknown,
    FaceVisible,
    FaceCovered
};

enum class WitnessReaction : std::uint8_t {
    Freeze,
    Hide,
    Flee,
    CallPolice,
    PanicButton,
    Shout,
    RefuseToReport
};

enum class ReportingState : std::uint8_t {
    None,
    Waiting,
    Reporting,
    PartialReported,
    Reported,
    Interrupted,
    Refused
};

struct WitnessEmotion final {
    float fear = 0.5f;
    float panic = 0.3f;
    float defiance = 0.2f;
};

template <typename T>
struct ObservedFact final {
    bool observed = false;
    T value{};
    float confidence = 0.0f;
    std::uint64_t observedAtMs = 0;
};

struct VehicleVisual final {
    std::uint32_t modelHash = 0;
    int primaryColor = 0;
    int secondaryColor = 0;
};

struct DirectionVisual final {
    platform::Vec3 position{};
    float heading = 0.0f;
};

struct WitnessObservation final {
    LogicalId caseId = 0;
    LogicalId crimeId = 0;
    bool heardThreat = false;
    bool heardGunshot = false;
    bool heardWitnessViolence = false;
    bool sawCrime = false;
    bool sawWitnessViolence = false;
    std::uint64_t firstAwareAtMs = 0;
    std::uint64_t lastObservedAtMs = 0;
    std::uint64_t accumulatedVisualMs = 0;
    float bestVisualConfidence = 0.0f;

    ObservedFact<FaceCoverKnowledge> faceCover{};
    ObservedFact<std::string> outfit{};
    ObservedFact<WeaponClass> weapon{};
    ObservedFact<VehicleVisual> vehicle{};
    ObservedFact<std::string> plate{};
    ObservedFact<DirectionVisual> lastKnownDirection{};

    [[nodiscard]] bool meaningful() const noexcept;
};

struct WitnessPerceptionPolicy final {
    float visualRadius = 32.0f;
    float hearingRadius = 42.0f;
    float halfFovDegrees = 62.0f;
    float minimumVisualConfidence = 0.12f;
    float faceEvidenceThreshold = 0.38f;
    float outfitEvidenceThreshold = 0.20f;
    float weaponEvidenceThreshold = 0.24f;
    float vehicleEvidenceThreshold = 0.20f;
    float plateEvidenceThreshold = 0.46f;
    float directionEvidenceThreshold = 0.16f;
    std::uint64_t plateMinimumViewMs = 700;
    std::uint64_t faceMinimumViewMs = 350;
};

struct PerceptionSample final {
    float distance = 0.0f;
    bool inFov = false;
    bool clearLos = false;
    float fovQuality = 0.0f;
    float lightingFactor = 1.0f;
    std::uint64_t visualDeltaMs = 0;

    bool heardThreat = false;
    bool heardGunshot = false;
    bool heardWitnessViolence = false;
    float hearingStrength = 0.0f;

    float faceViewQuality = 0.0f;
    FaceCoverKnowledge faceCover = FaceCoverKnowledge::Unknown;
    std::string outfitSignature;
    bool weaponVisible = false;
    WeaponClass weaponClass = WeaponClass::Unknown;
    bool vehicleVisible = false;
    VehicleVisual vehicle{};
    float plateViewQuality = 0.0f;
    std::string plate;
    bool directionVisible = false;
    DirectionVisual direction{};
    bool sawWitnessViolence = false;
};

struct ReportingPlan final {
    WitnessReaction reaction = WitnessReaction::Freeze;
    ReportingState state = ReportingState::None;
    std::uint64_t eligibleAtMs = 0;
    std::uint64_t startedAtMs = 0;
    std::uint64_t partialAtMs = 0;
    std::uint64_t completeAtMs = 0;
    bool basicFactsCommitted = false;
    bool detailedFactsCommitted = false;
};

enum class ReportingAdvance : std::uint8_t {
    None,
    Begin,
    CommitBasic,
    CommitDetailed,
    Interrupted,
    Refused
};

class StaggeredScanCursor final {
public:
    explicit StaggeredScanCursor(std::size_t samplesPerTick = 3) noexcept
        : samplesPerTick_(samplesPerTick) {}

    [[nodiscard]] std::vector<std::size_t> next(std::size_t candidateCount);
    void reset() noexcept { cursor_ = 0; }
    void setSamplesPerTick(std::size_t value) noexcept { samplesPerTick_ = value; }

private:
    std::size_t samplesPerTick_ = 3;
    std::size_t cursor_ = 0;
};

[[nodiscard]] float clamp01(float value) noexcept;
[[nodiscard]] float angularFrontQuality(
    const platform::Vec3& origin,
    float headingDegrees,
    const platform::Vec3& target,
    float halfFovDegrees) noexcept;
[[nodiscard]] float visualConfidence(
    float distance,
    float visualRadius,
    float fovQuality,
    float lightingFactor,
    bool clearLos,
    std::uint64_t accumulatedViewMs) noexcept;
[[nodiscard]] float plateGeometryQuality(
    const platform::Vec3& witnessPosition,
    const platform::Vec3& vehiclePosition,
    float vehicleHeadingDegrees,
    float distance) noexcept;
[[nodiscard]] std::string outfitSignature(const platform::PedSnapshot& ped);

void applyPerceptionSample(
    WitnessObservation& observation,
    const PerceptionSample& sample,
    std::uint64_t nowMs,
    const WitnessPerceptionPolicy& policy = {});

[[nodiscard]] WitnessEmotion makeEmotion(std::uint64_t seed) noexcept;
[[nodiscard]] WitnessReaction chooseReaction(
    const WitnessEmotion& emotion,
    bool canUsePanicButton,
    bool hasMeaningfulObservation,
    std::uint64_t seed) noexcept;
[[nodiscard]] ReportingPlan makeReportingPlan(
    const WitnessEmotion& emotion,
    WitnessReaction reaction,
    std::uint64_t nowMs,
    std::uint64_t seed) noexcept;
ReportingAdvance advanceReporting(ReportingPlan& plan, std::uint64_t nowMs, bool interrupted) noexcept;

[[nodiscard]] std::string_view weaponClassName(WeaponClass value) noexcept;
[[nodiscard]] std::string_view faceCoverKnowledgeName(FaceCoverKnowledge value) noexcept;
[[nodiscard]] std::string_view witnessReactionName(WitnessReaction value) noexcept;
[[nodiscard]] std::string_view reportingStateName(ReportingState value) noexcept;

} // namespace gco::witness
