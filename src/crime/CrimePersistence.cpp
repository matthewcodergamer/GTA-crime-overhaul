#include "CrimePersistence.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <string_view>
#include <utility>

namespace gco::crime {
namespace {

enum class JsonType : std::uint8_t { Null, Bool, Number, String, Array, Object };

struct JsonValue final {
    JsonType type = JsonType::Null;
    bool boolean = false;
    std::string text;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;

    static JsonValue null() { return {}; }
    static JsonValue booleanValue(const bool value) {
        JsonValue out; out.type = JsonType::Bool; out.boolean = value; return out;
    }
    static JsonValue number(std::string value) {
        JsonValue out; out.type = JsonType::Number; out.text = std::move(value); return out;
    }
    static JsonValue string(std::string value) {
        JsonValue out; out.type = JsonType::String; out.text = std::move(value); return out;
    }
    static JsonValue arrayValue() { JsonValue out; out.type = JsonType::Array; return out; }
    static JsonValue objectValue() { JsonValue out; out.type = JsonType::Object; return out; }
};

class JsonParser final {
public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    bool parse(JsonValue& out, std::string* reason) {
        skipWhitespace();
        if (!parseValue(out, reason)) {
            return false;
        }
        skipWhitespace();
        if (position_ != input_.size()) {
            return fail(reason, "unexpected trailing JSON content");
        }
        return true;
    }

private:
    bool parseValue(JsonValue& out, std::string* reason) {
        skipWhitespace();
        if (position_ >= input_.size()) return fail(reason, "unexpected end of JSON");
        const char ch = input_[position_];
        if (ch == '{') return parseObject(out, reason);
        if (ch == '[') return parseArray(out, reason);
        if (ch == '"') {
            std::string value;
            if (!parseString(value, reason)) return false;
            out = JsonValue::string(std::move(value));
            return true;
        }
        if (ch == 't') return parseLiteral("true", JsonValue::booleanValue(true), out, reason);
        if (ch == 'f') return parseLiteral("false", JsonValue::booleanValue(false), out, reason);
        if (ch == 'n') return parseLiteral("null", JsonValue::null(), out, reason);
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) return parseNumber(out, reason);
        return fail(reason, "invalid JSON value");
    }

    bool parseObject(JsonValue& out, std::string* reason) {
        ++position_;
        out = JsonValue::objectValue();
        skipWhitespace();
        if (consume('}')) return true;
        while (position_ < input_.size()) {
            std::string key;
            if (!parseString(key, reason)) return false;
            skipWhitespace();
            if (!consume(':')) return fail(reason, "expected ':' in JSON object");
            JsonValue value;
            if (!parseValue(value, reason)) return false;
            out.object[std::move(key)] = std::move(value);
            skipWhitespace();
            if (consume('}')) return true;
            if (!consume(',')) return fail(reason, "expected ',' in JSON object");
            skipWhitespace();
        }
        return fail(reason, "unterminated JSON object");
    }

    bool parseArray(JsonValue& out, std::string* reason) {
        ++position_;
        out = JsonValue::arrayValue();
        skipWhitespace();
        if (consume(']')) return true;
        while (position_ < input_.size()) {
            JsonValue value;
            if (!parseValue(value, reason)) return false;
            out.array.push_back(std::move(value));
            skipWhitespace();
            if (consume(']')) return true;
            if (!consume(',')) return fail(reason, "expected ',' in JSON array");
            skipWhitespace();
        }
        return fail(reason, "unterminated JSON array");
    }

