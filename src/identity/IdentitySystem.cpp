#include "IdentitySystem.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace gco::identity {
namespace {

constexpr std::uint32_t joaat(const std::string_view text) noexcept {
    std::uint32_t hash = 0;
    for (const char raw : text) {
        const unsigned char ch = static_cast<unsigned char>(raw >= 'A' && raw <= 'Z' ? raw - 'A' + 'a' : raw);
        hash += ch;
        hash += hash << 10U;
        hash ^= hash >> 6U;
    }
    hash += hash << 3U;
    hash ^= hash >> 11U;
    hash += hash << 15U;
    return hash;
}

constexpr std::uint32_t kMichael = joaat("player_zero");
constexpr std::uint32_t kFranklin = joaat("player_one");
constexpr std::uint32_t kTrevor = joaat("player_two");
constexpr std::uint32_t kFreemodeMale = joaat("mp_m_freemode_01");
constexpr std::uint32_t kFreemodeFemale = joaat("mp_f_freemode_01");

float clamp01(const float value) noexcept {
    if (!std::isfinite(value)) return 0.0f;
    return std::clamp(value, 0.0f, 1.0f);
}

std::size_t identityIndex(const CharacterIdentity identity) noexcept {
    const auto value = static_cast<std::size_t>(identity);
    const auto count = static_cast<std::size_t>(CharacterIdentity::Count);
    return value < count ? value : 0;
}

bool readText(const std::filesystem::path& file, std::string& out) {
    std::ifstream input(file, std::ios::binary);
    if (!input.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
    return input.good() || input.eof();
}

std::optional<std::string> extractString(const std::string& object, const std::string_view key) {
    const std::string quoted = "\"" + std::string(key) + "\"";
    const auto keyPos = object.find(quoted);
    if (keyPos == std::string::npos) return std::nullopt;
    const auto colon = object.find(':', keyPos + quoted.size());
    if (colon == std::string::npos) return std::nullopt;
    const auto first = object.find('"', colon + 1);
    if (first == std::string::npos) return std::nullopt;
    std::string result;
    bool escaped = false;
    for (std::size_t i = first + 1; i < object.size(); ++i) {
        const char ch = object[i];
        if (escaped) {
            result.push_back(ch);
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == '"') {
            return result;
        } else {
            result.push_back(ch);
        }
    }
    return std::nullopt;
}

std::optional<int> extractInt(const std::string& object, const std::string_view key) {
    const std::string quoted = "\"" + std::string(key) + "\"";
    const auto keyPos = object.find(quoted);
    if (keyPos == std::string::npos) return std::nullopt;
    const auto colon = object.find(':', keyPos + quoted.size());
    if (colon == std::string::npos) return std::nullopt;
    const char* begin = object.data() + colon + 1;
    const char* end = object.data() + object.size();
    while (begin < end && (*begin == ' ' || *begin == '\n' || *begin == '\r' || *begin == '\t')) ++begin;
    int value = 0;
    const auto parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc{}) return std::nullopt;
    return value;
}

std::optional<bool> extractBool(const std::string& object, const std::string_view key) {
    const std::string quoted = "\"" + std::string(key) + "\"";
    const auto keyPos = object.find(quoted);
    if (keyPos == std::string::npos) return std::nullopt;
    const auto colon = object.find(':', keyPos + quoted.size());
    if (colon == std::string::npos) return std::nullopt;
    const auto truePos = object.find("true", colon + 1);
    const auto falsePos = object.find("false", colon + 1);
    const auto stop = object.find_first_of(",}", colon + 1);
    if (truePos != std::string::npos && (stop == std::string::npos || truePos < stop)) return true;
    if (falsePos != std::string::npos && (stop == std::string::npos || falsePos < stop)) return false;
    return std::nullopt;
}

bool findDelimitedValue(
    const std::string& text,
    const std::string_view key,
    const char open,
    const char close,
    std::size_t& openPos,
    std::size_t& closePos) {

    const std::string quoted = "\"" + std::string(key) + "\"";
    const auto keyPos = text.find(quoted);
    if (keyPos == std::string::npos) return false;
    const auto colon = text.find(':', keyPos + quoted.size());
    if (colon == std::string::npos) return false;
    openPos = text.find(open, colon + 1);
    if (openPos == std::string::npos) return false;

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t i = openPos; i < text.size(); ++i) {
        const char ch = text[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') { inString = true; continue; }
        if (ch == open) ++depth;
        else if (ch == close) {
            --depth;
            if (depth == 0) {
                closePos = i;
                return true;
            }
        }
    }
    return false;
}

std::vector<std::string> splitObjects(const std::string_view body) {
    std::vector<std::string> result;
    bool inString = false;
    bool escaped = false;
    int depth = 0;
    std::size_t start = std::string_view::npos;
    for (std::size_t i = 0; i < body.size(); ++i) {
        const char ch = body[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') { inString = true; continue; }
        if (ch == '{') {
            if (depth == 0) start = i;
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0 && start != std::string_view::npos) {
                result.emplace_back(body.substr(start, i - start + 1));
                start = std::string_view::npos;
            }
        }
    }
    return result;
}

float deterministicRoll(std::uint64_t seed) noexcept {
    seed += 0x9E3779B97F4A7C15ull;
    seed = (seed ^ (seed >> 30U)) * 0xBF58476D1CE4E5B9ull;
    seed = (seed ^ (seed >> 27U)) * 0x94D049BB133111EBull;
    seed ^= seed >> 31U;
    return static_cast<float>(seed & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

bool sameComponent(const platform::PedComponentVariation& a, const platform::PedComponentVariation& b) noexcept {
    return a.drawable == b.drawable && a.texture == b.texture && a.palette == b.palette;
}

bool sameProp(const platform::PedPropVariation& a, const platform::PedPropVariation& b) noexcept {
    return a.drawable == b.drawable && a.texture == b.texture;
}

} // namespace

bool IdentitySystem::loadMaskCatalog(const std::filesystem::path& file, std::string* reason) {
    approvedMasks_.clear();
    lowerFaceBandanaCustomRequired_ = true;

    std::string text;
    if (!readText(file, text)) {
        if (reason) *reason = "mask catalog missing; identity remains functional with no production-approved face covering";
        return false;
    }

    std::size_t lowerOpen = 0;
    std::size_t lowerClose = 0;
    if (findDelimitedValue(text, "lowerFaceBandana", '{', '}', lowerOpen, lowerClose)) {
        const std::string object = text.substr(lowerOpen, lowerClose - lowerOpen + 1);
        const auto status = extractString(object, "status");
        lowerFaceBandanaCustomRequired_ = !status || *status == "CUSTOM_REQUIRED";
    }

    std::size_t arrayOpen = 0;
    std::size_t arrayClose = 0;
    if (!findDelimitedValue(text, "approved", '[', ']', arrayOpen, arrayClose)) {
        if (reason) *reason = "mask catalog has no approved array; face capture remains enabled";
        return true;
    }

    for (const auto& object : splitObjects(std::string_view(text).substr(arrayOpen + 1, arrayClose - arrayOpen - 1))) {
        const auto id = extractString(object, "id");
        const auto modelName = extractString(object, "modelName");
        const auto componentId = extractInt(object, "componentId");
        const auto drawableId = extractInt(object, "drawableId");
        const auto textureId = extractInt(object, "textureId");
        const auto coverageText = extractString(object, "coverage");
        const auto statusText = extractString(object, "status");
        const auto blocksFace = extractBool(object, "blocksNewFaceCapture");
        const auto coverage = coverageText ? maskCoverageFromString(*coverageText) : std::nullopt;
        const auto status = statusText ? assetValidationStatusFromString(*statusText) : std::nullopt;
        if (!id || !modelName || !componentId || !drawableId || !textureId || !coverage || !status || !blocksFace) {
            continue;
        }

        // Only assets personally validated in target GTA builds become production-authoritative.
        if (*status != AssetValidationStatus::VerifiedInGame) continue;
        if (*componentId < 0 || *componentId >= 12 || *drawableId < 0) continue;

        MaskCatalogEntry entry{};
        entry.id = *id;
        entry.modelName = *modelName;
        entry.modelHash = joaat(*modelName);
        entry.componentId = *componentId;
        entry.drawableId = *drawableId;
        entry.textureId = *textureId;
        entry.coverage = *coverage;
        entry.status = *status;
        entry.blocksNewFaceCapture = *blocksFace;
        approvedMasks_.push_back(std::move(entry));
    }

    if (reason) {
        *reason = "loaded " + std::to_string(approvedMasks_.size())
            + " VERIFIED_IN_GAME mask entries; unapproved candidates never suppress face capture";
    }
    return true;
}

IdentitySnapshot IdentitySystem::snapshot(const platform::PedSnapshot& ped) const {
    IdentitySnapshot result{};
    result.character = characterIdentity(ped.modelHash);
    result.outfit = buildOutfitSignature(ped);
    result.mask = classifyMask(ped);
    result.faceCaptureAllowed = !result.mask.blocksNewFaceCapture;
    return result;
}

OutfitSignature IdentitySystem::buildOutfitSignature(const platform::PedSnapshot& ped) const {
    OutfitSignature result{};
    result.modelHash = ped.modelHash;
    result.components = ped.components;
    result.props = ped.props;

    // Face/head, mask/beard and hair are intentionally excluded from clothing identity.
    // A mask change must not erase the clothing observation, and a haircut is not a full outfit change.
    std::ostringstream out;
    out << std::hex << std::uppercase << ped.modelHash << std::dec << "|C";
    for (std::size_t i = 3; i < ped.components.size(); ++i) {
        const auto& value = ped.components[i];
        out << ':' << i << '=' << value.drawable << '.' << value.texture << '.' << value.palette;
    }
    out << "|P";
    for (std::size_t i = 0; i < ped.props.size(); ++i) {
        const auto& value = ped.props[i];
        out << ':' << i << '=' << value.drawable << '.' << value.texture;
    }
    result.stableKey = out.str();
    return result;
}

MaskState IdentitySystem::classifyMask(const platform::PedSnapshot& ped) const {
    MaskState result{};
    const int observedDrawable = ped.components[1].drawable;
    const int observedTexture = ped.components[1].texture;
    result.observedDrawable = observedDrawable;
    result.observedTexture = observedTexture;

    for (const auto& entry : approvedMasks_) {
        if (entry.modelHash != ped.modelHash || entry.componentId < 0 || entry.componentId >= 12) continue;
        const auto& component = ped.components[static_cast<std::size_t>(entry.componentId)];
        if (component.drawable != entry.drawableId) continue;
        if (entry.textureId >= 0 && component.texture != entry.textureId) continue;
        result.coverage = entry.coverage;
        result.status = entry.status;
        result.catalogId = entry.id;
        result.blocksNewFaceCapture = entry.blocksNewFaceCapture;
        result.observedDrawable = component.drawable;
        result.observedTexture = component.texture;
        return result;
    }

    // Component slot 1 is a useful research signal but not authoritative across every ped model.
    // Unknown/nonzero clothing is logged as a candidate and deliberately does NOT block face capture.
    if (observedDrawable > 0) {
        result.coverage = MaskCoverage::UnapprovedCandidate;
        result.status = AssetValidationStatus::ReferenceOnly;
    } else {
        result.coverage = MaskCoverage::None;
        result.status = AssetValidationStatus::Unknown;
    }
    return result;
}

CharacterIdentity IdentitySystem::characterIdentity(const std::uint32_t modelHash) const noexcept {
    if (modelHash == kMichael) return CharacterIdentity::Michael;
    if (modelHash == kFranklin) return CharacterIdentity::Franklin;
    if (modelHash == kTrevor) return CharacterIdentity::Trevor;
    if (modelHash == kFreemodeMale) return CharacterIdentity::FreemodeMale;
    if (modelHash == kFreemodeFemale) return CharacterIdentity::FreemodeFemale;
    return CharacterIdentity::Unknown;
}

float IdentitySystem::clothingSimilarity(const OutfitSignature& a, const OutfitSignature& b) const noexcept {
    if (a.modelHash == 0 || b.modelHash == 0 || a.modelHash != b.modelHash) return 0.0f;

    constexpr std::array<float, 12> componentWeights{
        0.0f, 0.0f, 0.0f, 1.35f, 1.35f, 0.45f, 0.75f, 0.40f, 0.80f, 0.50f, 0.35f, 1.20f};
    constexpr std::array<float, 8> propWeights{0.55f, 0.35f, 0.15f, 0.10f, 0.10f, 0.10f, 0.20f, 0.20f};

    float total = 0.0f;
    float matched = 0.0f;
    for (std::size_t i = 0; i < componentWeights.size(); ++i) {
        if (componentWeights[i] <= 0.0f) continue;
        total += componentWeights[i];
        if (sameComponent(a.components[i], b.components[i])) matched += componentWeights[i];
    }
    for (std::size_t i = 0; i < propWeights.size(); ++i) {
        total += propWeights[i];
        if (sameProp(a.props[i], b.props[i])) matched += propWeights[i];
    }
    return total > 0.0f ? clamp01(matched / total) : 0.0f;
}

bool IdentitySystem::observeFace(
    RecognitionMemory& memory,
    const IdentitySnapshot& observed,
    const float confidence,
    const std::uint64_t nowMs) const {

    if (!observed.faceCaptureAllowed || observed.character == CharacterIdentity::Unknown) return false;
    const std::size_t index = identityIndex(observed.character);
    const float next = std::max(memory.faceConfidence[index], clamp01(confidence));
    const bool changed = next > memory.faceConfidence[index] + 0.001f;
    memory.faceConfidence[index] = next;
    if (memory.firstFaceSeenAtMs[index] == 0) memory.firstFaceSeenAtMs[index] = nowMs;
    memory.lastFaceSeenAtMs[index] = nowMs;
    return changed;
}

bool IdentitySystem::observeOutfit(
    RecognitionMemory& memory,
    const IdentitySnapshot& observed,
    const float confidence,
    const std::uint64_t nowMs) const {

    if (observed.outfit.stableKey.empty()) return false;
    const bool changed = memory.lastOutfitKey != observed.outfit.stableKey
        || clamp01(confidence) > memory.clothingConfidence + 0.001f;
    memory.lastOutfitKey = observed.outfit.stableKey;
    memory.clothingConfidence = clamp01(confidence);
    memory.lastOutfitSeenAtMs = nowMs;
    return changed;
}

RecognitionResult IdentitySystem::evaluateRecognition(
    const RecognitionMemory& memory,
    const IdentitySnapshot& current,
    const std::uint64_t seed) const noexcept {

    RecognitionResult result{};
    const float rememberedFace = rememberedFaceConfidence(memory, current.character);
    result.confirmedIdentityPreserved = rememberedFace >= tuning_.confirmedFaceThreshold;

    // Face memory can only be matched when the face is currently available to the observer.
    // The player model hash is never used to bypass a current mask.
    if (current.faceCaptureAllowed && current.character != CharacterIdentity::Unknown) {
        result.faceMatch = rememberedFace;
    }

    if (!memory.lastOutfitKey.empty() && !current.outfit.stableKey.empty()) {
        result.clothingMatch = memory.lastOutfitKey == current.outfit.stableKey
            ? memory.clothingConfidence
            : memory.clothingConfidence * 0.15f;
    }

    if (result.faceMatch >= tuning_.confirmedFaceThreshold) {
        result.level = RecognitionLevel::Recognized;
        const float roll = deterministicRoll(seed ^ static_cast<std::uint64_t>(identityIndex(current.character)));
        if (roll < 0.34f) result.behavior = RecognitionBehavior::StarePause;
        else if (roll < 0.68f) result.behavior = RecognitionBehavior::BackAway;
        else result.behavior = RecognitionBehavior::SilentAlarmPossible;
        return result;
    }
    if (result.faceMatch >= tuning_.probableFaceThreshold) {
        result.level = RecognitionLevel::Probable;
        result.behavior = RecognitionBehavior::StarePause;
        return result;
    }
    if (result.clothingMatch >= tuning_.strongClothingThreshold) {
        // Clothing can create suspicion, but clothing alone never becomes a confirmed person identity.
        result.level = result.confirmedIdentityPreserved && !current.faceCaptureAllowed
            ? RecognitionLevel::Probable
            : RecognitionLevel::Suspicious;
        result.behavior = RecognitionBehavior::Notice;
        return result;
    }
    if (result.clothingMatch >= tuning_.suspiciousClothingThreshold) {
        result.level = RecognitionLevel::Suspicious;
        result.behavior = RecognitionBehavior::Notice;
    }
    return result;
}

float IdentitySystem::rememberedFaceConfidence(
    const RecognitionMemory& memory,
    const CharacterIdentity identity) const noexcept {

    return memory.faceConfidence[identityIndex(identity)];
}

bool IdentitySystem::hasConfirmedIdentity(
    const RecognitionMemory& memory,
    const CharacterIdentity identity) const noexcept {

    return identity != CharacterIdentity::Unknown
        && rememberedFaceConfidence(memory, identity) >= tuning_.confirmedFaceThreshold;
}

std::string IdentitySystem::debugDescribe(const IdentitySnapshot& value) const {
    std::ostringstream out;
    out << "character=" << characterIdentityName(value.character)
        << ";outfit=" << value.outfit.stableKey
        << ";mask=" << maskCoverageName(value.mask.coverage)
        << ";maskStatus=" << assetValidationStatusName(value.mask.status)
        << ";maskId=" << (value.mask.catalogId.empty() ? "none" : value.mask.catalogId)
        << ";component1=" << value.mask.observedDrawable << '.' << value.mask.observedTexture
        << ";faceCaptureAllowed=" << (value.faceCaptureAllowed ? "true" : "false")
        << ";approvedMasks=" << approvedMasks_.size()
        << ";lowerFaceBandana=" << (lowerFaceBandanaCustomRequired_ ? "CUSTOM_REQUIRED" : "catalogued");
    return out.str();
}

std::string_view characterIdentityName(const CharacterIdentity value) noexcept {
    switch (value) {
    case CharacterIdentity::Unknown: return "unknown";
    case CharacterIdentity::Michael: return "michael";
    case CharacterIdentity::Franklin: return "franklin";
    case CharacterIdentity::Trevor: return "trevor";
    case CharacterIdentity::FreemodeMale: return "freemode_male";
    case CharacterIdentity::FreemodeFemale: return "freemode_female";
    case CharacterIdentity::Count: break;
    }
    return "unknown";
}

std::optional<CharacterIdentity> characterIdentityFromString(const std::string_view value) noexcept {
    if (value == "michael") return CharacterIdentity::Michael;
    if (value == "franklin") return CharacterIdentity::Franklin;
    if (value == "trevor") return CharacterIdentity::Trevor;
    if (value == "freemode_male") return CharacterIdentity::FreemodeMale;
    if (value == "freemode_female") return CharacterIdentity::FreemodeFemale;
    if (value == "unknown") return CharacterIdentity::Unknown;
    return std::nullopt;
}

std::string_view maskCoverageName(const MaskCoverage value) noexcept {
    switch (value) {
    case MaskCoverage::None: return "none";
    case MaskCoverage::CosmeticOnly: return "cosmetic_only";
    case MaskCoverage::LowerFace: return "face_hidden_lower";
    case MaskCoverage::FullFace: return "face_hidden_full";
    case MaskCoverage::UnapprovedCandidate: return "unapproved_candidate";
    }
    return "none";
}

std::optional<MaskCoverage> maskCoverageFromString(const std::string_view value) noexcept {
    if (value == "none") return MaskCoverage::None;
    if (value == "cosmetic_only") return MaskCoverage::CosmeticOnly;
    if (value == "face_hidden_lower") return MaskCoverage::LowerFace;
    if (value == "face_hidden_full") return MaskCoverage::FullFace;
    if (value == "unapproved_candidate") return MaskCoverage::UnapprovedCandidate;
    return std::nullopt;
}

std::string_view assetValidationStatusName(const AssetValidationStatus value) noexcept {
    switch (value) {
    case AssetValidationStatus::VerifiedInGame: return "VERIFIED_IN_GAME";
    case AssetValidationStatus::VerifiedData: return "VERIFIED_DATA";
    case AssetValidationStatus::ReferenceOnly: return "REFERENCE_ONLY";
    case AssetValidationStatus::CustomRequired: return "CUSTOM_REQUIRED";
    case AssetValidationStatus::Rejected: return "REJECTED";
    case AssetValidationStatus::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::optional<AssetValidationStatus> assetValidationStatusFromString(const std::string_view value) noexcept {
    if (value == "VERIFIED_IN_GAME") return AssetValidationStatus::VerifiedInGame;
    if (value == "VERIFIED_DATA") return AssetValidationStatus::VerifiedData;
    if (value == "REFERENCE_ONLY") return AssetValidationStatus::ReferenceOnly;
    if (value == "CUSTOM_REQUIRED") return AssetValidationStatus::CustomRequired;
    if (value == "REJECTED") return AssetValidationStatus::Rejected;
    return std::nullopt;
}

std::string_view recognitionLevelName(const RecognitionLevel value) noexcept {
    switch (value) {
    case RecognitionLevel::None: return "none";
    case RecognitionLevel::Suspicious: return "suspicious";
    case RecognitionLevel::Probable: return "probable";
    case RecognitionLevel::Recognized: return "recognized";
    }
    return "none";
}

std::string_view recognitionBehaviorName(const RecognitionBehavior value) noexcept {
    switch (value) {
    case RecognitionBehavior::None: return "none";
    case RecognitionBehavior::Notice: return "notice";
    case RecognitionBehavior::StarePause: return "stare_pause";
    case RecognitionBehavior::BackAway: return "back_away";
    case RecognitionBehavior::SilentAlarmPossible: return "silent_alarm_possible";
    }
    return "none";
}

} // namespace gco::identity
