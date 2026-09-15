#include "Runtime.h"

#include <Windows.h>
#include <main.h>

#include <cmath>
#include <exception>
#include <filesystem>
#include <sstream>
#include <string>

namespace gco {
namespace {

constexpr float kDebugWorldRadius = 80.0f;
constexpr std::size_t kDebugPedLimit = 64;
constexpr std::size_t kDebugVehicleLimit = 64;
constexpr float kInvestigationDemoActorRadius = 22.0f;
constexpr float kInvestigationDemoHearRadius = 18.0f;
constexpr std::uint64_t kInvestigationDemoTurnDelayMs = 3000;

std::uint64_t nowMs() {
    return static_cast<std::uint64_t>(GetTickCount64());
}

std::string persistenceStatusName(const PersistenceStatus status) {
    switch (status) {
    case PersistenceStatus::LoadedPrimary: return "LoadedPrimary";
    case PersistenceStatus::RecoveredBackup: return "RecoveredBackup";
    case PersistenceStatus::CreatedNew: return "CreatedNew";
    case PersistenceStatus::Failed: return "Failed";
    }
    return "Unknown";
}

std::string compatibilityLevelName(const MissionCompatibilityLevel level) {
    switch (level) {
    case MissionCompatibilityLevel::Normal: return "Normal";
    case MissionCompatibilityLevel::Restricted: return "Restricted";
    case MissionCompatibilityLevel::Suspended: return "Suspended";
    }
    return "Unknown";
}

bool keyDown(const int virtualKey) {
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

} // namespace

Runtime::Runtime()
    : paths_(RuntimePaths::discover()),
      config_(RuntimeConfig::load(paths_.configFile)),
      worldState_(paths_),
      adapterDiagnostics_(platform_) {}

Runtime::~Runtime() {
    shutdown();
}

bool Runtime::initialize() {
    if (initialized_) {
        return true;
    }

    paths_.ensureDirectories();

    if (!Logger::instance().open(paths_.logRoot / L"GTA_Crime_Overhaul.log")) {
        return false;
    }

    Logger::instance().info("GTA Crime Overhaul starting.");
    Logger::instance().info(
        std::string("Build: version=") + std::string(BuildInfo::Version)
        + ", config=" + std::string(BuildInfo::BuildConfig)
        + ", git=" + std::string(BuildInfo::GitSha)
        + ", saveSchema=" + std::to_string(BuildInfo::SaveSchemaVersion));
    Logger::instance().info("Runtime scope: GTA V Story Mode/offline only.");

    config_ = RuntimeConfig::load(paths_.configFile);
    if (!std::filesystem::exists(paths_.configFile)) {
        Logger::instance().warn("Configuration file not found; using compiled safe defaults.");
    }

    const PersistenceReport persistence = worldState_.loadOrCreate();
    if (!persistence.ok()) {
        Logger::instance().error("Unable to initialize world save state: " + persistence.detail);
        return false;
    }
    Logger::instance().info(
        "Persistence: " + persistenceStatusName(persistence.status)
        + "; schema=" + std::to_string(persistence.schemaVersion)
        + "; " + persistence.detail);

    std::string idStateReason;
    if (!worldState_.loadLogicalIdState(idGenerator_, &idStateReason)) {
        Logger::instance().error("Unable to restore logical ID state: " + idStateReason);
        return false;
    }

    if (!config_.enabled) {
        Logger::instance().warn("Mod is disabled in configuration; runtime will remain idle except for lifecycle cleanup.");
    }

    configureScheduler();
    configureDebugCommands();

    lastHeartbeatMs_ = nowMs();
    initialized_ = true;
    stopRequested_.store(false, std::memory_order_release);

    eventBus_.publish(RuntimeEvent{"runtime.started", 0, std::string(BuildInfo::Version)});
    Logger::instance().info("Native ASI runtime initialized successfully.");
    Logger::instance().info("Debug hotkeys: F7=investigation dialogue overhear demo, F8=Stage 1 adapter probes, F9=dump diagnostics, F10=toggle overlay, F11=validate save (when DebugHotkeys=true).");
    return true;
}

void Runtime::configureScheduler() {
    scheduler_.setErrorHandler([](const std::string_view taskName, const std::exception_ptr error) {
        try {
            if (error) {
                std::rethrow_exception(error);
            }
        } catch (const std::exception& ex) {
            Logger::instance().error(
                "Scheduler task '" + std::string(taskName) + "' threw std::exception: " + ex.what());
        } catch (...) {
            Logger::instance().error(
                "Scheduler task '" + std::string(taskName) + "' threw an unknown exception.");
        }
    });

    scheduler_.addRecurring(SchedulerLane::Frame, "runtime.frame", [this]() { tickFrame(); });
    scheduler_.addRecurring(SchedulerLane::FiveHz, "runtime.5hz", [this]() { tickFiveHz(); });
    scheduler_.addRecurring(SchedulerLane::TwoHz, "runtime.2hz", [this]() { tickTwoHz(); });
    scheduler_.addRecurring(SchedulerLane::OneHz, "runtime.1hz", [this]() { tickOneHz(); });
}

void Runtime::configureDebugCommands() {
    debugCommands_.registerCommand("toggle_overlay", [this]() {
        config_.debugOverlay = !config_.debugOverlay;
        Logger::instance().info(std::string("Debug overlay ") + (config_.debugOverlay ? "enabled" : "disabled") + ".");
    });
    debugCommands_.registerCommand("dump_status", [this]() { logDiagnostics(); });
    debugCommands_.registerCommand("validate_save", [this]() { validateSaveDiagnostic(); });
    debugCommands_.registerCommand("dialogue.investigation_demo", [this]() {
        startInvestigationDialogueDemo();
    });

    const auto allowed = [this]() {
        if (!missionGate_.gameplayAllowed()) {
            Logger::instance().warn("Stage 1 adapter probe blocked by mission compatibility gate; return to normal controllable free roam.");
            return false;
        }
        return true;
    };

    debugCommands_.registerCommand("adapter.world", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeWorld(), "adapter.world");
    });
    debugCommands_.registerCommand("adapter.animation", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeAnimation(), "adapter.animation");
    });
    debugCommands_.registerCommand("adapter.props", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeProps(), "adapter.props");
    });
    debugCommands_.registerCommand("adapter.interiors", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeInteriorsAndDoors(), "adapter.interiors");
    });
    debugCommands_.registerCommand("adapter.ui", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeUi(), "adapter.ui");
    });
    debugCommands_.registerCommand("adapter.audio", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeAudio(), "adapter.audio");
    });
    debugCommands_.registerCommand("adapter.input", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeInput(nowMs()), "adapter.input");
    });
    debugCommands_.registerCommand("adapter.debug_draw", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeDebugDraw(nowMs()), "adapter.debug_draw");
    });
    debugCommands_.registerCommand("adapter.all", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeAll(nowMs()), "adapter.all");
    });
}

