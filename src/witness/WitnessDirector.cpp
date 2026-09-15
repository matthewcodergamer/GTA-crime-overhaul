#include "WitnessDirector.h"

#include "Foundation.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace gco::witness {
namespace {

float distance(const platform::Vec3& a, const platform::Vec3& b) noexcept {
    return std::sqrt(platform::distanceSquared(a, b));
}

crime::CrimeLocation crimeLocation(const platform::Vec3& position, const std::string& tag) {
    return crime::CrimeLocation{position.x, position.y, position.z, tag};
}

float hearingStrength(const float d, const float radius) noexcept {
    if (radius <= 0.0f || d < 0.0f || d > radius) return 0.0f;
    return clamp01(1.0f - d / radius);
}

std::string vehicleDescriptor(const VehicleVisual& value) {
    std::ostringstream out;
    out << "model=0x" << std::hex << std::uppercase << value.modelHash << std::dec
        << ";primaryColor=" << value.primaryColor
        << ";secondaryColor=" << value.secondaryColor;
    return out.str();
}

} // namespace

WitnessDirector::WitnessDirector(
    platform::PlatformServices& platform,
    platform::NativePedPresentationAdapter& pedPresentation,
    crime::CrimeDirector& crimeDirector,
    crime::CrimeRegistry& crimeRegistry,
    EventBus& events,
    WitnessDirectorTuning tuning)
    : platform_(platform),
      pedPresentation_(pedPresentation),
      crimeDirector_(crimeDirector),
      crimeRegistry_(crimeRegistry),
      events_(events),
      tuning_(std::move(tuning)),
      scanCursor_(tuning_.samplesPerFiveHzTick) {}

WitnessDirector::~WitnessDirector() {
    shutdown();
}

void WitnessDirector::initialize() {
    if (initialized_) return;
    robberyStartedSub_ = events_.subscribe("business.robbery_started", [this](const RuntimeEvent& event) {
        onRobberyStarted(event);
    });
    robberyFinishedSub_ = events_.subscribe("business.robbery_finished", [this](const RuntimeEvent& event) {
        onRobberyFinished(event);
    });
    clerkKilledSub_ = events_.subscribe("business.clerk_killed", [this](const RuntimeEvent& event) {
        onViolenceEvent(event);
    });
    initialized_ = true;
}

void WitnessDirector::shutdown() {
    if (!initialized_) return;
    if (robberyStartedSub_ != 0) events_.unsubscribe(robberyStartedSub_);
    if (robberyFinishedSub_ != 0) events_.unsubscribe(robberyFinishedSub_);
    if (clerkKilledSub_ != 0) events_.unsubscribe(clerkKilledSub_);
    robberyStartedSub_ = robberyFinishedSub_ = clerkKilledSub_ = 0;
    candidates_.clear();
    clearIncident();
    initialized_ = false;
}

void WitnessDirector::onRobberyStarted(const RuntimeEvent& event) {
    const auto caseId = payloadId(event.payload, "caseId");
    const auto crimeId = payloadId(event.payload, "crimeId");
    if (!caseId || !crimeId) return;
    const auto* crime = crimeRegistry_.findCrime(*crimeId);
    if (crime == nullptr || crime->caseId != *caseId) return;

    incident_ = {};
    incident_.businessId = event.subjectId;
    incident_.caseId = *caseId;
    incident_.crimeId = *crimeId;
    incident_.origin = {crime->location.x, crime->location.y, crime->location.z};
    incident_.startedAtMs = crime->occurredAtMs;
    incident_.noiseUntilMs = crime->occurredAtMs + tuning_.initialNoiseWindowMs;
    candidates_.clear();
    scanCursor_.reset();
    nextCandidateRefreshMs_ = 0;
    sourceOrdinal_ = 1;

    Logger::instance().info(
        "WitnessDirector activated for case=" + std::to_string(incident_.caseId)
        + ", crime=" + std::to_string(incident_.crimeId)
        + ". Candidate scans are bounded/staggered; hearing does not grant visual identity facts.");
}

