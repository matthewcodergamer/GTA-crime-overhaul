#include "Runtime.h"

#include <Windows.h>
#include <main.h>

#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <sstream>
#include <string>

namespace gco {
namespace {

constexpr float kDebugWorldRadius = 80.0f;
constexpr std::size_t kDebugPedLimit = 64;
constexpr std::size_t kDebugVehicleLimit = 64;
constexpr float kInvestigationDemoActorRadius = 22.0f;
constexpr float kInvestigationDemoHearRadius = 18.0f;
constexpr std::uint64_t kInvestigationDemoTurnDelayMs = 3000;

std::uint64_t nowMs() {
    return static_cast<std::uint64_t>(GetTickCount64());
}

std::uint64_t persistentNowMs() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string persistenceStatusName(const PersistenceStatus status) {
    switch (status) {
    case PersistenceStatus::LoadedPrimary: return "LoadedPrimary";
    case PersistenceStatus::RecoveredBackup: return "RecoveredBackup";
    case PersistenceStatus::CreatedNew: return "CreatedNew";
    case PersistenceStatus::Failed: return "Failed";
    }
    return "Unknown";
}

std::string compatibilityLevelName(const MissionCompatibilityLevel level) {
    switch (level) {
    case MissionCompatibilityLevel::Normal: return "Normal";
    case MissionCompatibilityLevel::Restricted: return "Restricted";
    case MissionCompatibilityLevel::Suspended: return "Suspended";
    }
    return "Unknown";
}

bool keyDown(const int virtualKey) {
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

dialogue::VoiceGender dialogueVoiceGender(const platform::PedVoiceGender gender) noexcept {
    switch (gender) {
    case platform::PedVoiceGender::Masculine: return dialogue::VoiceGender::Masculine;
    case platform::PedVoiceGender::Feminine: return dialogue::VoiceGender::Feminine;
    case platform::PedVoiceGender::Unknown: break;
    }
    return dialogue::VoiceGender::Unknown;
}

dialogue::SpeakerRole demoRoleFor(const dialogue::InterviewSpeaker speaker) noexcept {
    return speaker == dialogue::InterviewSpeaker::Officer
        ? dialogue::SpeakerRole::InvestigatingOfficer
        : dialogue::SpeakerRole::Clerk;
}

} // namespace

Runtime::Runtime()
    : paths_(RuntimePaths::discover()),
      config_(RuntimeConfig::load(paths_.configFile)),
      worldState_(paths_),
      adapterDiagnostics_(platform_),
      crimePersistence_(paths_, worldState_),
      crimeDirector_(crimeRegistry_, idGenerator_, eventBus_),
      storeRuntime_(
          paths_,
          worldState_,
          platform_,
          pedPresentation_,
          crimeDirector_,
          crimeRegistry_,
          idGenerator_,
          eventBus_),
      clerkRecognitionDirector_(
          platform_,
          identitySystem_,
          storeRuntime_,
          crimeDirector_,
          eventBus_),
      witnessDirector_(
          platform_,
          pedPresentation_,
          identitySystem_,
          crimeDirector_,
          crimeRegistry_,
          eventBus_) {}

Runtime::~Runtime() {
    shutdown();
}

bool Runtime::initialize() {
    if (initialized_) {
        return true;
    }

    paths_.ensureDirectories();

    if (!Logger::instance().open(paths_.logRoot / L"GTA_Crime_Overhaul.log")) {
        return false;
    }

    Logger::instance().info("GTA Crime Overhaul starting.");
    Logger::instance().info(
        std::string("Build: version=") + std::string(BuildInfo::Version)
        + ", config=" + std::string(BuildInfo::BuildConfig)
        + ", git=" + std::string(BuildInfo::GitSha)
        + ", saveSchema=" + std::to_string(BuildInfo::SaveSchemaVersion));
    Logger::instance().info("Runtime scope: GTA V Story Mode/offline only.");

    config_ = RuntimeConfig::load(paths_.configFile);
    if (!std::filesystem::exists(paths_.configFile)) {
        Logger::instance().warn("Configuration file not found; using compiled safe defaults.");
    }

    const PersistenceReport persistence = worldState_.loadOrCreate();
    if (!persistence.ok()) {
        Logger::instance().error("Unable to initialize world save state: " + persistence.detail);
        return false;
    }
    Logger::instance().info(
        "Persistence: " + persistenceStatusName(persistence.status)
        + "; schema=" + std::to_string(persistence.schemaVersion)
        + "; " + persistence.detail);

    std::string idStateReason;
    if (!worldState_.loadLogicalIdState(idGenerator_, &idStateReason)) {
        Logger::instance().error("Unable to restore logical ID state: " + idStateReason);
        return false;
    }

    if (config_.persistentCases) {
        std::string caseReason;
        if (!crimePersistence_.load(crimeRegistry_, idGenerator_, &caseReason)) {
            Logger::instance().error("Unable to restore persistent crime cases: " + caseReason);
            return false;
        }
        Logger::instance().info(
            "Crime persistence loaded: cases=" + std::to_string(crimeRegistry_.caseCount())
            + ", crimes=" + std::to_string(crimeRegistry_.crimeCount())
            + ", nextCase=" + std::to_string(idGenerator_.nextSequence(LogicalIdDomain::Case))
            + ", nextCrime=" + std::to_string(idGenerator_.nextSequence(LogicalIdDomain::Crime)) + ".");
    } else {
        Logger::instance().warn("PersistentCases=false; case save/load is disabled for this session.");
    }

    std::string identityReason;
    if (!identitySystem_.loadMaskCatalog(paths_.dataRoot / L"identity" / L"mask_catalog.json", &identityReason)) {
        Logger::instance().warn("Identity mask catalog unavailable: " + identityReason);
    } else {
        Logger::instance().info("Identity mask catalog: " + identityReason);
    }
    if (identitySystem_.lowerFaceBandanaCustomRequired()) {
        Logger::instance().warn(
            "Lower-face protagonist bandana remains CUSTOM_REQUIRED. GTA/RDR2 assets are not copied; identity gameplay continues without an authoritative bandana until an original or validated GTA V option exists.");
    }

    witnessDirector_.initialize();

    if (config_.robberySystem) {
        std::string storeReason;
        if (!storeRuntime_.initialize(persistentNowMs(), &storeReason)) {
            Logger::instance().error("Unable to initialize prototype store state: " + storeReason);
            return false;
        }
        if (!storeRuntime_.saveIfDirty(&storeReason)) {
            Logger::instance().error("Unable to persist initial prototype store state: " + storeReason);
            return false;
        }
        clerkRecognitionDirector_.initialize();
    } else {
        Logger::instance().warn("RobberySystem=false; prototype-store runtime is disabled for this session.");
    }

    if (!config_.enabled) {
        Logger::instance().warn("Mod is disabled in configuration; runtime will remain idle except for lifecycle cleanup.");
    }

    configureScheduler();
    configureDebugCommands();

    lastHeartbeatMs_ = nowMs();
    initialized_ = true;
    stopRequested_.store(false, std::memory_order_release);

    eventBus_.publish(RuntimeEvent{"runtime.started", 0, std::string(BuildInfo::Version)});
    Logger::instance().info("Native ASI runtime initialized successfully.");
    Logger::instance().info("Debug hotkeys: F3=toggle witness FOV/LOS debug, F4=prototype-store survey candidate, F5=case inspector, F6=synthetic Stage 2 case/save/reload test, F7=investigation dialogue demo, F8=Stage 1 adapter probes, F9=dump diagnostics, F10=toggle overlay, F11=validate save (when DebugHotkeys=true). Commands: identity.inspect, store.inspect, witness.inspect.");
    return true;
}

void Runtime::configureScheduler() {
    scheduler_.setErrorHandler([](const std::string_view taskName, const std::exception_ptr error) {
        try {
            if (error) {
                std::rethrow_exception(error);
            }
        } catch (const std::exception& ex) {
            Logger::instance().error(
                "Scheduler task '" + std::string(taskName) + "' threw std::exception: " + ex.what());
        } catch (...) {
            Logger::instance().error(
                "Scheduler task '" + std::string(taskName) + "' threw an unknown exception.");
        }
    });

    scheduler_.addRecurring(SchedulerLane::Frame, "runtime.frame", [this]() { tickFrame(); });
    scheduler_.addRecurring(SchedulerLane::FiveHz, "runtime.5hz", [this]() { tickFiveHz(); });
    scheduler_.addRecurring(SchedulerLane::TwoHz, "runtime.2hz", [this]() { tickTwoHz(); });
    scheduler_.addRecurring(SchedulerLane::OneHz, "runtime.1hz", [this]() { tickOneHz(); });
}

void Runtime::configureDebugCommands() {
    debugCommands_.registerCommand("toggle_overlay", [this]() {
        config_.debugOverlay = !config_.debugOverlay;
        Logger::instance().info(std::string("Debug overlay ") + (config_.debugOverlay ? "enabled" : "disabled") + ".");
    });
    debugCommands_.registerCommand("dump_status", [this]() { logDiagnostics(); });
    debugCommands_.registerCommand("validate_save", [this]() { validateSaveDiagnostic(); });
    debugCommands_.registerCommand("crime.inspect", [this]() { logCaseInspector(); });
    debugCommands_.registerCommand("crime.synthetic", [this]() { runSyntheticCrimeDiagnostic(); });
    debugCommands_.registerCommand("identity.inspect", [this]() {
        const auto player = platform_.world.playerPed();
        const auto snapshot = platform_.world.snapshotPed(player);
        if (!snapshot) {
            Logger::instance().warn("identity.inspect requires a live player snapshot.");
            return;
        }
        Logger::instance().info("Player identity: " + identitySystem_.debugDescribe(identitySystem_.snapshot(*snapshot)));
        if (config_.robberySystem && storeRuntime_.initialized()) {
            Logger::instance().info(clerkRecognitionDirector_.debugSummary());
        }
    });
    debugCommands_.registerCommand("witness.debug_toggle", [this]() {
        witnessDirector_.toggleDebug();
        Logger::instance().info(std::string("Witness spatial/FOV/LOS debug ")
            + (witnessDirector_.debugEnabled() ? "enabled" : "disabled") + ".");
    });
    debugCommands_.registerCommand("witness.inspect", [this]() {
        Logger::instance().info(witnessDirector_.debugSummary());
    });
    debugCommands_.registerCommand("store.survey", [this]() {
        if (config_.robberySystem) storeRuntime_.debugSurveyCurrentPosition();
    });
    debugCommands_.registerCommand("store.inspect", [this]() {
        if (config_.robberySystem) {
            storeRuntime_.debugInspect();
            Logger::instance().info(clerkRecognitionDirector_.debugSummary());
        }
    });
    debugCommands_.registerCommand("store.demand.open_register", [this]() {
        if (config_.robberySystem) storeRuntime_.debugIssueDemand(robbery::StoreDemand::OpenRegister, persistentNowMs());
    });
    debugCommands_.registerCommand("store.demand.second_register", [this]() {
        if (config_.robberySystem) storeRuntime_.debugIssueDemand(robbery::StoreDemand::OpenSecondRegister, persistentNowMs());
    });
    debugCommands_.registerCommand("store.demand.safe", [this]() {
        if (config_.robberySystem) storeRuntime_.debugIssueDemand(robbery::StoreDemand::EmptySafe, persistentNowMs());
    });
    debugCommands_.registerCommand("store.demand.hands_up", [this]() {
        if (config_.robberySystem) storeRuntime_.debugIssueDemand(robbery::StoreDemand::HandsUp, persistentNowMs());
    });
    debugCommands_.registerCommand("store.demand.get_down", [this]() {
        if (config_.robberySystem) storeRuntime_.debugIssueDemand(robbery::StoreDemand::GetDown, persistentNowMs());
    });
    debugCommands_.registerCommand("store.demand.move_alarm", [this]() {
        if (config_.robberySystem) storeRuntime_.debugIssueDemand(robbery::StoreDemand::MoveAwayFromAlarm, persistentNowMs());
    });
    debugCommands_.registerCommand("store.demand.dont_move", [this]() {
        if (config_.robberySystem) storeRuntime_.debugIssueDemand(robbery::StoreDemand::DontMove, persistentNowMs());
    });
    debugCommands_.registerCommand("dialogue.investigation_demo", [this]() {
        startInvestigationDialogueDemo();
    });

    const auto allowed = [this]() {
        if (!missionGate_.gameplayAllowed()) {
            Logger::instance().warn("Stage 1 adapter probe blocked by mission compatibility gate; return to normal controllable free roam.");
            return false;
        }
        return true;
    };

    debugCommands_.registerCommand("adapter.world", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeWorld(), "adapter.world");
    });
    debugCommands_.registerCommand("adapter.animation", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeAnimation(), "adapter.animation");
    });
    debugCommands_.registerCommand("adapter.props", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeProps(), "adapter.props");
    });
    debugCommands_.registerCommand("adapter.interiors", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeInteriorsAndDoors(), "adapter.interiors");
    });
    debugCommands_.registerCommand("adapter.ui", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeUi(), "adapter.ui");
    });
    debugCommands_.registerCommand("adapter.audio", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeAudio(), "adapter.audio");
    });
    debugCommands_.registerCommand("adapter.input", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeInput(nowMs()), "adapter.input");
    });
    debugCommands_.registerCommand("adapter.debug_draw", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeDebugDraw(nowMs()), "adapter.debug_draw");
    });
    debugCommands_.registerCommand("adapter.all", [this, allowed]() {
        if (allowed()) logAdapterProbeReport(adapterDiagnostics_.probeAll(nowMs()), "adapter.all");
    });
}

