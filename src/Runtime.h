#pragma once

#include "CoreServices.h"
#include "Foundation.h"
#include "crime/CrimeDebugInspector.h"
#include "crime/CrimeDirector.h"
#include "crime/CrimePersistence.h"
#include "dialogue/InvestigationDialogue.h"
#include "identity/ClerkRecognitionDirector.h"
#include "identity/IdentitySystem.h"
#include "investigation/DispatchDirector.h"
#include "investigation/InvestigationDirector.h"
#include "platform/AdapterDiagnostics.h"
#include "platform/FacialAnimationAdapter.h"
#include "platform/PedPresentationAdapter.h"
#include "platform/PlatformAdapters.h"
#include "platform/PoliceInvestigationAdapter.h"
#include "robbery/StoreRuntime.h"
#include "witness/WitnessDirector.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace gco {

class Runtime final {
public:
    Runtime();
    ~Runtime();

    void run();
    void requestStop() noexcept;

private:
    bool initialize();
    void configureScheduler();
    void configureDebugCommands();
    void tick();
    void tickFrame();
    void tickFiveHz();
    void tickTwoHz();
    void tickOneHz();
    void handleDebugHotkeys();
    void renderDebugOverlay();
    void runSyntheticCrimeDiagnostic();
    void logCaseInspector();
    bool saveCrimeState(const char* context);
    void startInvestigationDialogueDemo();
    void tickInvestigationDialogueDemo(std::uint64_t nowMs);
    void stopInvestigationDialogueDemo(const char* reason);
    void logDiagnostics();
    void logAdapterProbeReport(const platform::AdapterProbeReport& report, const char* commandName);
    void validateSaveDiagnostic();
    void shutdown();

    RuntimePaths paths_;
    RuntimeConfig config_;
    WorldStateStore worldState_;
    platform::PlatformServices platform_;
    platform::AdapterDiagnostics adapterDiagnostics_;
    platform::NativePedPresentationAdapter pedPresentation_;
    platform::NativeFacialAnimationAdapter facialAnimation_;
    platform::NativePoliceInvestigationAdapter policeInvestigationAdapter_;

    Scheduler scheduler_;
    EventBus eventBus_;
    LogicalIdGenerator idGenerator_;
    MissionCompatibilityGate missionGate_;
    DebugCommandRegistry debugCommands_;

    crime::CrimeRegistry crimeRegistry_;
    crime::CrimePersistenceStore crimePersistence_;
    crime::CrimeDirector crimeDirector_;
    identity::IdentitySystem identitySystem_;
    robbery::PrototypeStoreRuntime storeRuntime_;
    identity::ClerkRecognitionDirector clerkRecognitionDirector_;
    witness::WitnessDirector witnessDirector_;
    investigation::DispatchDirector dispatchDirector_;
    investigation::InvestigationDirector investigationDirector_;

    std::atomic_bool stopRequested_{false};
    bool initialized_ = false;
    bool f3WasDown_ = false;
    bool f4WasDown_ = false;
    bool f5WasDown_ = false;
    bool f6WasDown_ = false;
    bool f7WasDown_ = false;
    bool f8WasDown_ = false;
    bool f9WasDown_ = false;
    bool f10WasDown_ = false;
    bool f11WasDown_ = false;
    std::uint64_t frameCount_ = 0;
    std::uint64_t lastHeartbeatMs_ = 0;

    platform::MissionState missionState_{};
    std::optional<platform::PedSnapshot> playerSnapshot_;
    std::size_t nearbyPedCount_ = 0;
    std::size_t nearbyVehicleCount_ = 0;

    dialogue::InterviewPlan investigationDemoPlan_{};
    std::size_t investigationDemoTurnIndex_ = 0;
    platform::PedHandle investigationDemoOfficer_ = 0;
    platform::PedHandle investigationDemoWitness_ = 0;
    std::uint64_t investigationDemoNextTurnMs_ = 0;
};

} // namespace gco
