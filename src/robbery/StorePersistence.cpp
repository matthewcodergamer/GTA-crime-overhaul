#include "StorePersistence.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string_view>
#include <vector>

namespace gco::robbery {
namespace {

bool readText(const std::filesystem::path& file, std::string& out) {
    std::ifstream input(file, std::ios::binary);
    if (!input.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
    return input.good() || input.eof();
}

std::string escapeJson(const std::string_view value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << ch; break;
        }
    }
    return out.str();
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

std::vector<std::string> splitArrayEntries(const std::string_view body) {
    std::vector<std::string> result;
    std::size_t start = 0;
    int objectDepth = 0;
    int arrayDepth = 0;
    bool inString = false;
    bool escaped = false;

    auto pushSegment = [&](const std::size_t end) {
        std::size_t first = start;
        while (first < end && std::isspace(static_cast<unsigned char>(body[first]))) ++first;
        std::size_t last = end;
        while (last > first && std::isspace(static_cast<unsigned char>(body[last - 1]))) --last;
        if (last > first) result.emplace_back(body.substr(first, last - first));
    };

    for (std::size_t i = 0; i < body.size(); ++i) {
        const char ch = body[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') { inString = true; continue; }
        if (ch == '{') ++objectDepth;
        else if (ch == '}') --objectDepth;
        else if (ch == '[') ++arrayDepth;
        else if (ch == ']') --arrayDepth;
        else if (ch == ',' && objectDepth == 0 && arrayDepth == 0) {
            pushSegment(i);
            start = i + 1;
        }
    }
    pushSegment(body.size());
    return result;
}

std::optional<std::uint64_t> extractUInt64(const std::string& object, const std::string_view key) {
    const std::string quoted = "\"" + std::string(key) + "\"";
    const auto keyPos = object.find(quoted);
    if (keyPos == std::string::npos) return std::nullopt;
    const auto colon = object.find(':', keyPos + quoted.size());
    if (colon == std::string::npos) return std::nullopt;
    const char* begin = object.data() + colon + 1;
    const char* end = object.data() + object.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(*begin))) ++begin;
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc{}) return std::nullopt;
    return value;
}

std::optional<double> extractNumber(const std::string& object, const std::string_view key) {
    const std::string quoted = "\"" + std::string(key) + "\"";
    const auto keyPos = object.find(quoted);
    if (keyPos == std::string::npos) return std::nullopt;
    const auto colon = object.find(':', keyPos + quoted.size());
    if (colon == std::string::npos) return std::nullopt;
    std::size_t begin = colon + 1;
    while (begin < object.size() && std::isspace(static_cast<unsigned char>(object[begin]))) ++begin;
    std::size_t end = begin;
    while (end < object.size()) {
        const char ch = object[end];
        if (!(std::isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E')) break;
        ++end;
    }
    if (begin == end) return std::nullopt;
    try { return std::stod(object.substr(begin, end - begin)); }
    catch (...) { return std::nullopt; }
}

std::optional<bool> extractBool(const std::string& object, const std::string_view key) {
    const std::string quoted = "\"" + std::string(key) + "\"";
    const auto keyPos = object.find(quoted);
    if (keyPos == std::string::npos) return std::nullopt;
    const auto colon = object.find(':', keyPos + quoted.size());
    if (colon == std::string::npos) return std::nullopt;
    const auto truePos = object.find("true", colon + 1);
    const auto falsePos = object.find("false", colon + 1);
    const auto comma = object.find_first_of(",}", colon + 1);
    if (truePos != std::string::npos && (comma == std::string::npos || truePos < comma)) return true;
    if (falsePos != std::string::npos && (comma == std::string::npos || falsePos < comma)) return false;
    return std::nullopt;
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
            switch (ch) {
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            default: result.push_back(ch); break;
            }
            escaped = false;
        } else if (ch == '\\') escaped = true;
        else if (ch == '"') return result;
        else result.push_back(ch);
    }
    return std::nullopt;
}

