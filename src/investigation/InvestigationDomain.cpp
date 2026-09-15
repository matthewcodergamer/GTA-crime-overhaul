#include "InvestigationDomain.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <set>
#include <sstream>

namespace gco::investigation {
namespace {

bool containsInsensitive(const std::string_view haystack, const std::string_view needle) {
    if (needle.empty() || haystack.size() < needle.size()) return false;
    for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool equal = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            const auto a = static_cast<unsigned char>(haystack[i + j]);
            const auto b = static_cast<unsigned char>(needle[j]);
            if (std::tolower(a) != std::tolower(b)) {
                equal = false;
                break;
            }
        }
        if (equal) return true;
    }
    return false;
}

std::optional<std::string> descriptorField(const std::string_view descriptor, const std::string_view key) {
    const std::string prefix = std::string(key) + "=";
    const auto start = descriptor.find(prefix);
    if (start == std::string_view::npos) return std::nullopt;
    const auto valueStart = start + prefix.size();
    const auto end = descriptor.find(';', valueStart);
    return std::string(descriptor.substr(
        valueStart,
        end == std::string_view::npos ? descriptor.size() - valueStart : end - valueStart));
}

crime::CrimeLocation offsetLocation(
    const crime::CrimeLocation& base,
    const float x,
    const float y,
    std::string zoneTag) {
    crime::CrimeLocation value = base;
    value.x += x;
    value.y += y;
    value.zoneTag = std::move(zoneTag);
    return value;
}

void pushFact(
    InterviewSourceSummary& summary,
    const InterviewSemanticEvent semantic,
    const crime::EvidenceRecord& evidence) {
    InterviewFact fact{};
    fact.semantic = semantic;
    fact.evidenceKind = evidence.kind;
    fact.confidence = evidence.confidence;
    fact.observedAtMs = evidence.observedAtMs;
    fact.sourceKey = evidence.independenceKey;
    fact.descriptor = evidence.snapshot.descriptor;
    fact.location = evidence.snapshot.location;
    summary.facts.push_back(std::move(fact));
}

void pushNegativeFact(
    InterviewSourceSummary& summary,
    const InterviewSemanticEvent semantic,
    const crime::EvidenceRecord& reportEvidence,
    const crime::EvidenceKind kind,
    std::string descriptor) {
    InterviewFact fact{};
    fact.semantic = semantic;
    fact.evidenceKind = kind;
    fact.confidence = reportEvidence.confidence;
    fact.observedAtMs = reportEvidence.observedAtMs;
    fact.sourceKey = reportEvidence.independenceKey;
    fact.descriptor = std::move(descriptor);
    fact.location = reportEvidence.snapshot.location;
    summary.facts.push_back(std::move(fact));
}

} // namespace

DispatchFacts deriveDispatchFacts(
    const crime::CaseFile& file,
    const std::vector<const crime::CrimeEvent*>& crimes) noexcept {

    DispatchFacts facts{};
    facts.severity = file.severity;
    for (const auto* event : crimes) {
        if (event == nullptr) continue;
        facts.severity = crime::maxSeverity(facts.severity, event->severity);
        switch (event->type) {
        case crime::CrimeType::OfficerAssault:
            facts.officerAttack = true;
            facts.injuries = true;
            break;
        case crime::CrimeType::Homicide:
            facts.death = true;
            break;
        case crime::CrimeType::Assault:
            facts.injuries = true;
            break;
        case crime::CrimeType::ArmedRobbery:
            facts.armed = true;
            break;
        default:
            break;
        }
    }

    for (const auto& evidence : file.evidence) {
        switch (evidence.kind) {
        case crime::EvidenceKind::Weapon:
            if (!containsInsensitive(evidence.snapshot.descriptor, "unarmed")) facts.armed = true;
            break;
        case crime::EvidenceKind::Injury:
            facts.injuries = true;
            break;
        case crime::EvidenceKind::Body:
            facts.death = true;
            break;
        case crime::EvidenceKind::CrimeObserved:
            if (containsInsensitive(evidence.snapshot.descriptor, "gunshot")
                || containsInsensitive(evidence.snapshot.descriptor, "shots_fired")) {
                facts.shotsFired = true;
            }
            break;
        default:
            break;
        }
    }
    return facts;
}

