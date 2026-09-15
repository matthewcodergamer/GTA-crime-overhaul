#include "StoreRuntime.h"

#include "Foundation.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace gco::robbery {
namespace {

crime::CrimeLocation crimeLocation(const platform::Vec3& position, const std::string& tag) {
    return crime::CrimeLocation{position.x, position.y, position.z, tag};
}

float distance(const platform::Vec3& a, const platform::Vec3& b) noexcept {
    return std::sqrt(platform::distanceSquared(a, b));
}

StoreAction openingAction(const ClerkReaction reaction) {
    switch (reaction) {
    case ClerkReaction::HandsUp: return {StoreActionKind::HandsUp, 0, 0, "opening reaction"};
    case ClerkReaction::Cower: return {StoreActionKind::Cower, 0, 0, "opening reaction"};
    case ClerkReaction::Flee: return {StoreActionKind::Flee, 0, 0, "opening reaction"};
    case ClerkReaction::PhoneReport: return {StoreActionKind::Cower, 0, 0, "phone visual unavailable; cower fallback while logical report timer runs"};
    case ClerkReaction::SecretAlarm: return {StoreActionKind::HandsUp, 0, 0, "secret alarm has no mandatory animation"};
    case ClerkReaction::ArmedResistance: return {StoreActionKind::HandsUp, 0, 0, "armed clerk waits for an opening while threatened"};
    case ClerkReaction::HoldPosition: return {StoreActionKind::HoldPosition, 0, 0, "opening reaction"};
    }
    return {};
}

} // namespace

PrototypeStoreRuntime::PrototypeStoreRuntime(
    RuntimePaths paths,
    WorldStateStore& worldState,
    platform::PlatformServices& platform,
    platform::NativePedPresentationAdapter& pedPresentation,
    crime::CrimeDirector& crimeDirector,
    crime::CrimeRegistry& crimeRegistry,
    LogicalIdGenerator& ids,
    EventBus& events)
    : paths_(std::move(paths)),
      worldState_(worldState),
      platform_(platform),
      pedPresentation_(pedPresentation),
      crimeDirector_(crimeDirector),
      crimeRegistry_(crimeRegistry),
      ids_(ids),
      events_(events),
      persistence_(paths_, worldState_) {}

bool PrototypeStoreRuntime::initialize(
    const std::uint64_t persistentNowMs,
    std::string* reason) {

    if (initialized_) return true;

    targetLoad_ = PrototypeStoreTargetLoader::load(paths_.dataRoot / L"businesses.json", target_);
    if (!targetLoad_.parsed) {
        if (reason) *reason = targetLoad_.detail;
        return false;
    }

    std::string persistenceReason;
    if (!persistence_.load(model_, ids_, persistentNowMs, target_.tuning, &persistenceReason)) {
        if (reason) *reason = persistenceReason;
        return false;
    }

    targetReady_ = targetLoad_.productionReady;
    initialized_ = true;
    persistenceDirty_ = true; // Persist first allocated business/clerk IDs if this is a fresh save.

    Logger::instance().info(
        "Prototype store target: parsed=true; ready=" + std::string(targetReady_ ? "true" : "false")
        + "; validation=" + std::string(targetValidationStatusName(target_.validation))
        + "; detail=" + targetLoad_.detail + ".");
    if (!targetReady_) {
        Logger::instance().warn(
            "Stage 3 production path is intentionally gated. Use store.survey/F4 in the chosen 24/7, then update data/businesses.json only after Legacy/Enhanced in-game validation.");
    }
    return true;
}

void PrototypeStoreRuntime::tickFrame(
    const std::uint64_t persistentNowMs,
    const bool gameplayAllowed) {

    if (!initialized_ || !targetReady_ || !gameplayAllowed || !detailedActive_
        || model_.session().state != RobberySessionState::Active) {
        return;
    }

    if (platform_.input.justPressed(platform::InputAction::Interact)) {
        const StoreDemand demand = nextContextDemand();
        debugIssueDemand(demand, persistentNowMs);
    }
}

