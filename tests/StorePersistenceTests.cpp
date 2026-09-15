#include "Foundation.h"
#include "robbery/StorePersistence.h"

#include <Windows.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {
int failures = 0;

void expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::string readText(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}
} // namespace

int main() {
    using namespace gco;
    using namespace gco::robbery;

    const auto root = std::filesystem::temp_directory_path()
        / (L"gco-stage3-store-persistence-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    RuntimePaths paths{};
    paths.gameRoot = root;
    paths.modRoot = root / L"GTA_Crime_Overhaul";
    paths.configFile = paths.modRoot / L"GTA_Crime_Overhaul.ini";
    paths.dataRoot = paths.modRoot / L"data";
    paths.saveRoot = paths.modRoot / L"saves";
    paths.logRoot = paths.modRoot / L"logs";
    paths.worldSave = paths.saveRoot / L"world.json";
    paths.worldBackup = paths.saveRoot / L"world.backup.json";
    paths.ensureDirectories();

    WorldStateStore world(paths);
    expect(world.loadOrCreate().ok(), "base world save initializes");

    // Seed an unrelated future business record; Stage 3 must preserve records it does not own.
    std::string document = world.emptyWorldJson();
    const auto businesses = document.find("\"businesses\": []");
    expect(businesses != std::string::npos, "empty world has reserved businesses section");
    if (businesses != std::string::npos) {
        document.replace(
            businesses,
            std::string("\"businesses\": []").size(),
            "\"businesses\": [{\"kind\":\"future_other_business\",\"value\":7}]");
    }
    expect(world.writeWorldAtomically(document), "world with unrelated business record writes");

    LogicalIdGenerator ids;
    std::string reason;
    expect(world.loadLogicalIdState(ids, &reason), "logical ID state loads before store persistence");

    StoreTuning tuning{};
    tuning.clerkReplacementDelayMs = 5000;
    tuning.businessRecoveryMs = 2000;

    PrototypeStoreModel original;
    original.initializePersistent(ids, 1000, tuning);
    const LogicalId businessId = original.persistent().businessId;
    const LogicalId clerkId = original.persistent().clerk.id;
    expect(businessId != 0 && clerkId != 0, "initial business/clerk IDs allocated");

    original.beginThreat(1100);
    auto sources = PrototypeStoreModel::makeCashSources(
        1, false, 500, 500, 0, 0, original.persistent().clerk, 88);
    expect(original.beginSession(
        makeLogicalId(LogicalIdDomain::Crime, 1),
        makeLogicalId(LogicalIdDomain::Case, 1),
        false,
        2200,
        tuning,
        std::move(sources)), "test robbery session starts");
    original.issueDemand(StoreDemand::OpenRegister, 2300);
    original.complete(2400, tuning);
    original.markClerkDead(3000, tuning);
    original.persistent().lastCaseId = makeLogicalId(LogicalIdDomain::Case, 1);

    PrototypeStorePersistence persistence(paths, world);
    expect(persistence.save(original, ids, &reason), "prototype business/clerk memory saves atomically");

    const std::string saved = readText(paths.worldSave);
    expect(saved.find("future_other_business") != std::string::npos, "store save preserves unrelated business records");
    expect(saved.find("prototype_store_state") != std::string::npos, "store save writes owned business record");
    expect(saved.find("\"clerkVacant\": true") != std::string::npos, "clerk vacancy is persisted");

    LogicalIdGenerator reloadedIds;
    expect(world.loadLogicalIdState(reloadedIds, &reason), "base logical ID counters reload after store save");
    PrototypeStoreModel reloaded;
    expect(persistence.load(reloaded, reloadedIds, 3500, tuning, &reason), "prototype business/clerk memory reloads");

    const auto& state = reloaded.persistent();
    expect(state.businessId == businessId, "business logical identity survives save/load");
    expect(state.clerk.id == clerkId, "dead clerk logical identity remains historical until replacement");
    expect(!state.clerk.alive && state.clerkVacant, "clerk death/vacancy survive save/load");
    expect(state.clerk.diedAtMs == 3000, "clerk death timestamp survives save/load");
    expect(state.replacementEligibleAtMs == 8000, "replacement eligibility survives save/load");
    expect(state.robberyCount == 1, "robbery count survives save/load");
    expect(state.lastCaseId == makeLogicalId(LogicalIdDomain::Case, 1), "last case relationship survives save/load");
    expect(state.totalCashExposed > 0, "finite cash-source exposure contributes to persistent business history");
    expect(reloadedIds.nextSequence(LogicalIdDomain::Business) > logicalIdSequence(businessId), "business ID counter resumes above persisted ID");
    expect(reloadedIds.nextSequence(LogicalIdDomain::Clerk) > logicalIdSequence(clerkId), "clerk ID counter resumes above persisted ID");

    expect(!reloaded.ensureReplacementClerk(reloadedIds, 7999, tuning), "reloaded dead clerk cannot be replaced before timer");
    expect(reloaded.ensureReplacementClerk(reloadedIds, 8000, tuning), "reloaded business creates replacement when timer expires");
    expect(reloaded.persistent().clerk.id != clerkId, "replacement after reload gets a new logical Clerk ID");
    expect(reloaded.persistent().clerk.generation == 2, "replacement generation remains continuous after reload");
    expect(reloaded.persistent().businessId == businessId, "business identity remains stable across clerk replacement");

    std::filesystem::remove_all(root, ec);
    if (failures != 0) {
        std::cerr << failures << " StorePersistence assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "StorePersistenceTests passed.\n";
    return EXIT_SUCCESS;
}
