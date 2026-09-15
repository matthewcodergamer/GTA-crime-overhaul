#include "Runtime.h"

#include <Windows.h>
#include <main.h>

#include <exception>
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
#ifdef GCO_VERSION
    Logger::instance().info(std::string("Build version: ") + GCO_VERSION);
#endif

    config_ = RuntimeConfig::load(paths_.configFile);

    if (!worldState_.ensureInitialized()) {
        Logger::instance().error("Unable to initialize world save state.");
        return false;
    }

    if (!config_.enabled) {
        Logger::instance().warn("Mod is disabled in configuration; runtime will remain idle.");
    }

    const auto now = nowMs();
    lastFiveHzMs_ = now;
    lastTwoHzMs_ = now;
    lastOneHzMs_ = now;
    lastHeartbeatMs_ = now;
    initialized_ = true;

    Logger::instance().info("Native ASI runtime initialized successfully.");
    return true;
}

void Runtime::run() {
    try {
        if (!initialize()) {
            for (;;) {
                scriptWait(1000);
            }
        }

        for (;;) {
            if (config_.enabled) {
                tick();
            }
            scriptWait(0);
        }
    } catch (const std::exception& ex) {
        Logger::instance().error(std::string("Unhandled std::exception in ScriptMain: ") + ex.what());
        for (;;) {
            scriptWait(1000);
        }
    } catch (...) {
        Logger::instance().error("Unhandled unknown exception in ScriptMain.");
        for (;;) {
            scriptWait(1000);
        }
    }
}

void Runtime::tick() {
    ++frameCount_;
    const auto now = nowMs();

    if (now - lastFiveHzMs_ >= 200) {
        lastFiveHzMs_ = now;
        tickFiveHz();
    }

    if (now - lastTwoHzMs_ >= 500) {
        lastTwoHzMs_ = now;
        tickTwoHz();
    }

    if (now - lastOneHzMs_ >= 1000) {
        lastOneHzMs_ = now;
        tickOneHz();
    }

    if (config_.debugOverlay) {
        renderDebugOverlay();
    }

    if (config_.debugLogging && now - lastHeartbeatMs_ >= 30000) {
        lastHeartbeatMs_ = now;
        std::ostringstream out;
        out << "Runtime heartbeat; frames=" << frameCount_
            << ", missionSuspended=" << (missionSuspended_ ? "true" : "false")
            << ", nearbyPeds=" << nearbyPedCount_
            << ", nearbyVehicles=" << nearbyVehicleCount_;
        Logger::instance().debug(out.str());
    }
}

void Runtime::tickFiveHz() {
    missionState_ = platform_.world.missionState();
    const bool shouldSuspend = missionState_.shouldSuspendGameplay();

    if (shouldSuspend != missionSuspended_) {
        missionSuspended_ = shouldSuspend;
        Logger::instance().info(missionSuspended_
            ? "Story mission/cutscene restriction detected; gameplay-facing world sampling suspended."
            : "Story mission/cutscene restriction cleared; world sampling resumed.");
    }

    if (missionSuspended_) {
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
    // Stage 1 deliberately keeps case/business/persistence domain work out of the adapter layer.
}

void Runtime::renderDebugOverlay() {
    std::ostringstream text;
    text << "GCO Stage 1 | Wanted " << platform_.world.wantedLevel();

    if (missionSuspended_) {
        text << " | SUSPENDED (mission/cutscene)";
        platform_.ui.helpText(text.str(), false);
        return;
    }

    text << " | Peds " << nearbyPedCount_ << " | Vehicles " << nearbyVehicleCount_;
    platform_.ui.helpText(text.str(), false);

    if (!playerSnapshot_ || !playerSnapshot_->alive) {
        return;
    }

    // Debug-only proof of the spatial adapter used later by witness perception.
    auto origin = playerSnapshot_->position;
    origin.z += 0.75f;
    platform_.debugDraw.witnessCone(
        origin,
        playerSnapshot_->heading,
        35.0f,
        18.0f,
        platform::Rgba{255, 255, 255, 160});
}

void Runtime::shutdown() {
    if (!initialized_) {
        return;
    }

    playerSnapshot_.reset();
    nearbyPedCount_ = 0;
    nearbyVehicleCount_ = 0;

    Logger::instance().info("GTA Crime Overhaul shutting down.");
    initialized_ = false;
    Logger::instance().close();
}

} // namespace gco
