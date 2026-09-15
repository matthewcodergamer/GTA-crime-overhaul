#pragma once

#include "StorePersistence.h"
#include "StoreTarget.h"
#include "crime/CrimeDirector.h"
#include "crime/CrimeRegistry.h"
#include "platform/PedPresentationAdapter.h"
#include "platform/PlatformAdapters.h"
#include "platform/RobberyPedAdapter.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace gco::robbery {

class PrototypeStoreRuntime final {
public:
    PrototypeStoreRuntime(
        RuntimePaths paths,
        WorldStateStore& worldState,
        platform::PlatformServices& platform,
        platform::NativePedPresentationAdapter& pedPresentation,
        crime::CrimeDirector& crimeDirector,
        crime::CrimeRegistry& crimeRegistry,
        LogicalIdGenerator& ids,
        EventBus& events);

    bool initialize(std::uint64_t persistentNowMs, std::string* reason = nullptr);
    void tickFrame(std::uint64_t persistentNowMs, bool gameplayAllowed);
    void tickFiveHz(std::uint64_t persistentNowMs, bool gameplayAllowed);
    void suspend(std::uint64_t persistentNowMs, RobberyAbortReason reason);
    bool saveIfDirty(std::string* reason = nullptr);
    bool save(std::string* reason = nullptr);
    void shutdown(std::uint64_t persistentNowMs);

    void debugSurveyCurrentPosition();
    void debugInspect() const;
    bool debugIssueDemand(StoreDemand demand, std::uint64_t persistentNowMs);
    void renderDebug() const;

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool targetReady() const noexcept { return targetReady_; }
    [[nodiscard]] bool detailedActive() const noexcept { return detailedActive_; }
    [[nodiscard]] bool persistenceDirty() const noexcept { return persistenceDirty_; }
    [[nodiscard]] platform::PedHandle boundClerkPed() const noexcept { return clerkPed_; }
    [[nodiscard]] const PrototypeStoreTarget& target() const noexcept { return target_; }
    [[nodiscard]] const PrototypeStoreModel& model() const noexcept { return model_; }

private:
    platform::PedHandle acquireClerkPed(std::uint64_t persistentNowMs);
    bool validateBoundClerk(std::uint64_t persistentNowMs);
    bool threatCondition() const;
    void beginRobbery(std::uint64_t persistentNowMs);
    void executeOpeningReaction(std::uint64_t persistentNowMs);
    void executeAction(const StoreAction& action, std::uint64_t persistentNowMs);
    void recordReportEvidence(bool alarmSource, std::uint64_t persistentNowMs);
    void finishSession(RobberyAbortReason reason, std::uint64_t persistentNowMs);
    StoreDemand nextContextDemand() const;
    std::optional<StoreAnchor> cashAnchor(std::size_t cashSourceIndex) const;
    void publish(std::string topic, std::string payload = {});

    RuntimePaths paths_;
    WorldStateStore& worldState_;
    platform::PlatformServices& platform_;
    platform::NativePedPresentationAdapter& pedPresentation_;
    platform::NativeRobberyPedAdapter robberyPed_;
    crime::CrimeDirector& crimeDirector_;
    crime::CrimeRegistry& crimeRegistry_;
    LogicalIdGenerator& ids_;
    EventBus& events_;
    PrototypeStorePersistence persistence_;

    PrototypeStoreTarget target_{};
    PrototypeStoreModel model_{};
    TargetLoadReport targetLoad_{};

    platform::PedHandle clerkPed_ = 0;
    bool initialized_ = false;
    bool targetReady_ = false;
    bool detailedActive_ = false;
    bool persistenceDirty_ = false;
    bool phoneFallbackLogged_ = false;
    std::string activeIncidentKey_;
    std::optional<StoreAction> pendingCashOffer_;
    std::uint64_t pendingCashOfferAtMs_ = 0;
};

} // namespace gco::robbery
