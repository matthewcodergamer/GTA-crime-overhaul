#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gco::platform {

// Runtime-only Script Hook V handles. These aliases must never appear in persisted data.
using EntityHandle = int;
using PedHandle = int;
using VehicleHandle = int;
using ObjectHandle = int;
using BlipHandle = int;
using InteriorId = int;

struct Vec3 final {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

constexpr float distanceSquared(const Vec3& a, const Vec3& b) noexcept {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

constexpr bool withinRadius(const Vec3& a, const Vec3& b, const float radius) noexcept {
    return radius >= 0.0f && distanceSquared(a, b) <= radius * radius;
}

struct GridCell final {
    int x = 0;
    int y = 0;
    int z = 0;

    friend constexpr bool operator==(const GridCell&, const GridCell&) = default;
};

GridCell gridCellFor(const Vec3& position, float cellSize) noexcept;

struct MissionState final {
    bool missionFlag = false;
    bool cutsceneActive = false;
    bool cutscenePlaying = false;
    bool playerControlOn = true;

    [[nodiscard]] bool shouldSuspendGameplay() const noexcept {
        return missionFlag || cutsceneActive || cutscenePlaying || !playerControlOn;
    }
};

struct PedComponentVariation final {
    int drawable = 0;
    int texture = 0;
    int palette = 0;
};

struct PedPropVariation final {
    int drawable = -1;
    int texture = 0;
};

enum class PedGender : std::uint8_t {
    Unknown,
    Masculine,
    Feminine
};

std::string_view pedGenderName(PedGender gender) noexcept;

struct PedSnapshot final {
    std::uint32_t modelHash = 0;
    Vec3 position{};
    float heading = 0.0f;
    bool alive = false;
    bool ragdoll = false;
    bool isPlayer = false;
    bool isHuman = false;
    PedGender gender = PedGender::Unknown;
    std::array<PedComponentVariation, 12> components{};
    std::array<PedPropVariation, 8> props{};
};

struct VehicleSnapshot final {
    std::uint32_t modelHash = 0;
    Vec3 position{};
    float heading = 0.0f;
    int primaryColor = 0;
    int secondaryColor = 0;
    std::string plate;
    int plateStyle = 0;
    std::optional<std::uint64_t> projectVehicleId;
};

struct BlipStyle final {
    int sprite = 1;
    int color = 0;
    float scale = 1.0f;
    bool shortRange = false;
};

struct Rgba final {
    int r = 255;
    int g = 255;
    int b = 255;
    int a = 255;
};

// Domain code selects a semantic visibility policy, never a native trace bitmask.
// Native trace flags remain an implementation detail of NativeWorldAdapter.
enum class LineOfSightProfile : std::uint8_t {
    DefaultVisibility,
    Count
};

std::string_view lineOfSightProfileName(LineOfSightProfile profile) noexcept;

enum class InputAction : std::uint8_t {
    Interact,
    Cancel,
    Sprint,
    Aim,
    Attack,
    EnterVehicle,
    Count
};

std::string_view inputActionName(InputAction action) noexcept;

// Pure bookkeeping for mod-owned runtime objects. Tracking is independent of GTA and
// therefore unit-testable. NativePropAttachmentAdapter consumes takeAll() during cleanup.
class OwnedObjectTracker final {
public:
    [[nodiscard]] bool track(ObjectHandle object);
    [[nodiscard]] bool untrack(ObjectHandle object);
    [[nodiscard]] bool contains(ObjectHandle object) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return objects_.size(); }
    [[nodiscard]] std::vector<ObjectHandle> takeAll();

private:
    std::vector<ObjectHandle> objects_;
};

} // namespace gco::platform