void Runtime::run() {
    try {
        if (!initialize()) {
            Logger::instance().error("Runtime initialization failed; ScriptMain is exiting without entering the tick loop.");
            Logger::instance().close();
            return;
        }

        while (!stopRequested_.load(std::memory_order_acquire)) {
            if (config_.enabled) {
                tick();
            }
            scriptWait(0);
        }
    } catch (const std::exception& ex) {
        Logger::instance().error(std::string("Unhandled std::exception in ScriptMain: ") + ex.what());
    } catch (...) {
        Logger::instance().error("Unhandled unknown exception in ScriptMain.");
    }

    shutdown();
}

void Runtime::requestStop() noexcept {
    stopRequested_.store(true, std::memory_order_release);
}

void Runtime::tick() {
    ++frameCount_;
    scheduler_.tick(nowMs());
}

void Runtime::tickFrame() {
    if (config_.debugHotkeys) {
        handleDebugHotkeys();
    }

    const auto now = nowMs();
    if (missionGate_.gameplayAllowed()) {
        for (const auto action : adapterDiagnostics_.tickInputProbe(now)) {
            Logger::instance().info(
                "Adapter input probe: " + std::string(platform::inputActionName(action)) + " justPressed=true");
        }
        adapterDiagnostics_.renderVisualProbe(now);
        tickInvestigationDialogueDemo(now);
        if (config_.robberySystem && storeRuntime_.detailedActive()) {
            storeRuntime_.tickFrame(persistentNowMs(), true);
        }
    }

    witnessDirector_.renderDebug();

    if (config_.debugOverlay) {
        renderDebugOverlay();
    }
}