void WitnessDirector::onRobberyFinished(const RuntimeEvent& event) {
    if (incident_.caseId == 0 || event.subjectId != incident_.businessId) return;
    const auto caseId = payloadId(event.payload, "caseId");
    if (caseId && *caseId != incident_.caseId) return;
    incident_.endedAtMs = lastTickNowMs_ != 0 ? lastTickNowMs_ : incident_.startedAtMs;
}

void WitnessDirector::onViolenceEvent(const RuntimeEvent& event) {
    if (incident_.caseId == 0 || event.subjectId != incident_.businessId) return;
    const auto homicideId = payloadId(event.payload, "homicideCrimeId");
    if (!homicideId) return;
    const auto* crime = crimeRegistry_.findCrime(*homicideId);
    if (crime == nullptr || crime->caseId != incident_.caseId) return;
    markViolence({crime->location.x, crime->location.y, crime->location.z},
        lastTickNowMs_ != 0 ? lastTickNowMs_ : crime->occurredAtMs);
}

void WitnessDirector::tickFiveHz(
    const std::uint64_t persistentNowMs,
    const bool gameplayAllowed,
    const platform::PedHandle excludedPed) {

    lastTickNowMs_ = persistentNowMs;
    if (!initialized_ || incident_.caseId == 0) return;
    if (!gameplayAllowed) return;

    if (nextCandidateRefreshMs_ == 0 || persistentNowMs >= nextCandidateRefreshMs_) {
        refreshCandidates(persistentNowMs, excludedPed);
        nextCandidateRefreshMs_ = persistentNowMs + tuning_.candidateRefreshMs;
    }

    const auto indices = scanCursor_.next(candidates_.size());
    for (const auto index : indices) {
        if (index < candidates_.size()) sampleCandidate(candidates_[index], persistentNowMs);
    }

    for (auto& candidate : candidates_) {
        advanceReport(candidate, persistentNowMs);
    }

    prune(persistentNowMs);
}

void WitnessDirector::refreshCandidates(
    const std::uint64_t nowMs,
    const platform::PedHandle excludedPed) {

    const float scanRadius = std::max(tuning_.perception.visualRadius, tuning_.perception.hearingRadius);
    const auto peds = platform_.world.nearbyPeds(incident_.origin, scanRadius, tuning_.maxCandidates);
    const auto player = platform_.world.playerPed();

    for (const auto ped : peds) {
        if (ped == 0 || ped == player || ped == excludedPed || !platform_.world.pedExists(ped)
            || behaviorPed_.missionEntity(ped)) {
            continue;
        }
        const auto snapshot = platform_.world.snapshotPed(ped);
        if (!snapshot || !snapshot->alive || snapshot->isPlayer || !pedPresentation_.classify(ped).human) continue;

        const auto found = std::find_if(candidates_.begin(), candidates_.end(), [ped](const Candidate& candidate) {
            return candidate.ped == ped;
        });
        if (found != candidates_.end()) {
            found->lastPresentAtMs = nowMs;
            continue;
        }
        if (candidates_.size() >= tuning_.maxCandidates) break;

        Candidate candidate{};
        candidate.ped = ped;
        candidate.sourceKey = "case:" + std::to_string(incident_.caseId)
            + ":ambient-witness:" + std::to_string(sourceOrdinal_++);
        candidate.emotion = makeEmotion(
            incident_.caseId
            ^ static_cast<std::uint64_t>(snapshot->modelHash)
            ^ static_cast<std::uint64_t>(sourceOrdinal_ * 0x9E37u));
        candidate.observation.caseId = incident_.caseId;
        candidate.observation.crimeId = incident_.crimeId;
        candidate.firstTrackedAtMs = nowMs;
        candidate.lastPresentAtMs = nowMs;
        candidate.lastPosition = snapshot->position;
        candidate.lastHeading = snapshot->heading;
        candidate.wasAlive = true;
        candidates_.push_back(std::move(candidate));
    }
}

