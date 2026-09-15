#include "Foundation.h"
#include "vehicle/GarageVehicleService.h"
#include "vehicle/VehicleIdentitySystem.h"
#include "vehicle/VehiclePersistence.h"

#include <Windows.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
int failures = 0;

void expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
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

gco::crime::CaseFile makeCase(const gco::LogicalId caseId) {
    using namespace gco;
    crime::CaseFile file{};
    file.id = caseId;
    file.state = crime::CaseState::Reported;

    crime::EvidenceRecord vehicle{};
    vehicle.source = crime::EvidenceSource::Witness;
    vehicle.kind = crime::EvidenceKind::Vehicle;
    vehicle.confidence = 0.82f;
    vehicle.observedAtMs = 1000;
    vehicle.independenceKey = "witness:A";
    vehicle.dedupKey = "witness:A:vehicle";
    vehicle.snapshot.descriptor = "model=0x1234ABCD;primaryColor=27;secondaryColor=0";
    file.evidence.push_back(vehicle);

    crime::EvidenceRecord plate{};
    plate.source = crime::EvidenceSource::Witness;
    plate.kind = crime::EvidenceKind::Plate;
    plate.confidence = 0.94f;
    plate.observedAtMs = 1100;
    plate.independenceKey = "witness:A";
    plate.dedupKey = "witness:A:plate";
    plate.snapshot.descriptor = "plate=ABC123";
    file.evidence.push_back(plate);
    return file;
}

void testPlateRules() {
    using namespace gco::vehicle;
    const auto normalized = validatePlateText(" ab 123 ");
    expect(normalized.valid && normalized.normalized == "AB 123", "plate text normalizes to GTA-safe uppercase subset");
    expect(!validatePlateText("ABCDEFGHI").valid, "plate longer than 8 characters is rejected");
    expect(!validatePlateText("AB-123").valid, "unsupported punctuation is rejected");
    expect(!validatePlateText("   ").valid, "all-space plate is rejected");
}

void testHistoricalEvidenceAndGarageCounterplay(const std::filesystem::path& root) {
    using namespace gco;
    using namespace gco::vehicle;

    const auto paths = makePaths(root);
    LogicalIdGenerator ids;
    VehicleIdentitySystem system(ids);
    VehiclePersistenceStore persistence(paths);

    VehicleAppearance appearance{};
    appearance.modelHash = 0x1234ABCDu;
    appearance.modelName = "TESTCAR";
    appearance.primaryColor = 27;
    appearance.secondaryColor = 0;
    appearance.plateText = "ABC123";
    appearance.plateStyle = 0;
    VehicleModificationState mods{};
    mods.modKit = 0;
    mods.wheelType = 2;
    mods.windowTint = 1;
    mods.livery = 3;
    mods.mods[0] = 2;
    mods.mods[11] = 4;
    mods.toggleMods[18] = true;

    auto& owned = system.registerVehicle(VehicleRecordKind::Owned, appearance, mods, 500);
    const LogicalId vehicleId = owned.id;
    expect(logicalIdDomain(vehicleId) == LogicalIdDomain::Vehicle, "owned vehicle gets project logical Vehicle ID");
    expect(system.bindLiveHandle(77, vehicleId), "transient GTA handle binds to logical vehicle without becoming persistence identity");
    expect(system.markCrimeAssociation(vehicleId, makeLogicalId(LogicalIdDomain::Case, 1), true, 900),
        "crime history associates case with getaway car");

    crime::CaseFile file = makeCase(makeLogicalId(LogicalIdDomain::Case, 1));
    expect(system.refreshBoloFromCase(file, vehicleId, 1200), "witness vehicle/plate evidence creates separate vehicle BOLO");
    const auto* initialBolo = system.findBolo(file.id);
    expect(initialBolo != nullptr && initialBolo->active, "vehicle BOLO active independently of person warrant state");
    expect(file.activePersonWarrant == false, "vehicle BOLO construction does not create person warrant");
    expect(initialBolo != nullptr && initialBolo->plateText == std::optional<std::string>{"ABC123"},
        "BOLO snapshots reported historical plate");
    expect(initialBolo != nullptr && matchVehicleBolo(*initialBolo, *system.find(vehicleId)).directPlateMatch,
        "original current car directly matches reported plate");

    std::string reason;
    expect(persistence.save(system, &reason), "baseline vehicle identity state saves");

    int cash = 1000;
    std::string livePlate = "ABC123";
    int livePlateStyle = 0;
    int livePrimary = 27;
    int liveSecondary = 0;
    GarageNativeCallbacks native{};
    native.applyPlate = [&](platform::VehicleHandle handle, std::string_view plate, int style) {
        if (handle != 77) return false;
        livePlate = std::string(plate);
        livePlateStyle = style;
        return true;
    };
    native.applyPaint = [&](platform::VehicleHandle handle, int primary, int secondary) {
        if (handle != 77) return false;
        livePrimary = primary;
        liveSecondary = secondary;
        return true;
    };
    GaragePaymentCallbacks payment{};
    payment.balance = [&]() -> std::optional<int> { return cash; };
    payment.charge = [&](int amount) { if (cash < amount) return false; cash -= amount; return true; };
    payment.credit = [&](int amount) { cash += amount; return true; };

    GarageVehicleService garage(system, persistence, native, payment, GarageServicePricing{500});
    const auto plateResult = garage.changePlate(vehicleId, 77, "xyz789", 1, 2000);
    expect(plateResult.ok() && plateResult.charged == 500, "garage plate service succeeds and charges configured price");
    expect(cash == 500, "plate service deducts money exactly once");
    expect(livePlate == "XYZ789" && livePlateStyle == 1, "current live car receives XYZ789 and new style");
    expect(system.find(vehicleId)->appearance.plateText == "XYZ789", "logical current record persists new plate");
    expect(file.evidence[1].snapshot.descriptor == "plate=ABC123", "historical case evidence remains ABC123 after plate change");
    expect(system.findBolo(file.id)->plateText == std::optional<std::string>{"ABC123"}, "vehicle BOLO historical plate remains ABC123");
    const auto afterPlate = matchVehicleBolo(*system.findBolo(file.id), *system.find(vehicleId));
    expect(!afterPlate.directPlateMatch, "new plate defeats direct old-plate matching");
    expect(afterPlate.strongPhysicalLink, "stronger known physical continuity can still link same logical vehicle");

    const auto paintResult = garage.repaint(vehicleId, 77, 64, 64, 2100);
    expect(paintResult.ok(), "repaint service succeeds");
    expect(livePrimary == 64 && liveSecondary == 64, "live vehicle receives repaint");
    expect(system.find(vehicleId)->appearance.primaryColor == 64, "current logical record receives repaint");
    expect(file.evidence[0].snapshot.descriptor == "model=0x1234ABCD;primaryColor=27;secondaryColor=0",
        "historical color report remains old color snapshot");
    const auto afterPaint = matchVehicleBolo(*system.findBolo(file.id), *system.find(vehicleId));
    expect(!afterPaint.colorMatch, "repaint defeats direct old-color channel");

    expect(system.setStorageStatus(vehicleId, VehicleStorageStatus::Garage, "safehouse.alpha", 2200),
        "storage/status is mutable project state");
    expect(persistence.save(system, &reason), "updated vehicle state saves");

    LogicalIdGenerator reloadedIds;
    VehicleIdentitySystem reloaded(reloadedIds);
    expect(persistence.load(reloaded, &reason), "vehicle identity state reloads");
    const auto* restored = reloaded.find(vehicleId);
    expect(restored != nullptr, "logical owned vehicle survives reload");
    expect(restored != nullptr && restored->appearance.plateText == "XYZ789", "XYZ789 survives save/load");
    expect(restored != nullptr && restored->appearance.primaryColor == 64, "repaint survives save/load");
    expect(restored != nullptr && restored->storageStatus == VehicleStorageStatus::Garage,
        "garage storage status survives save/load");
    expect(restored != nullptr && restored->modifications.mods[11] == 4 && restored->modifications.toggleMods[18],
        "mod reconstruction state survives save/load");
    expect(restored != nullptr && restored->crimeHistory.usedInCrime && restored->crimeHistory.usedAsGetaway,
        "vehicle crime-history flags survive save/load");
    expect(reloaded.findBolo(file.id) != nullptr && reloaded.findBolo(file.id)->plateText == std::optional<std::string>{"ABC123"},
        "historical BOLO snapshot survives reload independently of current XYZ789 plate");
}

