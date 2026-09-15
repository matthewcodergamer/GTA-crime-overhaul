#include "VehicleIdentityAdapter.h"

#include <main.h>
#include <natives.h>

#include <algorithm>
#include <string>

namespace gco::platform {
namespace {

constexpr std::uint32_t kMichaelModel = 225514697u;
constexpr std::uint32_t kFranklinModel = 2602752943u;
constexpr std::uint32_t kTrevorModel = 2608926626u;

bool validVehicle(const VehicleHandle vehicle) noexcept {
    return vehicle != 0
        && ENTITY::DOES_ENTITY_EXIST(vehicle) != FALSE
        && ENTITY::IS_ENTITY_A_VEHICLE(vehicle) != FALSE;
}

} // namespace

std::optional<VehicleHandle> NativeVehicleIdentityAdapter::currentPlayerVehicle() const noexcept {
    const PedHandle player = PLAYER::PLAYER_PED_ID();
    if (player == 0 || ENTITY::DOES_ENTITY_EXIST(player) == FALSE
        || PED::IS_PED_IN_ANY_VEHICLE(player, FALSE) == FALSE) {
        return std::nullopt;
    }
    const VehicleHandle vehicle = PED::GET_VEHICLE_PED_IS_IN(player, FALSE);
    return validVehicle(vehicle) ? std::optional<VehicleHandle>{vehicle} : std::nullopt;
}

std::optional<VehicleReconstructionCapture> NativeVehicleIdentityAdapter::capture(const VehicleHandle vehicle) const {
    if (!validVehicle(vehicle)) return std::nullopt;
    const auto snapshot = services_.world.snapshotVehicle(vehicle);
    if (!snapshot) return std::nullopt;

    VehicleReconstructionCapture capture{};
    capture.appearance.modelHash = snapshot->modelHash;
    capture.appearance.primaryColor = snapshot->primaryColor;
    capture.appearance.secondaryColor = snapshot->secondaryColor;
    capture.appearance.plateText = snapshot->plate;
    capture.appearance.plateStyle = snapshot->plateStyle;
    if (const char* label = VEHICLE::GET_DISPLAY_NAME_FROM_VEHICLE_MODEL(snapshot->modelHash); label != nullptr) {
        capture.appearance.modelName = label;
    }

    capture.modifications.modKit = VEHICLE::GET_VEHICLE_MOD_KIT(vehicle);
    capture.modifications.wheelType = VEHICLE::GET_VEHICLE_WHEEL_TYPE(vehicle);
    capture.modifications.windowTint = VEHICLE::GET_VEHICLE_WINDOW_TINT(vehicle);
    capture.modifications.livery = VEHICLE::GET_VEHICLE_LIVERY(vehicle);
    for (int slot = 0; slot < static_cast<int>(vehicle::kVehicleModSlotCount); ++slot) {
        capture.modifications.mods[static_cast<std::size_t>(slot)] = VEHICLE::GET_VEHICLE_MOD(vehicle, slot);
        capture.modifications.toggleMods[static_cast<std::size_t>(slot)] = VEHICLE::IS_TOGGLE_MOD_ON(vehicle, slot) != FALSE;
    }
    capture.reportedStolen = VEHICLE::IS_VEHICLE_STOLEN(vehicle) != FALSE;
    return capture;
}

bool NativeVehicleIdentityAdapter::applyPlate(
    const VehicleHandle vehicle,
    const std::string_view normalizedPlate,
    const int plateStyle) const {

    if (!validVehicle(vehicle)) return false;
    const auto validation = vehicle::validatePlateText(normalizedPlate);
    if (!validation.valid) return false;
    const std::string text = validation.normalized;
    VEHICLE::SET_VEHICLE_NUMBER_PLATE_TEXT(vehicle, text.c_str());
    VEHICLE::SET_VEHICLE_NUMBER_PLATE_TEXT_INDEX(vehicle, plateStyle);
    return true;
}

bool NativeVehicleIdentityAdapter::applyPaint(
    const VehicleHandle vehicle,
    const int primaryColor,
    const int secondaryColor) const {

    if (!validVehicle(vehicle)) return false;
    VEHICLE::SET_VEHICLE_COLOURS(vehicle, primaryColor, secondaryColor);
    return true;
}

bool NativeVehicleIdentityAdapter::applyReconstruction(
    const VehicleHandle vehicle,
    const VehicleReconstructionCapture& state) const {

    if (!validVehicle(vehicle)) return false;
    VEHICLE::SET_VEHICLE_MOD_KIT(vehicle, state.modifications.modKit);
    VEHICLE::SET_VEHICLE_WHEEL_TYPE(vehicle, state.modifications.wheelType);
    for (int slot = 0; slot < static_cast<int>(vehicle::kVehicleModSlotCount); ++slot) {
        const auto index = static_cast<std::size_t>(slot);
        if (state.modifications.mods[index] >= 0) {
            VEHICLE::SET_VEHICLE_MOD(vehicle, slot, state.modifications.mods[index], FALSE);
        }
        if (state.modifications.toggleMods[index]) {
            VEHICLE::TOGGLE_VEHICLE_MOD(vehicle, slot, TRUE);
        }
    }
    VEHICLE::SET_VEHICLE_WINDOW_TINT(vehicle, state.modifications.windowTint);
    if (state.modifications.livery >= 0) VEHICLE::SET_VEHICLE_LIVERY(vehicle, state.modifications.livery);
    if (!applyPaint(vehicle, state.appearance.primaryColor, state.appearance.secondaryColor)) return false;
    return applyPlate(vehicle, state.appearance.plateText, state.appearance.plateStyle);
}

const char* StoryModeVehicleServicePayment::activeCashStatName() noexcept {
    const PedHandle player = PLAYER::PLAYER_PED_ID();
    if (player == 0 || ENTITY::DOES_ENTITY_EXIST(player) == FALSE) return nullptr;
    const auto model = static_cast<std::uint32_t>(ENTITY::GET_ENTITY_MODEL(player));
    if (model == kMichaelModel) return "SP0_TOTAL_CASH";
    if (model == kFranklinModel) return "SP1_TOTAL_CASH";
    if (model == kTrevorModel) return "SP2_TOTAL_CASH";
    return nullptr;
}

bool StoryModeVehicleServicePayment::supported() const noexcept {
    return activeCashStatName() != nullptr;
}

std::optional<int> StoryModeVehicleServicePayment::balance() const noexcept {
    const char* stat = activeCashStatName();
    if (stat == nullptr) return std::nullopt;
    int value = 0;
    if (STATS::STAT_GET_INT(MISC::GET_HASH_KEY(stat), &value, -1) == FALSE) return std::nullopt;
    return std::max(0, value);
}

bool StoryModeVehicleServicePayment::charge(const int amount) const noexcept {
    if (amount < 0) return false;
    const char* stat = activeCashStatName();
    const auto current = balance();
    if (stat == nullptr || !current || *current < amount) return false;
    return STATS::STAT_SET_INT(MISC::GET_HASH_KEY(stat), *current - amount, TRUE) != FALSE;
}

} // namespace gco::platform