void PrototypeStoreRuntime::tickFiveHz(
    const std::uint64_t persistentNowMs,
    const bool gameplayAllowed) {

    if (!initialized_) return;
    if (!gameplayAllowed) {
        if (model_.hasActiveSession()) {
            suspend(persistentNowMs, RobberyAbortReason::MissionSuspended);
        }
        detailedActive_ = false;
        clerkPed_ = 0;
        return;
    }
    if (!targetReady_) return;

    const auto player = platform_.world.playerPed();
    const auto playerSnapshot = platform_.world.snapshotPed(player);
    if (!playerSnapshot || !playerSnapshot->alive) {
        if (model_.hasActiveSession()) finishSession(RobberyAbortReason::PlayerLeft, persistentNowMs);
        detailedActive_ = false;
        clerkPed_ = 0;
        return;
    }

    const bool nearTarget = platform::withinRadius(
        playerSnapshot->position,
        target_.activationCenter(),
        target_.activationRadius);

    // Smart-activation rule: if the player is far and no robbery is live, do no ped scan/AI work.
    if (!nearTarget && !model_.hasActiveSession()) {
        detailedActive_ = false;
        clerkPed_ = 0;
        model_.clearThreat();
        return;
    }
    detailedActive_ = true;

    if (!validateBoundClerk(persistentNowMs)) {
        if (model_.persistent().clerkVacant
            && persistentNowMs < model_.persistent().replacementEligibleAtMs) {
            return;
        }
        clerkPed_ = acquireClerkPed(persistentNowMs);
        if (clerkPed_ == 0) return;

        if (model_.persistent().clerkVacant) {
            if (!model_.ensureReplacementClerk(ids_, persistentNowMs, target_.tuning)) {
                clerkPed_ = 0;
                return;
            }
            persistenceDirty_ = true;
            publish("business.clerk_replaced", "clerkId=" + std::to_string(model_.persistent().clerk.id));
        }
    }

    if (!validateBoundClerk(persistentNowMs)) return;

    if (pendingCashOffer_ && persistentNowMs >= pendingCashOfferAtMs_) {
        const StoreAction offer = *pendingCashOffer_;
        pendingCashOffer_.reset();
        pendingCashOfferAtMs_ = 0;
        publish(
            "business.cash_source_exposed",
            "sourceIndex=" + std::to_string(offer.cashSourceIndex)
                + ";amount=" + std::to_string(offer.amount));
        // Stage 11 owns actual loot transfer. Stage 3 exposes finite source state only.
        robberyPed_.handsUp(clerkPed_, player, 5000);
    }

    if (model_.session().state == RobberySessionState::Active) {
        const bool inside = target_.businessVolume.contains(playerSnapshot->position);
        if (!inside) {
            if (model_.session().playerLeftAtMs == 0) {
                model_.session().playerLeftAtMs = persistentNowMs;
            } else if (persistentNowMs - model_.session().playerLeftAtMs >= target_.tuning.playerLeaveGraceMs) {
                finishSession(RobberyAbortReason::PlayerLeft, persistentNowMs);
                return;
            }
        } else {
            model_.session().playerLeftAtMs = 0;
        }

        if (const auto action = model_.tick(persistentNowMs, target_.tuning, threatCondition())) {
            executeAction(*action, persistentNowMs);
        }
        return;
    }

    if (!target_.businessVolume.contains(playerSnapshot->position)) {
        model_.clearThreat();
        return;
    }

    // Normal store entry remains inert. Only sustained, precise free-aim at the acquired clerk crosses the threshold.
    if (threatCondition()) {
        model_.beginThreat(persistentNowMs);
        if (model_.threatSustained(persistentNowMs, target_.tuning.threatSustainMs)) {
            beginRobbery(persistentNowMs);
        }
    } else {
        model_.clearThreat();
    }
}

void PrototypeStoreRuntime::suspend(
    const std::uint64_t persistentNowMs,
    const RobberyAbortReason reason) {

    if (model_.hasActiveSession()) {
        finishSession(reason, persistentNowMs);
    }
    model_.clearThreat();
    detailedActive_ = false;
    clerkPed_ = 0;
}

bool PrototypeStoreRuntime::saveIfDirty(std::string* reason) {
    if (!persistenceDirty_) return true;
    return save(reason);
}

