#include "AdapterDiagnostics.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace gco::platform {

std::string_view adapterProbeStatusName(const AdapterProbeStatus status) noexcept {
    switch (status) {
    case AdapterProbeStatus::Pass: return "PASS";
    case AdapterProbeStatus::Fail: return "FAIL";
    case AdapterProbeStatus::Skip: return "SKIP";
    case AdapterProbeStatus::ManualRequired: return "MANUAL_REQUIRED";
    case AdapterProbeStatus::ResearchRequired: return "RESEARCH_REQUIRED";
    }
    return "UNKNOWN";
}

void AdapterProbeReport::add(std::string id, const AdapterProbeStatus status, std::string detail) {
    results.push_back(AdapterProbeResult{std::move(id), status, std::move(detail)});
}

void AdapterProbeReport::append(AdapterProbeReport other) {
    results.reserve(results.size() + other.results.size());
    for (auto& result : other.results) {
        results.push_back(std::move(result));
    }
}

std::size_t AdapterProbeReport::failures() const noexcept {
    return static_cast<std::size_t>(std::count_if(results.begin(), results.end(), [](const AdapterProbeResult& result) {
        return result.status == AdapterProbeStatus::Fail;
    }));
}

std::size_t AdapterProbeReport::passes() const noexcept {
    return static_cast<std::size_t>(std::count_if(results.begin(), results.end(), [](const AdapterProbeResult& result) {
        return result.status == AdapterProbeStatus::Pass;
    }));
}

AdapterProbeReport AdapterDiagnostics::probeWorld() {
    AdapterProbeReport report;
    auto& world = services_.world;

    const PedHandle player = world.playerPed();
    if (!world.pedExists(player)) {
        report.add("world.player", AdapterProbeStatus::Fail, "player ped is unavailable or not a ped");
        return report;
    }

    const auto position = world.entityPosition(player);
    const auto snapshot = world.snapshotPed(player);
    report.add(
        "world.player",
        position.has_value() && snapshot.has_value() ? AdapterProbeStatus::Pass : AdapterProbeStatus::Fail,
        position.has_value() && snapshot.has_value()
            ? "player position and PedSnapshot acquired through adapter"
            : "player exists but position/snapshot acquisition failed");

    const MissionState mission = world.missionState();
    std::ostringstream state;
    state << "alive=" << (world.playerAlive() ? "true" : "false")
          << ", wanted=" << world.wantedLevel()
          << ", mission=" << (mission.missionFlag ? "true" : "false")
          << ", cutscene=" << ((mission.cutsceneActive || mission.cutscenePlaying) ? "true" : "false")
          << ", control=" << (mission.playerControlOn ? "true" : "false");
    report.add("world.player_state", AdapterProbeStatus::Pass, state.str());

    if (!position.has_value()) {
        report.add("world.bounded_queries", AdapterProbeStatus::Skip, "player position unavailable");
        report.add("world.los", AdapterProbeStatus::Skip, "no valid player origin");
        return report;
    }

    constexpr std::size_t kProbeLimit = 8;
    const auto peds = world.nearbyPeds(*position, 60.0f, kProbeLimit);
    const auto vehicles = world.nearbyVehicles(*position, 60.0f, kProbeLimit);
    const bool bounded = peds.size() <= kProbeLimit && vehicles.size() <= kProbeLimit;

    std::ostringstream counts;
    counts << "peds=" << peds.size() << ", vehicles=" << vehicles.size() << ", cap=" << kProbeLimit;
    report.add(
        "world.bounded_queries",
        bounded ? AdapterProbeStatus::Pass : AdapterProbeStatus::Fail,
        counts.str());

    EntityHandle target = 0;
    for (const auto ped : peds) {
        if (ped != player && world.pedExists(ped)) {
            target = ped;
            break;
        }
    }
    if (target == 0) {
        for (const auto vehicle : vehicles) {
            if (world.vehicleExists(vehicle)) {
                target = vehicle;
                break;
            }
        }
    }

    if (target == 0) {
        report.add("world.los", AdapterProbeStatus::Skip, "no nearby valid target; retry in a populated area");
    } else {
        const bool clear = world.hasLineOfSight(player, target, LineOfSightProfile::DefaultVisibility);
        report.add(
            "world.los",
            AdapterProbeStatus::ManualRequired,
            std::string("DefaultVisibility returned ") + (clear ? "true" : "false")
                + "; compare with actual obstruction in the scene");
    }

    if (!vehicles.empty()) {
        const auto vehicle = world.snapshotVehicle(vehicles.front());
        report.add(
            "world.vehicle_snapshot",
            vehicle.has_value() ? AdapterProbeStatus::Pass : AdapterProbeStatus::Skip,
            vehicle.has_value()
                ? "VehicleSnapshot acquired without assigning a fake project vehicle ID"
                : "nearby vehicle streamed/deleted before snapshot; retry after streaming stabilizes");
    } else {
        report.add("world.vehicle_snapshot", AdapterProbeStatus::Skip, "no nearby vehicle");
    }

    report.add(
        "world.invalid_entity",
        !world.entityExists(0) && !world.pedExists(0) && !world.vehicleExists(0)
            && !world.entityPosition(0).has_value()
            ? AdapterProbeStatus::Pass
            : AdapterProbeStatus::Fail,
        "zero handle must fail closed without native-side gameplay effects");
    return report;
}

