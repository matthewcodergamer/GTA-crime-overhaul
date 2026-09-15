#include "robbery/StoreDomain.h"

#include <cstdlib>
#include <iostream>

namespace {
int failures = 0;

void expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testPersistentClerkAndReplacement() {
    using namespace gco;
    using namespace gco::robbery;

    LogicalIdGenerator ids;
    StoreTuning tuning{};
    tuning.clerkReplacementDelayMs = 1000;
    PrototypeStoreModel model;
    model.initializePersistent(ids, 100, tuning);
    const auto firstBusiness = model.persistent().businessId;
    const auto firstClerk = model.persistent().clerk.id;
    expect(firstBusiness != 0 && logicalIdDomain(firstBusiness) == LogicalIdDomain::Business, "business gets stable logical ID");
    expect(firstClerk != 0 && logicalIdDomain(firstClerk) == LogicalIdDomain::Clerk, "clerk gets stable logical ID");

    model.markClerkDead(500, tuning);
    expect(model.persistent().clerkVacant, "clerk death leaves persistent vacancy");
    expect(!model.persistent().clerk.alive, "dead logical clerk remains historically dead");
    expect(model.persistent().replacementEligibleAtMs == 1500, "replacement timer persists from death time");
    expect(!model.ensureReplacementClerk(ids, 1499, tuning), "replacement cannot occur early");
    expect(model.ensureReplacementClerk(ids, 1500, tuning), "replacement becomes eligible at configured time");
    expect(!model.persistent().clerkVacant, "replacement clears vacancy");
    expect(model.persistent().clerk.id != firstClerk, "replacement is a new logical person");
    expect(model.persistent().clerk.generation == 2, "replacement increments logical clerk generation");
    expect(model.persistent().businessId == firstBusiness, "business identity survives clerk replacement");
}

void testThreatThresholdAndSession() {
    using namespace gco;
    using namespace gco::robbery;

    LogicalIdGenerator ids;
    StoreTuning tuning{};
    tuning.threatSustainMs = 1000;
    PrototypeStoreModel model;
    model.initializePersistent(ids, 0, tuning);

    model.beginThreat(100);
    expect(model.session().state == RobberySessionState::ThreatDetected, "threat begins without starting robbery");
    expect(!model.threatSustained(1099, 1000), "threat below sustain threshold does not start robbery");
    expect(model.threatSustained(1100, 1000), "sustained threat crosses threshold exactly");

    auto sources = PrototypeStoreModel::makeCashSources(2, true, 300, 500, 1000, 1500, model.persistent().clerk, 99);
    expect(model.beginSession(makeLogicalId(LogicalIdDomain::Crime, 1), makeLogicalId(LogicalIdDomain::Case, 1), false, 1100, tuning, std::move(sources)), "valid sustained threat can create robbery session");
    expect(model.session().state == RobberySessionState::Active, "robbery session becomes active");
    expect(model.cashSources().size() == 3, "two registers plus safe become finite cash sources");
    expect(model.persistent().robberyCount == 1, "starting robbery updates persistent business memory");
}

void testDemandSurfaceAndPartialCash() {
    using namespace gco;
    using namespace gco::robbery;

    LogicalIdGenerator ids;
    StoreTuning tuning{};
    PrototypeStoreModel model;
    model.initializePersistent(ids, 0, tuning);

    auto state = model.persistent();
    state.clerk.personality = ClerkPersonality::Cowardly;
    state.clerk.compliance = 1.0f;
    state.clerk.withholdingTendency = 1.0f;
    model.restorePersistent(state);
    model.beginThreat(0);
    auto sources = PrototypeStoreModel::makeCashSources(2, true, 1000, 1000, 3000, 3000, model.persistent().clerk, 1234);
    expect(model.beginSession(makeLogicalId(LogicalIdDomain::Crime, 2), makeLogicalId(LogicalIdDomain::Case, 2), false, 1, tuning, std::move(sources)), "session for demand tests starts");

    const auto first = model.issueDemand(StoreDemand::OpenRegister, 10);
    expect(first.accepted && first.usesCashSource && first.cashSourceIndex == 0, "open-register demand targets first register");
    expect(first.action.kind == StoreActionKind::MoveToCashSource, "accepted register demand yields cash-source movement action");
    expect(first.action.amount > 0 && first.action.amount < 1000, "high-withholding clerk exposes only part of finite register cash");
    expect(model.cashSources()[0].remainingAmount > 0, "withheld register reserve remains historically in source state");

    const auto second = model.issueDemand(StoreDemand::OpenSecondRegister, 20);
    expect(second.usesCashSource && second.cashSourceIndex == 1, "second-register demand is distinct from first register");
    const auto safe = model.issueDemand(StoreDemand::EmptySafe, 30);
    expect(safe.usesCashSource && model.cashSources()[safe.cashSourceIndex].safe, "empty-safe demand resolves configured safe source");

    expect(model.issueDemand(StoreDemand::HandsUp, 40).action.kind == StoreActionKind::HandsUp, "hands-up demand supported");
    expect(model.issueDemand(StoreDemand::GetDown, 50).action.kind == StoreActionKind::Cower, "get-down demand supported");
    expect(model.issueDemand(StoreDemand::DontMove, 60).action.kind == StoreActionKind::HoldPosition, "don't-move demand supported");

    model.session().alarmDueAtMs = 100;
    const auto alarmMove = model.issueDemand(StoreDemand::MoveAwayFromAlarm, 70);
    expect(alarmMove.accepted && model.session().alarmOpportunitySuppressed, "move-away-from-alarm can suppress an untriggered alarm opportunity");
    expect(model.session().alarmDueAtMs == 0, "suppressed alarm timer is cleared");
}

void testPersonalityBranchesDoNotInventWeapons() {
    using namespace gco;
    using namespace gco::robbery;

    StoreTuning tuning{};
    ClerkProfile armed = PrototypeStoreModel::makeClerkProfile(makeLogicalId(LogicalIdDomain::Clerk, 99), 1, 0, tuning, 1);
    armed.personality = ClerkPersonality::Armed;
    armed.resistance = 1.0f;

    expect(PrototypeStoreModel::chooseOpeningReaction(armed, false, 7) != ClerkReaction::ArmedResistance,
        "armed personality cannot become armed resistance when physical ped is unarmed");
    expect(PrototypeStoreModel::chooseOpeningReaction(armed, true, 7) == ClerkReaction::ArmedResistance,
        "armed physical clerk can use armed-resistance branch");

    ClerkProfile coward = armed;
    coward.personality = ClerkPersonality::Cowardly;
    expect(PrototypeStoreModel::chooseOpeningReaction(coward, true, 7) == ClerkReaction::HandsUp,
        "cowardly personality remains compliance even if physical ped happens to be armed");
}

void testAlarmReportTimersAndTimeout() {
    using namespace gco;
    using namespace gco::robbery;

    LogicalIdGenerator ids;
    StoreTuning tuning{};
    tuning.secretAlarmDelayMs = 100;
    tuning.reportDelayMs = 200;
    tuning.sessionTimeoutMs = 1000;
    PrototypeStoreModel model;
    model.initializePersistent(ids, 0, tuning);
    auto state = model.persistent();
    state.clerk.personality = ClerkPersonality::Experienced;
    state.clerk.alarmTendency = 1.0f;
    model.restorePersistent(state);
    model.beginThreat(0);
    auto sources = PrototypeStoreModel::makeCashSources(1, false, 100, 100, 0, 0, model.persistent().clerk, 1);
    expect(model.beginSession(makeLogicalId(LogicalIdDomain::Crime, 3), makeLogicalId(LogicalIdDomain::Case, 3), false, 1, tuning, std::move(sources)), "experienced-clerk session starts");
    model.session().openingReaction = ClerkReaction::SecretAlarm;
    model.session().alarmDueAtMs = 101;

    const auto alarm = model.tick(101, tuning, true);
    expect(alarm && alarm->kind == StoreActionKind::TriggerSecretAlarm, "secret alarm becomes a timed logical action");
    const auto report = model.tick(301, tuning, true);
    expect(report && report->kind == StoreActionKind::BeginPhoneReport, "alarm/report completion is separately timed");

    const auto timeout = model.tick(1001, tuning, true);
    expect(timeout && timeout->kind == StoreActionKind::AbortSession, "session timeout always produces recoverable abort action");
    expect(model.session().abortReason == RobberyAbortReason::SessionTimeout, "timeout records canonical abort reason");
}

} // namespace

int main() {
    testPersistentClerkAndReplacement();
    testThreatThresholdAndSession();
    testDemandSurfaceAndPartialCash();
    testPersonalityBranchesDoNotInventWeapons();
    testAlarmReportTimersAndTimeout();

    if (failures != 0) {
        std::cerr << failures << " StoreDomain assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "StoreDomainTests passed.\n";
    return EXIT_SUCCESS;
}
