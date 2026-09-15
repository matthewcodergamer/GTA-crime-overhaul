#pragma once

#include "PlatformTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gco::platform {

class IWorldAdapter {
public:
    virtual ~IWorldAdapter() = default;

    [[nodiscard]] virtual PedHandle playerPed() const = 0;
    [[nodiscard]] virtual bool entityExists(EntityHandle entity) const = 0;
    [[nodiscard]] virtual bool pedExists(PedHandle ped) const = 0;
    [[nodiscard]] virtual bool vehicleExists(VehicleHandle vehicle) const = 0;
    [[nodiscard]] virtual std::optional<Vec3> entityPosition(EntityHandle entity) const = 0;
    [[nodiscard]] virtual bool playerAlive() const = 0;
    [[nodiscard]] virtual int wantedLevel() const = 0;
    [[nodiscard]] virtual MissionState missionState() const = 0;

    [[nodiscard]] virtual std::vector<PedHandle> nearbyPeds(
        const Vec3& center, float radius, std::size_t maxResults) const = 0;
    [[nodiscard]] virtual std::vector<VehicleHandle> nearbyVehicles(
        const Vec3& center, float radius, std::size_t maxResults) const = 0;

    [[nodiscard]] virtual bool hasLineOfSight(
        EntityHandle from,
        EntityHandle to,
        LineOfSightProfile profile = LineOfSightProfile::DefaultVisibility) const = 0;
    [[nodiscard]] virtual std::optional<PedSnapshot> snapshotPed(PedHandle ped) const = 0;
    [[nodiscard]] virtual std::optional<VehicleSnapshot> snapshotVehicle(
        VehicleHandle vehicle,
        std::optional<std::uint64_t> projectVehicleId = std::nullopt) const = 0;
};

class IAnimationAdapter {
public:
    virtual ~IAnimationAdapter() = default;

    virtual bool requestDictionary(std::string_view dictionary, std::uint32_t timeoutMs) = 0;
    virtual void releaseDictionary(std::string_view dictionary) = 0;
    virtual bool stopAnimation(PedHandle ped, std::string_view dictionary, std::string_view clip, float blendOut) = 0;
    virtual bool cancelPedTasks(PedHandle ped, bool immediate) = 0;
};

class IPropAttachmentAdapter {
public:
    virtual ~IPropAttachmentAdapter() = default;

    // Only project-created objects should be adopted. Adopted objects are deleted by
    // cleanupOwned() on runtime shutdown if the owning gameplay system did not already delete them.
    virtual bool adoptOwned(ObjectHandle object) = 0;
    virtual bool releaseOwnership(ObjectHandle object) = 0;
    [[nodiscard]] virtual std::size_t ownedCount() const noexcept = 0;

    virtual bool attach(
        ObjectHandle object,
        EntityHandle parent,
        int parentBoneIndex,
        const Vec3& offset,
        const Vec3& rotation,
        bool collision,
        bool fixedRotation) = 0;
    virtual bool detach(ObjectHandle object, bool collision, bool dynamic) = 0;
    virtual void deleteOwned(ObjectHandle& object) = 0;
    virtual std::size_t cleanupOwned() = 0;
};

class IInteriorDoorAdapter {
public:
    virtual ~IInteriorDoorAdapter() = default;

    [[nodiscard]] virtual InteriorId interiorFromEntity(EntityHandle entity) const = 0;
    [[nodiscard]] virtual bool isInteriorReady(InteriorId interior) const = 0;
    virtual void refreshInterior(InteriorId interior) = 0;

    [[nodiscard]] virtual bool doorExists(std::uint32_t doorSystemHash) const = 0;
    virtual bool addDoorToSystem(
        std::uint32_t doorSystemHash,
        std::uint32_t modelHash,
        const Vec3& position) = 0;
    virtual void removeDoorFromSystem(std::uint32_t doorSystemHash) = 0;
    virtual void setClosestDoorLocked(
        std::uint32_t modelHash,
        const Vec3& position,
        bool locked,
        float heading) = 0;
};

class IUiAdapter {
public:
    virtual ~IUiAdapter() = default;

    [[nodiscard]] virtual BlipHandle addBlip(const Vec3& position, const BlipStyle& style) = 0;
    virtual void removeBlip(BlipHandle& blip) = 0;
    virtual void setBlipName(BlipHandle blip, std::string_view text) = 0;
    virtual void subtitle(std::string_view text, int durationMs, bool drawImmediately) = 0;
    virtual void helpText(std::string_view text, bool beep) = 0;
};

class IAudioAdapter {
public:
    virtual ~IAudioAdapter() = default;

    virtual bool playAmbientSpeech(
        PedHandle ped,
        std::string_view speechName,
        std::string_view speechParam) = 0;
    virtual void stopPedSpeaking(PedHandle ped) = 0;
};

class IInputAdapter {
public:
    virtual ~IInputAdapter() = default;

    // Gameplay code sees semantic actions only. GTA control IDs remain inside the native adapter.
    [[nodiscard]] virtual bool pressed(InputAction action) const = 0;
    [[nodiscard]] virtual bool justPressed(InputAction action) const = 0;
    [[nodiscard]] virtual bool justReleased(InputAction action) const = 0;
    [[nodiscard]] virtual float normal(InputAction action) const = 0;
};

class IDebugDrawAdapter {
public:
    virtual ~IDebugDrawAdapter() = default;