void Runtime::tickFiveHz() {
    missionState_ = platform_.world.missionState();
    const auto previousLevel = missionGate_.level();
    const auto currentLevel = missionGate_.update(MissionCompatibilitySignals{
        missionState_.missionFlag,
        missionState_.cutsceneActive,
        missionState_.cutscenePlaying,
        missionState_.playerControlOn
    });

    if (currentLevel != previousLevel) {
        Logger::instance().info(
            "Mission compatibility changed: " + compatibilityLevelName(previousLevel)
            + " -> " + compatibilityLevelName(currentLevel) + ".");

        if (currentLevel == MissionCompatibilityLevel::Suspended) {
            eventBus_.publish(RuntimeEvent{"runtime.mission_suspend", 0, {}});
        } else if (currentLevel == MissionCompatibilityLevel::Restricted) {
            eventBus_.publish(RuntimeEvent{"runtime.mission_restricted", 0, {}});
        } else {
            eventBus_.publish(RuntimeEvent{"runtime.mission_resume", 0, {}});
        }
    }

    const bool gameplayAllowed = missionGate_.gameplayAllowed();
    const auto persistentNow = persistentNowMs();
    if (config_.robberySystem) {
        storeRuntime_.tickFiveHz(persistentNow, gameplayAllowed);
        clerkRecognitionDirector_.tickFiveHz(persistentNow, gameplayAllowed);
    }
    witnessDirector_.tickFiveHz(
        persistentNow,
        gameplayAllowed,
        config_.robberySystem ? storeRuntime_.boundClerkPed() : 0);

    if (!gameplayAllowed) {
        if (!investigationDemoPlan_.empty()) {
            stopInvestigationDialogueDemo("mission compatibility gate became active");
        }
        playerSnapshot_.reset();
        nearbyPedCount_ = 0;
        nearbyVehicleCount_ = 0;
        return;
    }

    const auto player = platform_.world.playerPed();
    playerSnapshot_ = platform_.world.snapshotPed(player);
    if (!playerSnapshot_) {
        nearbyPedCount_ = 0;
        nearbyVehicleCount_ = 0;
        return;
    }

    const auto peds = platform_.world.nearbyPeds(
        playerSnapshot_->position,
        kDebugWorldRadius,
        kDebugPedLimit);
    const auto vehicles = platform_.world.nearbyVehicles(
        playerSnapshot_->position,
        kDebugWorldRadius,
        kDebugVehicleLimit);

    nearbyPedCount_ = peds.size();
    nearbyVehicleCount_ = vehicles.size();
}

