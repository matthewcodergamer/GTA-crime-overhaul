#include "StoreTarget.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <fstream>
#include <iterator>
#include <map>
#include <string_view>

namespace gco::robbery {
namespace {

enum class JsonType : std::uint8_t { Null, Bool, Number, String, Array, Object };

struct JsonValue final {
    JsonType type = JsonType::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;

    const JsonValue* get(const std::string_view key) const {
        if (type != JsonType::Object) return nullptr;
        const auto it = object.find(std::string(key));
        return it == object.end() ? nullptr : &it->second;
    }
};

class JsonParser final {
public:
    explicit JsonParser(std::string_view text) : text_(text) {}

    bool parse(JsonValue& out, std::string& error) {
        skipSpace();
        if (!parseValue(out, error)) return false;
        skipSpace();
        if (pos_ != text_.size()) {
            error = "trailing content after JSON value";
            return false;
        }
        return true;
    }

private:
    void skipSpace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
    }

    bool consume(const char ch) {
        skipSpace();
        if (pos_ >= text_.size() || text_[pos_] != ch) return false;
        ++pos_;
        return true;
    }

    bool parseValue(JsonValue& out, std::string& error) {
        skipSpace();
        if (pos_ >= text_.size()) { error = "unexpected end of JSON"; return false; }
        const char ch = text_[pos_];
        if (ch == '{') return parseObject(out, error);
        if (ch == '[') return parseArray(out, error);
        if (ch == '"') { out.type = JsonType::String; return parseString(out.string, error); }
        if (ch == 't' && text_.substr(pos_, 4) == "true") { pos_ += 4; out.type = JsonType::Bool; out.boolean = true; return true; }
        if (ch == 'f' && text_.substr(pos_, 5) == "false") { pos_ += 5; out.type = JsonType::Bool; out.boolean = false; return true; }
        if (ch == 'n' && text_.substr(pos_, 4) == "null") { pos_ += 4; out.type = JsonType::Null; return true; }
        return parseNumber(out, error);
    }

    bool parseObject(JsonValue& out, std::string& error) {
        if (!consume('{')) { error = "expected object"; return false; }
        out = {}; out.type = JsonType::Object;
        skipSpace();
        if (consume('}')) return true;
        while (true) {
            std::string key;
            if (!parseString(key, error)) return false;
            if (!consume(':')) { error = "expected ':' after object key"; return false; }
            JsonValue value;
            if (!parseValue(value, error)) return false;
            out.object.emplace(std::move(key), std::move(value));
            skipSpace();
            if (consume('}')) return true;
            if (!consume(',')) { error = "expected ',' in object"; return false; }
        }
    }

    bool parseArray(JsonValue& out, std::string& error) {
        if (!consume('[')) { error = "expected array"; return false; }
        out = {}; out.type = JsonType::Array;
        skipSpace();
        if (consume(']')) return true;
        while (true) {
            JsonValue value;
            if (!parseValue(value, error)) return false;
            out.array.push_back(std::move(value));
            skipSpace();
            if (consume(']')) return true;
            if (!consume(',')) { error = "expected ',' in array"; return false; }
        }
    }