void Runtime::run() {
    try {
        if (!initialize()) {
            Logger::instance().error("Runtime initialization failed; ScriptMain is exiting without entering the tick loop.");
            Logger::instance().close();
            return;
        }

        while (!stopRequested_.load(std::memory_order_acquire)) {
            if (config_.enabled) {
                tick();
            }
            scriptWait(0);
        }
    } catch (const std::exception& ex) {
        Logger::instance().error(std::string("Unhandled std::exception in ScriptMain: ") + ex.what());
    } catch (...) {
        Logger::instance().error("Unhandled unknown exception in ScriptMain.");
    }

    shutdown();
}

void Runtime::requestStop() noexcept {
    stopRequested_.store(true, std::memory_order_release);
}

void Runtime::tick() {
    ++frameCount_;
    scheduler_.tick(nowMs());
}

void Runtime::tickFrame() {
    if (config_.debugHotkeys) {
        handleDebugHotkeys();
    }

    const auto now = nowMs();
    if (missionGate_.gameplayAllowed()) {
        for (const auto action : adapterDiagnostics_.tickInputProbe(now)) {
            Logger::instance().info(
                "Adapter input probe: " + std::string(platform::inputActionName(action)) + " justPressed=true");
        }
        adapterDiagnostics_.renderVisualProbe(now);
        tickInvestigationDialogueDemo(now);
    }

    if (config_.debugOverlay) {
        renderDebugOverlay();
    }
}

