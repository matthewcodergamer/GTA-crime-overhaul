#pragma once

#include "PlatformAdapters.h"
#include "investigation/InvestigationDomain.h"

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gco::platform {

struct PoliceSceneSnapshot final {
    bool active = false;
    bool arrived = false;
    std::size_t liveOfficerCount = 0;
    PedHandle leadOfficer = 0;
};

class IPoliceInvestigationAdapter {
public:
    virtual ~IPoliceInvestigationAdapter() = default;
    virtual bool activateScene(const investigation::DispatchPlan& plan) = 0;
    virtual void taskInvestigation(const investigation::DispatchPlan& plan) = 0;
    [[nodiscard]] virtual PoliceSceneSnapshot sceneSnapshot(
        LogicalId caseId,
        const crime::CrimeLocation& center) const = 0;
    virtual bool beginInterview(
        LogicalId caseId,
        PedHandle witness,
        investigation::PresentationStyle style) = 0;
    virtual void presentInterviewTurn(
        LogicalId caseId,
        PedHandle witness,
        dialogue::InterviewSpeaker speaker,
        investigation::PresentationStyle style,
        std::string_view subtitle) = 0;
    virtual void endInterview(LogicalId caseId, PedHandle witness) = 0;
    virtual void cleanupScene(LogicalId caseId) = 0;
    virtual void cleanupAll() = 0;
};

class NativePoliceInvestigationAdapter final : public IPoliceInvestigationAdapter {
public:
    explicit NativePoliceInvestigationAdapter(PlatformServices& services) : services_(services) {}
    ~NativePoliceInvestigationAdapter() override;

    bool activateScene(const investigation::DispatchPlan& plan) override;
    void taskInvestigation(const investigation::DispatchPlan& plan) override;
    [[nodiscard]] PoliceSceneSnapshot sceneSnapshot(
        LogicalId caseId,
        const crime::CrimeLocation& center) const override;
    bool beginInterview(
        LogicalId caseId,
        PedHandle witness,
        investigation::PresentationStyle style) override;
    void presentInterviewTurn(
        LogicalId caseId,
        PedHandle witness,
        dialogue::InterviewSpeaker speaker,
        investigation::PresentationStyle style,
        std::string_view subtitle) override;
    void endInterview(LogicalId caseId, PedHandle witness) override;
    void cleanupScene(LogicalId caseId) override;
    void cleanupAll() override;

private:
    struct LiveScene final {
        investigation::DispatchPlan plan{};
        std::vector<PedHandle> officers;
        bool tasksIssued = false;
    };

    [[nodiscard]] bool policeModelReady() const;
    void requestPoliceModel() const;
    [[nodiscard]] PedHandle createOfficer(const crime::CrimeLocation& position, float heading) const;
    static float distanceSquared(const platform::Vec3& a, const crime::CrimeLocation& b) noexcept;
    void taskToPoint(PedHandle ped, const crime::CrimeLocation& point, float speed = 1.25f) const;
    void applyPresentation(PedHandle ped, investigation::PresentationStyle style) const;
    void deleteOfficer(PedHandle& ped) const;
    [[nodiscard]] LiveScene* findScene(LogicalId caseId) noexcept;
    [[nodiscard]] const LiveScene* findScene(LogicalId caseId) const noexcept;

    PlatformServices& services_;
    std::unordered_map<LogicalId, LiveScene> scenes_;
};

} // namespace gco::platform