    bool parseString(std::string& out, std::string& error) {
        skipSpace();
        if (pos_ >= text_.size() || text_[pos_] != '"') { error = "expected string"; return false; }
        ++pos_; out.clear();
        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') return true;
            if (ch == '\\') {
                if (pos_ >= text_.size()) { error = "unterminated string escape"; return false; }
                const char escaped = text_[pos_++];
                switch (escaped) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: error = "unsupported JSON string escape"; return false;
                }
            } else out.push_back(ch);
        }
        error = "unterminated string";
        return false;
    }

    bool parseNumber(JsonValue& out, std::string& error) {
        skipSpace();
        const std::size_t start = pos_;
        if (pos_ < text_.size() && text_[pos_] == '-') ++pos_;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        if (start == pos_) { error = "expected JSON value"; return false; }
        try {
            out.type = JsonType::Number;
            out.number = std::stod(std::string(text_.substr(start, pos_ - start)));
            return std::isfinite(out.number);
        } catch (...) {
            error = "invalid JSON number";
            return false;
        }
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

bool readText(const std::filesystem::path& file, std::string& out) {
    std::ifstream input(file, std::ios::binary);
    if (!input.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
    return input.good() || input.eof();
}

std::optional<std::string> stringField(const JsonValue& object, const std::string_view key) {
    const auto* value = object.get(key);
    if (value == nullptr || value->type == JsonType::Null || value->type != JsonType::String) return std::nullopt;
    return value->string;
}

std::optional<double> numberField(const JsonValue& object, const std::string_view key) {
    const auto* value = object.get(key);
    if (value == nullptr || value->type == JsonType::Null || value->type != JsonType::Number || !std::isfinite(value->number)) return std::nullopt;
    return value->number;
}

std::optional<bool> boolField(const JsonValue& object, const std::string_view key) {
    const auto* value = object.get(key);
    if (value == nullptr || value->type == JsonType::Null || value->type != JsonType::Bool) return std::nullopt;
    return value->boolean;
}

std::optional<platform::Vec3> vec3Value(const JsonValue* value) {
    if (value == nullptr || value->type == JsonType::Null || value->type != JsonType::Object) return std::nullopt;
    const auto x = numberField(*value, "x"); const auto y = numberField(*value, "y"); const auto z = numberField(*value, "z");
    if (!x || !y || !z) return std::nullopt;
    return platform::Vec3{static_cast<float>(*x), static_cast<float>(*y), static_cast<float>(*z)};
}

std::optional<StoreAnchor> anchorValue(const JsonValue* value) {
    if (value == nullptr || value->type == JsonType::Null || value->type != JsonType::Object) return std::nullopt;
    const auto position = vec3Value(value->get("position"));
    if (!position) return std::nullopt;
    StoreAnchor anchor{}; anchor.position = *position;
    if (const auto heading = numberField(*value, "heading")) anchor.heading = static_cast<float>(*heading);
    return anchor;
}

std::optional<StoreVolume> volumeValue(const JsonValue* value) {
    if (value == nullptr || value->type == JsonType::Null || value->type != JsonType::Object) return std::nullopt;
    const auto min = vec3Value(value->get("min")); const auto max = vec3Value(value->get("max"));
    if (!min || !max) return std::nullopt;
    StoreVolume volume{*min, *max};
    if (!volume.valid()) return std::nullopt;
    return volume;
}

std::vector<StoreAnchor> anchorArray(const JsonValue* value) {
    std::vector<StoreAnchor> result;
    if (value == nullptr || value->type != JsonType::Array) return result;
    for (const auto& entry : value->array) if (const auto anchor = anchorValue(&entry)) result.push_back(*anchor);
    return result;
}

std::vector<StoreVolume> volumeArray(const JsonValue* value) {
    std::vector<StoreVolume> result;
    if (value == nullptr || value->type != JsonType::Array) return result;
    for (const auto& entry : value->array) if (const auto volume = volumeValue(&entry)) result.push_back(*volume);
    return result;
}

void applyPersonalityWeights(const JsonValue* value, StoreTuning& tuning) {
    if (value == nullptr || value->type != JsonType::Object) return;
    for (std::size_t index = 0; index < static_cast<std::size_t>(ClerkPersonality::Count); ++index) {
        const auto personality = static_cast<ClerkPersonality>(index);
        const auto weight = numberField(*value, clerkPersonalityName(personality));
        if (weight && *weight >= 0.0) tuning.personalityWeights[index] = static_cast<float>(*weight);
    }
}

} // namespace

bool StoreVolume::valid() const noexcept {
    return std::isfinite(min.x) && std::isfinite(min.y) && std::isfinite(min.z)
        && std::isfinite(max.x) && std::isfinite(max.y) && std::isfinite(max.z)
        && min.x < max.x && min.y < max.y && min.z < max.z;
}

bool StoreVolume::contains(const platform::Vec3& point) const noexcept {
    return valid() && point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y && point.z >= min.z && point.z <= max.z;
}

platform::Vec3 StoreVolume::center() const noexcept {
    return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f};
}

