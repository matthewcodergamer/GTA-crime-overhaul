#pragma once

#include "Foundation.h"
#include "VehicleIdentitySystem.h"

#include <filesystem>
#include <string>
#include <vector>

namespace gco::vehicle {

class VehiclePersistenceStore final {
public:
    explicit VehiclePersistenceStore(RuntimePaths paths);

    bool load(VehicleIdentitySystem& system, std::string* reason = nullptr);
    bool save(const VehicleIdentitySystem& system, std::string* reason = nullptr) const;

    [[nodiscard]] const std::filesystem::path& primaryPath() const noexcept { return primary_; }
    [[nodiscard]] const std::filesystem::path& backupPath() const noexcept { return backup_; }

private:
    bool decode(
        const std::string& document,
        std::vector<OwnedVehicleRecord>& records,
        std::vector<VehicleBoloState>& bolos,
        std::string* reason) const;
    [[nodiscard]] std::string encode(const VehicleIdentitySystem& system) const;
    bool atomicWrite(const std::string& document, std::string* reason) const;

    RuntimePaths paths_;
    std::filesystem::path primary_;
    std::filesystem::path backup_;
};

} // namespace gco::vehicle
