#pragma once

#include "StoreDomain.h"
#include "platform/PlatformTypes.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gco::robbery {

enum class TargetValidationStatus : std::uint8_t {
    VerifiedInGame,
    VerifiedData,
    ReferenceOnly,
    CustomRequired,
    Rejected,
    Unknown
};

struct StoreAnchor final {
    platform::Vec3 position{};
    float heading = 0.0f;
};

struct StoreVolume final {
    platform::Vec3 min{};
    platform::Vec3 max{};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool contains(const platform::Vec3& point) const noexcept;
    [[nodiscard]] platform::Vec3 center() const noexcept;
};

struct PrototypeStoreTarget final {
    std::string id = "prototype_24_7";
    std::string displayName = "Prototype 24/7";
    bool enabled = false;
    TargetValidationStatus validation = TargetValidationStatus::ReferenceOnly;
    std::string validationNotes;
    std::string legacyBuild;
    std::string enhancedBuild;

    float activationRadius = 75.0f;
    float clerkAcquireRadius = 2.0f;
    float threatMaxDistance = 8.0f;
    StoreVolume businessVolume{};
    bool hasBusinessVolume = false;
    StoreAnchor clerk{};
    bool hasClerkAnchor = false;
    std::vector<StoreAnchor> registers;
    std::optional<StoreAnchor> safe;
    std::vector<StoreVolume> customerZones;
    std::vector<StoreAnchor> entrances;
    std::vector<StoreAnchor> exits;

    int registerCashMin = 300;
    int registerCashMax = 1500;
    int safeCashMin = 1000;
    int safeCashMax = 5000;
    StoreTuning tuning{};

    [[nodiscard]] bool productionReady(std::string* reason = nullptr) const;
    [[nodiscard]] platform::Vec3 activationCenter() const noexcept;
};

struct TargetLoadReport final {
    bool parsed = false;
    bool productionReady = false;
    std::string detail;
};

class PrototypeStoreTargetLoader final {
public:
    static TargetLoadReport load(
        const std::filesystem::path& file,
        PrototypeStoreTarget& outTarget);
};

std::string_view targetValidationStatusName(TargetValidationStatus value) noexcept;
std::optional<TargetValidationStatus> targetValidationStatusFromString(std::string_view value) noexcept;

} // namespace gco::robbery