void WitnessDirector::sampleCandidate(Candidate& candidate, const std::uint64_t nowMs) {
    if (candidate.ped == 0 || !platform_.world.pedExists(candidate.ped)) return;
    const auto snapshot = platform_.world.snapshotPed(candidate.ped);
    if (!snapshot) return;

    const auto player = platform_.world.playerPed();
    const auto playerSnapshot = platform_.world.snapshotPed(player);
    if (!playerSnapshot || !playerSnapshot->alive) return;

    if (!snapshot->alive) {
        if (candidate.wasAlive && perception_.entityDamagedBy(candidate.ped, player)) {
            markViolence(snapshot->position, nowMs);
        }
        candidate.wasAlive = false;
        return;
    }
    candidate.wasAlive = true;
    candidate.lastPresentAtMs = nowMs;
    candidate.lastPosition = snapshot->position;
    candidate.lastHeading = snapshot->heading;

    const float playerDistance = distance(snapshot->position, playerSnapshot->position);
    const float fov = angularFrontQuality(
        snapshot->position,
        snapshot->heading,
        playerSnapshot->position,
        tuning_.perception.halfFovDegrees);
    const bool inFov = fov > 0.0f && playerDistance <= tuning_.perception.visualRadius;
    const bool los = inFov && platform_.world.hasLineOfSight(
        candidate.ped,
        player,
        platform::LineOfSightProfile::DefaultVisibility);

    const std::uint64_t delta = candidate.lastSampleAtMs == 0
        ? 200
        : std::min<std::uint64_t>(1000, nowMs >= candidate.lastSampleAtMs ? nowMs - candidate.lastSampleAtMs : 0);
    candidate.lastSampleAtMs = nowMs;
    candidate.lastDistance = playerDistance;
    candidate.lastFovQuality = fov;
    candidate.lastLos = los;

    PerceptionSample sample{};
    sample.distance = playerDistance;
    sample.inFov = inFov;
    sample.clearLos = los;
    sample.fovQuality = fov;
    sample.lightingFactor = perception_.ambientVisibilityFactor();
    sample.visualDeltaMs = delta;

    const float threatDistance = distance(snapshot->position, incident_.origin);
    if (nowMs <= incident_.noiseUntilMs && threatDistance <= tuning_.perception.hearingRadius) {
        sample.heardThreat = true;
        sample.hearingStrength = hearingStrength(threatDistance, tuning_.perception.hearingRadius);
    }
    if (perception_.playerShooting() && playerDistance <= tuning_.perception.hearingRadius) {
        sample.heardGunshot = true;
        sample.hearingStrength = std::max(
            sample.hearingStrength,
            hearingStrength(playerDistance, tuning_.perception.hearingRadius));
    }
    if (nowMs <= incident_.violenceNoiseUntilMs) {
        const float violenceDistance = distance(snapshot->position, incident_.violenceOrigin);
        if (violenceDistance <= tuning_.perception.hearingRadius) {
            sample.heardWitnessViolence = true;
            sample.hearingStrength = std::max(
                sample.hearingStrength,
                hearingStrength(violenceDistance, tuning_.perception.hearingRadius));
        }
        if (los && distance(playerSnapshot->position, incident_.violenceOrigin) <= 8.0f
            && violenceDistance <= tuning_.perception.visualRadius) {
            sample.sawWitnessViolence = true;
        }
    }

    if (los) {
        sample.faceViewQuality = angularFrontQuality(
            playerSnapshot->position,
            playerSnapshot->heading,
            snapshot->position,
            78.0f);
        sample.faceCover = mapFaceCover(perception_.faceCoverState(player));
        sample.outfitSignature = outfitSignature(*playerSnapshot);
        sample.directionVisible = true;
        sample.direction = DirectionVisual{playerSnapshot->position, playerSnapshot->heading};

        const auto vehicleHandle = perception_.vehiclePedIsUsing(player);
        if (vehicleHandle && platform_.world.vehicleExists(*vehicleHandle)) {
            const auto vehicle = platform_.world.snapshotVehicle(*vehicleHandle);
            if (vehicle) {
                const bool vehicleLos = platform_.world.hasLineOfSight(
                    candidate.ped,
                    *vehicleHandle,
                    platform::LineOfSightProfile::DefaultVisibility);
                if (vehicleLos) {
                    sample.vehicleVisible = true;
                    sample.vehicle = VehicleVisual{
                        vehicle->modelHash,
                        vehicle->primaryColor,
                        vehicle->secondaryColor};
                    const float vehicleDistance = distance(snapshot->position, vehicle->position);
                    sample.plateViewQuality = plateGeometryQuality(
                        snapshot->position,
                        vehicle->position,
                        vehicle->heading,
                        vehicleDistance);
                    sample.plate = vehicle->plate;
                }
            }
        } else {
            sample.weaponClass = mapWeaponClass(perception_.selectedWeaponClass(player));
            sample.weaponVisible = sample.weaponClass != WeaponClass::Unknown
                && sample.weaponClass != WeaponClass::Unarmed
                && playerDistance <= 22.0f;
        }
    }

    applyPerceptionSample(candidate.observation, sample, nowMs, tuning_.perception);
    candidate.lastHeard = sample.heardThreat || sample.heardGunshot || sample.heardWitnessViolence;
    candidate.lastVisualConfidence = candidate.observation.bestVisualConfidence;

    if (!candidate.reactionPlanned && candidate.observation.meaningful()) {
        planReaction(candidate, nowMs);
    }
}