void Runtime::tickTwoHz() {
    // Witness perception and Stage 5 identity recognition are budgeted on the existing 5 Hz lane.
}

void Runtime::tickOneHz() {
    const auto now = nowMs();
    if (missionGate_.persistenceAllowed()) {
        if (config_.persistentCases) {
            const std::size_t decayed = crimeDirector_.applyDecay(persistentNowMs());
            if (decayed > 0) {
                saveCrimeState("case decay housekeeping");
            }
        }

        if (config_.robberySystem && storeRuntime_.persistenceDirty()) {
            std::string storeReason;
            if (!storeRuntime_.saveIfDirty(&storeReason)) {
                Logger::instance().error("Prototype store checkpoint save failed: " + storeReason);
            } else if (!saveCrimeState("prototype store checkpoint")) {
                Logger::instance().warn("Prototype business memory saved but related case checkpoint failed; atomic backup remains available.");
            }
        }

        const bool witnessCaseDirty = config_.persistentCases && witnessDirector_.casePersistenceDirty();
        const bool clerkCaseDirty = config_.persistentCases && clerkRecognitionDirector_.casePersistenceDirty();
        if (witnessCaseDirty || clerkCaseDirty) {
            if (saveCrimeState("witness/identity evidence checkpoint")) {
                if (witnessCaseDirty) witnessDirector_.clearCasePersistenceDirty();
                if (clerkCaseDirty) clerkRecognitionDirector_.clearCasePersistenceDirty();
            }
        }
    }

    if (config_.debugLogging && now - lastHeartbeatMs_ >= 30000) {
        lastHeartbeatMs_ = now;
        logDiagnostics();
    }
}

void Runtime::handleDebugHotkeys() {
    const bool f3Down = keyDown(VK_F3);
    const bool f4Down = keyDown(VK_F4);
    const bool f5Down = keyDown(VK_F5);
    const bool f6Down = keyDown(VK_F6);
    const bool f7Down = keyDown(VK_F7);
    const bool f8Down = keyDown(VK_F8);
    const bool f9Down = keyDown(VK_F9);
    const bool f10Down = keyDown(VK_F10);
    const bool f11Down = keyDown(VK_F11);

    if (f3Down && !f3WasDown_) {
        debugCommands_.execute("witness.debug_toggle");
    }
    if (f4Down && !f4WasDown_) {
        debugCommands_.execute("store.survey");
    }
    if (f5Down && !f5WasDown_) {
        debugCommands_.execute("crime.inspect");
    }
    if (f6Down && !f6WasDown_) {
        debugCommands_.execute("crime.synthetic");
    }
    if (f7Down && !f7WasDown_) {
        debugCommands_.execute("dialogue.investigation_demo");
    }
    if (f8Down && !f8WasDown_) {
        debugCommands_.execute("adapter.all");
    }
    if (f9Down && !f9WasDown_) {
        debugCommands_.execute("dump_status");
    }
    if (f10Down && !f10WasDown_) {
        debugCommands_.execute("toggle_overlay");
    }
    if (f11Down && !f11WasDown_) {
        debugCommands_.execute("validate_save");
    }

    f3WasDown_ = f3Down;
    f4WasDown_ = f4Down;
    f5WasDown_ = f5Down;
    f6WasDown_ = f6Down;
    f7WasDown_ = f7Down;
    f8WasDown_ = f8Down;
    f9WasDown_ = f9Down;
    f10WasDown_ = f10Down;
    f11WasDown_ = f11Down;
}

void Runtime::renderDebugOverlay() {
    std::ostringstream text;
    text << "GCO Stage 5 | " << BuildInfo::Version
         << " | " << compatibilityLevelName(missionGate_.level())
         << " | Wanted " << platform_.world.wantedLevel()
         << " | Cases " << crimeRegistry_.caseCount();

    if (config_.robberySystem && storeRuntime_.initialized()) {
        text << " | Store " << (storeRuntime_.targetReady() ? "READY" : "RESEARCH")
             << (storeRuntime_.detailedActive() ? "/ACTIVE" : "/ABSTRACT");
    }
    if (witnessDirector_.incidentActive()) {
        text << " | Witnesses " << witnessDirector_.candidateCount();
    }

    if (!missionGate_.gameplayAllowed()) {
        text << " | world sampling paused";
        platform_.ui.helpText(text.str(), false);
        return;
    }

    text << " | Peds " << nearbyPedCount_ << " | Vehicles " << nearbyVehicleCount_;
    if (!investigationDemoPlan_.empty()) {
        text << " | Interview demo " << investigationDemoTurnIndex_ << '/' << investigationDemoPlan_.size();
    }
    platform_.ui.helpText(text.str(), false);

    if (config_.robberySystem) {
        storeRuntime_.renderDebug();
    }

    if (!playerSnapshot_ || !playerSnapshot_->alive) {
        return;
    }

    auto origin = playerSnapshot_->position;
    origin.z += 0.75f;
    platform_.debugDraw.witnessCone(
        origin,
        playerSnapshot_->heading,
        35.0f,
        18.0f,
        platform::Rgba{255, 255, 255, 160});
}