float rememberedFace(const identity::RecognitionMemory& memory, const identity::CharacterIdentity identityValue) {
    const auto index = static_cast<std::size_t>(identityValue);
    return index < memory.faceConfidence.size() ? memory.faceConfidence[index] : 0.0f;
}

void setRememberedFace(identity::RecognitionMemory& memory, const identity::CharacterIdentity identityValue, const float value) {
    const auto index = static_cast<std::size_t>(identityValue);
    if (index < memory.faceConfidence.size()) memory.faceConfidence[index] = value;
}

std::string serializeState(const StorePersistentState& state) {
    const auto& clerk = state.clerk;
    std::ostringstream out;
    out << std::fixed << std::setprecision(4)
        << "{\n"
        << "      \"modelVersion\": " << StorePersistentState::ModelVersion << ",\n"
        << "      \"kind\": \"prototype_store_state\",\n"
        << "      \"targetKey\": \"" << escapeJson(state.targetKey) << "\",\n"
        << "      \"businessId\": " << state.businessId << ",\n"
        << "      \"robberyCount\": " << state.robberyCount << ",\n"
        << "      \"lastRobberyAtMs\": " << state.lastRobberyAtMs << ",\n"
        << "      \"lastCaseId\": " << state.lastCaseId << ",\n"
        << "      \"totalCashExposed\": " << state.totalCashExposed << ",\n"
        << "      \"clerkVacant\": " << (state.clerkVacant ? "true" : "false") << ",\n"
        << "      \"replacementEligibleAtMs\": " << state.replacementEligibleAtMs << ",\n"
        << "      \"recoveryUntilMs\": " << state.recoveryUntilMs << ",\n"
        << "      \"clerkId\": " << clerk.id << ",\n"
        << "      \"clerkPersonality\": \"" << clerkPersonalityName(clerk.personality) << "\",\n"
        << "      \"clerkAlive\": " << (clerk.alive ? "true" : "false") << ",\n"
        << "      \"clerkGeneration\": " << clerk.generation << ",\n"
        << "      \"clerkCreatedAtMs\": " << clerk.createdAtMs << ",\n"
        << "      \"clerkDiedAtMs\": " << clerk.diedAtMs << ",\n"
        << "      \"clerkFear\": " << clerk.fear << ",\n"
        << "      \"clerkCompliance\": " << clerk.compliance << ",\n"
        << "      \"clerkAlarmTendency\": " << clerk.alarmTendency << ",\n"
        << "      \"clerkResistance\": " << clerk.resistance << ",\n"
        << "      \"clerkWithholdingTendency\": " << clerk.withholdingTendency << ",\n"
        << "      \"clerkFaceMichael\": " << rememberedFace(clerk.recognition, identity::CharacterIdentity::Michael) << ",\n"
        << "      \"clerkFaceFranklin\": " << rememberedFace(clerk.recognition, identity::CharacterIdentity::Franklin) << ",\n"
        << "      \"clerkFaceTrevor\": " << rememberedFace(clerk.recognition, identity::CharacterIdentity::Trevor) << ",\n"
        << "      \"clerkFaceFreemodeMale\": " << rememberedFace(clerk.recognition, identity::CharacterIdentity::FreemodeMale) << ",\n"
        << "      \"clerkFaceFreemodeFemale\": " << rememberedFace(clerk.recognition, identity::CharacterIdentity::FreemodeFemale) << ",\n"
        << "      \"clerkLastOutfitKey\": \"" << escapeJson(clerk.recognition.lastOutfitKey) << "\",\n"
        << "      \"clerkClothingConfidence\": " << clerk.recognition.clothingConfidence << ",\n"
        << "      \"clerkLastOutfitSeenAtMs\": " << clerk.recognition.lastOutfitSeenAtMs << ",\n"
        << "      \"clerkRecognitionCount\": " << clerk.recognition.recognitionCount << "\n"
        << "    }";
    return out.str();
}