void WitnessDirector::planReaction(Candidate& candidate, const std::uint64_t nowMs) {
    candidate.reactionPlanned = true;
    const auto reaction = chooseReaction(
        candidate.emotion,
        false,
        candidate.observation.meaningful(),
        incident_.caseId ^ static_cast<std::uint64_t>(candidate.firstTrackedAtMs) ^ sourceOrdinal_);
    candidate.report = makeReportingPlan(
        candidate.emotion,
        reaction,
        nowMs,
        incident_.crimeId ^ static_cast<std::uint64_t>(candidate.firstTrackedAtMs));
    applyReaction(candidate);
}

void WitnessDirector::applyReaction(Candidate& candidate) {
    if (candidate.ped == 0 || !platform_.world.pedExists(candidate.ped)) return;
    const auto player = platform_.world.playerPed();

    switch (candidate.report.reaction) {
    case WitnessReaction::Freeze:
        perception_.standStill(candidate.ped, 4500);
        break;
    case WitnessReaction::Hide:
        if (!behaviorPed_.cower(candidate.ped, 6500)) perception_.standStill(candidate.ped, 4500);
        break;
    case WitnessReaction::Flee:
        if (!behaviorPed_.fleeFrom(candidate.ped, player, 34.0f, 12000)) behaviorPed_.cower(candidate.ped, 5000);
        break;
    case WitnessReaction::CallPolice:
        perception_.standStill(candidate.ped, 5500);
        if (!phoneFallbackLogged_) {
            phoneFallbackLogged_ = true;
            Logger::instance().warn(
                "Witness reporting uses logical call timing + safe task fallback. No unverified phone animation/prop is treated as production-authoritative.");
        }
        break;
    case WitnessReaction::PanicButton:
        perception_.standStill(candidate.ped, 3500);
        break;
    case WitnessReaction::Shout:
        if (!behaviorPed_.fleeFrom(candidate.ped, player, 28.0f, 9000)) behaviorPed_.cower(candidate.ped, 4500);
        events_.publish(RuntimeEvent{"witness.shout", incident_.caseId, candidate.sourceKey});
        break;
    case WitnessReaction::RefuseToReport:
        break;
    }
}

