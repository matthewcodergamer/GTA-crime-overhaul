#include "crime/CrimeDirector.h"
#include "crime/CrimePersistence.h"

#include <Windows.h>

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
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

gco::RuntimePaths makePaths(const std::filesystem::path& root) {
    gco::RuntimePaths paths{};
    paths.gameRoot = root;
    paths.modRoot = root / L"GTA_Crime_Overhaul";
    paths.configFile = paths.modRoot / L"GTA_Crime_Overhaul.ini";
    paths.dataRoot = paths.modRoot / L"data";
    paths.saveRoot = paths.modRoot / L"saves";
    paths.logRoot = paths.modRoot / L"logs";
    paths.worldSave = paths.saveRoot / L"world.json";
    paths.worldBackup = paths.saveRoot / L"world.backup.json";
    paths.ensureDirectories();
    return paths;
}

} // namespace

int main() {
    using namespace gco;
    using namespace gco::crime;

    const auto root = std::filesystem::temp_directory_path()
        / (L"gco-stage2-test-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    const RuntimePaths paths = makePaths(root);

    WorldStateStore world(paths);
    expect(world.loadOrCreate().ok(), "test world must initialize");

    LogicalIdGenerator ids;
    std::string reason;
    expect(world.loadLogicalIdState(ids, &reason), "Stage 0 ID counters must load before cases");

    CrimeRegistry registry;
    EventBus bus;
    CrimeDirector director(registry, ids, bus);
    CrimePersistenceStore persistence(paths, world);

    CrimeOccurrence first{};
    first.type = CrimeType::ArmedRobbery;
    first.occurredAtMs = 1000;
    first.incidentKey = "synthetic.persistence";
    first.location = {123.25f, -44.5f, 18.0f, "unit_test"};
    const auto primary = director.recordCrime(first);

    CrimeOccurrence related = first;
    related.type = CrimeType::PropertyDamage;
    related.occurredAtMs = 1010;
    const auto merged = director.recordCrime(related);
    expect(primary.caseId == merged.caseId && merged.mergedIntoExistingCase,
        "related synthetic crimes must share persisted case");

    EvidenceRecord observation{};
    observation.source = EvidenceSource::SyntheticDebug;
    observation.kind = EvidenceKind::Vehicle;
    observation.confidence = 0.65f;
    observation.observedAtMs = 1020;
    observation.independenceKey = "debug-witness-1";
    observation.dedupKey = "persist-evidence-1";
    observation.snapshot.descriptor = "black coupe / partial plate 46E";
    observation.snapshot.location = first.location;
    observation.snapshot.relatedVehicleId = makeLogicalId(LogicalIdDomain::Vehicle, 44);
    expect(director.addEvidence(primary.caseId, observation) == EvidenceAddResult::Added,
        "synthetic evidence must add before persistence");

    ImmediateResponseState immediate{};
    immediate.active = false;
    immediate.reportPending = false;
    immediate.pursuitActive = false;
    immediate.tacticalLevel = 2;
    immediate.lastUpdatedAtMs = 1030;
    expect(director.setImmediateResponse(primary.caseId, immediate, 1030),
        "immediate response must persist independently");
    expect(director.beginReporting(primary.caseId, 1040), "observed -> reporting");
    expect(director.markReported(primary.caseId, 1050), "reporting -> reported");
    expect(director.beginInvestigation(primary.caseId, 1060), "reported -> investigating");
    expect(director.markUnknownSuspect(primary.caseId, 0.30f, 1070), "investigating -> unknown");
    expect(director.issueBoloOrWarrant(primary.caseId, false, true, 1080), "vehicle BOLO must persist");
    expect(director.markDormant(primary.caseId, 1090), "BOLO -> dormant must persist");

    expect(persistence.save(registry, ids, &reason), "case registry must save into world.json");
    std::string saved = readText(paths.worldSave);
    expect(saved.find("\"modelVersion\": 1") != std::string::npos, "case model version must be serialized");
    expect(saved.find("\"crime\"") != std::string::npos, "crime next-ID counter must be serialized");
    expect(saved.find("black coupe / partial plate 46E") != std::string::npos, "immutable evidence snapshot must serialize");
    expect(saved.find("entityHandle") == std::string::npos, "case persistence must never contain raw GTA handles");

    LogicalIdGenerator loadedIds;
    expect(world.loadLogicalIdState(loadedIds, &reason), "existing Stage 0 counters must still restore");
    CrimeRegistry loaded;
    expect(persistence.load(loaded, loadedIds, &reason), "case registry must reload from world.json");
    expect(loaded.caseCount() == 1, "one merged case must reload");
    expect(loaded.crimeCount() == 2, "both related crimes must reload");

    const CaseFile* loadedCase = loaded.findCase(primary.caseId);
    expect(loadedCase != nullptr, "case logical ID must survive reload");
    expect(loadedCase != nullptr && loadedCase->state == CaseState::Dormant, "long-term state must survive reload");
    expect(loadedCase != nullptr && loadedCase->activeVehicleBolo, "vehicle BOLO must survive reload");
    expect(loadedCase != nullptr && loadedCase->evidence.size() == 1, "evidence history must survive reload exactly");
    expect(loadedCase != nullptr && loadedCase->evidence[0].snapshot.descriptor == observation.snapshot.descriptor,
        "evidence snapshot text must remain immutable across reload");
    expect(loadedIds.nextSequence(LogicalIdDomain::Case) > logicalIdSequence(primary.caseId),
        "case ID generator must resume after persisted max");
    expect(loadedIds.nextSequence(LogicalIdDomain::Crime) > logicalIdSequence(merged.crimeId),
        "crime ID generator must resume after persisted max");

    // Unknown top-level world data must survive the Stage 2 read/modify/write cycle.
    saved = readText(paths.worldSave);
    const auto finalBrace = saved.find_last_of('}');
    expect(finalBrace != std::string::npos, "saved world must have final object brace");
    if (finalBrace != std::string::npos) {
        saved.insert(finalBrace, ",\n  \"futureStageField\": { \"kept\": true }\n");
        expect(world.writeWorldAtomically(saved), "world with unknown future field must remain valid");
        expect(persistence.save(loaded, loadedIds, &reason), "case save must preserve unknown world sections");
        expect(readText(paths.worldSave).find("futureStageField") != std::string::npos,
            "unknown top-level field must survive case save");
    }

    // World schema is still valid, but a future/unknown case model must fail closed with a diagnostic.
    std::string incompatible = readText(paths.worldSave);
    const auto modelVersion = incompatible.find("\"modelVersion\": 1");
    expect(modelVersion != std::string::npos, "serialized case model version must be findable for schema test");
    if (modelVersion != std::string::npos) {
        incompatible.replace(modelVersion, std::string("\"modelVersion\": 1").size(), "\"modelVersion\": 999");
        expect(world.writeWorldAtomically(incompatible), "world-level schema accepts opaque future case payload");
        CrimeRegistry rejected;
        LogicalIdGenerator rejectedIds;
        expect(world.loadLogicalIdState(rejectedIds, &reason), "base ID state still loads for schema rejection test");
        reason.clear();
        expect(!persistence.load(rejected, rejectedIds, &reason), "unsupported case modelVersion must be rejected");
        expect(reason.find("unsupported case modelVersion") != std::string::npos,
            "case schema rejection must explain modelVersion incompatibility");
    }

    std::filesystem::remove_all(root, ec);
    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "CrimePersistenceTests passed.\n";
    return EXIT_SUCCESS;
}
