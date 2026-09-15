#pragma once

#include "CoreServices.h"
#include "crime/CrimeDomain.h"
#include "platform/PlatformTypes.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gco::vehicle {

constexpr std::size_t kVehicleModSlotCount = 50;

enum class VehicleRecordKind : std::uint8_t {
    Owned,
    Temporary,
    Stolen
};

enum class VehicleStorageStatus : std::uint8_t {
    World,
    Garage,
    Impound,
    Destroyed,
    Missing
};

struct VehicleModificationState final {
    int modKit = 0;
    int wheelType = 0;
    int windowTint = -1;
    int livery = -1;
    std::array<int, kVehicleModSlotCount> mods{};
    std::array<bool, kVehicleModSlotCount> toggleMods{};

    VehicleModificationState() { mods.fill(-1); }
};

struct VehicleAppearance final {
    std::uint32_t modelHash = 0;
    std::string modelName;
    int primaryColor = 0;
    int secondaryColor = 0;
    std::string plateText;
    int plateStyle = 0;
};

struct VehicleCrimeHistory final {
    bool usedInCrime = false;
    bool usedAsGetaway = false;
    bool reportedStolen = false;
    std::uint32_t crimeCount = 0;
    std::uint64_t lastCrimeAtMs = 0;
    std::vector<LogicalId> caseIds;
};

struct OwnedVehicleRecord final {
    static constexpr std::uint32_t ModelVersion = 1;

    LogicalId id = 0;
    VehicleRecordKind kind = VehicleRecordKind::Owned;
    VehicleAppearance appearance{};
    VehicleModificationState modifications{};
    VehicleStorageStatus storageStatus = VehicleStorageStatus::World;
    std::string storageKey;
    VehicleCrimeHistory crimeHistory{};
    std::uint64_t createdAtMs = 0;
    std::uint64_t updatedAtMs = 0;
};

struct VehicleBoloState final {
    LogicalId caseId = 0;
    std::optional<LogicalId> linkedVehicleId;
    bool active = false;
    std::optional<std::uint32_t> modelHash;
    std::optional<int> primaryColor;
    std::optional<int> secondaryColor;
    std::optional<std::string> plateText;
    float modelConfidence = 0.0f;
    float colorConfidence = 0.0f;
    float plateConfidence = 0.0f;
    bool physicalContinuityKnown = false;
    std::uint64_t updatedAtMs = 0;
};

struct VehicleMatchResult final {
    float score = 0.0f;
    bool directPlateMatch = false;
    bool modelMatch = false;
    bool colorMatch = false;
    bool strongPhysicalLink = false;
};

struct PlateValidationResult final {
    bool valid = false;
    std::string normalized;
    std::string reason;
};

struct VehicleSwapResult final {
    bool continuityPreserved = false;
    std::optional<LogicalId> previousVehicleId;
    std::optional<LogicalId> currentVehicleId;
};

[[nodiscard]] std::string_view vehicleRecordKindName(VehicleRecordKind value) noexcept;
[[nodiscard]] std::optional<VehicleRecordKind> vehicleRecordKindFromString(std::string_view value) noexcept;
[[nodiscard]] std::string_view vehicleStorageStatusName(VehicleStorageStatus value) noexcept;
[[nodiscard]] std::optional<VehicleStorageStatus> vehicleStorageStatusFromString(std::string_view value) noexcept;

// GTA V plates display at most 8 useful characters. The project intentionally uses a conservative,
// deterministic subset: A-Z, 0-9 and internal spaces. Unsupported punctuation is rejected rather
// than relying on build/font-specific glyph behavior.
[[nodiscard]] PlateValidationResult validatePlateText(std::string_view value);
[[nodiscard]] VehicleMatchResult matchVehicleBolo(
    const VehicleBoloState& bolo,
    const OwnedVehicleRecord& current) noexcept;

} // namespace gco::vehicle