void Runtime::runSyntheticCrimeDiagnostic() {
    if (!config_.persistentCases) {
        Logger::instance().warn("Synthetic Stage 2 diagnostic requires PersistentCases=true.");
        return;
    }
    if (!missionGate_.gameplayAllowed()) {
        Logger::instance().warn("Synthetic Stage 2 diagnostic blocked by mission compatibility gate.");
        return;
    }

    const auto player = platform_.world.playerPed();
    const auto snapshot = platform_.world.snapshotPed(player);
    if (!snapshot || !snapshot->alive) {
        Logger::instance().warn("Synthetic Stage 2 diagnostic requires a live controllable player.");
        return;
    }

    const std::uint64_t stamp = persistentNowMs();
    const std::string incidentKey = "debug.synthetic." + std::to_string(stamp);
    crime::CrimeOccurrence armed{};
    armed.type = crime::CrimeType::ArmedRobbery;
    armed.occurredAtMs = stamp;
    armed.incidentKey = incidentKey;
    armed.location = {snapshot->position.x, snapshot->position.y, snapshot->position.z, "debug_synthetic"};

    const auto primary = crimeDirector_.recordCrime(armed);
    if (primary.caseId == 0 || primary.crimeId == 0) {
        Logger::instance().error("Synthetic Stage 2 diagnostic FAIL: could not allocate primary crime/case IDs.");
        return;
    }

    auto related = armed;
    related.type = crime::CrimeType::PropertyDamage;
    related.occurredAtMs = stamp + 1;
    const auto merged = crimeDirector_.recordCrime(related);
    if (merged.caseId != primary.caseId || !merged.mergedIntoExistingCase) {
        Logger::instance().error("Synthetic Stage 2 diagnostic FAIL: related crime did not merge into the same case.");
        return;
    }

    crime::ImmediateResponseState immediate{};
    immediate.active = true;
    immediate.reportPending = true;
    immediate.tacticalLevel = 2;
    immediate.lastUpdatedAtMs = stamp + 2;
    if (!crimeDirector_.setImmediateResponse(primary.caseId, immediate, stamp + 2)) {
        Logger::instance().error("Synthetic Stage 2 diagnostic FAIL: immediate response state update failed.");
        return;
    }

    crime::EvidenceRecord observed{};
    observed.source = crime::EvidenceSource::SyntheticDebug;
    observed.kind = crime::EvidenceKind::CrimeObserved;
    observed.confidence = 0.60f;
    observed.observedAtMs = stamp + 3;
    observed.independenceKey = incidentKey + ".witnessA";
    observed.dedupKey = incidentKey + ".observed";
    observed.snapshot.descriptor = "synthetic witness observed armed robbery";
    observed.snapshot.location = armed.location;

    crime::EvidenceRecord vehicle{};
    vehicle.source = crime::EvidenceSource::SyntheticDebug;
    vehicle.kind = crime::EvidenceKind::Vehicle;
    vehicle.confidence = 0.45f;
    vehicle.observedAtMs = stamp + 4;
    vehicle.independenceKey = incidentKey + ".witnessB";
    vehicle.dedupKey = incidentKey + ".vehicle";
    vehicle.snapshot.descriptor = "synthetic black coupe / partial plate 46E";
    vehicle.snapshot.location = armed.location;

    if (crimeDirector_.addEvidence(primary.caseId, std::move(observed)) != crime::EvidenceAddResult::Added
        || crimeDirector_.addEvidence(primary.caseId, std::move(vehicle)) != crime::EvidenceAddResult::Added) {
        Logger::instance().error("Synthetic Stage 2 diagnostic FAIL: evidence insertion failed.");
        return;
    }

    const bool transitioned = crimeDirector_.beginReporting(primary.caseId, stamp + 5)
        && crimeDirector_.markReported(primary.caseId, stamp + 6)
        && crimeDirector_.beginInvestigation(primary.caseId, stamp + 7)
        && crimeDirector_.markUnknownSuspect(primary.caseId, 0.30f, stamp + 8)
        && crimeDirector_.issueBoloOrWarrant(primary.caseId, false, true, stamp + 9)
        && crimeDirector_.beginPursuitOrSearch(primary.caseId, stamp + 10)
        && crimeDirector_.markDormant(primary.caseId, stamp + 11);
    if (!transitioned) {
        Logger::instance().error("Synthetic Stage 2 diagnostic FAIL: canonical case transition sequence failed.");
        return;
    }

    immediate.active = false;
    immediate.reportPending = false;
    immediate.pursuitActive = false;
    immediate.lastUpdatedAtMs = stamp + 12;
    if (!crimeDirector_.setImmediateResponse(primary.caseId, immediate, stamp + 12)) {
        Logger::instance().error("Synthetic Stage 2 diagnostic FAIL: immediate-response cleanup failed.");
        return;
    }

    if (!saveCrimeState("synthetic diagnostic")) {
        return;
    }

    LogicalIdGenerator reloadedIds;
    std::string reason;
    if (!worldState_.loadLogicalIdState(reloadedIds, &reason)) {
        Logger::instance().error("Synthetic Stage 2 diagnostic FAIL: base ID reload failed: " + reason);
        return;
    }
    crime::CrimeRegistry reloadedRegistry;
    if (!crimePersistence_.load(reloadedRegistry, reloadedIds, &reason)) {
        Logger::instance().error("Synthetic Stage 2 diagnostic FAIL: case reload failed: " + reason);
        return;
    }

    const crime::CaseFile* reloaded = reloadedRegistry.findCase(primary.caseId);
    if (reloaded == nullptr
        || reloaded->state != crime::CaseState::Dormant
        || reloaded->crimeIds.size() != 2
        || reloaded->evidence.size() != 2
        || !reloaded->activeVehicleBolo) {
        Logger::instance().error("Synthetic Stage 2 diagnostic FAIL: reloaded case did not preserve state/crimes/evidence/BOLO.");
        return;
    }

    crimeRegistry_ = std::move(reloadedRegistry);
    idGenerator_ = reloadedIds;
    Logger::instance().info(
        "Synthetic Stage 2 diagnostic PASS: created case=" + std::to_string(primary.caseId)
        + ", merged crime=" + std::to_string(merged.crimeId)
        + ", persisted/reloaded state=dormant with two evidence records and vehicle BOLO.");
    platform_.ui.subtitle("GCO Stage 2 synthetic case PASS - see log / F5 inspector", 2600, true);
    logCaseInspector();
}