    bool parseString(std::string& out, std::string* reason) {
        skipWhitespace();
        if (!consume('"')) return fail(reason, "expected JSON string");
        out.clear();
        while (position_ < input_.size()) {
            const char ch = input_[position_++];
            if (ch == '"') return true;
            if (static_cast<unsigned char>(ch) < 0x20) return fail(reason, "control character in JSON string");
            if (ch != '\\') {
                out.push_back(ch);
                continue;
            }
            if (position_ >= input_.size()) return fail(reason, "unterminated JSON escape");
            const char escaped = input_[position_++];
            switch (escaped) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                if (position_ + 4 > input_.size()) return fail(reason, "short unicode JSON escape");
                unsigned value = 0;
                for (int i = 0; i < 4; ++i) {
                    const char hex = input_[position_++];
                    value <<= 4U;
                    if (hex >= '0' && hex <= '9') value |= static_cast<unsigned>(hex - '0');
                    else if (hex >= 'a' && hex <= 'f') value |= static_cast<unsigned>(hex - 'a' + 10);
                    else if (hex >= 'A' && hex <= 'F') value |= static_cast<unsigned>(hex - 'A' + 10);
                    else return fail(reason, "invalid unicode JSON escape");
                }
                if (value <= 0x7FU) out.push_back(static_cast<char>(value));
                else if (value <= 0x7FFU) {
                    out.push_back(static_cast<char>(0xC0U | (value >> 6U)));
                    out.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
                } else {
                    out.push_back(static_cast<char>(0xE0U | (value >> 12U)));
                    out.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
                    out.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
                }
                break;
            }
            default: return fail(reason, "unsupported JSON escape");
            }
        }
        return fail(reason, "unterminated JSON string");
    }

    bool parseNumber(JsonValue& out, std::string* reason) {
        const std::size_t start = position_;
        if (input_[position_] == '-') ++position_;
        if (position_ >= input_.size()) return fail(reason, "invalid JSON number");
        if (input_[position_] == '0') {
            ++position_;
        } else {
            if (!std::isdigit(static_cast<unsigned char>(input_[position_]))) return fail(reason, "invalid JSON number");
            while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) ++position_;
        }
        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_;
            if (position_ >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                return fail(reason, "invalid JSON fraction");
            }
            while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) ++position_;
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) ++position_;
            if (position_ >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                return fail(reason, "invalid JSON exponent");
            }
            while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) ++position_;
        }
        out = JsonValue::number(std::string(input_.substr(start, position_ - start)));
        return true;
    }

    bool parseLiteral(
        const std::string_view literal,
        JsonValue value,
        JsonValue& out,
        std::string* reason) {
        if (input_.substr(position_, literal.size()) != literal) return fail(reason, "invalid JSON literal");
        position_ += literal.size();
        out = std::move(value);
        return true;
    }

    bool consume(const char expected) {
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    void skipWhitespace() {
        while (position_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[position_]))) ++position_;
    }

    static bool fail(std::string* reason, const std::string& message) {
        if (reason != nullptr) *reason = message;
        return false;
    }

    std::string_view input_;
    std::size_t position_ = 0;
};

std::string escapeJson(const std::string_view value) {
    std::ostringstream out;
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20U) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(ch)
                    << std::dec << std::setfill(' ');
            } else {
                out << static_cast<char>(ch);
            }
            break;
        }
    }
    return out.str();
}

void writeJson(const JsonValue& value, std::ostringstream& out, const int indent) {
    const auto pad = [&out](const int count) { for (int i = 0; i < count; ++i) out.put(' '); };
    switch (value.type) {
    case JsonType::Null: out << "null"; break;
    case JsonType::Bool: out << (value.boolean ? "true" : "false"); break;
    case JsonType::Number: out << value.text; break;
    case JsonType::String: out << '"' << escapeJson(value.text) << '"'; break;
    case JsonType::Array:
        if (value.array.empty()) { out << "[]"; break; }
        out << "[\n";
        for (std::size_t i = 0; i < value.array.size(); ++i) {
            pad(indent + 2);
            writeJson(value.array[i], out, indent + 2);
            if (i + 1 < value.array.size()) out << ',';
            out << '\n';
        }
        pad(indent); out << ']';
        break;
    case JsonType::Object:
        if (value.object.empty()) { out << "{}"; break; }
        out << "{\n";
        for (auto it = value.object.begin(); it != value.object.end(); ++it) {
            pad(indent + 2);
            out << '"' << escapeJson(it->first) << "\": ";
            writeJson(it->second, out, indent + 2);
            if (std::next(it) != value.object.end()) out << ',';
            out << '\n';
        }
        pad(indent); out << '}';
        break;
    }
}

std::string serializeJson(const JsonValue& value) {
    std::ostringstream out;
    writeJson(value, out, 0);
    out << '\n';
    return out.str();
}

const JsonValue* field(const JsonValue& object, const std::string_view key) {
    if (object.type != JsonType::Object) return nullptr;
    const auto it = object.object.find(std::string(key));
    return it == object.object.end() ? nullptr : &it->second;
}

JsonValue* field(JsonValue& object, const std::string_view key) {
    if (object.type != JsonType::Object) return nullptr;
    const auto it = object.object.find(std::string(key));
    return it == object.object.end() ? nullptr : &it->second;
}

