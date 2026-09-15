#include "Runtime.h"

#include <Windows.h>
#include <main.h>

#include <exception>
#include <filesystem>
#include <sstream>
#include <string>

namespace gco {
namespace {

constexpr float kDebugWorldRadius = 80.0f;
constexpr std::size_t kDebugPedLimit = 64;
constexpr std::size_t kDebugVehicleLimit = 64;

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
      worldState_(paths_) {}

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
    Logger::instance().info("Debug hotkeys: F9=dump diagnostics, F10=toggle overlay, F11=validate save (when DebugHotkeys=true).");
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
    // Stage 1 deliberately keeps perception/police logic out of the adapter layer.
}

void Runtime::tickOneHz() {
    const auto now = nowMs();
    if (config_.debugLogging && now - lastHeartbeatMs_ >= 30000) {
        lastHeartbeatMs_ = now;
        logDiagnostics();
    }
}

void Runtime::handleDebugHotkeys() {
    const bool f9Down = keyDown(VK_F9);
    const bool f10Down = keyDown(VK_F10);
    const bool f11Down = keyDown(VK_F11);

    if (f9Down && !f9WasDown_) {
        debugCommands_.execute("dump_status");
    }
    if (f10Down && !f10WasDown_) {
        debugCommands_.execute("toggle_overlay");
    }
    if (f11Down && !f11WasDown_) {
        debugCommands_.execute("validate_save");
    }

    f9WasDown_ = f9Down;
    f10WasDown_ = f10Down;
    f11WasDown_ = f11Down;
}

void Runtime::renderDebugOverlay() {
    std::ostringstream text;
    text << "GCO " << BuildInfo::Version
         << " | " << compatibilityLevelName(missionGate_.level())
         << " | Wanted " << platform_.world.wantedLevel();

    if (!missionGate_.gameplayAllowed()) {
        text << " | world sampling paused";
        platform_.ui.helpText(text.str(), false);
        return;
    }

    text << " | Peds " << nearbyPedCount_ << " | Vehicles " << nearbyVehicleCount_;
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

void Runtime::logDiagnostics() {
    std::ostringstream out;
    out << "Diagnostics: version=" << BuildInfo::Version
        << ", git=" << BuildInfo::GitSha
        << ", schema=" << BuildInfo::SaveSchemaVersion
        << ", frames=" << frameCount_
        << ", compatibility=" << compatibilityLevelName(missionGate_.level())
        << ", nearbyPeds=" << nearbyPedCount_
        << ", nearbyVehicles=" << nearbyVehicleCount_
        << ", schedulerTasks=" << scheduler_.recurringTaskCount()
        << ", queuedTasks=" << scheduler_.queuedTaskCount()
        << ", eventSubscribers=" << eventBus_.subscriberCount()
        << ", nextCaseSequence=" << idGenerator_.nextSequence(LogicalIdDomain::Case)
        << ", nextVehicleSequence=" << idGenerator_.nextSequence(LogicalIdDomain::Vehicle);
    Logger::instance().debug(out.str());
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