AdapterProbeReport AdapterDiagnostics::probeAnimation() {
    AdapterProbeReport report;

    // Deliberately invalid sentinel: this is not a claimed GTA asset identifier.
    constexpr std::string_view kInvalidDictionary = "__gco_stage1_missing_anim_dict__";
    const bool invalidRejected = !services_.animation.requestDictionary(kInvalidDictionary, 1);
    services_.animation.releaseDictionary({});

    report.add(
        "animation.invalid_dictionary",
        invalidRejected ? AdapterProbeStatus::Pass : AdapterProbeStatus::Fail,
        "unknown dictionary should fail immediately instead of waiting indefinitely");
    report.add(
        "animation.invalid_ped_recovery",
        !services_.animation.cancelPedTasks(0, false)
            && !services_.animation.stopAnimation(0, {}, {}, 0.0f)
            ? AdapterProbeStatus::Pass
            : AdapterProbeStatus::Fail,
        "task/animation recovery rejects invalid peds and empty clip identifiers");
    report.add(
        "animation.positive_asset",
        AdapterProbeStatus::ResearchRequired,
        "positive playback/load test requires a VERIFIED_IN_GAME dictionary/clip from the animation lab");
    return report;
}

AdapterProbeReport AdapterDiagnostics::probeProps() {
    AdapterProbeReport report;
    const std::size_t before = services_.props.ownedCount();
    ObjectHandle invalid = 0;

    const bool safe = !services_.props.adoptOwned(0)
        && !services_.props.releaseOwnership(0)
        && !services_.props.attach(0, services_.world.playerPed(), 0, {}, {}, false, true)
        && !services_.props.detach(0, false, false);
    services_.props.deleteOwned(invalid);

    report.add(
        "props.invalid_cleanup",
        safe && invalid == 0 && services_.props.ownedCount() == before
            ? AdapterProbeStatus::Pass
            : AdapterProbeStatus::Fail,
        "invalid attachment/delete paths fail closed and do not pollute ownership tracking");
    report.add(
        "props.positive_attachment",
        AdapterProbeStatus::ResearchRequired,
        "positive attach/detach requires a validated project-owned model, parent bone and offsets; no bone is guessed by Stage 1");
    return report;
}

AdapterProbeReport AdapterDiagnostics::probeInteriorsAndDoors() {
    AdapterProbeReport report;
    const PedHandle player = services_.world.playerPed();
    if (!services_.world.pedExists(player)) {
        report.add("interior.player", AdapterProbeStatus::Skip, "player ped unavailable");
    } else {
        const InteriorId interior = services_.interiors.interiorFromEntity(player);
        if (interior == 0) {
            report.add("interior.player", AdapterProbeStatus::Skip, "player is not in a resolved interior; retry indoors");
        } else {
            report.add(
                "interior.player",
                AdapterProbeStatus::ManualRequired,
                std::string("interior resolved; ready=") + (services_.interiors.isInteriorReady(interior) ? "true" : "false"));
        }
    }

    const bool zeroDoorRejected = !services_.interiors.doorExists(0)
        && !services_.interiors.addDoorToSystem(0, 0, {});
    services_.interiors.removeDoorFromSystem(0);
    services_.interiors.setClosestDoorLocked(0, {}, false, 0.0f);
    report.add(
        "doors.invalid_definition",
        zeroDoorRejected ? AdapterProbeStatus::Pass : AdapterProbeStatus::Fail,
        "zero door/model hashes are rejected and no door is created");
    report.add(
        "doors.positive_target",
        AdapterProbeStatus::ResearchRequired,
        "positive door lock/add/remove requires an exact surveyed model/hash/coordinate for the target interior");
    return report;
}

AdapterProbeReport AdapterDiagnostics::probeUi() {
    AdapterProbeReport report;
    const PedHandle player = services_.world.playerPed();
    const auto position = services_.world.entityPosition(player);
    if (!position.has_value()) {
        report.add("ui.blip", AdapterProbeStatus::Skip, "player position unavailable");
        report.add("ui.subtitle", AdapterProbeStatus::Skip, "player position unavailable");
        return report;
    }

    BlipHandle blip = services_.ui.addBlip(*position, BlipStyle{});
    const bool blipCreated = blip != 0;
    if (blipCreated) {
        services_.ui.setBlipName(blip, "GCO Adapter Probe");
        services_.ui.removeBlip(blip);
    }
    report.add(
        "ui.blip",
        blipCreated && blip == 0 ? AdapterProbeStatus::Pass : AdapterProbeStatus::Fail,
        blipCreated && blip == 0
            ? "temporary debug blip was created/named/removed and handle cleared"
            : "temporary debug blip could not be created or cleanup did not clear its handle");

    services_.ui.subtitle("GCO adapter probe: subtitle path", 1500, true);
    report.add(
        "ui.subtitle",
        AdapterProbeStatus::ManualRequired,
        "confirm the short 'GCO adapter probe' subtitle renders once and then clears");
    return report;
}