bool PrototypeStoreRuntime::save(std::string* reason) {
    if (!initialized_) return true;
    if (!persistence_.save(model_, ids_, reason)) return false;
    persistenceDirty_ = false;
    return true;
}

void PrototypeStoreRuntime::shutdown(const std::uint64_t persistentNowMs) {
    if (!initialized_) return;
    if (model_.hasActiveSession()) finishSession(RobberyAbortReason::RuntimeShutdown, persistentNowMs);
    if (clerkPed_ != 0 && platform_.world.pedExists(clerkPed_)) {
        robberyPed_.clearTasks(clerkPed_, false);
    }
    clerkPed_ = 0;
    detailedActive_ = false;
    std::string reason;
    if (!save(&reason)) {
        Logger::instance().warn("Prototype store shutdown save failed: " + reason);
    }
}

platform::PedHandle PrototypeStoreRuntime::acquireClerkPed(const std::uint64_t persistentNowMs) {
    (void)persistentNowMs;
    if (!target_.hasClerkAnchor) return 0;
    const auto player = platform_.world.playerPed();
    const auto candidates = platform_.world.nearbyPeds(
        target_.clerk.position,
        target_.clerkAcquireRadius,
        12);

    platform::PedHandle best = 0;
    float bestDistance = target_.clerkAcquireRadius * target_.clerkAcquireRadius + 1.0f;
    for (const auto ped : candidates) {
        if (ped == 0 || ped == player || !platform_.world.pedExists(ped) || robberyPed_.missionEntity(ped)) continue;
        const auto snapshot = platform_.world.snapshotPed(ped);
        if (!snapshot || !snapshot->alive || snapshot->isPlayer) continue;
        const auto traits = pedPresentation_.classify(ped);
        if (!traits.human) continue;
        const float d2 = platform::distanceSquared(snapshot->position, target_.clerk.position);
        if (d2 < bestDistance) {
            bestDistance = d2;
            best = ped;
        }
    }
    if (best != 0) {
        Logger::instance().info(
            "Prototype store acquired physical clerk ped for logical clerkId="
            + std::to_string(model_.persistent().clerk.id) + ".");
    }
    return best;
}

bool PrototypeStoreRuntime::validateBoundClerk(const std::uint64_t persistentNowMs) {
    if (clerkPed_ == 0) return false;
    if (!platform_.world.pedExists(clerkPed_)) {
        if (model_.hasActiveSession()) finishSession(RobberyAbortReason::ClerkInvalid, persistentNowMs);
        clerkPed_ = 0;
        return false;
    }

    const auto snapshot = platform_.world.snapshotPed(clerkPed_);
    if (!snapshot) {
        if (model_.hasActiveSession()) finishSession(RobberyAbortReason::ClerkInvalid, persistentNowMs);
        clerkPed_ = 0;
        return false;
    }
    if (!snapshot->alive) {
        if (model_.hasActiveSession() && !activeIncidentKey_.empty()) {
            crime::CrimeOccurrence homicide{};
            homicide.type = crime::CrimeType::Homicide;
            homicide.location = crimeLocation(snapshot->position, target_.id);
            homicide.businessId = model_.persistent().businessId;
            homicide.occurredAtMs = persistentNowMs;
            homicide.incidentKey = activeIncidentKey_;
            const auto result = crimeDirector_.recordCrime(homicide);
            if (result.crimeId != 0) {
                publish("business.clerk_killed", "homicideCrimeId=" + std::to_string(result.crimeId));
            }
        }
        model_.markClerkDead(persistentNowMs, target_.tuning);
        persistenceDirty_ = true;
        if (model_.hasActiveSession() || model_.session().state == RobberySessionState::Aborting) {
            finishSession(RobberyAbortReason::ClerkDied, persistentNowMs);
        }
        clerkPed_ = 0;
        return false;
    }
    return true;
}

