#include "identity/IdentitySystem.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
int failures = 0;

void expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

gco::platform::PedSnapshot makeFranklinSnapshot() {
    gco::platform::PedSnapshot ped{};
    ped.modelHash = 2602752943u; // player_one, VERIFIED_DATA from GTA V ped data.
    ped.alive = true;
    ped.isPlayer = true;
    for (std::size_t i = 0; i < ped.components.size(); ++i) {
        ped.components[i].drawable = static_cast<int>(i + 1);
        ped.components[i].texture = static_cast<int>(i % 3);
        ped.components[i].palette = 0;
    }
    for (std::size_t i = 0; i < ped.props.size(); ++i) {
        ped.props[i].drawable = static_cast<int>(i);
        ped.props[i].texture = static_cast<int>(i % 2);
    }
    ped.components[1].drawable = 0;
    ped.components[1].texture = 0;
    return ped;
}

std::filesystem::path writeTestCatalog() {
    const auto path = std::filesystem::temp_directory_path() / "gco-stage5-mask-catalog.json";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << R"json({
  "schemaVersion": 1,
  "lowerFaceBandana": {"status":"CUSTOM_REQUIRED"},
  "approved": [
    {
      "id":"test_franklin_lower_face",
      "modelName":"player_one",
      "componentId":1,
      "drawableId":5,
      "textureId":2,
      "coverage":"face_hidden_lower",
      "status":"VERIFIED_IN_GAME",
      "blocksNewFaceCapture":true
    },
    {
      "id":"reference_only_must_not_activate",
      "modelName":"player_one",
      "componentId":1,
      "drawableId":6,
      "textureId":0,
      "coverage":"face_hidden_full",
      "status":"REFERENCE_ONLY",
      "blocksNewFaceCapture":true
    }
  ]
})json";
    return path;
}

void testOutfitSignatureIgnoresMaskAndHairButTracksClothes() {
    using namespace gco::identity;
    IdentitySystem system;
    const auto base = makeFranklinSnapshot();
    const auto first = system.buildOutfitSignature(base);

    auto maskHairChanged = base;
    maskHairChanged.components[1].drawable = 99;
    maskHairChanged.components[1].texture = 7;
    maskHairChanged.components[2].drawable = 88;
    maskHairChanged.components[2].texture = 6;
    const auto second = system.buildOutfitSignature(maskHairChanged);
    expect(first.stableKey == second.stableKey,
        "mask/hair changes do not masquerade as a full clothing change");

    auto clothesChanged = base;
    clothesChanged.components[3].drawable += 10;
    const auto third = system.buildOutfitSignature(clothesChanged);
    expect(first.stableKey != third.stableKey,
        "real clothing component change produces a different stable outfit signature");
    expect(system.clothingSimilarity(first, first) > 0.99f,
        "identical outfit has near-perfect similarity");
    expect(system.clothingSimilarity(first, third) < 1.0f,
        "changed clothing weakens similarity");
}