    virtual void line(const Vec3& from, const Vec3& to, const Rgba& color) = 0;
    virtual void box(const Vec3& min, const Vec3& max, const Rgba& color) = 0;
    virtual void witnessCone(
        const Vec3& origin,
        float headingDegrees,
        float halfAngleDegrees,
        float range,
        const Rgba& color) = 0;
};

class NativeWorldAdapter final : public IWorldAdapter {
public:
    [[nodiscard]] PedHandle playerPed() const override;
    [[nodiscard]] bool entityExists(EntityHandle entity) const override;
    [[nodiscard]] bool pedExists(PedHandle ped) const override;
    [[nodiscard]] bool vehicleExists(VehicleHandle vehicle) const override;
    [[nodiscard]] std::optional<Vec3> entityPosition(EntityHandle entity) const override;
    [[nodiscard]] bool playerAlive() const override;
    [[nodiscard]] int wantedLevel() const override;
    [[nodiscard]] MissionState missionState() const override;
    [[nodiscard]] std::vector<PedHandle> nearbyPeds(
        const Vec3& center, float radius, std::size_t maxResults) const override;
    [[nodiscard]] std::vector<VehicleHandle> nearbyVehicles(
        const Vec3& center, float radius, std::size_t maxResults) const override;
    [[nodiscard]] bool hasLineOfSight(
        EntityHandle from,
        EntityHandle to,
        LineOfSightProfile profile) const override;
    [[nodiscard]] std::optional<PedSnapshot> snapshotPed(PedHandle ped) const override;
    [[nodiscard]] std::optional<VehicleSnapshot> snapshotVehicle(
        VehicleHandle vehicle,
        std::optional<std::uint64_t> projectVehicleId = std::nullopt) const override;
};

class NativeAnimationAdapter final : public IAnimationAdapter {
public:
    bool requestDictionary(std::string_view dictionary, std::uint32_t timeoutMs) override;
    void releaseDictionary(std::string_view dictionary) override;
    bool stopAnimation(PedHandle ped, std::string_view dictionary, std::string_view clip, float blendOut) override;
    bool cancelPedTasks(PedHandle ped, bool immediate) override;
};

class NativePropAttachmentAdapter final : public IPropAttachmentAdapter {
public:
    bool adoptOwned(ObjectHandle object) override;
    bool releaseOwnership(ObjectHandle object) override;
    [[nodiscard]] std::size_t ownedCount() const noexcept override { return owned_.size(); }
    bool attach(
        ObjectHandle object,
        EntityHandle parent,
        int parentBoneIndex,
        const Vec3& offset,
        const Vec3& rotation,
        bool collision,
        bool fixedRotation) override;
    bool detach(ObjectHandle object, bool collision, bool dynamic) override;
    void deleteOwned(ObjectHandle& object) override;
    std::size_t cleanupOwned() override;

private:
    OwnedObjectTracker owned_;
};

class NativeInteriorDoorAdapter final : public IInteriorDoorAdapter {
public:
    [[nodiscard]] InteriorId interiorFromEntity(EntityHandle entity) const override;
    [[nodiscard]] bool isInteriorReady(InteriorId interior) const override;
    void refreshInterior(InteriorId interior) override;
    [[nodiscard]] bool doorExists(std::uint32_t doorSystemHash) const override;
    bool addDoorToSystem(
        std::uint32_t doorSystemHash,
        std::uint32_t modelHash,
        const Vec3& position) override;
    void removeDoorFromSystem(std::uint32_t doorSystemHash) override;
    void setClosestDoorLocked(
        std::uint32_t modelHash,
        const Vec3& position,
        bool locked,
        float heading) override;
};

class NativeUiAdapter final : public IUiAdapter {
public:
    [[nodiscard]] BlipHandle addBlip(const Vec3& position, const BlipStyle& style) override;
    void removeBlip(BlipHandle& blip) override;
    void setBlipName(BlipHandle blip, std::string_view text) override;
    void subtitle(std::string_view text, int durationMs, bool drawImmediately) override;
    void helpText(std::string_view text, bool beep) override;
};

class NativeAudioAdapter final : public IAudioAdapter {
public:
    bool playAmbientSpeech(PedHandle ped, std::string_view speechName, std::string_view speechParam) override;
    void stopPedSpeaking(PedHandle ped) override;
};

class NativeInputAdapter final : public IInputAdapter {
public:
    [[nodiscard]] bool pressed(InputAction action) const override;
    [[nodiscard]] bool justPressed(InputAction action) const override;
    [[nodiscard]] bool justReleased(InputAction action) const override;
    [[nodiscard]] float normal(InputAction action) const override;
};

class NativeDebugDrawAdapter final : public IDebugDrawAdapter {
public:
    void line(const Vec3& from, const Vec3& to, const Rgba& color) override;
    void box(const Vec3& min, const Vec3& max, const Rgba& color) override;
    void witnessCone(
        const Vec3& origin,
        float headingDegrees,
        float halfAngleDegrees,
        float range,
        const Rgba& color) override;
};

class PlatformServices final {
public:
    NativeWorldAdapter world;
    NativeAnimationAdapter animation;
    NativePropAttachmentAdapter props;
    NativeInteriorDoorAdapter interiors;
    NativeUiAdapter ui;
    NativeAudioAdapter audio;
    NativeInputAdapter input;
    NativeDebugDrawAdapter debugDraw;

    // Best-effort cleanup of resources owned by the adapter layer. Safe to call repeatedly.
    std::size_t cleanupOwnedResources();
};

} // namespace gco::platform