DispatchPlan makeDispatchPlan(
    const crime::CaseFile& file,
    const std::vector<const crime::CrimeEvent*>& crimes,
    SceneGeometry geometry) {

    DispatchPlan plan{};
    plan.caseId = file.id;
    if (!crimes.empty() && crimes.front() != nullptr) {
        plan.businessId = crimes.front()->businessId;
        if (geometry.center.zoneTag.empty()
            && geometry.center.x == 0.0f && geometry.center.y == 0.0f && geometry.center.z == 0.0f) {
            geometry.center = crimes.front()->location;
        }
    }
    plan.geometry = std::move(geometry);

    const auto facts = deriveDispatchFacts(file, crimes);
    const bool reportable = file.state == crime::CaseState::Reported
        || file.state == crime::CaseState::Investigating
        || file.state == crime::CaseState::UnknownSuspect
        || file.state == crime::CaseState::IdentifiedSuspect
        || file.state == crime::CaseState::BoloOrWarrant
        || file.state == crime::CaseState::PursuitOrSearch
        || file.state == crime::CaseState::Dormant;
    if (!reportable || file.resolution != crime::CaseResolution::None) return plan;

    if (facts.severity == crime::CrimeSeverity::Critical || facts.officerAttack
        || (facts.death && facts.shotsFired)) {
        plan.response = PoliceResponseType::Critical;
        plan.officerCount = 6;
        plan.tacticalLevel = 4;
        plan.abstractArrivalDelayMs = 6000;
    } else if (facts.severity == crime::CrimeSeverity::Major || facts.death
        || (facts.shotsFired && facts.injuries)) {
        plan.response = PoliceResponseType::Armed;
        plan.officerCount = 5;
        plan.tacticalLevel = 3;
        plan.abstractArrivalDelayMs = 8000;
    } else if (facts.severity == crime::CrimeSeverity::Serious || facts.armed
        || facts.shotsFired || facts.injuries) {
        plan.response = PoliceResponseType::MultiUnit;
        plan.officerCount = 4;
        plan.tacticalLevel = 2;
        plan.abstractArrivalDelayMs = 10000;
    } else {
        plan.response = PoliceResponseType::Patrol;
        plan.officerCount = 2;
        plan.tacticalLevel = 1;
        plan.abstractArrivalDelayMs = 14000;
    }

    plan.tasks.push_back(SceneTask{
        SceneTaskKind::Approach,
        plan.geometry.center,
        PresentationStyle::Radio,
        "navigate_to_reported_crime_scene"});

    constexpr std::array<std::array<float, 2>, 4> perimeterOffsets{{
        {{9.0f, 0.0f}}, {{-9.0f, 0.0f}}, {{0.0f, 9.0f}}, {{0.0f, -9.0f}}
    }};
    for (std::size_t i = 0; i < std::min<std::size_t>(plan.officerCount, perimeterOffsets.size()); ++i) {
        plan.tasks.push_back(SceneTask{
            SceneTaskKind::Perimeter,
            offsetLocation(plan.geometry.center, perimeterOffsets[i][0], perimeterOffsets[i][1], "scene_perimeter"),
            PresentationStyle::Idle,
            "temporary_scene_perimeter"});
    }

    for (const auto& point : plan.geometry.interiorPoints) {
        plan.tasks.push_back(SceneTask{
            SceneTaskKind::CheckInterior,
            point,
            PresentationStyle::Clipboard,
            "inspect_business_interior"});
    }

    std::set<std::string> pointDedup;
    for (const auto& evidence : file.evidence) {
        SceneTaskKind taskKind = SceneTaskKind::Idle;
        PresentationStyle presentation = PresentationStyle::Clipboard;
        switch (evidence.kind) {
        case crime::EvidenceKind::Body:
        case crime::EvidenceKind::Injury:
            taskKind = SceneTaskKind::CheckBody;
            break;
        case crime::EvidenceKind::Vehicle:
            taskKind = SceneTaskKind::CheckAbandonedVehicle;
            break;
        default:
            continue;
        }
        std::ostringstream key;
        key << static_cast<unsigned>(taskKind) << ':'
            << evidence.snapshot.location.x << ':' << evidence.snapshot.location.y << ':' << evidence.snapshot.location.z;
        if (!pointDedup.insert(key.str()).second) continue;
        plan.tasks.push_back(SceneTask{
            taskKind,
            evidence.snapshot.location,
            presentation,
            evidence.snapshot.descriptor});
    }

    for (const auto& exit : plan.geometry.exitPoints) {
        plan.tasks.push_back(SceneTask{
            SceneTaskKind::CheckExit,
            exit,
            PresentationStyle::Radio,
            "check_known_exit"});
    }

    plan.tasks.push_back(SceneTask{
        SceneTaskKind::RadioUpdate,
        plan.geometry.center,
        PresentationStyle::Radio,
        "scene_status_update"});
    return plan;
}

bool isWitnessEvidence(const crime::EvidenceRecord& evidence) noexcept {
    return evidence.source == crime::EvidenceSource::Witness && !evidence.independenceKey.empty();
}

