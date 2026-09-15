#include "Runtime.h"

#include <Windows.h>
#include <main.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace gco {
namespace {

std::uint64_t stage7PersistentNowMs() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

crime::CrimeLocation investigationLocation(
    const platform::Vec3& point,
    std::string zoneTag) {
    return crime::CrimeLocation{point.x, point.y, point.z, std::move(zoneTag)};
}

} // namespace

void Runtime::runStage7Integrated() {
    bool stage7Ready = false;

    try {
        if (!initialize()) {
            Logger::instance().error(
                "Runtime initialization failed; integrated Stage 7 ScriptMain is exiting without entering the tick loop.");
            Logger::instance().close();
            return;
        }

        if (config_.persistentCases) {
            policeInvestigationAdapter_ = std::make_unique<platform::NativePoliceInvestigationAdapter>(platform_);

            auto geometryResolver = [this](
                const crime::CaseFile&,
                const std::vector<const crime::CrimeEvent*>& crimes) {

                investigation::SceneGeometry geometry{};
                if (crimes.empty() || crimes.front() == nullptr) return geometry;
                geometry.center = crimes.front()->location;

                if (!config_.robberySystem || !storeRuntime_.initialized() || !storeRuntime_.targetReady()) {
                    return geometry;
                }

                const auto& storeState = storeRuntime_.model().persistent();
                const bool belongsToPrototype = std::any_of(
                    crimes.begin(),
                    crimes.end(),
                    [&storeState](const crime::CrimeEvent* event) {
                        return event != nullptr
                            && event->businessId.has_value()
                            && *event->businessId == storeState.businessId;
                    });
                if (!belongsToPrototype) return geometry;

                const auto& target = storeRuntime_.target();
                if (target.hasClerkAnchor) {
                    geometry.interiorPoints.push_back(
                        investigationLocation(target.clerk.position, "prototype_store_clerk_area"));
                }
                for (const auto& anchor : target.registers) {
                    geometry.interiorPoints.push_back(
                        investigationLocation(anchor.position, "prototype_store_register"));
                }
                if (target.safe.has_value()) {
                    geometry.interiorPoints.push_back(
                        investigationLocation(target.safe->position, "prototype_store_safe"));
                }
                for (const auto& anchor : target.exits) {
                    geometry.exitPoints.push_back(
                        investigationLocation(anchor.position, "prototype_store_exit"));
                }
                return geometry;
            };

            dispatchDirector_ = std::make_unique<investigation::DispatchDirector>(
                crimeRegistry_,
                crimeDirector_,
                *policeInvestigationAdapter_,
                eventBus_,
                std::move(geometryResolver));
            investigationDirector_ = std::make_unique<investigation::InvestigationDirector>(
                crimeRegistry_,
                crimeDirector_,
                witnessDirector_,
                *dispatchDirector_,
                *policeInvestigationAdapter_,
                eventBus_);

            dispatchDirector_->initialize(stage7PersistentNowMs());
            investigationDirector_->initialize();
            stage7Ready = true;

            debugCommands_.registerCommand("dispatch.inspect", [this]() {
                if (dispatchDirector_) Logger::instance().info(dispatchDirector_->debugSummary());
            });
            debugCommands_.registerCommand("investigation.inspect", [this]() {
                if (investigationDirector_) Logger::instance().info(investigationDirector_->debugSummary());
            });

            Logger::instance().info(
                "Stage 7 dispatch/investigation initialized: police target recorded crime scenes only; "
                "player position is high-detail streaming input, never response GPS.");
        } else {
            Logger::instance().warn(
                "Stage 7 dispatch/investigation disabled because PersistentCases=false; investigation requires durable case evidence.");
        }

        std::uint64_t nextDispatchTickMs = 0;
        std::uint64_t nextInvestigationTickMs = 0;
        std::uint64_t nextCheckpointMs = 0;

        while (!stopRequested_.load(std::memory_order_acquire)) {
            if (config_.enabled) {
                // Preserve the existing runtime scheduler exactly. Its 5 Hz lane updates the mission
                // compatibility gate and witness observations before Stage 7 consumes them.
                tick();

                if (stage7Ready) {
                    const auto steadyNow = static_cast<std::uint64_t>(GetTickCount64());
                    const auto persistentNow = stage7PersistentNowMs();
                    const bool gameplayAllowed = missionGate_.gameplayAllowed();

                    if (steadyNow >= nextDispatchTickMs) {
                        std::optional<platform::Vec3> playerPosition;
                        if (gameplayAllowed) {
                            const auto player = platform_.world.playerPed();
                            const auto snapshot = platform_.world.snapshotPed(player);
                            if (snapshot && snapshot->alive) playerPosition = snapshot->position;
                        }

                        dispatchDirector_->tickTwoHz(
                            persistentNow,
                            gameplayAllowed,
                            playerPosition);
                        nextDispatchTickMs = steadyNow + 500;
                    }

                    if (steadyNow >= nextInvestigationTickMs) {
                        investigationDirector_->tickFiveHz(persistentNow, gameplayAllowed);
                        nextInvestigationTickMs = steadyNow + 200;
                    }

                    if (steadyNow >= nextCheckpointMs) {
                        if (missionGate_.persistenceAllowed()) {
                            const bool dispatchDirty = dispatchDirector_->casePersistenceDirty();
                            const bool investigationDirty = investigationDirector_->casePersistenceDirty();
                            if ((dispatchDirty || investigationDirty)
                                && saveCrimeState("Stage 7 dispatch/investigation checkpoint")) {
                                if (dispatchDirty) dispatchDirector_->clearCasePersistenceDirty();
                                if (investigationDirty) investigationDirector_->clearCasePersistenceDirty();
                            }
                        }
                        nextCheckpointMs = steadyNow + 1000;
                    }
                }
            }
            scriptWait(0);
        }
    } catch (const std::exception& ex) {
        Logger::instance().error(
            std::string("Unhandled std::exception in integrated Stage 7 ScriptMain: ") + ex.what());
    } catch (...) {
        Logger::instance().error("Unhandled unknown exception in integrated Stage 7 ScriptMain.");
    }

    // Stage 7 owns project-created investigation peds. Tear them down before Runtime::shutdown()
    // clears the event bus and performs general adapter cleanup.
    if (investigationDirector_) investigationDirector_->shutdown();
    if (dispatchDirector_) dispatchDirector_->shutdown();
    investigationDirector_.reset();
    dispatchDirector_.reset();
    policeInvestigationAdapter_.reset();

    shutdown();
}

} // namespace gco