bool readText(const std::filesystem::path& path, std::string& out) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return input.good() || input.eof();
}

bool asString(const JsonValue* value, std::string& out) {
    if (value == nullptr || value->type != JsonType::String) return false;
    out = value->text;
    return true;
}

bool asBool(const JsonValue* value, bool& out) {
    if (value == nullptr || value->type != JsonType::Bool) return false;
    out = value->boolean;
    return true;
}

bool asUInt64(const JsonValue* value, std::uint64_t& out) {
    if (value == nullptr || value->type != JsonType::Number || value->text.empty() || value->text.front() == '-') return false;
    const char* begin = value->text.data();
    const char* end = begin + value->text.size();
    const auto parsed = std::from_chars(begin, end, out);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool asUInt32(const JsonValue* value, std::uint32_t& out) {
    std::uint64_t wide = 0;
    if (!asUInt64(value, wide) || wide > std::numeric_limits<std::uint32_t>::max()) return false;
    out = static_cast<std::uint32_t>(wide);
    return true;
}

bool asFloat(const JsonValue* value, float& out) {
    if (value == nullptr || value->type != JsonType::Number) return false;
    char* end = nullptr;
    const double parsed = std::strtod(value->text.c_str(), &end);
    if (end == nullptr || end != value->text.c_str() + value->text.size() || !std::isfinite(parsed)) return false;
    out = static_cast<float>(parsed);
    return std::isfinite(out);
}

JsonValue number(const std::uint64_t value) { return JsonValue::number(std::to_string(value)); }
JsonValue number(const std::uint32_t value) { return JsonValue::number(std::to_string(value)); }
JsonValue number(const std::uint8_t value) { return JsonValue::number(std::to_string(static_cast<unsigned>(value))); }
JsonValue number(const float value) {
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
    return JsonValue::number(out.str());
}

JsonValue encodeLocation(const CrimeLocation& location) {
    JsonValue out = JsonValue::objectValue();
    out.object["x"] = number(location.x);
    out.object["y"] = number(location.y);
    out.object["z"] = number(location.z);
    out.object["zoneTag"] = JsonValue::string(location.zoneTag);
    return out;
}

bool decodeLocation(const JsonValue* value, CrimeLocation& out) {
    if (value == nullptr || value->type != JsonType::Object) return false;
    return asFloat(field(*value, "x"), out.x)
        && asFloat(field(*value, "y"), out.y)
        && asFloat(field(*value, "z"), out.z)
        && asString(field(*value, "zoneTag"), out.zoneTag);
}

JsonValue encodeOptionalId(const std::optional<LogicalId> value) {
    return value.has_value() ? number(*value) : JsonValue::null();
}

bool decodeOptionalId(const JsonValue* value, std::optional<LogicalId>& out) {
    if (value == nullptr) return false;
    if (value->type == JsonType::Null) { out.reset(); return true; }
    std::uint64_t id = 0;
    if (!asUInt64(value, id)) return false;
    out = id;
    return true;
}

JsonValue encodeCrime(const CrimeEvent& event) {
    JsonValue out = JsonValue::objectValue();
    out.object["id"] = number(event.id);
    out.object["caseId"] = number(event.caseId);
    out.object["type"] = JsonValue::string(std::string(crimeTypeName(event.type)));
    out.object["severity"] = JsonValue::string(std::string(crimeSeverityName(event.severity)));
    out.object["location"] = encodeLocation(event.location);
    out.object["businessId"] = encodeOptionalId(event.businessId);
    out.object["occurredAtMs"] = number(event.occurredAtMs);
    return out;
}

bool decodeCrime(const JsonValue& value, CrimeEvent& out, std::string* reason) {
    if (value.type != JsonType::Object) { if (reason) *reason = "crime entry is not an object"; return false; }
    std::string typeName;
    std::string severityName;
    if (!asUInt64(field(value, "id"), out.id)
        || !asUInt64(field(value, "caseId"), out.caseId)
        || !asString(field(value, "type"), typeName)
        || !asString(field(value, "severity"), severityName)
        || !decodeLocation(field(value, "location"), out.location)
        || !decodeOptionalId(field(value, "businessId"), out.businessId)
        || !asUInt64(field(value, "occurredAtMs"), out.occurredAtMs)) {
        if (reason) *reason = "crime entry is missing or has invalid fields";
        return false;
    }
    const auto type = crimeTypeFromName(typeName);
    const auto severity = crimeSeverityFromName(severityName);
    if (!type.has_value() || !severity.has_value()) {
        if (reason) *reason = "crime entry has unknown type/severity";
        return false;
    }
    out.type = *type;
    out.severity = *severity;
    return true;
}

JsonValue encodeEvidence(const EvidenceRecord& evidence) {
    JsonValue out = JsonValue::objectValue();
    out.object["source"] = JsonValue::string(std::string(evidenceSourceName(evidence.source)));
    out.object["kind"] = JsonValue::string(std::string(evidenceKindName(evidence.kind)));
    out.object["confidence"] = number(evidence.confidence);
    out.object["observedAtMs"] = number(evidence.observedAtMs);
    out.object["independenceKey"] = JsonValue::string(evidence.independenceKey);
    out.object["dedupKey"] = JsonValue::string(evidence.dedupKey);

    JsonValue snapshot = JsonValue::objectValue();
    snapshot.object["descriptor"] = JsonValue::string(evidence.snapshot.descriptor);
    snapshot.object["location"] = encodeLocation(evidence.snapshot.location);
    snapshot.object["sourceLogicalId"] = encodeOptionalId(evidence.snapshot.sourceLogicalId);
    snapshot.object["relatedVehicleId"] = encodeOptionalId(evidence.snapshot.relatedVehicleId);
    out.object["snapshot"] = std::move(snapshot);
    return out;
}

bool decodeEvidence(const JsonValue& value, EvidenceRecord& out, std::string* reason) {
    if (value.type != JsonType::Object) { if (reason) *reason = "evidence entry is not an object"; return false; }
    std::string sourceName;
    std::string kindName;
    const JsonValue* snapshot = field(value, "snapshot");
    if (!asString(field(value, "source"), sourceName)
        || !asString(field(value, "kind"), kindName)
        || !asFloat(field(value, "confidence"), out.confidence)
        || !asUInt64(field(value, "observedAtMs"), out.observedAtMs)
        || !asString(field(value, "independenceKey"), out.independenceKey)
        || !asString(field(value, "dedupKey"), out.dedupKey)
        || snapshot == nullptr || snapshot->type != JsonType::Object
        || !asString(field(*snapshot, "descriptor"), out.snapshot.descriptor)
        || !decodeLocation(field(*snapshot, "location"), out.snapshot.location)
        || !decodeOptionalId(field(*snapshot, "sourceLogicalId"), out.snapshot.sourceLogicalId)
        || !decodeOptionalId(field(*snapshot, "relatedVehicleId"), out.snapshot.relatedVehicleId)) {
        if (reason) *reason = "evidence entry is missing or has invalid fields";
        return false;
    }
    const auto source = evidenceSourceFromName(sourceName);
    const auto kind = evidenceKindFromName(kindName);
    if (!source.has_value() || !kind.has_value() || out.confidence < 0.0f || out.confidence > 1.0f) {
        if (reason) *reason = "evidence entry has unknown enum or invalid confidence";
        return false;
    }
    out.source = *source;
    out.kind = *kind;
    return true;
}

JsonValue encodeCase(const CaseFile& file, const CrimeRegistry& registry) {
    JsonValue out = JsonValue::objectValue();
    out.object["modelVersion"] = number(CaseFile::ModelVersion);
    out.object["id"] = number(file.id);
    out.object["incidentKey"] = JsonValue::string(file.incidentKey);
    out.object["state"] = JsonValue::string(std::string(caseStateName(file.state)));
    out.object["severity"] = JsonValue::string(std::string(crimeSeverityName(file.severity)));
    out.object["suspectKnowledge"] = JsonValue::string(std::string(suspectKnowledgeName(file.suspectKnowledge)));
    out.object["identityConfidence"] = number(file.identityConfidence);
    out.object["activePersonWarrant"] = JsonValue::booleanValue(file.activePersonWarrant);
    out.object["activeVehicleBolo"] = JsonValue::booleanValue(file.activeVehicleBolo);
    out.object["createdAtMs"] = number(file.createdAtMs);
    out.object["updatedAtMs"] = number(file.updatedAtMs);
    out.object["lastEvidenceAtMs"] = number(file.lastEvidenceAtMs);
    out.object["resolution"] = JsonValue::string(std::string(caseResolutionName(file.resolution)));

    JsonValue immediate = JsonValue::objectValue();
    immediate.object["active"] = JsonValue::booleanValue(file.immediate.active);
    immediate.object["reportPending"] = JsonValue::booleanValue(file.immediate.reportPending);
    immediate.object["pursuitActive"] = JsonValue::booleanValue(file.immediate.pursuitActive);
    immediate.object["tacticalLevel"] = number(file.immediate.tacticalLevel);
    immediate.object["lastUpdatedAtMs"] = number(file.immediate.lastUpdatedAtMs);
    out.object["immediate"] = std::move(immediate);

    JsonValue crimes = JsonValue::arrayValue();
    for (const CrimeEvent* crime : registry.crimesForCase(file.id)) {
        crimes.array.push_back(encodeCrime(*crime));
    }
    out.object["crimes"] = std::move(crimes);

    JsonValue evidence = JsonValue::arrayValue();
    for (const auto& item : file.evidence) {
        evidence.array.push_back(encodeEvidence(item));
    }
    out.object["evidence"] = std::move(evidence);
    return out;
}

bool decodeCase(
    const JsonValue& value,
    CaseFile& file,
    std::vector<CrimeEvent>& crimes,
    std::string* reason) {

    if (value.type != JsonType::Object) { if (reason) *reason = "case entry is not an object"; return false; }
    std::uint32_t version = 0;
    std::string stateName;
    std::string severityName;
    std::string knowledgeName;
    std::string resolutionName;
    const JsonValue* immediate = field(value, "immediate");
    const JsonValue* crimesValue = field(value, "crimes");
    const JsonValue* evidenceValue = field(value, "evidence");
    if (!asUInt32(field(value, "modelVersion"), version) || version != CaseFile::ModelVersion) {
        if (reason) *reason = "unsupported case modelVersion";
        return false;
    }
    if (!asUInt64(field(value, "id"), file.id)
        || !asString(field(value, "incidentKey"), file.incidentKey)
        || !asString(field(value, "state"), stateName)
        || !asString(field(value, "severity"), severityName)
        || !asString(field(value, "suspectKnowledge"), knowledgeName)
        || !asFloat(field(value, "identityConfidence"), file.identityConfidence)
        || !asBool(field(value, "activePersonWarrant"), file.activePersonWarrant)
        || !asBool(field(value, "activeVehicleBolo"), file.activeVehicleBolo)
        || !asUInt64(field(value, "createdAtMs"), file.createdAtMs)
        || !asUInt64(field(value, "updatedAtMs"), file.updatedAtMs)
        || !asUInt64(field(value, "lastEvidenceAtMs"), file.lastEvidenceAtMs)
        || !asString(field(value, "resolution"), resolutionName)
        || immediate == nullptr || immediate->type != JsonType::Object
        || crimesValue == nullptr || crimesValue->type != JsonType::Array
        || evidenceValue == nullptr || evidenceValue->type != JsonType::Array) {
        if (reason) *reason = "case entry is missing or has invalid fields";
        return false;
    }

    const auto state = caseStateFromName(stateName);
    const auto severity = crimeSeverityFromName(severityName);
    const auto knowledge = suspectKnowledgeFromName(knowledgeName);
    const auto resolution = caseResolutionFromName(resolutionName);
    if (!state.has_value() || !severity.has_value() || !knowledge.has_value() || !resolution.has_value()) {
        if (reason) *reason = "case entry contains unknown enum value";
        return false;
    }
    file.state = *state;
    file.severity = *severity;
    file.suspectKnowledge = *knowledge;
    file.resolution = *resolution;

    std::uint64_t tacticalLevel = 0;
    if (!asBool(field(*immediate, "active"), file.immediate.active)
        || !asBool(field(*immediate, "reportPending"), file.immediate.reportPending)
        || !asBool(field(*immediate, "pursuitActive"), file.immediate.pursuitActive)
        || !asUInt64(field(*immediate, "tacticalLevel"), tacticalLevel)
        || tacticalLevel > std::numeric_limits<std::uint8_t>::max()
        || !asUInt64(field(*immediate, "lastUpdatedAtMs"), file.immediate.lastUpdatedAtMs)) {
        if (reason) *reason = "case immediate response fields are invalid";
        return false;
    }
    file.immediate.tacticalLevel = static_cast<std::uint8_t>(tacticalLevel);

    crimes.clear();
    file.crimeIds.clear();
    for (const auto& item : crimesValue->array) {
        CrimeEvent event{};
        if (!decodeCrime(item, event, reason)) return false;
        if (event.caseId != file.id) { if (reason) *reason = "crime caseId does not match parent case"; return false; }
        file.crimeIds.push_back(event.id);
        crimes.push_back(std::move(event));
    }

    file.evidence.clear();
    for (const auto& item : evidenceValue->array) {
        EvidenceRecord evidence{};
        if (!decodeEvidence(item, evidence, reason)) return false;
        file.evidence.push_back(std::move(evidence));
    }
    return true;
}

bool parseWorld(const RuntimePaths& paths, JsonValue& root, std::string* reason) {
    std::string document;
    if (!readText(paths.worldSave, document)) {
        if (reason) *reason = "unable to read world save";
        return false;
    }
    JsonParser parser(document);
    if (!parser.parse(root, reason) || root.type != JsonType::Object) {
        if (reason && reason->empty()) *reason = "world save root is not an object";
        return false;
    }
    return true;
}

} // namespace