void Runtime::logCaseInspector() {
    Logger::instance().info("Stage 2 case inspector:\n" + crime::CrimeDebugInspector::formatRegistry(crimeRegistry_));
}

bool Runtime::saveCrimeState(const char* context) {
    if (!config_.persistentCases) {
        return true;
    }
    std::string reason;
    if (!crimePersistence_.save(crimeRegistry_, idGenerator_, &reason)) {
        Logger::instance().error(
            std::string("Crime persistence save failed")
            + (context != nullptr ? std::string(" during ") + context : std::string{})
            + ": " + reason);
        return false;
    }
    return true;
}

void Runtime::startInvestigationDialogueDemo() {
    if (!missionGate_.gameplayAllowed()) {
        Logger::instance().warn("Investigation dialogue demo blocked by mission compatibility gate.");
        return;
    }

    const auto player = platform_.world.playerPed();
    const auto playerSnapshot = platform_.world.snapshotPed(player);
    if (!playerSnapshot || !playerSnapshot->alive) {
        Logger::instance().warn("Investigation dialogue demo requires a live controllable player.");
        return;
    }

    const auto nearby = platform_.world.nearbyPeds(
        playerSnapshot->position,
        kInvestigationDemoActorRadius,
        16);

    platform::PedHandle actors[2]{0, 0};
    platform::PedPresentationTraits actorTraits[2]{};
    std::size_t found = 0;
    for (const auto ped : nearby) {
        if (ped == player || !platform_.world.pedExists(ped)) {
            continue;
        }
        const auto actorSnapshot = platform_.world.snapshotPed(ped);
        if (!actorSnapshot || !actorSnapshot->alive || actorSnapshot->isPlayer) {
            continue;
        }
        const auto traits = pedPresentation_.classify(ped);
        if (!traits.human || traits.voiceGender == platform::PedVoiceGender::Unknown) {
            continue;
        }
        actors[found] = ped;
        actorTraits[found] = traits;
        ++found;
        if (found == 2) {
            break;
        }
    }

    if (found < 2) {
        Logger::instance().warn("Investigation dialogue demo needs two nearby live human non-player peds. Move to a populated area and press F7 again.");
        platform_.ui.subtitle("GCO demo: need two nearby human NPCs", 1800, true);
        return;
    }

    dialogue::WitnessStatementFacts facts{};
    facts.witnessKind = dialogue::WitnessKind::Clerk;
    facts.suspectKnowledge = dialogue::SuspectKnowledge::Unknown;
    facts.faceObserved = false;
    facts.faceCovered = true;
    facts.faceConfidence = 0.1f;
    facts.clothingObserved = true;
    facts.clothingDescription = "a dark jacket and gray pants";
    facts.vehicleObserved = true;
    facts.vehicleColor = "black";
    facts.vehicleDescription = "two-door coupe";
    facts.plateKnowledge = dialogue::PlateKnowledge::Partial;
    facts.plateText = "46E";
    facts.directionObserved = true;
    facts.directionDescription = "east toward the freeway";
    facts.witnessPanicked = true;

    investigationDemoPlan_ = dialogue::InvestigationDialogueComposer::compose(
        facts,
        dialogue::InterviewOptions{4, static_cast<std::uint32_t>(frameCount_), true, true, true});

    investigationDemoPlan_.officerProfile = dialogue::SpeakerPresentationProfile{
        "debug.investigating_officer",
        dialogueVoiceGender(actorTraits[0].voiceGender),
        dialogue::AgeBand::Adult,
        dialogue::SpeechRegister::NeutralProfessional};
    investigationDemoPlan_.witnessProfile = dialogue::SpeakerPresentationProfile{
        "debug.clerk",
        dialogueVoiceGender(actorTraits[1].voiceGender),
        dialogue::AgeBand::Adult,
        dialogue::SpeechRegister::GroundedContemporary};

    investigationDemoTurnIndex_ = 0;
    investigationDemoOfficer_ = actors[0];
    investigationDemoWitness_ = actors[1];
    investigationDemoNextTurnMs_ = nowMs() + 500;

    const auto officerVoiceSet = dialogue::voiceSetIdFor(
        dialogue::SpeakerRole::InvestigatingOfficer,
        investigationDemoPlan_.officerProfile.voiceGender,
        investigationDemoPlan_.officerProfile.ageBand);
    const auto clerkVoiceSet = dialogue::voiceSetIdFor(
        dialogue::SpeakerRole::Clerk,
        investigationDemoPlan_.witnessProfile.voiceGender,
        investigationDemoPlan_.witnessProfile.ageBand);

    Logger::instance().info(
        "Investigation dialogue demo started with synthetic UNKNOWN-SUSPECT facts. "
        "Actors are nearby ambient human peds used only to exercise conversation/overhearing; no case state is mutated. "
        "Age is not inferred: both debug personas use the adult fallback.");
    Logger::instance().info(
        "Investigation demo persona match: officerVoiceSet=" + officerVoiceSet
        + ", clerkVoiceSet=" + clerkVoiceSet
        + ". Voice sets are selection metadata only; generated audio/lip-sync assets remain REFERENCE_ONLY.");
    platform_.ui.subtitle("GCO investigation demo started - stay close to overhear", 2200, true);
}