bool PrototypeStoreTarget::productionReady(std::string* reason) const {
    const auto reject = [reason](const std::string& message) { if (reason) *reason = message; return false; };
    if (!enabled) return reject("prototype target is disabled");
    if (validation != TargetValidationStatus::VerifiedInGame) return reject("prototype target validation.status is not VERIFIED_IN_GAME");
    if (legacyBuild.empty() || enhancedBuild.empty()) return reject("both Legacy and Enhanced validation build IDs must be recorded");
    if (!hasBusinessVolume || !businessVolume.valid()) return reject("business volume is unset/invalid");
    if (!hasClerkAnchor) return reject("clerk anchor is unset");
    if (!businessVolume.contains(clerk.position)) return reject("clerk anchor is outside the business volume");
    if (registers.empty()) return reject("at least one register anchor is required");
    for (const auto& registerAnchor : registers) {
        if (!businessVolume.contains(registerAnchor.position)) return reject("a register anchor is outside the business volume");
    }
    if (customerZones.empty()) return reject("at least one validated customer zone is required");
    if (entrances.empty()) return reject("at least one validated entrance anchor is required");
    if (exits.empty()) return reject("at least one validated exit anchor is required");
    if (!std::isfinite(activationRadius) || activationRadius <= 5.0f) return reject("activationRadius is invalid");
    if (!std::isfinite(clerkAcquireRadius) || clerkAcquireRadius <= 0.25f) return reject("clerkAcquireRadius is invalid");
    if (!std::isfinite(threatMaxDistance) || threatMaxDistance <= 1.0f) return reject("threatMaxDistance is invalid");
    if (registerCashMin < 0 || registerCashMax < registerCashMin) return reject("register cash range is invalid");
    if (safeCashMin < 0 || safeCashMax < safeCashMin) return reject("safe cash range is invalid");
    return true;
}

platform::Vec3 PrototypeStoreTarget::activationCenter() const noexcept {
    if (hasBusinessVolume && businessVolume.valid()) return businessVolume.center();
    if (hasClerkAnchor) return clerk.position;
    return {};
}