bool PrototypeStoreRuntime::threatCondition() const {
    if (clerkPed_ == 0 || !platform_.world.pedExists(clerkPed_)) return false;
    const auto player = platform_.world.playerPed();
    const auto playerSnapshot = platform_.world.snapshotPed(player);
    const auto clerkSnapshot = platform_.world.snapshotPed(clerkPed_);
    if (!playerSnapshot || !clerkSnapshot || !playerSnapshot->alive || !clerkSnapshot->alive) return false;
    if (!target_.businessVolume.contains(playerSnapshot->position)) return false;
    if (distance(playerSnapshot->position, clerkSnapshot->position) > target_.threatMaxDistance) return false;
    if (!platform_.world.hasLineOfSight(player, clerkPed_, platform::LineOfSightProfile::DefaultVisibility)) return false;
    return robberyPed_.playerFreeAimingAt(clerkPed_);
}

void PrototypeStoreRuntime::beginRobbery(const std::uint64_t persistentNowMs) {
    if (clerkPed_ == 0 || model_.session().state != RobberySessionState::ThreatDetected) return;
    const auto player = platform_.world.playerPed();
    const auto playerSnapshot = platform_.world.snapshotPed(player);
    if (!playerSnapshot) return;

    activeIncidentKey_ = target_.id + ":" + std::to_string(model_.persistent().businessId)
        + ":" + std::to_string(persistentNowMs);

    crime::CrimeOccurrence occurrence{};
    occurrence.type = crime::CrimeType::ArmedRobbery;
    occurrence.location = crimeLocation(playerSnapshot->position, target_.id);
    occurrence.businessId = model_.persistent().businessId;
    occurrence.occurredAtMs = persistentNowMs;
    occurrence.incidentKey = activeIncidentKey_;
    const auto recorded = crimeDirector_.recordCrime(occurrence);
    if (recorded.crimeId == 0 || recorded.caseId == 0) {
        model_.clearThreat();
        Logger::instance().error("Prototype store could not create Stage 2 crime/case; robbery session not started.");
        return;
    }

    auto cashSources = PrototypeStoreModel::makeCashSources(
        target_.registers.size(),
        target_.safe.has_value(),
        target_.registerCashMin,
        target_.registerCashMax,
        target_.safeCashMin,
        target_.safeCashMax,
        model_.persistent().clerk,
        recorded.crimeId ^ model_.persistent().clerk.id);

    if (!model_.beginSession(
            recorded.crimeId,
            recorded.caseId,
            robberyPed_.pedArmed(clerkPed_),
            persistentNowMs,
            target_.tuning,
            std::move(cashSources))) {
        Logger::instance().error("Prototype store rejected robbery-session creation after crime creation.");
        return;
    }

    crime::ImmediateResponseState immediate{};
    immediate.active = true;
    immediate.reportPending = model_.session().openingReaction == ClerkReaction::SecretAlarm
        || model_.session().openingReaction == ClerkReaction::PhoneReport;
    immediate.tacticalLevel = 2;
    immediate.lastUpdatedAtMs = persistentNowMs;
    crimeDirector_.setImmediateResponse(recorded.caseId, immediate, persistentNowMs);

    persistenceDirty_ = true;
    publish(
        "business.robbery_started",
        "caseId=" + std::to_string(recorded.caseId)
            + ";crimeId=" + std::to_string(recorded.crimeId)
            + ";clerkId=" + std::to_string(model_.persistent().clerk.id)
            + ";personality=" + std::string(clerkPersonalityName(model_.persistent().clerk.personality))
            + ";reaction=" + std::string(clerkReactionName(model_.session().openingReaction)));
    executeOpeningReaction(persistentNowMs);
}

void PrototypeStoreRuntime::executeOpeningReaction(const std::uint64_t persistentNowMs) {
    if (model_.session().openingReaction == ClerkReaction::PhoneReport && !phoneFallbackLogged_) {
        phoneFallbackLogged_ = true;
        Logger::instance().warn(
            "Prototype store phone branch uses logical report timing + cower fallback. Phone animation/prop candidates remain unverified and are not production-authoritative.");
    }
    executeAction(openingAction(model_.session().openingReaction), persistentNowMs);
}