std::vector<std::string> reportingWitnessSourceKeys(const crime::CaseFile& file) {
    std::vector<std::string> result;
    for (const auto& evidence : file.evidence) {
        if (!isWitnessEvidence(evidence) || evidence.kind != crime::EvidenceKind::CrimeObserved) continue;
        if (std::find(result.begin(), result.end(), evidence.independenceKey) == result.end()) {
            result.push_back(evidence.independenceKey);
        }
    }
    return result;
}

InterviewSourceSummary deriveInterviewSummary(
    const crime::CaseFile& file,
    const std::string_view sourceKey) {

    InterviewSourceSummary summary{};
    summary.sourceKey = std::string(sourceKey);
    if (sourceKey.empty()) return summary;

    const crime::EvidenceRecord* report = nullptr;
    const crime::EvidenceRecord* face = nullptr;
    const crime::EvidenceRecord* mask = nullptr;
    const crime::EvidenceRecord* clothing = nullptr;
    const crime::EvidenceRecord* weapon = nullptr;
    const crime::EvidenceRecord* vehicle = nullptr;
    const crime::EvidenceRecord* plate = nullptr;
    const crime::EvidenceRecord* direction = nullptr;
    const crime::EvidenceRecord* injury = nullptr;
    bool shotsReported = false;

    for (const auto& evidence : file.evidence) {
        if (!isWitnessEvidence(evidence) || evidence.independenceKey != sourceKey) continue;
        switch (evidence.kind) {
        case crime::EvidenceKind::CrimeObserved:
            if (report == nullptr || evidence.confidence > report->confidence) report = &evidence;
            shotsReported = shotsReported || containsInsensitive(evidence.snapshot.descriptor, "gunshot");
            break;
        case crime::EvidenceKind::Face:
        case crime::EvidenceKind::Identity:
            if (face == nullptr || evidence.confidence > face->confidence) face = &evidence;
            break;
        case crime::EvidenceKind::Mask:
            if (mask == nullptr || evidence.confidence > mask->confidence) mask = &evidence;
            break;
        case crime::EvidenceKind::Clothing:
            if (clothing == nullptr || evidence.confidence > clothing->confidence) clothing = &evidence;
            break;
        case crime::EvidenceKind::Weapon:
            if (weapon == nullptr || evidence.confidence > weapon->confidence) weapon = &evidence;
            break;
        case crime::EvidenceKind::Vehicle:
            if (vehicle == nullptr || evidence.confidence > vehicle->confidence) vehicle = &evidence;
            break;
        case crime::EvidenceKind::Plate:
            if (plate == nullptr || evidence.confidence > plate->confidence) plate = &evidence;
            break;
        case crime::EvidenceKind::Direction:
            if (direction == nullptr || evidence.confidence > direction->confidence) direction = &evidence;
            break;
        case crime::EvidenceKind::Injury:
        case crime::EvidenceKind::Body:
            if (injury == nullptr || evidence.confidence > injury->confidence) injury = &evidence;
            break;
        default:
            break;
        }
    }

    if (report == nullptr) return summary;
    summary.hasReport = true;
    auto& dialogueFacts = summary.dialogueFacts;
    dialogueFacts.witnessKind = sourceKey.rfind("clerk:", 0) == 0
        ? dialogue::WitnessKind::Clerk
        : dialogue::WitnessKind::Civilian;
    switch (file.suspectKnowledge) {
    case crime::SuspectKnowledge::Identified:
        dialogueFacts.suspectKnowledge = dialogue::SuspectKnowledge::Identified;
        break;
    case crime::SuspectKnowledge::DescriptionOnly:
    case crime::SuspectKnowledge::ProbableIdentity:
        dialogueFacts.suspectKnowledge = dialogue::SuspectKnowledge::DescriptionOnly;
        break;
    case crime::SuspectKnowledge::Unknown:
        dialogueFacts.suspectKnowledge = dialogue::SuspectKnowledge::Unknown;
        break;
    }

    if (face != nullptr) {
        pushFact(summary, InterviewSemanticEvent::FaceSeen, *face);
        dialogueFacts.faceObserved = true;
        dialogueFacts.faceConfidence = face->confidence;
    } else {
        pushNegativeFact(summary, InterviewSemanticEvent::FaceNotSeen, *report,
            crime::EvidenceKind::CrimeObserved, "interview_face_not_seen");
        dialogueFacts.faceObserved = false;
    }

    if (mask != nullptr) {
        pushFact(summary, InterviewSemanticEvent::MaskSeen, *mask);
        dialogueFacts.faceCovered = true;
    }
    if (clothing != nullptr) {
        pushFact(summary, InterviewSemanticEvent::ClothingSeen, *clothing);
        dialogueFacts.clothingObserved = true;
        dialogueFacts.clothingDescription = clothing->snapshot.descriptor;
    }
    if (weapon != nullptr) {
        pushFact(summary, InterviewSemanticEvent::WeaponSeen, *weapon);
    }
    if (vehicle != nullptr) {
        pushFact(summary, InterviewSemanticEvent::VehicleSeen, *vehicle);
        dialogueFacts.vehicleObserved = true;
        dialogueFacts.vehicleDescription = vehicle->snapshot.descriptor;
    } else {
        pushNegativeFact(summary, InterviewSemanticEvent::VehicleNotSeen, *report,
            crime::EvidenceKind::CrimeObserved, "interview_vehicle_not_seen");
        dialogueFacts.vehicleObserved = false;
    }
    if (plate != nullptr) {
        pushFact(summary, InterviewSemanticEvent::PlateSeen, *plate);
        dialogueFacts.plateKnowledge = dialogue::PlateKnowledge::Full;
        dialogueFacts.plateText = descriptorField(plate->snapshot.descriptor, "plate").value_or(plate->snapshot.descriptor);
    } else {
        pushNegativeFact(summary, InterviewSemanticEvent::PlateNotSeen, *report,
            crime::EvidenceKind::CrimeObserved, "interview_plate_not_seen");
        dialogueFacts.plateKnowledge = dialogue::PlateKnowledge::None;
    }
    if (direction != nullptr) {
        pushFact(summary, InterviewSemanticEvent::DirectionKnown, *direction);
        dialogueFacts.directionObserved = true;
        dialogueFacts.directionDescription = direction->snapshot.descriptor;
    }
    if (shotsReported) {
        pushNegativeFact(summary, InterviewSemanticEvent::ShotsReported, *report,
            crime::EvidenceKind::CrimeObserved, "interview_heard_gunshot");
        dialogueFacts.shotsFired = true;
    }
    if (injury != nullptr) {
        pushFact(summary, InterviewSemanticEvent::InjuryOrBodySeen, *injury);
    }
    return summary;
}