bool decodeState(const std::string& object, StorePersistentState& state, std::string* reason) {
    const auto version = extractUInt64(object, "modelVersion");
    const auto kind = extractString(object, "kind");
    const auto targetKey = extractString(object, "targetKey");
    const auto businessId = extractUInt64(object, "businessId");
    const auto clerkId = extractUInt64(object, "clerkId");
    const auto personalityName = extractString(object, "clerkPersonality");
    const auto personality = personalityName ? clerkPersonalityFromString(*personalityName) : std::nullopt;
    if (!version || (*version != 1 && *version != StorePersistentState::ModelVersion)
        || !kind || *kind != "prototype_store_state"
        || !targetKey || *targetKey != "prototype_24_7"
        || !businessId || logicalIdDomain(*businessId) != LogicalIdDomain::Business
        || !clerkId || (*clerkId != 0 && logicalIdDomain(*clerkId) != LogicalIdDomain::Clerk)
        || !personality) {
        if (reason) *reason = "prototype store persistence identity/schema fields are invalid";
        return false;
    }

    StorePersistentState loaded{};
    loaded.targetKey = *targetKey;
    loaded.businessId = *businessId;
    loaded.clerk.id = *clerkId;
    loaded.clerk.personality = *personality;

    const auto robberyCount = extractUInt64(object, "robberyCount");
    const auto lastRobbery = extractUInt64(object, "lastRobberyAtMs");
    const auto lastCase = extractUInt64(object, "lastCaseId");
    const auto totalCash = extractUInt64(object, "totalCashExposed");
    const auto vacant = extractBool(object, "clerkVacant");
    const auto replacement = extractUInt64(object, "replacementEligibleAtMs");
    const auto recovery = extractUInt64(object, "recoveryUntilMs");
    const auto alive = extractBool(object, "clerkAlive");
    const auto generation = extractUInt64(object, "clerkGeneration");
    const auto created = extractUInt64(object, "clerkCreatedAtMs");
    const auto died = extractUInt64(object, "clerkDiedAtMs");
    const auto fear = extractNumber(object, "clerkFear");
    const auto compliance = extractNumber(object, "clerkCompliance");
    const auto alarm = extractNumber(object, "clerkAlarmTendency");
    const auto resistance = extractNumber(object, "clerkResistance");
    const auto withholding = extractNumber(object, "clerkWithholdingTendency");

    if (!robberyCount || *robberyCount > 1000000ull
        || !lastRobbery || !lastCase || !totalCash || !vacant || !replacement || !recovery
        || !alive || !generation || *generation > 1000000ull || !created || !died
        || !fear || !compliance || !alarm || !resistance || !withholding) {
        if (reason) *reason = "prototype store persistence fields are missing or invalid";
        return false;
    }

    loaded.robberyCount = static_cast<std::uint32_t>(*robberyCount);
    loaded.lastRobberyAtMs = *lastRobbery;
    loaded.lastCaseId = *lastCase;
    loaded.totalCashExposed = static_cast<std::int64_t>(*totalCash);
    loaded.clerkVacant = *vacant;
    loaded.replacementEligibleAtMs = *replacement;
    loaded.recoveryUntilMs = *recovery;
    loaded.clerk.alive = *alive;
    loaded.clerk.generation = static_cast<std::uint32_t>(*generation);
    loaded.clerk.createdAtMs = *created;
    loaded.clerk.diedAtMs = *died;
    loaded.clerk.fear = static_cast<float>(*fear);
    loaded.clerk.compliance = static_cast<float>(*compliance);
    loaded.clerk.alarmTendency = static_cast<float>(*alarm);
    loaded.clerk.resistance = static_cast<float>(*resistance);
    loaded.clerk.withholdingTendency = static_cast<float>(*withholding);

    if (*version >= 2) {
        const auto faceMichael = extractNumber(object, "clerkFaceMichael");
        const auto faceFranklin = extractNumber(object, "clerkFaceFranklin");
        const auto faceTrevor = extractNumber(object, "clerkFaceTrevor");
        const auto faceFreemodeMale = extractNumber(object, "clerkFaceFreemodeMale");
        const auto faceFreemodeFemale = extractNumber(object, "clerkFaceFreemodeFemale");
        const auto outfitKey = extractString(object, "clerkLastOutfitKey");
        const auto clothing = extractNumber(object, "clerkClothingConfidence");
        const auto outfitSeen = extractUInt64(object, "clerkLastOutfitSeenAtMs");
        const auto recognitionCount = extractUInt64(object, "clerkRecognitionCount");
        if (!faceMichael || !faceFranklin || !faceTrevor || !faceFreemodeMale || !faceFreemodeFemale
            || !outfitKey || !clothing || !outfitSeen || !recognitionCount || *recognitionCount > 1000000ull) {
            if (reason) *reason = "prototype store v2 recognition fields are missing or invalid";
            return false;
        }
        setRememberedFace(loaded.clerk.recognition, identity::CharacterIdentity::Michael, static_cast<float>(*faceMichael));
        setRememberedFace(loaded.clerk.recognition, identity::CharacterIdentity::Franklin, static_cast<float>(*faceFranklin));
        setRememberedFace(loaded.clerk.recognition, identity::CharacterIdentity::Trevor, static_cast<float>(*faceTrevor));
        setRememberedFace(loaded.clerk.recognition, identity::CharacterIdentity::FreemodeMale, static_cast<float>(*faceFreemodeMale));
        setRememberedFace(loaded.clerk.recognition, identity::CharacterIdentity::FreemodeFemale, static_cast<float>(*faceFreemodeFemale));
        loaded.clerk.recognition.lastOutfitKey = *outfitKey;
        loaded.clerk.recognition.clothingConfidence = static_cast<float>(*clothing);
        loaded.clerk.recognition.lastOutfitSeenAtMs = *outfitSeen;
        loaded.clerk.recognition.recognitionCount = static_cast<std::uint32_t>(*recognitionCount);
    }

    state = std::move(loaded);
    return true;
}