void Runtime::tickFiveHz() {
    missionState_ = platform_.world.missionState();
    const auto previousLevel = missionGate_.level();
    const auto currentLevel = missionGate_.update(MissionCompatibilitySignals{
        missionState_.missionFlag,
        missionState_.cutsceneActive,
        missionState_.cutscenePlaying,
        missionState_.playerControlOn
    });

    if (currentLevel != previousLevel) {
        Logger::instance().info(
            "Mission compatibility changed: " + compatibilityLevelName(previousLevel)
            + " -> " + compatibilityLevelName(currentLevel) + ".");

        if (currentLevel == MissionCompatibilityLevel::Suspended) {
            eventBus_.publish(RuntimeEvent{"runtime.mission_suspend", 0, {}});
        } else if (currentLevel == MissionCompatibilityLevel::Restricted) {
            eventBus_.publish(RuntimeEvent{"runtime.mission_restricted", 0, {}});
        } else {
            eventBus_.publish(RuntimeEvent{"runtime.mission_resume", 0, {}});
        }
    }

    if (!missionGate_.gameplayAllowed()) {
        if (!investigationDemoPlan_.empty()) {
            stopInvestigationDialogueDemo("mission compatibility gate became active");
        }
        playerSnapshot_.reset();
        nearbyPedCount_ = 0;
        nearbyVehicleCount_ = 0;
        return;
    }

    const auto player = platform_.world.playerPed();
    playerSnapshot_ = platform_.world.snapshotPed(player);
    if (!playerSnapshot_) {
        nearbyPedCount_ = 0;
        nearbyVehicleCount_ = 0;
        return;
    }

    const auto peds = platform_.world.nearbyPeds(
        playerSnapshot_->position,
        kDebugWorldRadius,
        kDebugPedLimit);
    const auto vehicles = platform_.world.nearbyVehicles(
        playerSnapshot_->position,
        kDebugWorldRadius,
        kDebugVehicleLimit);

    nearbyPedCount_ = peds.size();
    nearbyVehicleCount_ = vehicles.size();
}

void Runtime::tickTwoHz() {
    // Investigation dialogue composition is domain-owned. Scene spawning/interview orchestration
    // remains a later police-system responsibility rather than being added to the GTA adapter layer.
}

void Runtime::tickOneHz() {
    const auto now = nowMs();
    if (config_.debugLogging && now - lastHeartbeatMs_ >= 30000) {
        lastHeartbeatMs_ = now;
        logDiagnostics();
    }
}

void Runtime::handleDebugHotkeys() {
    const bool f7Down = keyDown(VK_F7);
    const bool f8Down = keyDown(VK_F8);
    const bool f9Down = keyDown(VK_F9);
    const bool f10Down = keyDown(VK_F10);
    const bool f11Down = keyDown(VK_F11);

    if (f7Down && !f7WasDown_) {
        debugCommands_.execute("dialogue.investigation_demo");
    }
    if (f8Down && !f8WasDown_) {
        debugCommands_.execute("adapter.all");
    }
    if (f9Down && !f9WasDown_) {
        debugCommands_.execute("dump_status");
    }
    if (f10Down && !f10WasDown_) {
        debugCommands_.execute("toggle_overlay");
    }
    if (f11Down && !f11WasDown_) {
        debugCommands_.execute("validate_save");
    }

    f7WasDown_ = f7Down;
    f8WasDown_ = f8Down;
    f9WasDown_ = f9Down;
    f10WasDown_ = f10Down;
    f11WasDown_ = f11Down;
}

void Runtime::renderDebugOverlay() {
    std::ostringstream text;
    text << "GCO Stage 1 | " << BuildInfo::Version
         << " | " << compatibilityLevelName(missionGate_.level())
         << " | Wanted " << platform_.world.wantedLevel();

    if (!missionGate_.gameplayAllowed()) {
        text << " | world sampling paused";
        platform_.ui.helpText(text.str(), false);
        return;
    }

    text << " | Peds " << nearbyPedCount_ << " | Vehicles " << nearbyVehicleCount_;
    if (!investigationDemoPlan_.empty()) {
        text << " | Interview demo " << investigationDemoTurnIndex_ << '/' << investigationDemoPlan_.size();
    }
    platform_.ui.helpText(text.str(), false);

    if (!playerSnapshot_ || !playerSnapshot_->alive) {
        return;
    }

    auto origin = playerSnapshot_->position;
    origin.z += 0.75f;
    platform_.debugDraw.witnessCone(
        origin,
        playerSnapshot_->heading,
        35.0f,
        18.0f,
        platform::Rgba{255, 255, 255, 160});
}