void PrototypeStoreRuntime::executeAction(
    const StoreAction& action,
    const std::uint64_t persistentNowMs) {

    if (action.kind == StoreActionKind::None) return;
    if (action.kind != StoreActionKind::AbortSession
        && (clerkPed_ == 0 || !platform_.world.pedExists(clerkPed_))) {
        finishSession(RobberyAbortReason::ClerkInvalid, persistentNowMs);
        return;
    }

    const auto player = platform_.world.playerPed();
    switch (action.kind) {
    case StoreActionKind::None:
        break;
    case StoreActionKind::HandsUp:
        if (!robberyPed_.handsUp(clerkPed_, player, 5000)) {
            robberyPed_.cower(clerkPed_, 5000);
        }
        publish("clerk.comply", "mode=hands_up");
        break;
    case StoreActionKind::Cower:
        if (!robberyPed_.cower(clerkPed_, 5000)) {
            robberyPed_.handsUp(clerkPed_, player, 5000);
        }
        publish("clerk.comply", "mode=cower");
        break;
    case StoreActionKind::Flee:
        if (robberyPed_.fleeFrom(clerkPed_, player, 28.0f, 12000)) {
            publish("clerk.flee", "reason=opening_reaction");
            finishSession(RobberyAbortReason::ClerkFled, persistentNowMs);
        } else {
            robberyPed_.cower(clerkPed_, 5000);
            publish("clerk.comply", "mode=cower_flee_fallback");
        }
        break;
    case StoreActionKind::BeginPhoneReport: {
        const auto* file = crimeRegistry_.findCase(model_.session().caseId);
        if (file == nullptr) break;
        if (!model_.session().alarmTriggered) {
            recordReportEvidence(false, persistentNowMs);
            file = crimeRegistry_.findCase(model_.session().caseId);
            if (file != nullptr && file->state == crime::CaseState::Observed) {
                crimeDirector_.beginReporting(file->id, persistentNowMs);
            }
        }
        file = crimeRegistry_.findCase(model_.session().caseId);
        if (file != nullptr && file->state == crime::CaseState::Reporting) {
            crimeDirector_.markReported(file->id, persistentNowMs);
        }
        model_.markReportCompleted(persistentNowMs);
        if (file != nullptr) {
            crime::ImmediateResponseState immediate = file->immediate;
            immediate.active = true;
            immediate.reportPending = false;
            immediate.lastUpdatedAtMs = persistentNowMs;
            crimeDirector_.setImmediateResponse(file->id, immediate, persistentNowMs);
        }
        publish("witness.report_completed", "source=clerk_or_alarm");
        persistenceDirty_ = true;
        break;
    }
    case StoreActionKind::TriggerSecretAlarm: {
        recordReportEvidence(true, persistentNowMs);
        const auto* file = crimeRegistry_.findCase(model_.session().caseId);
        if (file != nullptr && file->state == crime::CaseState::Observed) {
            crimeDirector_.beginReporting(file->id, persistentNowMs);
        }
        publish("clerk.secret_alarm", "triggered=true");
        persistenceDirty_ = true;
        break;
    }
    case StoreActionKind::FightPlayer:
        if (robberyPed_.pedArmed(clerkPed_) && robberyPed_.combatPed(clerkPed_, player)) {
            model_.markClerkFought(persistentNowMs);
            publish("clerk.defiant", "mode=armed_resistance");
        } else {
            robberyPed_.handsUp(clerkPed_, player, 5000);
            publish("clerk.comply", "mode=armed_branch_safe_fallback");
        }
        break;
    case StoreActionKind::MoveToCashSource: {
        const auto anchor = cashAnchor(action.cashSourceIndex);
        if (anchor) {
            robberyPed_.goStraightTo(clerkPed_, anchor->position, 1.0f, anchor->heading);
        }
        pendingCashOffer_ = StoreAction{StoreActionKind::OfferCash, action.cashSourceIndex, action.amount, action.reason};
        pendingCashOfferAtMs_ = persistentNowMs + (anchor ? 1800 : 500);
        publish(
            "clerk.cash_source_action",
            "sourceIndex=" + std::to_string(action.cashSourceIndex)
                + ";partial=" + std::string(action.reason.find("partial") != std::string::npos ? "true" : "false"));
        break;
    }
    case StoreActionKind::OfferCash:
        publish("business.cash_source_exposed", "amount=" + std::to_string(action.amount));
        break;
    case StoreActionKind::RefuseDemand:
        robberyPed_.handsUp(clerkPed_, player, 5000);
        publish("clerk.defiant", "mode=refuse_or_stall");
        break;
    case StoreActionKind::HoldPosition:
        robberyPed_.handsUp(clerkPed_, player, 5000);
        publish("clerk.comply", "mode=hold_position");
        break;
    case StoreActionKind::AbortSession:
        finishSession(model_.session().abortReason, persistentNowMs);
        break;
    }
}