void WitnessDirector::advanceReport(Candidate& candidate, const std::uint64_t nowMs) {
    if (!candidate.reactionPlanned || candidate.report.state == ReportingState::Reported
        || candidate.report.state == ReportingState::Interrupted
        || candidate.report.state == ReportingState::Refused) {
        return;
    }
    if (candidate.ped == 0 || !platform_.world.pedExists(candidate.ped)) return;
    const auto snapshot = platform_.world.snapshotPed(candidate.ped);
    if (!snapshot || !snapshot->alive) {
        advanceReporting(candidate.report, nowMs, true);
        return;
    }

    const auto player = platform_.world.playerPed();
    const bool threatened = behaviorPed_.playerFreeAimingAt(candidate.ped);
    const bool damaged = perception_.entityDamagedBy(candidate.ped, player);
    const bool interrupted = snapshot->ragdoll || threatened || damaged;
    const auto advance = advanceReporting(candidate.report, nowMs, interrupted);

    switch (advance) {
    case ReportingAdvance::None:
        break;
    case ReportingAdvance::Begin:
        events_.publish(RuntimeEvent{"witness.reporting", incident_.caseId, candidate.sourceKey});
        break;
    case ReportingAdvance::CommitBasic:
        persistBasicEvidence(candidate, nowMs);
        break;
    case ReportingAdvance::CommitDetailed:
        if (!candidate.report.basicFactsCommitted) persistBasicEvidence(candidate, nowMs);
        persistDetailedEvidence(candidate, nowMs);
        if (const auto* file = crimeRegistry_.findCase(incident_.caseId);
            file != nullptr && file->state == crime::CaseState::Reporting) {
            crimeDirector_.markReported(file->id, nowMs);
        }
        events_.publish(RuntimeEvent{"witness.report_completed", incident_.caseId, candidate.sourceKey});
        break;
    case ReportingAdvance::Interrupted:
        events_.publish(RuntimeEvent{"witness.report_interrupted", incident_.caseId, candidate.sourceKey});
        if (candidate.report.basicFactsCommitted) {
            // Any facts already committed remain immutable case evidence. Interrupting one caller
            // never erases this or evidence supplied by another witness.
        }
        break;
    case ReportingAdvance::Refused:
        events_.publish(RuntimeEvent{"witness.report_refused", incident_.caseId, candidate.sourceKey});
        break;
    }
}

void WitnessDirector::persistBasicEvidence(Candidate& candidate, const std::uint64_t nowMs) {
    const auto& observation = candidate.observation;
    if (!observation.meaningful()) return;

    const float confidence = observation.sawCrime
        ? std::max(0.18f, observation.bestVisualConfidence)
        : 0.22f;
    std::string descriptor = observation.sawCrime ? "witness_visually_observed_crime" : "witness_heard_crime_only";
    if (observation.heardGunshot) descriptor += ";heard_gunshot";
    if (observation.heardThreat) descriptor += ";heard_threat";
    persistEvidence(
        candidate,
        crime::EvidenceKind::CrimeObserved,
        confidence,
        observation.firstAwareAtMs != 0 ? observation.firstAwareAtMs : nowMs,
        std::move(descriptor),
        candidate.lastPosition);

    if (observation.sawWitnessViolence) {
        persistEvidence(
            candidate,
            crime::EvidenceKind::Injury,
            std::max(0.20f, observation.bestVisualConfidence),
            observation.lastObservedAtMs != 0 ? observation.lastObservedAtMs : nowMs,
            "witness_saw_violence_against_another_witness",
            incident_.violenceOrigin);
    }

    if (const auto* file = crimeRegistry_.findCase(incident_.caseId);
        file != nullptr && file->state == crime::CaseState::Observed) {
        crimeDirector_.beginReporting(file->id, nowMs);
    }
}

void WitnessDirector::persistDetailedEvidence(Candidate& candidate, const std::uint64_t nowMs) {
    const auto& observation = candidate.observation;
    if (observation.faceCover.observed) {
        if (observation.faceCover.value == FaceCoverKnowledge::FaceVisible) {
            persistEvidence(candidate, crime::EvidenceKind::Face, observation.faceCover.confidence,
                observation.faceCover.observedAtMs, "witness_saw_uncovered_face", candidate.lastPosition);
        } else if (observation.faceCover.value == FaceCoverKnowledge::FaceCovered) {
            persistEvidence(candidate, crime::EvidenceKind::Mask, observation.faceCover.confidence,
                observation.faceCover.observedAtMs, "witness_saw_face_cover", candidate.lastPosition);
        }
    }
    if (observation.outfit.observed) {
        persistEvidence(candidate, crime::EvidenceKind::Clothing, observation.outfit.confidence,
            observation.outfit.observedAtMs, "outfit=" + observation.outfit.value, candidate.lastPosition);
    }
    if (observation.weapon.observed) {
        persistEvidence(candidate, crime::EvidenceKind::Weapon, observation.weapon.confidence,
            observation.weapon.observedAtMs,
            "weapon_class=" + std::string(weaponClassName(observation.weapon.value)), candidate.lastPosition);
    }
    if (observation.vehicle.observed) {
        persistEvidence(candidate, crime::EvidenceKind::Vehicle, observation.vehicle.confidence,
            observation.vehicle.observedAtMs, vehicleDescriptor(observation.vehicle.value), candidate.lastPosition);
    }
    if (observation.plate.observed) {
        persistEvidence(candidate, crime::EvidenceKind::Plate, observation.plate.confidence,
            observation.plate.observedAtMs, "plate=" + observation.plate.value, candidate.lastPosition);
    }
    if (observation.lastKnownDirection.observed) {
        const auto& direction = observation.lastKnownDirection.value;
        std::ostringstream out;
        out << "heading=" << direction.heading
            << ";x=" << direction.position.x
            << ";y=" << direction.position.y
            << ";z=" << direction.position.z;
        persistEvidence(candidate, crime::EvidenceKind::Direction, observation.lastKnownDirection.confidence,
            observation.lastKnownDirection.observedAtMs, out.str(), direction.position);
    }
    if (observation.sawWitnessViolence) {
        persistEvidence(candidate, crime::EvidenceKind::Body,
            std::max(0.20f, observation.bestVisualConfidence),
            observation.lastObservedAtMs != 0 ? observation.lastObservedAtMs : nowMs,
            "witness_saw_victim_after_violence", incident_.violenceOrigin);
    }
}