TargetLoadReport PrototypeStoreTargetLoader::load(const std::filesystem::path& file, PrototypeStoreTarget& outTarget) {
    TargetLoadReport report{};
    std::string text;
    if (!readText(file, text)) { report.detail = "unable to read businesses.json"; return report; }
    JsonValue root; std::string error;
    if (!JsonParser(text).parse(root, error) || root.type != JsonType::Object) { report.detail = "businesses.json parse failed: " + error; return report; }
    const auto* businesses = root.get("businesses");
    if (businesses == nullptr || businesses->type != JsonType::Array) { report.detail = "businesses.json is missing businesses[]"; return report; }

    const JsonValue* selected = nullptr;
    for (const auto& entry : businesses->array) {
        if (entry.type != JsonType::Object) continue;
        const auto id = stringField(entry, "id");
        if (id && *id == "prototype_24_7") { selected = &entry; break; }
    }
    if (!selected) { report.detail = "prototype_24_7 target is missing"; return report; }

    PrototypeStoreTarget target{};
    if (const auto id = stringField(*selected, "id")) target.id = *id;
    if (const auto name = stringField(*selected, "displayName")) target.displayName = *name;
    if (const auto enabled = boolField(*selected, "enabled")) target.enabled = *enabled;
    if (const auto radius = numberField(*selected, "activationRadius")) target.activationRadius = static_cast<float>(*radius);
    if (const auto radius = numberField(*selected, "clerkAcquireRadius")) target.clerkAcquireRadius = static_cast<float>(*radius);

    if (const auto* validation = selected->get("validation"); validation && validation->type == JsonType::Object) {
        if (const auto status = stringField(*validation, "status")) target.validation = targetValidationStatusFromString(*status).value_or(TargetValidationStatus::Unknown);
        if (const auto notes = stringField(*validation, "notes")) target.validationNotes = *notes;
        if (const auto build = stringField(*validation, "legacyBuild")) target.legacyBuild = *build;
        if (const auto build = stringField(*validation, "enhancedBuild")) target.enhancedBuild = *build;
    }
    if (const auto volume = volumeValue(selected->get("volume"))) { target.businessVolume = *volume; target.hasBusinessVolume = true; }

    if (const auto* threat = selected->get("threat"); threat && threat->type == JsonType::Object) {
        if (const auto value = numberField(*threat, "maxDistance")) target.threatMaxDistance = static_cast<float>(*value);
        if (const auto value = numberField(*threat, "sustainMs")) target.tuning.threatSustainMs = static_cast<std::uint32_t>(std::max(0.0, *value));
        if (const auto value = numberField(*threat, "sessionTimeoutMs")) target.tuning.sessionTimeoutMs = static_cast<std::uint32_t>(std::max(0.0, *value));
        if (const auto value = numberField(*threat, "playerLeaveGraceMs")) target.tuning.playerLeaveGraceMs = static_cast<std::uint32_t>(std::max(0.0, *value));
    }
    if (const auto* clerkPolicy = selected->get("clerkPolicy"); clerkPolicy && clerkPolicy->type == JsonType::Object) {
        if (const auto value = numberField(*clerkPolicy, "replacementDelayMs")) target.tuning.clerkReplacementDelayMs = static_cast<std::uint64_t>(std::max(0.0, *value));
        if (const auto value = numberField(*clerkPolicy, "recoveryMs")) target.tuning.businessRecoveryMs = static_cast<std::uint64_t>(std::max(0.0, *value));
        applyPersonalityWeights(clerkPolicy->get("personalityWeights"), target.tuning);
    }
    if (const auto* cash = selected->get("cashProfile"); cash && cash->type == JsonType::Object) {
        if (const auto value = numberField(*cash, "registerMin")) target.registerCashMin = static_cast<int>(*value);
        if (const auto value = numberField(*cash, "registerMax")) target.registerCashMax = static_cast<int>(*value);
        if (const auto value = numberField(*cash, "safeMin")) target.safeCashMin = static_cast<int>(*value);
        if (const auto value = numberField(*cash, "safeMax")) target.safeCashMax = static_cast<int>(*value);
    }
    if (const auto* anchors = selected->get("anchors"); anchors && anchors->type == JsonType::Object) {
        if (const auto clerk = anchorValue(anchors->get("clerk"))) { target.clerk = *clerk; target.hasClerkAnchor = true; }
        target.registers = anchorArray(anchors->get("registers"));
        target.safe = anchorValue(anchors->get("safe"));
        target.customerZones = volumeArray(anchors->get("customerZones"));
        target.entrances = anchorArray(anchors->get("entrances"));
        target.exits = anchorArray(anchors->get("exits"));
    }

    report.parsed = true;
    std::string readyReason;
    report.productionReady = target.productionReady(&readyReason);
    report.detail = report.productionReady ? "prototype_24_7 target parsed and VERIFIED_IN_GAME-ready" : "prototype_24_7 parsed but gated: " + readyReason;
    outTarget = std::move(target);
    return report;
}

std::string_view targetValidationStatusName(const TargetValidationStatus value) noexcept {
    switch (value) {
    case TargetValidationStatus::VerifiedInGame: return "VERIFIED_IN_GAME";
    case TargetValidationStatus::VerifiedData: return "VERIFIED_DATA";
    case TargetValidationStatus::ReferenceOnly: return "REFERENCE_ONLY";
    case TargetValidationStatus::CustomRequired: return "CUSTOM_REQUIRED";
    case TargetValidationStatus::Rejected: return "REJECTED";
    case TargetValidationStatus::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::optional<TargetValidationStatus> targetValidationStatusFromString(const std::string_view value) noexcept {
    if (value == "VERIFIED_IN_GAME") return TargetValidationStatus::VerifiedInGame;
    if (value == "VERIFIED_DATA") return TargetValidationStatus::VerifiedData;
    if (value == "REFERENCE_ONLY") return TargetValidationStatus::ReferenceOnly;
    if (value == "CUSTOM_REQUIRED") return TargetValidationStatus::CustomRequired;
    if (value == "REJECTED") return TargetValidationStatus::Rejected;
    return std::nullopt;
}

} // namespace gco::robbery
