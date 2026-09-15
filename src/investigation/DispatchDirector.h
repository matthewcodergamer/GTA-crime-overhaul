#pragma once

#include "InvestigationDomain.h"
#include "crime/CrimeDirector.h"
#include "platform/PoliceInvestigationAdapter.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gco::investigation {

struct DispatchTuning final {
    float highDetailRadius = 180.0f;
    std::uint64_t sceneLifetimeMs = 6ull * 60ull * 1000ull;
    std::uint64_t businessRecoveryDelayMs = 2ull * 60ull * 1000ull;
};

class DispatchDirector final {
public:
    using GeometryResolver = std::function<SceneGeometry(
        const crime::CaseFile&,
        const std::vector<const crime::CrimeEvent*>&)>;

    DispatchDirector(
        crime::CrimeRegistry& registry,
        crime::CrimeDirector& crimeDirector,
        platform::IPoliceInvestigationAdapter& adapter,
        EventBus& events,
        GeometryResolver geometryResolver = {},
        DispatchTuning tuning = {});
    ~DispatchDirector();

    void initialize(std::uint64_t nowMs);
    void shutdown();
    void tickTwoHz(
        std::uint64_t nowMs,
        bool gameplayAllowed,
        std::optional<platform::Vec3> playerPosition);

    [[nodiscard]] const SceneRecord* findScene(LogicalId caseId) const noexcept;
    [[nodiscard]] const DispatchPlan* findPlan(LogicalId caseId) const noexcept;
    [[nodiscard]] std::vector<const SceneRecord*> scenes() const;
    [[nodiscard]] bool highDetailArrived(LogicalId caseId) const noexcept;
    [[nodiscard]] platform::PedHandle leadOfficer(LogicalId caseId) const;
    [[nodiscard]] bool casePersistenceDirty() const noexcept { return casePersistenceDirty_; }
    void clearCasePersistenceDirty() noexcept { casePersistenceDirty_ = false; }
    [[nodiscard]] std::string debugSummary() const;

private:
    void ensureScenes(std::uint64_t nowMs);
    void ensureScene(const crime::CaseFile& file, std::uint64_t nowMs);
    void recordLogicalArrival(SceneRecord& scene, const DispatchPlan& plan, std::uint64_t nowMs);
    void updateDetail(SceneRecord& scene, const DispatchPlan& plan, std::uint64_t nowMs,
        bool gameplayAllowed, const std::optional<platform::Vec3>& playerPosition);
    void expireScene(SceneRecord& scene, const DispatchPlan& plan, std::uint64_t nowMs);
    [[nodiscard]] static float distanceSquared(const platform::Vec3& point, const crime::CrimeLocation& target) noexcept;
    [[nodiscard]] static bool caseEligible(const crime::CaseFile& file) noexcept;

    crime::CrimeRegistry& registry_;
    crime::CrimeDirector& crimeDirector_;
    platform::IPoliceInvestigationAdapter& adapter_;
    EventBus& events_;
    GeometryResolver geometryResolver_;
    DispatchTuning tuning_;
    std::unordered_map<LogicalId, SceneRecord> scenes_;
    std::unordered_map<LogicalId, DispatchPlan> plans_;
    bool initialized_ = false;
    bool casePersistenceDirty_ = false;
};

} // namespace gco::investigation