std::string interviewSemanticName(const InterviewSemanticEvent value) {
    switch (value) {
    case InterviewSemanticEvent::FaceSeen: return "face_seen";
    case InterviewSemanticEvent::FaceNotSeen: return "face_not_seen";
    case InterviewSemanticEvent::MaskSeen: return "mask_seen";
    case InterviewSemanticEvent::ClothingSeen: return "clothing_seen";
    case InterviewSemanticEvent::WeaponSeen: return "weapon_seen";
    case InterviewSemanticEvent::VehicleSeen: return "vehicle_seen";
    case InterviewSemanticEvent::VehicleNotSeen: return "vehicle_not_seen";
    case InterviewSemanticEvent::PlateSeen: return "plate_seen";
    case InterviewSemanticEvent::PlateNotSeen: return "plate_not_seen";
    case InterviewSemanticEvent::DirectionKnown: return "direction_known";
    case InterviewSemanticEvent::ShotsReported: return "shots_reported";
    case InterviewSemanticEvent::InjuryOrBodySeen: return "injury_or_body_seen";
    }
    return "unknown";
}

std::string_view policeResponseTypeName(const PoliceResponseType value) noexcept {
    switch (value) {
    case PoliceResponseType::None: return "none";
    case PoliceResponseType::Patrol: return "patrol";
    case PoliceResponseType::MultiUnit: return "multi_unit";
    case PoliceResponseType::Armed: return "armed";
    case PoliceResponseType::Critical: return "critical";
    }
    return "unknown";
}

std::string_view sceneDetailLevelName(const SceneDetailLevel value) noexcept {
    switch (value) {
    case SceneDetailLevel::Abstract: return "abstract";
    case SceneDetailLevel::HighDetail: return "high_detail";
    }
    return "unknown";
}

std::string_view sceneTaskKindName(const SceneTaskKind value) noexcept {
    switch (value) {
    case SceneTaskKind::Approach: return "approach";
    case SceneTaskKind::Perimeter: return "perimeter";
    case SceneTaskKind::CheckInterior: return "check_interior";
    case SceneTaskKind::CheckBody: return "check_body";
    case SceneTaskKind::CheckAbandonedVehicle: return "check_abandoned_vehicle";
    case SceneTaskKind::CheckExit: return "check_exit";
    case SceneTaskKind::InterviewWitness: return "interview_witness";
    case SceneTaskKind::RadioUpdate: return "radio_update";
    case SceneTaskKind::Idle: return "idle";
    }
    return "unknown";
}

} // namespace gco::investigation