void Runtime::startInvestigationDialogueDemo() {
    if (!missionGate_.gameplayAllowed()) {
        Logger::instance().warn("Investigation dialogue demo blocked by mission compatibility gate.");
        return;
    }

    const auto player = platform_.world.playerPed();
    const auto playerSnapshot = platform_.world.snapshotPed(player);
    if (!playerSnapshot || !playerSnapshot->alive) {
        Logger::instance().warn("Investigation dialogue demo requires a live controllable player.");
        return;
    }

    const auto nearby = platform_.world.nearbyPeds(
        playerSnapshot->position,
        kInvestigationDemoActorRadius,
        16);

    platform::PedHandle actors[2]{0, 0};
    std::size_t found = 0;
    for (const auto ped : nearby) {
        if (ped == player || !platform_.world.pedExists(ped)) {
            continue;
        }
        const auto snapshot = platform_.world.snapshotPed(ped);
        if (!snapshot || !snapshot->alive || snapshot->isPlayer) {
            continue;
        }
        actors[found++] = ped;
        if (found == 2) {
            break;
        }
    }

    if (found < 2) {
        Logger::instance().warn("Investigation dialogue demo needs two nearby live non-player peds. Move to a populated area and press F7 again.");
        platform_.ui.subtitle("GCO demo: need two nearby NPCs", 1800, true);
        return;
    }

    dialogue::WitnessStatementFacts facts{};
    facts.witnessKind = dialogue::WitnessKind::Clerk;
    facts.suspectKnowledge = dialogue::SuspectKnowledge::Unknown;
    facts.faceObserved = false;
    facts.faceCovered = true;
    facts.faceConfidence = 0.1f;
    facts.clothingObserved = true;
    facts.clothingDescription = "a dark jacket and gray pants";
    facts.vehicleObserved = true;
    facts.vehicleColor = "black";
    facts.vehicleDescription = "two-door coupe";
    facts.plateKnowledge = dialogue::PlateKnowledge::Partial;
    facts.plateText = "46E";
    facts.directionObserved = true;
    facts.directionDescription = "east toward the freeway";
    facts.witnessPanicked = true;

    investigationDemoPlan_ = dialogue::InvestigationDialogueComposer::compose(
        facts,
        dialogue::InterviewOptions{4, static_cast<std::uint32_t>(frameCount_), true, true, true});
    investigationDemoTurnIndex_ = 0;
    investigationDemoOfficer_ = actors[0];
    investigationDemoWitness_ = actors[1];
    investigationDemoNextTurnMs_ = nowMs() + 500;

    Logger::instance().info(
        "Investigation dialogue demo started with synthetic UNKNOWN-SUSPECT facts. "
        "Actors are nearby ambient peds used only to exercise conversation/overhearing; no case state is mutated.");
    platform_.ui.subtitle("GCO investigation demo started - stay close to overhear", 2200, true);
}

void Runtime::tickInvestigationDialogueDemo(const std::uint64_t now) {
    if (investigationDemoPlan_.empty() || now < investigationDemoNextTurnMs_) {
        return;
    }
    if (investigationDemoTurnIndex_ >= investigationDemoPlan_.turns.size()) {
        stopInvestigationDialogueDemo("completed");
        return;
    }

    const auto& turn = investigationDemoPlan_.turns[investigationDemoTurnIndex_];
    const platform::PedHandle speaker = turn.speaker == dialogue::InterviewSpeaker::Officer
        ? investigationDemoOfficer_
        : investigationDemoWitness_;

    if (!platform_.world.pedExists(speaker)) {
        stopInvestigationDialogueDemo("a demo speaker streamed out or was deleted");
        return;
    }

    const auto player = platform_.world.playerPed();
    const auto playerPosition = platform_.world.entityPosition(player);
    const auto speakerPosition = platform_.world.entityPosition(speaker);
    if (!playerPosition || !speakerPosition) {
        stopInvestigationDialogueDemo("player or speaker position became unavailable");
        return;
    }

    const float distance = std::sqrt(platform::distanceSquared(*playerPosition, *speakerPosition));
    const bool clearLos = platform_.world.hasLineOfSight(
        player,
        speaker,
        platform::LineOfSightProfile::DefaultVisibility);

    // The demo uses distance as the hard audibility gate and logs LOS separately. Production
    // spatial audio should use soft occlusion rather than making one reference-only LOS bitmask
    // decide whether a human voice can pass through a doorway/counter/window.
    const bool audible = dialogue::canOverhear(
        dialogue::OverhearSample{distance, clearLos},
        dialogue::OverhearPolicy{kInvestigationDemoHearRadius, false});

    std::ostringstream log;
    log << "Investigation demo turn " << (investigationDemoTurnIndex_ + 1)
        << '/' << investigationDemoPlan_.size()
        << " speaker=" << dialogue::interviewSpeakerName(turn.speaker)
        << " topic=" << dialogue::interviewTopicName(turn.topic)
        << " audible=" << (audible ? "true" : "false")
        << " distance=" << distance
        << " los=" << (clearLos ? "true" : "false")
        << " event=" << turn.semanticEvent
        << " text=" << turn.text;
    Logger::instance().info(log.str());

    if (audible) {
        std::string subtitle = std::string(dialogue::interviewSpeakerName(turn.speaker)) + ": " + turn.text;
        platform_.ui.subtitle(subtitle, 2500, true);
    }

    ++investigationDemoTurnIndex_;
    investigationDemoNextTurnMs_ = now + kInvestigationDemoTurnDelayMs;
    if (investigationDemoTurnIndex_ >= investigationDemoPlan_.turns.size()) {
        // Leave one turn interval before clearing so the last subtitle remains readable.
        investigationDemoNextTurnMs_ = now + kInvestigationDemoTurnDelayMs;
    }
}

