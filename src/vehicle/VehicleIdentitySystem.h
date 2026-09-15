#pragma once

#include "VehicleDomain.h"
#include "crime/CrimeRegistry.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gco::vehicle {

class VehicleIdentitySystem final {
public:
    explicit VehicleIdentitySystem(LogicalIdGenerator& ids) : ids_(ids) {}

    OwnedVehicleRecord& registerVehicle(
        VehicleRecordKind kind,
        VehicleAppearance appearance,
        VehicleModificationState modifications,
        std::uint64_t nowMs);
    OwnedVehicleRecord& ensureTemporaryVehicle(
        platform::VehicleHandle liveHandle,
        const platform::VehicleSnapshot& snapshot,
        VehicleModificationState modifications,
        bool stolen,
        std::uint64_t nowMs);

    bool bindLiveHandle(platform::VehicleHandle handle, LogicalId vehicleId);
    void unbindLiveHandle(platform::VehicleHandle handle) noexcept;
    void clearLiveBindings() noexcept { liveToLogical_.clear(); }
    [[nodiscard]] std::optional<LogicalId> logicalIdForHandle(platform::VehicleHandle handle) const noexcept;

    [[nodiscard]] OwnedVehicleRecord* find(LogicalId vehicleId) noexcept;
    [[nodiscard]] const OwnedVehicleRecord* find(LogicalId vehicleId) const noexcept;
    [[nodiscard]] std::vector<OwnedVehicleRecord>& records() noexcept { return records_; }
    [[nodiscard]] const std::vector<OwnedVehicleRecord>& records() const noexcept { return records_; }
    [[nodiscard]] const std::vector<VehicleBoloState>& bolos() const noexcept { return bolos_; }

    bool updateAppearanceFromLive(LogicalId vehicleId, const platform::VehicleSnapshot& snapshot, std::uint64_t nowMs);
    bool changePlate(LogicalId vehicleId, std::string normalizedPlate, int plateStyle, std::uint64_t nowMs);
    bool repaint(LogicalId vehicleId, int primaryColor, int secondaryColor, std::uint64_t nowMs);
    bool setStorageStatus(LogicalId vehicleId, VehicleStorageStatus status, std::string storageKey, std::uint64_t nowMs);
    bool markCrimeAssociation(LogicalId vehicleId, LogicalId caseId, bool getaway, std::uint64_t nowMs);

    bool refreshBoloFromCase(const crime::CaseFile& file, std::optional<LogicalId> linkedVehicleId, std::uint64_t nowMs);
    [[nodiscard]] const VehicleBoloState* findBolo(LogicalId caseId) const noexcept;
    bool deactivateBolo(LogicalId caseId, std::uint64_t nowMs);

    VehicleSwapResult recordVehicleSwap(
        LogicalId caseId,
        std::optional<LogicalId> previousVehicleId,
        std::optional<LogicalId> currentVehicleId,
        bool swapObserved,
        std::uint64_t nowMs);

    void restore(std::vector<OwnedVehicleRecord> records, std::vector<VehicleBoloState> bolos);
    [[nodiscard]] std::string debugSummary() const;

private:
    [[nodiscard]] OwnedVehicleRecord* findByAppearance(const platform::VehicleSnapshot& snapshot) noexcept;
    static bool parseVehicleDescriptor(
        std::string_view descriptor,
        std::uint32_t& modelHash,
        int& primaryColor,
        int& secondaryColor);
    static std::optional<std::string> parsePlateDescriptor(std::string_view descriptor);

    LogicalIdGenerator& ids_;
    std::vector<OwnedVehicleRecord> records_;
    std::vector<VehicleBoloState> bolos_;
    std::unordered_map<platform::VehicleHandle, LogicalId> liveToLogical_;
};

} // namespace gco::vehicle