void Runtime::tickInvestigationDialogueDemo(const std::uint64_t now) {
    if (investigationDemoPlan_.empty() || now < investigationDemoNextTurnMs_) {
        return;
    }
    if (investigationDemoTurnIndex_ >= investigationDemoPlan_.turns.size()) {
        stopInvestigationDialogueDemo("completed");
        return;
    }

    const auto& turn = investigationDemoPlan_.turns[investigationDemoTurnIndex_];
    const platform::PedHandle speaker = turn.speaker == dialogue::InterviewSpeaker::Officer
        ? investigationDemoOfficer_
        : investigationDemoWitness_;
    const auto& speakerProfile = turn.speaker == dialogue::InterviewSpeaker::Officer
        ? investigationDemoPlan_.officerProfile
        : investigationDemoPlan_.witnessProfile;

    if (!platform_.world.pedExists(speaker)) {
        stopInvestigationDialogueDemo("a demo speaker streamed out or was deleted");
        return;
    }

    const auto player = platform_.world.playerPed();
    const auto playerPosition = platform_.world.entityPosition(player);
    const auto speakerPosition = platform_.world.entityPosition(speaker);
    if (!playerPosition || !speakerPosition) {
        stopInvestigationDialogueDemo("player or speaker position became unavailable");
        return;
    }

    const float distance = std::sqrt(platform::distanceSquared(*playerPosition, *speakerPosition));
    const bool clearLos = platform_.world.hasLineOfSight(
        player,
        speaker,
        platform::LineOfSightProfile::DefaultVisibility);

    const bool audible = dialogue::canOverhear(
        dialogue::OverhearSample{distance, clearLos},
        dialogue::OverhearPolicy{kInvestigationDemoHearRadius, false});

    const std::string voiceSet = dialogue::voiceSetIdFor(
        demoRoleFor(turn.speaker),
        speakerProfile.voiceGender,
        speakerProfile.ageBand);

    std::ostringstream log;
    log << "Investigation demo turn " << (investigationDemoTurnIndex_ + 1)
        << '/' << investigationDemoPlan_.size()
        << " speaker=" << dialogue::interviewSpeakerName(turn.speaker)
        << " topic=" << dialogue::interviewTopicName(turn.topic)
        << " audible=" << (audible ? "true" : "false")
        << " distance=" << distance
        << " los=" << (clearLos ? "true" : "false")
        << " voiceGender=" << dialogue::voiceGenderName(speakerProfile.voiceGender)
        << " ageBand=" << dialogue::ageBandName(speakerProfile.ageBand)
        << " voiceSet=" << voiceSet
        << " event=" << turn.semanticEvent
        << " text=" << turn.text;
    Logger::instance().info(log.str());

    if (audible) {
        std::string subtitle = std::string(dialogue::interviewSpeakerName(turn.speaker)) + ": " + turn.text;
        platform_.ui.subtitle(subtitle, 2500, true);
    }

    ++investigationDemoTurnIndex_;
    investigationDemoNextTurnMs_ = now + kInvestigationDemoTurnDelayMs;
    if (investigationDemoTurnIndex_ >= investigationDemoPlan_.turns.size()) {
        investigationDemoNextTurnMs_ = now + kInvestigationDemoTurnDelayMs;
    }
}

void Runtime::stopInvestigationDialogueDemo(const char* reason) {
    if (investigationDemoPlan_.empty()) {
        return;
    }

    Logger::instance().info(
        std::string("Investigation dialogue demo stopped: ")
        + (reason != nullptr ? reason : "unspecified") + ".");
    investigationDemoPlan_.turns.clear();
    investigationDemoPlan_.officerProfile = {};
    investigationDemoPlan_.witnessProfile = {};
    investigationDemoTurnIndex_ = 0;
    investigationDemoOfficer_ = 0;
    investigationDemoWitness_ = 0;
    investigationDemoNextTurnMs_ = 0;
}