void WitnessDirector::persistEvidence(
    const Candidate& candidate,
    const crime::EvidenceKind kind,
    const float confidence,
    const std::uint64_t observedAtMs,
    std::string descriptor,
    const platform::Vec3& location) {

    if (incident_.caseId == 0 || confidence <= 0.0f) return;
    crime::EvidenceRecord evidence{};
    evidence.source = crime::EvidenceSource::Witness;
    evidence.kind = kind;
    evidence.confidence = clamp01(confidence);
    evidence.observedAtMs = observedAtMs;
    evidence.independenceKey = candidate.sourceKey;
    evidence.dedupKey = candidate.sourceKey + ":" + std::string(crime::evidenceKindName(kind));
    evidence.snapshot.descriptor = std::move(descriptor);
    evidence.snapshot.location = crimeLocation(location, "witness_observation");
    crimeDirector_.addEvidence(incident_.caseId, std::move(evidence));
}

void WitnessDirector::markViolence(const platform::Vec3& origin, const std::uint64_t nowMs) {
    incident_.violenceOrigin = origin;
    incident_.violenceNoiseUntilMs = std::max(
        incident_.violenceNoiseUntilMs,
        nowMs + tuning_.violenceNoiseWindowMs);
}

void WitnessDirector::prune(const std::uint64_t nowMs) {
    candidates_.erase(
        std::remove_if(candidates_.begin(), candidates_.end(), [this, nowMs](const Candidate& candidate) {
            const bool settled = candidate.report.state == ReportingState::Reported
                || candidate.report.state == ReportingState::Interrupted
                || candidate.report.state == ReportingState::Refused;
            if (settled && nowMs >= candidate.lastPresentAtMs + 10000) return true;
            if (!platform_.world.pedExists(candidate.ped) && nowMs >= candidate.lastPresentAtMs + 4000) return true;
            return false;
        }),
        candidates_.end());

    if (incident_.endedAtMs != 0 && nowMs >= incident_.endedAtMs + tuning_.aftermathMs) {
        const bool pending = std::any_of(candidates_.begin(), candidates_.end(), [](const Candidate& candidate) {
            return candidate.reactionPlanned
                && candidate.report.state != ReportingState::Reported
                && candidate.report.state != ReportingState::Interrupted
                && candidate.report.state != ReportingState::Refused;
        });
        if (!pending) clearIncident();
    }
}

void WitnessDirector::clearIncident() {
    incident_ = {};
    candidates_.clear();
    scanCursor_.reset();
    nextCandidateRefreshMs_ = 0;
    sourceOrdinal_ = 1;
}