void PrototypeStoreRuntime::recordReportEvidence(
    const bool alarmSource,
    const std::uint64_t persistentNowMs) {

    if (model_.session().caseId == 0) return;
    crime::EvidenceRecord evidence{};
    evidence.source = alarmSource ? crime::EvidenceSource::Alarm : crime::EvidenceSource::Witness;
    evidence.kind = crime::EvidenceKind::CrimeObserved;
    evidence.confidence = 1.0f;
    evidence.observedAtMs = persistentNowMs;
    evidence.independenceKey = alarmSource
        ? "business:" + std::to_string(model_.persistent().businessId) + ":alarm"
        : "clerk:" + std::to_string(model_.persistent().clerk.id);
    evidence.dedupKey = "prototype-store-report:" + std::to_string(model_.session().caseId)
        + (alarmSource ? ":alarm" : ":clerk");
    evidence.snapshot.descriptor = alarmSource ? "prototype_store_alarm" : "prototype_store_clerk_report";
    evidence.snapshot.location = crimeLocation(target_.activationCenter(), target_.id);
    evidence.snapshot.sourceLogicalId = alarmSource
        ? std::optional<LogicalId>{model_.persistent().businessId}
        : std::optional<LogicalId>{model_.persistent().clerk.id};
    crimeDirector_.addEvidence(model_.session().caseId, std::move(evidence));
}

void PrototypeStoreRuntime::finishSession(
    const RobberyAbortReason reason,
    const std::uint64_t persistentNowMs) {

    if (!model_.hasActiveSession() && model_.session().state != RobberySessionState::Aborting) return;
    const LogicalId caseId = model_.session().caseId;
    const LogicalId crimeId = model_.session().crimeId;
    if (model_.session().state != RobberySessionState::Aborting) {
        model_.abort(reason, persistentNowMs);
    }

    pendingCashOffer_.reset();
    pendingCashOfferAtMs_ = 0;
    if (clerkPed_ != 0 && platform_.world.pedExists(clerkPed_)
        && reason != RobberyAbortReason::ClerkFled
        && reason != RobberyAbortReason::ClerkDied) {
        robberyPed_.clearTasks(clerkPed_, false);
    }

    model_.complete(persistentNowMs, target_.tuning);
    publish(
        "business.robbery_finished",
        "caseId=" + std::to_string(caseId)
            + ";crimeId=" + std::to_string(crimeId)
            + ";reason=" + std::string(abortReasonName(reason)));
    model_.session() = {};
    activeIncidentKey_.clear();
    persistenceDirty_ = true;
}

StoreDemand PrototypeStoreRuntime::nextContextDemand() const {
    const auto& sources = model_.cashSources();
    if (!sources.empty() && sources[0].state == CashSourceState::Available) {
        return StoreDemand::OpenRegister;
    }
    if (sources.size() > 1 && !sources[1].safe && sources[1].state == CashSourceState::Available) {
        return StoreDemand::OpenSecondRegister;
    }
    for (const auto& source : sources) {
        if (source.safe && source.state == CashSourceState::Available) {
            return StoreDemand::EmptySafe;
        }
    }
    return StoreDemand::DontMove;
}

std::optional<StoreAnchor> PrototypeStoreRuntime::cashAnchor(const std::size_t cashSourceIndex) const {
    if (cashSourceIndex < target_.registers.size()) {
        return target_.registers[cashSourceIndex];
    }
    if (target_.safe && cashSourceIndex == target_.registers.size()) {
        return target_.safe;
    }
    return std::nullopt;
}

