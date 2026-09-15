#include "VehicleDomain.h"

#include <algorithm>
#include <cctype>

namespace gco::vehicle {

std::string_view vehicleRecordKindName(const VehicleRecordKind value) noexcept {
    switch (value) {
    case VehicleRecordKind::Owned: return "owned";
    case VehicleRecordKind::Temporary: return "temporary";
    case VehicleRecordKind::Stolen: return "stolen";
    }
    return "temporary";
}

std::optional<VehicleRecordKind> vehicleRecordKindFromString(const std::string_view value) noexcept {
    if (value == "owned") return VehicleRecordKind::Owned;
    if (value == "temporary") return VehicleRecordKind::Temporary;
    if (value == "stolen") return VehicleRecordKind::Stolen;
    return std::nullopt;
}

std::string_view vehicleStorageStatusName(const VehicleStorageStatus value) noexcept {
    switch (value) {
    case VehicleStorageStatus::World: return "world";
    case VehicleStorageStatus::Garage: return "garage";
    case VehicleStorageStatus::Impound: return "impound";
    case VehicleStorageStatus::Destroyed: return "destroyed";
    case VehicleStorageStatus::Missing: return "missing";
    }
    return "missing";
}

std::optional<VehicleStorageStatus> vehicleStorageStatusFromString(const std::string_view value) noexcept {
    if (value == "world") return VehicleStorageStatus::World;
    if (value == "garage") return VehicleStorageStatus::Garage;
    if (value == "impound") return VehicleStorageStatus::Impound;
    if (value == "destroyed") return VehicleStorageStatus::Destroyed;
    if (value == "missing") return VehicleStorageStatus::Missing;
    return std::nullopt;
}

PlateValidationResult validatePlateText(const std::string_view value) {
    PlateValidationResult result{};
    if (value.empty()) {
        result.reason = "plate cannot be empty";
        return result;
    }

    std::string normalized;
    normalized.reserve(value.size());
    bool previousSpace = false;
    for (const unsigned char raw : value) {
        if (raw == ' ') {
            if (!normalized.empty() && !previousSpace) normalized.push_back(' ');
            previousSpace = true;
            continue;
        }
        previousSpace = false;
        if (!std::isalnum(raw)) {
            result.reason = "plate supports only A-Z, 0-9 and spaces";
            return result;
        }
        normalized.push_back(static_cast<char>(std::toupper(raw)));
    }

    while (!normalized.empty() && normalized.back() == ' ') normalized.pop_back();
    if (normalized.empty()) {
        result.reason = "plate cannot be all spaces";
        return result;
    }
    if (normalized.size() > 8) {
        result.reason = "plate cannot exceed 8 characters";
        return result;
    }

    result.valid = true;
    result.normalized = std::move(normalized);
    result.reason = "valid conservative GTA V plate text";
    return result;
}

VehicleMatchResult matchVehicleBolo(
    const VehicleBoloState& bolo,
    const OwnedVehicleRecord& current) noexcept {

    VehicleMatchResult result{};
    if (!bolo.active) return result;

    if (bolo.modelHash.has_value()) {
        result.modelMatch = current.appearance.modelHash == *bolo.modelHash;
        if (result.modelMatch) result.score += 0.28f * std::clamp(bolo.modelConfidence, 0.0f, 1.0f);
    }

    if (bolo.primaryColor.has_value() && bolo.secondaryColor.has_value()) {
        result.colorMatch = current.appearance.primaryColor == *bolo.primaryColor
            && current.appearance.secondaryColor == *bolo.secondaryColor;
        if (result.colorMatch) result.score += 0.22f * std::clamp(bolo.colorConfidence, 0.0f, 1.0f);
    }

    if (bolo.plateText.has_value()) {
        result.directPlateMatch = current.appearance.plateText == *bolo.plateText;
        if (result.directPlateMatch) result.score += 0.50f * std::clamp(bolo.plateConfidence, 0.0f, 1.0f);
    }

    result.strongPhysicalLink = bolo.physicalContinuityKnown
        && bolo.linkedVehicleId.has_value()
        && *bolo.linkedVehicleId == current.id;
    if (result.strongPhysicalLink) result.score = std::max(result.score, 0.85f);
    result.score = std::clamp(result.score, 0.0f, 1.0f);
    return result;
}

} // namespace gco::vehicle