bool replaceCounter(std::string& document, const std::string_view key, const std::uint64_t value) {
    std::size_t objectOpen = 0;
    std::size_t objectClose = 0;
    if (!findDelimitedValue(document, "nextIds", '{', '}', objectOpen, objectClose)) return false;
    const std::string quoted = "\"" + std::string(key) + "\"";
    const auto keyPos = document.find(quoted, objectOpen);
    if (keyPos == std::string::npos || keyPos > objectClose) return false;
    const auto colon = document.find(':', keyPos + quoted.size());
    if (colon == std::string::npos || colon > objectClose) return false;
    std::size_t begin = colon + 1;
    while (begin < document.size() && std::isspace(static_cast<unsigned char>(document[begin]))) ++begin;
    std::size_t end = begin;
    while (end < document.size() && std::isdigit(static_cast<unsigned char>(document[end]))) ++end;
    if (begin == end) return false;
    document.replace(begin, end - begin, std::to_string(value));
    return true;
}

bool patchBusinesses(std::string& document, const StorePersistentState& state, std::string* reason) {
    std::size_t open = 0;
    std::size_t close = 0;
    if (!findDelimitedValue(document, "businesses", '[', ']', open, close)) {
        if (reason) *reason = "world save businesses section is missing/invalid";
        return false;
    }

    const auto entries = splitArrayEntries(std::string_view(document).substr(open + 1, close - open - 1));
    std::vector<std::string> preserved;
    for (const auto& entry : entries) {
        if (entry.find("\"kind\"") != std::string::npos
            && entry.find("prototype_store_state") != std::string::npos
            && entry.find("prototype_24_7") != std::string::npos) {
            continue;
        }
        preserved.push_back(entry);
    }
    preserved.push_back(serializeState(state));

    std::ostringstream replacement;
    replacement << "[\n";
    for (std::size_t i = 0; i < preserved.size(); ++i) {
        replacement << "    " << preserved[i];
        if (i + 1 < preserved.size()) replacement << ',';
        replacement << '\n';
    }
    replacement << "  ]";

    document.replace(open, close - open + 1, replacement.str());
    return true;
}

