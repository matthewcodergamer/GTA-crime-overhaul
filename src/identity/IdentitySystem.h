#pragma once

#include "platform/PlatformTypes.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gco::identity {

enum class CharacterIdentity : std::uint8_t {
    Unknown,
    Michael,
    Franklin,
    Trevor,
    FreemodeMale,
    FreemodeFemale
};

enum class MaskCoverage : std::uint8_t {
    None,
    CosmeticOnly,
    LowerFace,
    FullFace,
    UnapprovedCandidate
};

enum class AssetValidationStatus : std::uint8_t {
    VerifiedInGame,
    VerifiedData,
    ReferenceOnly,
    CustomRequired,
    Rejected,
    Unknown
};

enum class RecognitionLevel : std::uint8_t {
    None,
    Suspicious,
    Probable,
    Recognized
};

enum class RecognitionBehavior : std::uint8_t {
    None,
    Notice,
    StarePause,
    BackAway,
    SilentAlarmPossible
};

struct OutfitSignature final {
    std::uint32_t modelHash = 0;
    std::array<platform::ComponentVariation, 12> components{};
    std::array<platform::PropVariation, 8> props{};
    std::string stableKey;
};

struct MaskCatalogEntry final {
    std::string id;
    std::string modelName;
    std::uint32_t modelHash = 0;
    int componentId = 1;
    int drawableId = -1;
    int textureId = -1;
    MaskCoverage coverage = MaskCoverage::None;
    AssetValidationStatus status = AssetValidationStatus::Unknown;
    bool blocksNewFaceCapture = false;
};

struct MaskState final {
    MaskCoverage coverage = MaskCoverage::None;
    AssetValidationStatus status = AssetValidationStatus::Unknown;
    std::string catalogId;
    bool blocksNewFaceCapture = false;
    int observedDrawable = -1;
    int observedTexture = -1;
};

struct IdentitySnapshot final {
    CharacterIdentity character = CharacterIdentity::Unknown;
    OutfitSignature outfit{};
    MaskState mask{};
    bool faceCaptureAllowed = true;
};

struct RecognitionMemory final {
    CharacterIdentity confirmedIdentity = CharacterIdentity::Unknown;
    float faceConfidence = 0.0f;
    std::string lastOutfitKey;
    float clothingConfidence = 0.0f;
    std::uint64_t firstFaceSeenAtMs = 0;
    std::uint64_t lastFaceSeenAtMs = 0;
    std::uint64_t lastOutfitSeenAtMs = 0;
    std::uint32_t recognitionCount = 0;
};

struct RecognitionResult final {
    RecognitionLevel level = RecognitionLevel::None;
    RecognitionBehavior behavior = RecognitionBehavior::None;
    float faceMatch = 0.0f;
    float clothingMatch = 0.0f;
    bool confirmedIdentityPreserved = false;
};

struct IdentityTuning final {
    float confirmedFaceThreshold = 0.72f;
    float probableFaceThreshold = 0.56f;
    float strongClothingThreshold = 0.82f;
    float suspiciousClothingThreshold = 0.58f;
};

class IdentitySystem final {
public:
    explicit IdentitySystem(IdentityTuning tuning = {}) : tuning_(tuning) {}

    bool loadMaskCatalog(const std::filesystem::path& file, std::string* reason = nullptr);

    [[nodiscard]] IdentitySnapshot snapshot(const platform::PedSnapshot& ped) const;
    [[nodiscard]] OutfitSignature buildOutfitSignature(const platform::PedSnapshot& ped) const;
    [[nodiscard]] MaskState classifyMask(const platform::PedSnapshot& ped) const;
    [[nodiscard]] CharacterIdentity characterIdentity(std::uint32_t modelHash) const noexcept;
    [[nodiscard]] float clothingSimilarity(const OutfitSignature& a, const OutfitSignature& b) const noexcept;

    bool observeFace(RecognitionMemory& memory, const IdentitySnapshot& observed, float confidence, std::uint64_t nowMs) const;
    bool observeOutfit(RecognitionMemory& memory, const IdentitySnapshot& observed, float confidence, std::uint64_t nowMs) const;
    [[nodiscard]] RecognitionResult evaluateRecognition(
        const RecognitionMemory& memory,
        const IdentitySnapshot& current,
        std::uint64_t seed = 0) const noexcept;

    [[nodiscard]] bool lowerFaceBandanaCustomRequired() const noexcept { return lowerFaceBandanaCustomRequired_; }
    [[nodiscard]] std::size_t approvedMaskCount() const noexcept { return approvedMasks_.size(); }
    [[nodiscard]] std::string debugDescribe(const IdentitySnapshot& snapshot) const;

private:
    IdentityTuning tuning_{};
    std::vector<MaskCatalogEntry> approvedMasks_;
    bool lowerFaceBandanaCustomRequired_ = true;
};

[[nodiscard]] std::string_view characterIdentityName(CharacterIdentity value) noexcept;
[[nodiscard]] std::optional<CharacterIdentity> characterIdentityFromString(std::string_view value) noexcept;
[[nodiscard]] std::string_view maskCoverageName(MaskCoverage value) noexcept;
[[nodiscard]] std::optional<MaskCoverage> maskCoverageFromString(std::string_view value) noexcept;
[[nodiscard]] std::string_view assetValidationStatusName(AssetValidationStatus value) noexcept;
[[nodiscard]] std::optional<AssetValidationStatus> assetValidationStatusFromString(std::string_view value) noexcept;
[[nodiscard]] std::string_view recognitionLevelName(RecognitionLevel value) noexcept;
[[nodiscard]] std::string_view recognitionBehaviorName(RecognitionBehavior value) noexcept;

} // namespace gco::identity
