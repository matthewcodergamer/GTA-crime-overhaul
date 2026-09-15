#pragma once

#include "PlatformAdapters.h"
#include "vehicle/VehicleDomain.h"

#include <cstdint>
#include <optional>
#include <string>

namespace gco::platform {

struct VehicleReconstructionCapture final {
    vehicle::VehicleAppearance appearance{};
    vehicle::VehicleModificationState modifications{};
    bool reportedStolen = false;
};

class NativeVehicleIdentityAdapter final {
public:
    explicit NativeVehicleIdentityAdapter(PlatformServices& services) : services_(services) {}

    [[nodiscard]] std::optional<VehicleHandle> currentPlayerVehicle() const noexcept;
    [[nodiscard]] std::optional<VehicleReconstructionCapture> capture(VehicleHandle vehicle) const;
    bool applyPlate(VehicleHandle vehicle, std::string_view normalizedPlate, int plateStyle) const;
    bool applyPaint(VehicleHandle vehicle, int primaryColor, int secondaryColor) const;
    bool applyReconstruction(VehicleHandle vehicle, const VehicleReconstructionCapture& state) const;

private:
    PlatformServices& services_;
};

class StoryModeVehicleServicePayment final {
public:
    [[nodiscard]] bool supported() const noexcept;
    [[nodiscard]] std::optional<int> balance() const noexcept;
    bool charge(int amount) const noexcept;
    bool credit(int amount) const noexcept;

private:
    [[nodiscard]] static const char* activeCashStatName() noexcept;
    bool setBalance(int amount) const noexcept;
};

} // namespace gco::platform
