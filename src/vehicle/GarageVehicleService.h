#pragma once

#include "VehicleIdentitySystem.h"
#include "VehiclePersistence.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace gco::vehicle {

struct GarageServicePricing final {
    int plateChangePrice = 500;
};

struct GaragePaymentCallbacks final {
    std::function<std::optional<int>()> balance;
    std::function<bool(int)> charge;
    std::function<bool(int)> credit;
};

struct GarageNativeCallbacks final {
    std::function<bool(platform::VehicleHandle, std::string_view, int)> applyPlate;
    std::function<bool(platform::VehicleHandle, int, int)> applyPaint;
};

enum class GarageServiceStatus : std::uint8_t {
    Success,
    VehicleMissing,
    LiveVehicleMismatch,
    InvalidPlate,
    UnsupportedPayment,
    InsufficientFunds,
    ChargeFailed,
    NativeApplyFailed,
    PersistenceFailed
};

struct GarageServiceResult final {
    GarageServiceStatus status = GarageServiceStatus::VehicleMissing;
    int charged = 0;
    std::string detail;
    [[nodiscard]] bool ok() const noexcept { return status == GarageServiceStatus::Success; }
};

class GarageVehicleService final {
public:
    GarageVehicleService(
        VehicleIdentitySystem& identity,
        VehiclePersistenceStore& persistence,
        GarageNativeCallbacks native,
        GaragePaymentCallbacks payment,
        GarageServicePricing pricing = {});

    [[nodiscard]] int plateChangePrice() const noexcept { return pricing_.plateChangePrice; }
    GarageServiceResult changePlate(
        LogicalId vehicleId,
        platform::VehicleHandle liveVehicle,
        std::string_view requestedPlate,
        int plateStyle,
        std::uint64_t nowMs);
    GarageServiceResult repaint(
        LogicalId vehicleId,
        platform::VehicleHandle liveVehicle,
        int primaryColor,
        int secondaryColor,
        std::uint64_t nowMs);

private:
    bool liveBindingMatches(LogicalId vehicleId, platform::VehicleHandle liveVehicle) const noexcept;

    VehicleIdentitySystem& identity_;
    VehiclePersistenceStore& persistence_;
    GarageNativeCallbacks native_;
    GaragePaymentCallbacks payment_;
    GarageServicePricing pricing_;
};

[[nodiscard]] std::string_view garageServiceStatusName(GarageServiceStatus value) noexcept;

} // namespace gco::vehicle
