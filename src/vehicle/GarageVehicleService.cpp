#include "GarageVehicleService.h"

#include <utility>

namespace gco::vehicle {

GarageVehicleService::GarageVehicleService(
    VehicleIdentitySystem& identity,
    VehiclePersistenceStore& persistence,
    GarageNativeCallbacks native,
    GaragePaymentCallbacks payment,
    GarageServicePricing pricing)
    : identity_(identity),
      persistence_(persistence),
      native_(std::move(native)),
      payment_(std::move(payment)),
      pricing_(pricing) {}

GarageServiceResult GarageVehicleService::changePlate(
    const LogicalId vehicleId,
    const platform::VehicleHandle liveVehicle,
    const std::string_view requestedPlate,
    const int plateStyle,
    const std::uint64_t nowMs) {

    auto* record = identity_.find(vehicleId);
    if (record == nullptr) return {GarageServiceStatus::VehicleMissing, 0, "logical vehicle record missing"};
    if (!liveBindingMatches(vehicleId, liveVehicle)) {
        return {GarageServiceStatus::LiveVehicleMismatch, 0, "live GTA handle is not bound to this logical vehicle"};
    }
    const auto validation = validatePlateText(requestedPlate);
    if (!validation.valid || plateStyle < 0 || plateStyle > 5) {
        return {GarageServiceStatus::InvalidPlate, 0,
            !validation.valid ? validation.reason : "plate style outside conservative GTA V range 0-5"};
    }
    if (!native_.applyPlate) {
        return {GarageServiceStatus::NativeApplyFailed, 0, "vehicle plate mutation port unavailable"};
    }
    if (!payment_.balance || !payment_.charge || !payment_.credit) {
        return {GarageServiceStatus::UnsupportedPayment, 0, "payment gateway unavailable"};
    }
    const auto balance = payment_.balance();
    if (!balance) return {GarageServiceStatus::UnsupportedPayment, 0, "Story Mode cash balance unavailable"};
    if (*balance < pricing_.plateChangePrice) {
        return {GarageServiceStatus::InsufficientFunds, 0, "insufficient funds for plate change"};
    }
    if (!payment_.charge(pricing_.plateChangePrice)) {
        return {GarageServiceStatus::ChargeFailed, 0, "plate-change charge failed"};
    }

    const auto oldAppearance = record->appearance;
    const auto oldUpdatedAt = record->updatedAtMs;
    if (!native_.applyPlate(liveVehicle, validation.normalized, plateStyle)) {
        payment_.credit(pricing_.plateChangePrice);
        return {GarageServiceStatus::NativeApplyFailed, 0, "GTA plate native rejected/unavailable"};
    }
    if (!identity_.changePlate(vehicleId, validation.normalized, plateStyle, nowMs)) {
        native_.applyPlate(liveVehicle, oldAppearance.plateText, oldAppearance.plateStyle);
        payment_.credit(pricing_.plateChangePrice);
        return {GarageServiceStatus::VehicleMissing, 0, "logical vehicle disappeared during plate transaction"};
    }

    std::string saveReason;
    if (!persistence_.save(identity_, &saveReason)) {
        record = identity_.find(vehicleId);
        if (record != nullptr) {
            record->appearance = oldAppearance;
            record->updatedAtMs = oldUpdatedAt;
        }
        native_.applyPlate(liveVehicle, oldAppearance.plateText, oldAppearance.plateStyle);
        payment_.credit(pricing_.plateChangePrice);
        return {GarageServiceStatus::PersistenceFailed, 0, "plate transaction rolled back: " + saveReason};
    }

    return {GarageServiceStatus::Success, pricing_.plateChangePrice,
        "current vehicle plate changed and persisted; historical case evidence was not touched"};
}

GarageServiceResult GarageVehicleService::repaint(
    const LogicalId vehicleId,
    const platform::VehicleHandle liveVehicle,
    const int primaryColor,
    const int secondaryColor,
    const std::uint64_t nowMs) {

    auto* record = identity_.find(vehicleId);
    if (record == nullptr) return {GarageServiceStatus::VehicleMissing, 0, "logical vehicle record missing"};
    if (!liveBindingMatches(vehicleId, liveVehicle)) {
        return {GarageServiceStatus::LiveVehicleMismatch, 0, "live GTA handle is not bound to this logical vehicle"};
    }
    if (!native_.applyPaint) {
        return {GarageServiceStatus::NativeApplyFailed, 0, "vehicle paint mutation port unavailable"};
    }
    const auto oldAppearance = record->appearance;
    const auto oldUpdatedAt = record->updatedAtMs;
    if (!native_.applyPaint(liveVehicle, primaryColor, secondaryColor)) {
        return {GarageServiceStatus::NativeApplyFailed, 0, "GTA paint native rejected/unavailable"};
    }
    if (!identity_.repaint(vehicleId, primaryColor, secondaryColor, nowMs)) {
        native_.applyPaint(liveVehicle, oldAppearance.primaryColor, oldAppearance.secondaryColor);
        return {GarageServiceStatus::VehicleMissing, 0, "logical vehicle disappeared during paint transaction"};
    }

    std::string saveReason;
    if (!persistence_.save(identity_, &saveReason)) {
        record = identity_.find(vehicleId);
        if (record != nullptr) {
            record->appearance = oldAppearance;
            record->updatedAtMs = oldUpdatedAt;
        }
        native_.applyPaint(liveVehicle, oldAppearance.primaryColor, oldAppearance.secondaryColor);
        return {GarageServiceStatus::PersistenceFailed, 0, "paint transaction rolled back: " + saveReason};
    }
    return {GarageServiceStatus::Success, 0,
        "current vehicle colors changed and persisted; historical color evidence was not touched"};
}

bool GarageVehicleService::liveBindingMatches(
    const LogicalId vehicleId,
    const platform::VehicleHandle liveVehicle) const noexcept {

    const auto bound = identity_.logicalIdForHandle(liveVehicle);
    return bound.has_value() && *bound == vehicleId;
}

std::string_view garageServiceStatusName(const GarageServiceStatus value) noexcept {
    switch (value) {
    case GarageServiceStatus::Success: return "success";
    case GarageServiceStatus::VehicleMissing: return "vehicle_missing";
    case GarageServiceStatus::LiveVehicleMismatch: return "live_vehicle_mismatch";
    case GarageServiceStatus::InvalidPlate: return "invalid_plate";
    case GarageServiceStatus::UnsupportedPayment: return "unsupported_payment";
    case GarageServiceStatus::InsufficientFunds: return "insufficient_funds";
    case GarageServiceStatus::ChargeFailed: return "charge_failed";
    case GarageServiceStatus::NativeApplyFailed: return "native_apply_failed";
    case GarageServiceStatus::PersistenceFailed: return "persistence_failed";
    }
    return "unknown";
}

} // namespace gco::vehicle