bool PrototypeStoreRuntime::debugIssueDemand(
    const StoreDemand demand,
    const std::uint64_t persistentNowMs) {

    if (!targetReady_ || model_.session().state != RobberySessionState::Active) return false;
    const auto result = model_.issueDemand(demand, persistentNowMs);
    publish("robber.demand", "type=" + std::string(storeDemandName(demand)));
    executeAction(result.action, persistentNowMs);
    return result.accepted;
}

void PrototypeStoreRuntime::debugSurveyCurrentPosition() {
    const auto player = platform_.world.playerPed();
    const auto snapshot = platform_.world.snapshotPed(player);
    if (!snapshot || !snapshot->alive) {
        Logger::instance().warn("Store survey requires a live controllable player.");
        return;
    }

    std::ostringstream out;
    out << "STORE_SURVEY_CANDIDATE position={\"x\":" << snapshot->position.x
        << ",\"y\":" << snapshot->position.y
        << ",\"z\":" << snapshot->position.z
        << "}, heading=" << snapshot->heading
        << ". This is a candidate only; do not mark VERIFIED_IN_GAME until the full target volume/anchors pass the checklist.";
    Logger::instance().info(out.str());

    const auto nearby = platform_.world.nearbyPeds(snapshot->position, 5.0f, 12);
    for (const auto ped : nearby) {
        if (ped == player || !platform_.world.pedExists(ped) || robberyPed_.missionEntity(ped)) continue;
        const auto pedSnapshot = platform_.world.snapshotPed(ped);
        if (!pedSnapshot || !pedSnapshot->alive || !pedPresentation_.classify(ped).human) continue;
        std::ostringstream pedLine;
        pedLine << "STORE_SURVEY_NEARBY_HUMAN pos={\"x\":" << pedSnapshot->position.x
                << ",\"y\":" << pedSnapshot->position.y
                << ",\"z\":" << pedSnapshot->position.z
                << "}, heading=" << pedSnapshot->heading;
        Logger::instance().info(pedLine.str());
    }
    platform_.ui.subtitle("GCO store survey candidate logged", 1500, true);
}

void PrototypeStoreRuntime::debugInspect() const {
    const auto& state = model_.persistent();
    std::ostringstream out;
    out << "Prototype store: parsed=" << (targetLoad_.parsed ? "true" : "false")
        << ", ready=" << (targetReady_ ? "true" : "false")
        << ", validation=" << targetValidationStatusName(target_.validation)
        << ", detailedActive=" << (detailedActive_ ? "true" : "false")
        << ", businessId=" << state.businessId
        << ", clerkId=" << state.clerk.id
        << ", clerkAlive=" << (state.clerk.alive ? "true" : "false")
        << ", clerkVacant=" << (state.clerkVacant ? "true" : "false")
        << ", personality=" << clerkPersonalityName(state.clerk.personality)
        << ", robberyCount=" << state.robberyCount
        << ", session=" << sessionStateName(model_.session().state)
        << ", clerkPedBound=" << (clerkPed_ != 0 ? "true" : "false")
        << ", targetDetail=" << targetLoad_.detail;
    Logger::instance().info(out.str());
}

void PrototypeStoreRuntime::renderDebug() const {
    if (!targetLoad_.parsed) return;
    if (target_.hasBusinessVolume) {
        platform_.debugDraw.box(
            target_.businessVolume.min,
            target_.businessVolume.max,
            targetReady_ ? platform::Rgba{255, 255, 255, 140} : platform::Rgba{180, 180, 180, 90});
    }
    const auto center = target_.activationCenter();
    if (target_.hasClerkAnchor) {
        platform_.debugDraw.line(center, target_.clerk.position, platform::Rgba{255, 255, 255, 200});
    }
    for (const auto& anchor : target_.registers) {
        platform_.debugDraw.line(center, anchor.position, platform::Rgba{255, 255, 255, 160});
    }
    for (const auto& anchor : target_.exits) {
        platform_.debugDraw.line(center, anchor.position, platform::Rgba{255, 255, 255, 120});
    }
}

void PrototypeStoreRuntime::publish(std::string topic, std::string payload) {
    events_.publish(RuntimeEvent{
        std::move(topic),
        model_.persistent().businessId,
        std::move(payload)});
}

} // namespace gco::robbery