bool CrimePersistenceStore::load(
    CrimeRegistry& registry,
    LogicalIdGenerator& ids,
    std::string* reason) const {

    JsonValue root;
    if (!parseWorld(paths_, root, reason)) return false;
    const JsonValue* cases = field(root, "cases");
    if (cases == nullptr || cases->type != JsonType::Array) {
        if (reason) *reason = "world save cases section is not an array";
        return false;
    }

    CrimeRegistry loaded;
    std::uint64_t maxCaseSequence = 0;
    std::uint64_t maxCrimeSequence = 0;
    for (const auto& entry : cases->array) {
        CaseFile file{};
        std::vector<CrimeEvent> crimes;
        if (!decodeCase(entry, file, crimes, reason)) return false;
        maxCaseSequence = std::max(maxCaseSequence, logicalIdSequence(file.id));
        for (const auto& event : crimes) {
            maxCrimeSequence = std::max(maxCrimeSequence, logicalIdSequence(event.id));
        }
        if (!loaded.importCase(std::move(file), std::move(crimes), reason)) return false;
    }

    std::uint64_t nextCase = std::max<std::uint64_t>(ids.nextSequence(LogicalIdDomain::Case), maxCaseSequence + 1);
    std::uint64_t nextCrime = std::max<std::uint64_t>(ids.nextSequence(LogicalIdDomain::Crime), maxCrimeSequence + 1);
    if (const JsonValue* nextIds = field(root, "nextIds"); nextIds != nullptr && nextIds->type == JsonType::Object) {
        std::uint64_t persisted = 0;
        if (asUInt64(field(*nextIds, "case"), persisted)) nextCase = std::max(nextCase, persisted);
        if (asUInt64(field(*nextIds, "crime"), persisted)) nextCrime = std::max(nextCrime, persisted);
    }

    if (!ids.setNextSequence(LogicalIdDomain::Case, nextCase)
        || !ids.setNextSequence(LogicalIdDomain::Crime, nextCrime)) {
        if (reason) *reason = "unable to restore case/crime logical ID counters";
        return false;
    }

    registry = std::move(loaded);
    return true;
}

bool CrimePersistenceStore::save(
    const CrimeRegistry& registry,
    const LogicalIdGenerator& ids,
    std::string* reason) const {

    JsonValue root;
    if (!parseWorld(paths_, root, reason)) return false;

    JsonValue cases = JsonValue::arrayValue();
    for (const CaseFile* file : registry.cases()) {
        cases.array.push_back(encodeCase(*file, registry));
    }
    root.object["cases"] = std::move(cases);

    JsonValue* nextIds = field(root, "nextIds");
    if (nextIds == nullptr || nextIds->type != JsonType::Object) {
        if (reason) *reason = "world save nextIds section is missing or invalid";
        return false;
    }
    nextIds->object["case"] = number(ids.nextSequence(LogicalIdDomain::Case));
    nextIds->object["crime"] = number(ids.nextSequence(LogicalIdDomain::Crime));

    const std::string document = serializeJson(root);
    if (!worldState_.writeWorldAtomically(document)) {
        if (reason) *reason = "atomic world save write failed while persisting cases";
        return false;
    }
    return true;
}

} // namespace gco::crime
