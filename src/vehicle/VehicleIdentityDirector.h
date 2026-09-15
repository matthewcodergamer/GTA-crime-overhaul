#pragma once

#include "VehicleIdentitySystem.h"
#include "VehiclePersistence.h"
#include "crime/CrimeRegistry.h"
#include "platform/VehicleIdentityAdapter.h"

#include <cstdint>
#include <optional>

namespace gco::vehicle {

class VehicleIdentityDirector final {
public:
    VehicleIdentityDirector(
        platform::NativeVehicleIdentityAdapter& nativeAdapter,
        VehicleIdentitySystem& identity,
        VehiclePersistenceStore& persistence,
        crime::CrimeRegistry& crimeRegistry,
        EventBus& events);
    ~VehicleIdentityDirector();

    void initialize();
    void shutdown();
    void tickFiveHz(std::uint64_t nowMs, bool gameplayAllowed);
    bool saveIfDirty(std::string* reason = nullptr);

    [[nodiscard]] LogicalId activeCaseId() const noexcept { return activeCaseId_; }
    [[nodiscard]] std::optional<LogicalId> currentGetawayVehicleId() const noexcept { return currentVehicleId_; }
    [[nodiscard]] bool persistenceDirty() const noexcept { return persistenceDirty_; }

private:
    void onRobberyStarted(const RuntimeEvent& event);
    void onRobberyFinished(const RuntimeEvent& event);
    void onObservedSwap(const RuntimeEvent& event);
    static std::optional<LogicalId> payloadId(std::string_view payload, std::string_view key);

    platform::NativeVehicleIdentityAdapter& nativeAdapter_;
    VehicleIdentitySystem& identity_;
    VehiclePersistenceStore& persistence_;
    crime::CrimeRegistry& crimeRegistry_;
    EventBus& events_;
    EventBus::SubscriptionId robberyStartedSub_ = 0;
    EventBus::SubscriptionId robberyFinishedSub_ = 0;
    EventBus::SubscriptionId observedSwapSub_ = 0;
    LogicalId activeCaseId_ = 0;
    std::optional<LogicalId> currentVehicleId_;
    platform::VehicleHandle currentHandle_ = 0;
    std::uint64_t lastTickNowMs_ = 0;
    bool initialized_ = false;
    bool persistenceDirty_ = false;
};

} // namespace gco::vehicle
