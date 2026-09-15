#include "WitnessDomain.h"

#include <algorithm>
#include <cmath>

namespace gco::witness {
namespace {

constexpr float kPi = 3.14159265358979323846f;

std::uint64_t mix64(std::uint64_t value) noexcept {
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31U);
}

float unitRoll(const std::uint64_t seed, const std::uint64_t salt) noexcept {
    return static_cast<float>(mix64(seed ^ salt) & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

template <typename T>
void updateFact(ObservedFact<T>& fact, const T& value, const float confidence, const std::uint64_t nowMs) {
    if (!fact.observed || confidence > fact.confidence + 0.01f) {
        fact.observed = true;
        fact.value = value;
        fact.confidence = clamp01(confidence);
        fact.observedAtMs = nowMs;
    }
}

float durationQuality(const std::uint64_t accumulatedViewMs) noexcept {
    if (accumulatedViewMs == 0) return 0.0f;
    return clamp01(static_cast<float>(accumulatedViewMs) / 2200.0f);
}

} // namespace

bool WitnessObservation::meaningful() const noexcept {
    return heardThreat || heardGunshot || heardWitnessViolence || sawCrime || sawWitnessViolence
        || faceCover.observed || faceIdentity.observed || outfit.observed || weapon.observed || vehicle.observed
        || plate.observed || lastKnownDirection.observed;
}

std::vector<std::size_t> StaggeredScanCursor::next(const std::size_t candidateCount) {
    std::vector<std::size_t> result;
    if (candidateCount == 0 || samplesPerTick_ == 0) {
        cursor_ = 0;
        return result;
    }
    const std::size_t count = std::min(samplesPerTick_, candidateCount);
    result.reserve(count);
    cursor_ %= candidateCount;
    for (std::size_t i = 0; i < count; ++i) {
        result.push_back((cursor_ + i) % candidateCount);
    }
    cursor_ = (cursor_ + count) % candidateCount;
    return result;
}

float clamp01(const float value) noexcept {
    if (!std::isfinite(value)) return 0.0f;
    return std::clamp(value, 0.0f, 1.0f);
}

float angularFrontQuality(
    const platform::Vec3& origin,
    const float headingDegrees,
    const platform::Vec3& target,
    const float halfFovDegrees) noexcept {

    if (!std::isfinite(headingDegrees) || !std::isfinite(halfFovDegrees) || halfFovDegrees <= 0.0f) {
        return 0.0f;
    }
    const float dx = target.x - origin.x;
    const float dy = target.y - origin.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.001f) return 1.0f;

    const float heading = headingDegrees * kPi / 180.0f;
    const float fx = -std::sin(heading);
    const float fy = std::cos(heading);
    const float dot = std::clamp((fx * dx + fy * dy) / length, -1.0f, 1.0f);
    const float angle = std::acos(dot) * 180.0f / kPi;
    if (angle > halfFovDegrees) return 0.0f;
    return clamp01(1.0f - angle / std::max(1.0f, halfFovDegrees));
}

float visualConfidence(
    const float distance,
    const float visualRadius,
    const float fovQuality,
    const float lightingFactor,
    const bool clearLos,
    const std::uint64_t accumulatedViewMs) noexcept {

    if (!clearLos || visualRadius <= 0.0f || distance < 0.0f || distance > visualRadius) return 0.0f;
    const float distanceQuality = clamp01(1.0f - distance / visualRadius);
    const float view = 0.35f + 0.65f * durationQuality(accumulatedViewMs);
    return clamp01(distanceQuality * clamp01(fovQuality) * clamp01(lightingFactor) * view);
}

float plateGeometryQuality(
    const platform::Vec3& witnessPosition,
    const platform::Vec3& vehiclePosition,
    const float vehicleHeadingDegrees,
    const float distance) noexcept {

    if (distance < 0.0f || distance > 18.0f) return 0.0f;
    const float longitudinal = angularFrontQuality(vehiclePosition, vehicleHeadingDegrees, witnessPosition, 42.0f);
    const float rearward = angularFrontQuality(vehiclePosition, vehicleHeadingDegrees + 180.0f, witnessPosition, 42.0f);
    const float axisQuality = std::max(longitudinal, rearward);
    const float distanceQuality = clamp01(1.0f - distance / 18.0f);
    return clamp01(axisQuality * distanceQuality);
}

void applyPerceptionSample(
    WitnessObservation& observation,
    const PerceptionSample& sample,
    const std::uint64_t nowMs,
    const WitnessPerceptionPolicy& policy) {

    const bool heardAnything = sample.heardThreat || sample.heardGunshot || sample.heardWitnessViolence;
    if (heardAnything && observation.firstAwareAtMs == 0) observation.firstAwareAtMs = nowMs;
    observation.heardThreat = observation.heardThreat || sample.heardThreat;
    observation.heardGunshot = observation.heardGunshot || sample.heardGunshot;
    observation.heardWitnessViolence = observation.heardWitnessViolence || sample.heardWitnessViolence;

    if (!sample.inFov || !sample.clearLos || sample.distance > policy.visualRadius) return;

    observation.accumulatedVisualMs += sample.visualDeltaMs;
    const float base = visualConfidence(
        sample.distance,
        policy.visualRadius,
        sample.fovQuality,
        sample.lightingFactor,
        sample.clearLos,
        observation.accumulatedVisualMs);
    if (base < policy.minimumVisualConfidence) return;

    if (observation.firstAwareAtMs == 0) observation.firstAwareAtMs = nowMs;
    observation.lastObservedAtMs = nowMs;
    observation.bestVisualConfidence = std::max(observation.bestVisualConfidence, base);
    observation.sawCrime = true;
    observation.sawWitnessViolence = observation.sawWitnessViolence || sample.sawWitnessViolence;

    if (sample.faceCover == FaceCoverKnowledge::FaceCovered) {
        if (observation.accumulatedVisualMs >= 120 && base >= policy.faceEvidenceThreshold) {
            updateFact(observation.faceCover, FaceCoverKnowledge::FaceCovered, base, nowMs);
        }
    } else if (sample.faceCover == FaceCoverKnowledge::FaceVisible) {
        const float faceConfidence = clamp01(base * sample.faceViewQuality);
        if (observation.accumulatedVisualMs >= policy.faceMinimumViewMs
            && faceConfidence >= policy.faceEvidenceThreshold) {
            updateFact(observation.faceCover, FaceCoverKnowledge::FaceVisible, faceConfidence, nowMs);
            if (sample.faceCaptureAllowed && sample.characterIdentity != identity::CharacterIdentity::Unknown) {
                // Historical face identity is write-on-observation. Later masks never clear this fact.
                updateFact(observation.faceIdentity, sample.characterIdentity, faceConfidence, nowMs);
            }
        }
    }

    // Clothing remains observable while the face is covered.
    if (!sample.outfitSignature.empty() && base >= policy.outfitEvidenceThreshold) {
        updateFact(observation.outfit, sample.outfitSignature, base, nowMs);
    }

    if (sample.weaponVisible && sample.weaponClass != WeaponClass::Unknown
        && base >= policy.weaponEvidenceThreshold) {
        updateFact(observation.weapon, sample.weaponClass, base, nowMs);
    }

    if (sample.vehicleVisible && sample.vehicle.modelHash != 0
        && base >= policy.vehicleEvidenceThreshold) {
        updateFact(observation.vehicle, sample.vehicle, base, nowMs);
    }

    const float plateConfidence = clamp01(base * sample.plateViewQuality);
    if (sample.vehicleVisible && !sample.plate.empty()
        && observation.accumulatedVisualMs >= policy.plateMinimumViewMs
        && plateConfidence >= policy.plateEvidenceThreshold) {
        updateFact(observation.plate, sample.plate, plateConfidence, nowMs);
    }

    if (sample.directionVisible && base >= policy.directionEvidenceThreshold) {
        updateFact(observation.lastKnownDirection, sample.direction, base, nowMs);
    }
}

WitnessEmotion makeEmotion(const std::uint64_t seed) noexcept {
    WitnessEmotion emotion{};
    emotion.fear = 0.20f + 0.75f * unitRoll(seed, 0xF341ull);
    emotion.panic = 0.10f + 0.80f * unitRoll(seed, 0xA11Cull);
    emotion.defiance = 0.05f + 0.75f * unitRoll(seed, 0xDEF1ull);
    return emotion;
}

WitnessReaction chooseReaction(
    const WitnessEmotion& emotion,
    const bool canUsePanicButton,
    const bool hasMeaningfulObservation,
    const std::uint64_t seed) noexcept {

    if (!hasMeaningfulObservation) return WitnessReaction::Freeze;
    const float roll = unitRoll(seed, 0xB00Bull);
    if (emotion.defiance > 0.72f && roll < 0.34f) return WitnessReaction::RefuseToReport;
    if (canUsePanicButton && emotion.defiance > 0.55f && roll < 0.48f) return WitnessReaction::PanicButton;
    if (emotion.panic > 0.78f) return roll < 0.55f ? WitnessReaction::Flee : WitnessReaction::Shout;
    if (emotion.fear > 0.76f) return roll < 0.50f ? WitnessReaction::Hide : WitnessReaction::Freeze;
    return WitnessReaction::CallPolice;
}

ReportingPlan makeReportingPlan(
    const WitnessEmotion& emotion,
    const WitnessReaction reaction,
    const std::uint64_t nowMs,
    const std::uint64_t seed) noexcept {

    ReportingPlan plan{};
    plan.reaction = reaction;
    if (reaction == WitnessReaction::RefuseToReport) {
        plan.state = ReportingState::Refused;
        return plan;
    }

    const std::uint64_t baseDelay = 1400 + static_cast<std::uint64_t>(unitRoll(seed, 0x1111ull) * 4200.0f);
    const std::uint64_t fearDelay = static_cast<std::uint64_t>(clamp01(emotion.fear) * 1800.0f);
    const std::uint64_t duration = 3000 + static_cast<std::uint64_t>(unitRoll(seed, 0x2222ull) * 2800.0f);
    plan.state = ReportingState::Waiting;
    plan.eligibleAtMs = nowMs + baseDelay + fearDelay;
    plan.startedAtMs = 0;
    plan.partialAtMs = plan.eligibleAtMs + duration / 3;
    plan.completeAtMs = plan.eligibleAtMs + duration;
    return plan;
}

ReportingAdvance advanceReporting(
    ReportingPlan& plan,
    const std::uint64_t nowMs,
    const bool interrupted) noexcept {

    if (plan.state == ReportingState::Reported || plan.state == ReportingState::Interrupted) return ReportingAdvance::None;
    if (plan.state == ReportingState::Refused) return ReportingAdvance::Refused;

    if (interrupted) {
        plan.state = ReportingState::Interrupted;
        return ReportingAdvance::Interrupted;
    }

    if (plan.state == ReportingState::Waiting && nowMs >= plan.eligibleAtMs) {
        plan.state = ReportingState::Reporting;
        plan.startedAtMs = nowMs;
        return ReportingAdvance::Begin;
    }

    if (plan.state == ReportingState::Reporting && !plan.basicFactsCommitted && nowMs >= plan.partialAtMs) {
        plan.basicFactsCommitted = true;
        plan.state = ReportingState::PartialReported;
        return ReportingAdvance::CommitBasic;
    }

    if ((plan.state == ReportingState::Reporting || plan.state == ReportingState::PartialReported)
        && !plan.detailedFactsCommitted && nowMs >= plan.completeAtMs) {
        plan.basicFactsCommitted = true;
        plan.detailedFactsCommitted = true;
        plan.state = ReportingState::Reported;
        return ReportingAdvance::CommitDetailed;
    }

    return ReportingAdvance::None;
}

std::string_view weaponClassName(const WeaponClass value) noexcept {
    switch (value) {
    case WeaponClass::Unknown: return "unknown";
    case WeaponClass::Unarmed: return "unarmed";
    case WeaponClass::Melee: return "melee";
    case WeaponClass::Handgun: return "handgun";
    case WeaponClass::Smg: return "smg";
    case WeaponClass::Shotgun: return "shotgun";
    case WeaponClass::Rifle: return "rifle";
    case WeaponClass::MachineGun: return "machine_gun";
    case WeaponClass::Sniper: return "sniper";
    case WeaponClass::Heavy: return "heavy";
    case WeaponClass::Thrown: return "thrown";
    }
    return "unknown";
}

std::string_view faceCoverKnowledgeName(const FaceCoverKnowledge value) noexcept {
    switch (value) {
    case FaceCoverKnowledge::Unknown: return "unknown";
    case FaceCoverKnowledge::FaceVisible: return "face_visible";
    case FaceCoverKnowledge::FaceCovered: return "face_covered";
    }
    return "unknown";
}

std::string_view witnessReactionName(const WitnessReaction value) noexcept {
    switch (value) {
    case WitnessReaction::Freeze: return "freeze";
    case WitnessReaction::Hide: return "hide";
    case WitnessReaction::Flee: return "flee";
    case WitnessReaction::CallPolice: return "call_police";
    case WitnessReaction::PanicButton: return "panic_button";
    case WitnessReaction::Shout: return "shout";
    case WitnessReaction::RefuseToReport: return "refuse_to_report";
    }
    return "unknown";
}

std::string_view reportingStateName(const ReportingState value) noexcept {
    switch (value) {
    case ReportingState::None: return "none";
    case ReportingState::Waiting: return "waiting";
    case ReportingState::Reporting: return "reporting";
    case ReportingState::PartialReported: return "partial_reported";
    case ReportingState::Reported: return "reported";
    case ReportingState::Interrupted: return "interrupted";
    case ReportingState::Refused: return "refused";
    }
    return "unknown";
}

} // namespace gco::witness