std::optional<std::string> findPrototypeObject(const std::string& document) {
    std::size_t open = 0;
    std::size_t close = 0;
    if (!findDelimitedValue(document, "businesses", '[', ']', open, close)) return std::nullopt;
    const auto entries = splitArrayEntries(std::string_view(document).substr(open + 1, close - open - 1));
    for (const auto& entry : entries) {
        if (entry.find("prototype_store_state") != std::string::npos
            && entry.find("prototype_24_7") != std::string::npos) {
            return entry;
        }
    }
    return std::nullopt;
}

} // namespace

bool PrototypeStorePersistence::load(
    PrototypeStoreModel& model,
    LogicalIdGenerator& ids,
    const std::uint64_t nowMs,
    const StoreTuning& tuning,
    std::string* reason) const {

    std::string document;
    if (!readText(paths_.worldSave, document)) {
        if (reason) *reason = "unable to read world save for prototype store";
        return false;
    }

    const auto object = findPrototypeObject(document);
    if (!object) {
        model.initializePersistent(ids, nowMs, tuning);
        if (model.persistent().businessId == 0 || model.persistent().clerk.id == 0) {
            if (reason) *reason = "unable to allocate initial prototype business/clerk IDs";
            return false;
        }
        return true;
    }

    StorePersistentState state{};
    if (!decodeState(*object, state, reason)) return false;
    model.restorePersistent(state);

    const auto nextBusiness = std::max<std::uint64_t>(
        ids.nextSequence(LogicalIdDomain::Business),
        logicalIdSequence(state.businessId) + 1);
    std::uint64_t nextClerk = ids.nextSequence(LogicalIdDomain::Clerk);
    if (state.clerk.id != 0) nextClerk = std::max<std::uint64_t>(nextClerk, logicalIdSequence(state.clerk.id) + 1);
    if (!ids.setNextSequence(LogicalIdDomain::Business, nextBusiness)
        || !ids.setNextSequence(LogicalIdDomain::Clerk, nextClerk)) {
        if (reason) *reason = "unable to restore prototype store logical ID counters";
        return false;
    }
    return true;
}

bool PrototypeStorePersistence::save(
    const PrototypeStoreModel& model,
    const LogicalIdGenerator& ids,
    std::string* reason) const {

    const auto& state = model.persistent();
    if (state.businessId == 0 || logicalIdDomain(state.businessId) != LogicalIdDomain::Business) {
        if (reason) *reason = "prototype store business ID is invalid";
        return false;
    }
    if (state.clerk.id != 0 && logicalIdDomain(state.clerk.id) != LogicalIdDomain::Clerk) {
        if (reason) *reason = "prototype store clerk ID is invalid";
        return false;
    }

    std::string document;
    if (!readText(paths_.worldSave, document)) {
        if (reason) *reason = "unable to read world save before prototype store write";
        return false;
    }
    if (!patchBusinesses(document, state, reason)) return false;
    if (!replaceCounter(document, "business", ids.nextSequence(LogicalIdDomain::Business))
        || !replaceCounter(document, "clerk", ids.nextSequence(LogicalIdDomain::Clerk))) {
        if (reason) *reason = "unable to update business/clerk nextIds while saving prototype store";
        return false;
    }

    if (!worldState_.writeWorldAtomically(document)) {
        if (reason) *reason = "atomic world write failed while persisting prototype store";
        return false;
    }
    return true;
}

} // namespace gco::robbery