void testTemporaryStolenAndSwapRules() {
    using namespace gco;
    using namespace gco::vehicle;

    LogicalIdGenerator ids;
    VehicleIdentitySystem system(ids);
    VehicleAppearance a{};
    a.modelHash = 1;
    a.plateText = "TEMP1";
    VehicleAppearance b{};
    b.modelHash = 2;
    b.plateText = "STOLEN2";
    VehicleModificationState mods{};
    auto& first = system.registerVehicle(VehicleRecordKind::Temporary, a, mods, 100);
    const LogicalId firstId = first.id;
    auto& second = system.registerVehicle(VehicleRecordKind::Stolen, b, mods, 110);
    const LogicalId secondId = second.id;
    expect(system.find(firstId)->kind == VehicleRecordKind::Temporary, "temporary getaway does not automatically become owned");
    expect(system.find(secondId)->kind == VehicleRecordKind::Stolen && system.find(secondId)->crimeHistory.reportedStolen,
        "stolen getaway remains explicitly stolen");

    crime::CaseFile file{};
    file.id = makeLogicalId(LogicalIdDomain::Case, 2);
    crime::EvidenceRecord plate{};
    plate.kind = crime::EvidenceKind::Plate;
    plate.confidence = 0.9f;
    plate.snapshot.descriptor = "plate=TEMP1";
    file.evidence.push_back(plate);
    expect(system.refreshBoloFromCase(file, firstId, 200), "swap fixture BOLO created");

    const auto observed = system.recordVehicleSwap(file.id, firstId, secondId, true, 300);
    expect(observed.continuityPreserved, "observed vehicle swap preserves search continuity");
    expect(system.findBolo(file.id)->linkedVehicleId == std::optional<LogicalId>{secondId},
        "observed swap transfers physical continuity to new logical vehicle");

    const auto unobserved = system.recordVehicleSwap(file.id, secondId, firstId, false, 400);
    expect(!unobserved.continuityPreserved, "unobserved vehicle swap can break search continuity");
    expect(!system.findBolo(file.id)->linkedVehicleId.has_value() && !system.findBolo(file.id)->physicalContinuityKnown,
        "unobserved swap removes physical-link shortcut without rewriting historical BOLO fields");
    expect(system.findBolo(file.id)->plateText == std::optional<std::string>{"TEMP1"},
        "vehicle swap never rewrites original reported plate");
}

} // namespace

int main() {
    testPlateRules();
    const auto root = std::filesystem::temp_directory_path()
        / (L"gco-stage6-vehicle-identity-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    testHistoricalEvidenceAndGarageCounterplay(root);
    testTemporaryStolenAndSwapRules();
    std::filesystem::remove_all(root, ec);

    if (failures != 0) {
        std::cerr << failures << " VehicleIdentity assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "VehicleIdentityTests passed.\n";
    return EXIT_SUCCESS;
}