void WitnessDirector::renderDebug() const {
    if (!debugEnabled_ || incident_.caseId == 0) return;
    const auto player = platform_.world.playerPed();
    const auto playerPosition = platform_.world.entityPosition(player);

    for (const auto& candidate : candidates_) {
        if (candidate.ped == 0 || !platform_.world.pedExists(candidate.ped)) continue;
        const platform::Rgba color = candidate.lastLos && candidate.lastFovQuality > 0.0f
            ? platform::Rgba{90, 220, 120, 180}
            : candidate.lastHeard
                ? platform::Rgba{90, 150, 255, 160}
                : platform::Rgba{220, 90, 90, 130};
        platform_.debugDraw.witnessCone(
            candidate.lastPosition,
            candidate.lastHeading,
            tuning_.perception.halfFovDegrees,
            tuning_.perception.visualRadius,
            color);
        if (playerPosition) {
            platform_.debugDraw.line(candidate.lastPosition, *playerPosition, color);
        }
    }
}

std::string WitnessDirector::debugSummary() const {
    std::ostringstream out;
    out << "WitnessDirector: case=" << incident_.caseId
        << ", crime=" << incident_.crimeId
        << ", candidates=" << candidates_.size()
        << ", debug=" << (debugEnabled_ ? "true" : "false");
    for (const auto& candidate : candidates_) {
        out << "\n - " << candidate.sourceKey
            << " ped=" << candidate.ped
            << " reaction=" << witnessReactionName(candidate.report.reaction)
            << " report=" << reportingStateName(candidate.report.state)
            << " heard=" << (candidate.lastHeard ? "true" : "false")
            << " los=" << (candidate.lastLos ? "true" : "false")
            << " fov=" << candidate.lastFovQuality
            << " visualConfidence=" << candidate.lastVisualConfidence
            << " face=" << (candidate.observation.faceCover.observed
                ? std::string(faceCoverKnowledgeName(candidate.observation.faceCover.value)) : "none")
            << " plate=" << (candidate.observation.plate.observed ? candidate.observation.plate.value : "none");
    }
    return out.str();
}

std::optional<LogicalId> WitnessDirector::payloadId(
    const std::string_view payload,
    const std::string_view key) {

    const std::string prefix = std::string(key) + "=";
    const std::size_t start = payload.find(prefix);
    if (start == std::string_view::npos) return std::nullopt;
    const std::size_t valueStart = start + prefix.size();
    const std::size_t end = payload.find(';', valueStart);
    const std::string text(payload.substr(valueStart, end == std::string_view::npos ? payload.size() - valueStart : end - valueStart));
    try {
        const auto value = static_cast<LogicalId>(std::stoull(text));
        if (value == 0) return std::nullopt;
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

WeaponClass WitnessDirector::mapWeaponClass(const platform::PerceivedWeaponClass value) noexcept {
    switch (value) {
    case platform::PerceivedWeaponClass::Unknown: return WeaponClass::Unknown;
    case platform::PerceivedWeaponClass::Unarmed: return WeaponClass::Unarmed;
    case platform::PerceivedWeaponClass::Melee: return WeaponClass::Melee;
    case platform::PerceivedWeaponClass::Handgun: return WeaponClass::Handgun;
    case platform::PerceivedWeaponClass::Smg: return WeaponClass::Smg;
    case platform::PerceivedWeaponClass::Shotgun: return WeaponClass::Shotgun;
    case platform::PerceivedWeaponClass::Rifle: return WeaponClass::Rifle;
    case platform::PerceivedWeaponClass::MachineGun: return WeaponClass::MachineGun;
    case platform::PerceivedWeaponClass::Sniper: return WeaponClass::Sniper;
    case platform::PerceivedWeaponClass::Heavy: return WeaponClass::Heavy;
    case platform::PerceivedWeaponClass::Thrown: return WeaponClass::Thrown;
    }
    return WeaponClass::Unknown;
}

FaceCoverKnowledge WitnessDirector::mapFaceCover(const platform::FaceCoverState value) noexcept {
    switch (value) {
    case platform::FaceCoverState::Unknown: return FaceCoverKnowledge::Unknown;
    case platform::FaceCoverState::Uncovered: return FaceCoverKnowledge::FaceVisible;
    case platform::FaceCoverState::Covered: return FaceCoverKnowledge::FaceCovered;
    }
    return FaceCoverKnowledge::Unknown;
}

} // namespace gco::witness