void testApprovedMaskBlocksOnlyFutureFaceCapture() {
    using namespace gco::identity;
    IdentitySystem system;
    std::string reason;
    const auto catalog = writeTestCatalog();
    expect(system.loadMaskCatalog(catalog, &reason), "test mask catalog loads");
    expect(system.approvedMaskCount() == 1,
        "only VERIFIED_IN_GAME entries become production-authoritative");
    expect(system.lowerFaceBandanaCustomRequired(),
        "lower-face bandana remains CUSTOM_REQUIRED when catalog says so");

    auto unmaskedPed = makeFranklinSnapshot();
    const auto unmasked = system.snapshot(unmaskedPed);
    expect(unmasked.character == CharacterIdentity::Franklin,
        "Franklin model maps to stable project identity");
    expect(unmasked.faceCaptureAllowed, "unmasked face can be captured");

    RecognitionMemory memory{};
    expect(system.observeFace(memory, unmasked, 0.88f, 1000),
        "clear pre-mask face observation is stored");
    expect(system.hasConfirmedIdentity(memory, CharacterIdentity::Franklin),
        "strong face observation confirms identity memory");
    const float beforeMask = system.rememberedFaceConfidence(memory, CharacterIdentity::Franklin);

    auto maskedPed = unmaskedPed;
    maskedPed.components[1].drawable = 5;
    maskedPed.components[1].texture = 2;
    const auto masked = system.snapshot(maskedPed);
    expect(masked.mask.coverage == MaskCoverage::LowerFace,
        "approved lower-face mask classifies correctly");
    expect(!masked.faceCaptureAllowed,
        "approved face covering blocks new face capture");
    expect(!system.observeFace(memory, masked, 0.99f, 2000),
        "mask prevents new face observation");
    expect(system.rememberedFaceConfidence(memory, CharacterIdentity::Franklin) == beforeMask,
        "mask never erases face confidence gathered earlier");

    auto unapprovedPed = unmaskedPed;
    unapprovedPed.components[1].drawable = 6;
    unapprovedPed.components[1].texture = 0;
    const auto unapproved = system.snapshot(unapprovedPed);
    expect(unapproved.mask.coverage == MaskCoverage::UnapprovedCandidate,
        "non-approved mask-like component is research-only");
    expect(unapproved.faceCaptureAllowed,
        "REFERENCE_ONLY candidate cannot silently suppress face capture");

    std::error_code ec;
    std::filesystem::remove(catalog, ec);
}

void testClothesCounterplayDoesNotClearConfirmedIdentity() {
    using namespace gco::identity;
    IdentitySystem system;
    RecognitionMemory memory{};

    auto originalPed = makeFranklinSnapshot();
    const auto original = system.snapshot(originalPed);
    expect(system.observeFace(memory, original, 0.90f, 1000), "face memory stored");
    expect(system.observeOutfit(memory, original, 0.92f, 1000), "outfit memory stored");

    auto changedPed = originalPed;
    changedPed.components[3].drawable += 20;
    changedPed.components[4].drawable += 30;
    changedPed.components[11].drawable += 40;
    const auto changed = system.snapshot(changedPed);
    const auto changedVisible = system.evaluateRecognition(memory, changed, 1234);
    expect(changedVisible.confirmedIdentityPreserved,
        "clothes change cannot clear already-confirmed face identity");
    expect(changedVisible.level == RecognitionLevel::Recognized,
        "same uncovered known face remains recognized despite changed clothes");
    expect(changedVisible.clothingMatch < memory.clothingConfidence,
        "changed clothes weaken clothing match");

    std::string reason;
    const auto catalog = writeTestCatalog();
    expect(system.loadMaskCatalog(catalog, &reason), "mask catalog reloads for masked counterplay test");
    changedPed.components[1].drawable = 5;
    changedPed.components[1].texture = 2;
    const auto changedMasked = system.snapshot(changedPed);
    const auto hidden = system.evaluateRecognition(memory, changedMasked, 5678);
    expect(hidden.confirmedIdentityPreserved,
        "confirmed identity remains stored even while current face is hidden");
    expect(hidden.faceMatch == 0.0f,
        "current mask prevents using face memory as a live visual match");

    RecognitionMemory clothingOnly{};
    expect(system.observeOutfit(clothingOnly, original, 0.95f, 1000),
        "clothing-only memory can be stored");
    const auto clothesOnlyMatch = system.evaluateRecognition(clothingOnly, original, 99);
    expect(clothesOnlyMatch.level == RecognitionLevel::Suspicious,
        "clothing alone creates suspicion rather than confirmed identity");

    std::error_code ec;
    std::filesystem::remove(catalog, ec);
}

} // namespace

int main() {
    testOutfitSignatureIgnoresMaskAndHairButTracksClothes();
    testApprovedMaskBlocksOnlyFutureFaceCapture();
    testClothesCounterplayDoesNotClearConfirmedIdentity();

    if (failures != 0) {
        std::cerr << failures << " IdentitySystem assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "IdentitySystemTests passed.\n";
    return EXIT_SUCCESS;
}