void Runtime::stopInvestigationDialogueDemo(const char* reason) {
    if (investigationDemoPlan_.empty()) {
        return;
    }

    Logger::instance().info(
        std::string("Investigation dialogue demo stopped: ")
        + (reason != nullptr ? reason : "unspecified") + ".");
    investigationDemoPlan_.turns.clear();
    investigationDemoTurnIndex_ = 0;
    investigationDemoOfficer_ = 0;
    investigationDemoWitness_ = 0;
    investigationDemoNextTurnMs_ = 0;
}

void Runtime::logDiagnostics() {
    std::ostringstream out;
    out << "Diagnostics: version=" << BuildInfo::Version
        << ", git=" << BuildInfo::GitSha
        << ", schema=" << BuildInfo::SaveSchemaVersion
        << ", frames=" << frameCount_
        << ", compatibility=" << compatibilityLevelName(missionGate_.level())
        << ", nearbyPeds=" << nearbyPedCount_
        << ", nearbyVehicles=" << nearbyVehicleCount_
        << ", ownedProps=" << platform_.props.ownedCount()
        << ", investigationDemoActive=" << (!investigationDemoPlan_.empty() ? "true" : "false")
        << ", schedulerTasks=" << scheduler_.recurringTaskCount()
        << ", queuedTasks=" << scheduler_.queuedTaskCount()
        << ", eventSubscribers=" << eventBus_.subscriberCount()
        << ", nextCaseSequence=" << idGenerator_.nextSequence(LogicalIdDomain::Case)
        << ", nextVehicleSequence=" << idGenerator_.nextSequence(LogicalIdDomain::Vehicle);
    Logger::instance().debug(out.str());
}

void Runtime::logAdapterProbeReport(
    const platform::AdapterProbeReport& report,
    const char* commandName) {

    for (const auto& result : report.results) {
        const auto status = platform::adapterProbeStatusName(result.status);
        const std::string line = "Adapter probe [" + result.id + "] " + std::string(status) + ": " + result.detail;
        if (result.status == platform::AdapterProbeStatus::Fail) {
            Logger::instance().error(line);
        } else if (result.status == platform::AdapterProbeStatus::ResearchRequired
            || result.status == platform::AdapterProbeStatus::ManualRequired) {
            Logger::instance().warn(line);
        } else {
            Logger::instance().info(line);
        }
    }

    std::ostringstream summary;
    summary << "Adapter probe command '" << commandName << "': "
            << report.passes() << " PASS, " << report.failures() << " FAIL, "
            << report.results.size() << " total results.";
    if (report.safeChecksPassed()) {
        Logger::instance().info(summary.str());
    } else {
        Logger::instance().error(summary.str());
    }
}

void Runtime::validateSaveDiagnostic() {
    std::string reason;
    if (worldState_.validateFile(paths_.worldSave, &reason)) {
        Logger::instance().info("Debug save validation PASS: schema and persistence invariants are valid.");
    } else {
        Logger::instance().error("Debug save validation FAIL: " + reason);
    }
}

void Runtime::shutdown() {
    if (!initialized_) {
        Logger::instance().close();
        return;
    }

    eventBus_.publish(RuntimeEvent{"runtime.stopping", 0, {}});
    stopInvestigationDialogueDemo("runtime shutdown");
    const std::size_t cleanedProps = platform_.cleanupOwnedResources();
    if (cleanedProps > 0) {
        Logger::instance().info("Platform cleanup deleted " + std::to_string(cleanedProps) + " tracked project-owned prop(s).");
    }

    scheduler_.clear();
    eventBus_.clear();
    playerSnapshot_.reset();
    nearbyPedCount_ = 0;
    nearbyVehicleCount_ = 0;

    std::string saveReason;
    if (!worldState_.validateFile(paths_.worldSave, &saveReason)) {
        Logger::instance().warn("World save failed shutdown validation: " + saveReason);
    }

    Logger::instance().info("GTA Crime Overhaul shutting down cleanly.");
    initialized_ = false;
    Logger::instance().close();
}

} // namespace gco