AdapterProbeReport AdapterDiagnostics::probeAudio() {
    AdapterProbeReport report;
    const PedHandle player = services_.world.playerPed();
    const bool rejected = !services_.audio.playAmbientSpeech(player, {}, {});
    report.add(
        "audio.invalid_selection",
        rejected ? AdapterProbeStatus::Pass : AdapterProbeStatus::Fail,
        "empty/unvalidated speech selection must not call the ambient-speech playback path");
    report.add(
        "audio.positive_speech",
        AdapterProbeStatus::ResearchRequired,
        "no speech name/voice pair is claimed until the speech compatibility lab marks it VERIFIED_IN_GAME");
    return report;
}

AdapterProbeReport AdapterDiagnostics::probeInput(const std::uint64_t nowMs) {
    AdapterProbeReport report;
    const bool invalidSafe = !services_.input.pressed(InputAction::Count)
        && !services_.input.justPressed(InputAction::Count)
        && !services_.input.justReleased(InputAction::Count)
        && services_.input.normal(InputAction::Count) == 0.0f;
    report.add(
        "input.invalid_action",
        invalidSafe ? AdapterProbeStatus::Pass : AdapterProbeStatus::Fail,
        "out-of-range semantic action fails closed and never aliases GTA control 0");

    beginInputProbe(nowMs);
    report.add(
        "input.semantic_actions",
        AdapterProbeStatus::ManualRequired,
        "10-second probe armed; press Interact/Cancel/Sprint/Aim/Attack/EnterVehicle and verify log events");
    return report;
}

AdapterProbeReport AdapterDiagnostics::probeDebugDraw(const std::uint64_t nowMs) {
    AdapterProbeReport report;
    beginVisualProbe(nowMs);
    report.add(
        "debug_draw.spatial",
        AdapterProbeStatus::ManualRequired,
        "5-second line/box/cone probe armed near the player; verify geometry follows the current streamed player snapshot");
    return report;
}

AdapterProbeReport AdapterDiagnostics::probeAll(const std::uint64_t nowMs) {
    AdapterProbeReport report;
    report.append(probeWorld());
    report.append(probeAnimation());
    report.append(probeProps());
    report.append(probeInteriorsAndDoors());
    report.append(probeUi());
    report.append(probeAudio());
    report.append(probeInput(nowMs));
    report.append(probeDebugDraw(nowMs));
    return report;
}

void AdapterDiagnostics::beginInputProbe(const std::uint64_t nowMs, const std::uint64_t durationMs) noexcept {
    inputProbeUntilMs_ = nowMs + durationMs;
}

std::vector<InputAction> AdapterDiagnostics::tickInputProbe(const std::uint64_t nowMs) {
    std::vector<InputAction> pressed;
    if (!inputProbeActive(nowMs)) {
        return pressed;
    }

    for (std::uint8_t raw = 0; raw < static_cast<std::uint8_t>(InputAction::Count); ++raw) {
        const auto action = static_cast<InputAction>(raw);
        if (services_.input.justPressed(action)) {
            pressed.push_back(action);
        }
    }
    return pressed;
}

bool AdapterDiagnostics::inputProbeActive(const std::uint64_t nowMs) const noexcept {
    return inputProbeUntilMs_ != 0 && nowMs < inputProbeUntilMs_;
}

void AdapterDiagnostics::beginVisualProbe(const std::uint64_t nowMs, const std::uint64_t durationMs) noexcept {
    visualProbeUntilMs_ = nowMs + durationMs;
}

void AdapterDiagnostics::renderVisualProbe(const std::uint64_t nowMs) {
    if (!visualProbeActive(nowMs)) {
        return;
    }

    const PedHandle player = services_.world.playerPed();
    const auto snapshot = services_.world.snapshotPed(player);
    if (!snapshot.has_value()) {
        return;
    }

    Vec3 origin = snapshot->position;
    origin.z += 0.5f;
    const Vec3 lineEnd{origin.x + 3.0f, origin.y, origin.z};
    const Vec3 boxMin{origin.x - 0.5f, origin.y + 2.0f, origin.z - 0.25f};
    const Vec3 boxMax{origin.x + 0.5f, origin.y + 3.0f, origin.z + 0.75f};

    services_.debugDraw.line(origin, lineEnd, Rgba{255, 255, 255, 220});
    services_.debugDraw.box(boxMin, boxMax, Rgba{255, 255, 255, 80});
    services_.debugDraw.witnessCone(origin, snapshot->heading, 30.0f, 8.0f, Rgba{255, 255, 255, 180});
}

bool AdapterDiagnostics::visualProbeActive(const std::uint64_t nowMs) const noexcept {
    return visualProbeUntilMs_ != 0 && nowMs < visualProbeUntilMs_;
}

} // namespace gco::platform