void Runtime::logDiagnostics() {
    std::ostringstream out;
    out << "Diagnostics: version=" << BuildInfo::Version
        << ", git=" << BuildInfo::GitSha
        << ", schema=" << BuildInfo::SaveSchemaVersion
        << ", frames=" << frameCount_
        << ", compatibility=" << compatibilityLevelName(missionGate_.level())
        << ", nearbyPeds=" << nearbyPedCount_
        << ", nearbyVehicles=" << nearbyVehicleCount_
        << ", ownedProps=" << platform_.props.ownedCount()
        << ", cases=" << crimeRegistry_.caseCount()
        << ", crimes=" << crimeRegistry_.crimeCount()
        << ", approvedMasks=" << identitySystem_.approvedMaskCount()
        << ", lowerFaceBandanaCustomRequired=" << (identitySystem_.lowerFaceBandanaCustomRequired() ? "true" : "false")
        << ", storeReady=" << (config_.robberySystem && storeRuntime_.targetReady() ? "true" : "false")
        << ", storeDetailed=" << (config_.robberySystem && storeRuntime_.detailedActive() ? "true" : "false")
        << ", witnessIncident=" << (witnessDirector_.incidentActive() ? "true" : "false")
        << ", witnessCandidates=" << witnessDirector_.candidateCount()
        << ", witnessDebug=" << (witnessDirector_.debugEnabled() ? "true" : "false")
        << ", clerkRecognition=" << identity::recognitionLevelName(clerkRecognitionDirector_.lastRecognition().level)
        << ", investigationDemoActive=" << (!investigationDemoPlan_.empty() ? "true" : "false")
        << ", schedulerTasks=" << scheduler_.recurringTaskCount()
        << ", queuedTasks=" << scheduler_.queuedTaskCount()
        << ", eventSubscribers=" << eventBus_.subscriberCount()
        << ", nextBusinessSequence=" << idGenerator_.nextSequence(LogicalIdDomain::Business)
        << ", nextClerkSequence=" << idGenerator_.nextSequence(LogicalIdDomain::Clerk)
        << ", nextCaseSequence=" << idGenerator_.nextSequence(LogicalIdDomain::Case)
        << ", nextCrimeSequence=" << idGenerator_.nextSequence(LogicalIdDomain::Crime)
        << ", nextVehicleSequence=" << idGenerator_.nextSequence(LogicalIdDomain::Vehicle);
    Logger::instance().debug(out.str());
}

void Runtime::logAdapterProbeReport(
    const platform::AdapterProbeReport& report,
    const char* commandName) {

    for (const auto& result : report.results) {
        const auto status = platform::adapterProbeStatusName(result.status);
        const std::string line = "Adapter probe [" + result.id + "] " + std::string(status) + ": " + result.detail;
        if (result.status == platform::AdapterProbeStatus::Fail) {
            Logger::instance().error(line);
        } else if (result.status == platform::AdapterProbeStatus::ResearchRequired
            || result.status == platform::AdapterProbeStatus::ManualRequired) {
            Logger::instance().warn(line);
        } else {
            Logger::instance().info(line);
        }
    }

    std::ostringstream summary;
    summary << "Adapter probe command '" << commandName << "': "
            << report.passes() << " PASS, " << report.failures() << " FAIL, "
            << report.results.size() << " total results.";
    if (report.safeChecksPassed()) {
        Logger::instance().info(summary.str());
    } else {
        Logger::instance().error(summary.str());
    }
}

void Runtime::validateSaveDiagnostic() {
    std::string reason;
    const bool worldValid = worldState_.validateFile(paths_.worldSave, &reason);
    if (!worldValid) {
        Logger::instance().error("Debug save validation FAIL: " + reason);
        return;
    }

    if (config_.persistentCases) {
        LogicalIdGenerator ids;
        if (!worldState_.loadLogicalIdState(ids, &reason)) {
            Logger::instance().error("Debug save validation FAIL (ID state): " + reason);
            return;
        }
        crime::CrimeRegistry cases;
        if (!crimePersistence_.load(cases, ids, &reason)) {
            Logger::instance().error("Debug save validation FAIL (cases): " + reason);
            return;
        }
    }

    if (config_.robberySystem && storeRuntime_.initialized()) {
        LogicalIdGenerator ids;
        if (!worldState_.loadLogicalIdState(ids, &reason)) {
            Logger::instance().error("Debug save validation FAIL (store ID state): " + reason);
            return;
        }
        robbery::PrototypeStoreModel store;
        robbery::PrototypeStorePersistence persistence(paths_, worldState_);
        if (!persistence.load(store, ids, persistentNowMs(), storeRuntime_.target().tuning, &reason)) {
            Logger::instance().error("Debug save validation FAIL (prototype store): " + reason);
            return;
        }
    }

    Logger::instance().info("Debug save validation PASS: world schema, logical IDs, cases, prototype business/clerk memory and Stage 5 recognition fields are valid.");
}

void Runtime::shutdown() {
    if (!initialized_) {
        Logger::instance().close();
        return;
    }

    eventBus_.publish(RuntimeEvent{"runtime.stopping", 0, {}});
    stopInvestigationDialogueDemo("runtime shutdown");

    if (config_.robberySystem) {
        clerkRecognitionDirector_.shutdown();
        storeRuntime_.shutdown(persistentNowMs());
    }
    witnessDirector_.shutdown();

    const std::size_t cleanedProps = platform_.cleanupOwnedResources();
    if (cleanedProps > 0) {
        Logger::instance().info("Platform cleanup deleted " + std::to_string(cleanedProps) + " tracked project-owned prop(s).");
    }

    if (!saveCrimeState("runtime shutdown")) {
        Logger::instance().warn("Runtime is shutting down after a failed case save; previous atomic primary/backup remains authoritative.");
    }

    scheduler_.clear();
    eventBus_.clear();
    playerSnapshot_.reset();
    nearbyPedCount_ = 0;
    nearbyVehicleCount_ = 0;

    std::string saveReason;
    if (!worldState_.validateFile(paths_.worldSave, &saveReason)) {
        Logger::instance().warn("World save failed shutdown validation: " + saveReason);
    }

    Logger::instance().info("GTA Crime Overhaul shutting down cleanly.");
    initialized_ = false;
    Logger::instance().close();
}

} // namespace gco
