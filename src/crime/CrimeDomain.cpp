#include "CrimeDomain.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace gco::crime {
namespace {

template <typename EnumT, std::size_t N>
std::optional<EnumT> enumFromName(
    const std::string_view value,
    const std::array<std::pair<EnumT, std::string_view>, N>& entries) noexcept {
    for (const auto& [candidate, name] : entries) {
        if (value == name) {
            return candidate;
        }
    }
    return std::nullopt;
}

} // namespace

CrimeSeverity baseSeverity(const CrimeType type) noexcept {
    switch (type) {
    case CrimeType::Robbery: return CrimeSeverity::Moderate;
    case CrimeType::ArmedRobbery: return CrimeSeverity::Serious;
    case CrimeType::Assault: return CrimeSeverity::Moderate;
    case CrimeType::Homicide: return CrimeSeverity::Major;
    case CrimeType::OfficerAssault: return CrimeSeverity::Major;
    case CrimeType::VehicleTheft: return CrimeSeverity::Moderate;
    case CrimeType::PropertyDamage: return CrimeSeverity::Minor;
    case CrimeType::Count: break;
    }
    return CrimeSeverity::Minor;
}

CrimeSeverity maxSeverity(const CrimeSeverity left, const CrimeSeverity right) noexcept {
    return static_cast<std::uint8_t>(left) >= static_cast<std::uint8_t>(right) ? left : right;
}

bool isMajorCase(const CrimeSeverity severity) noexcept {
    return severity == CrimeSeverity::Major || severity == CrimeSeverity::Critical;
}

bool canTransition(const CaseState from, const CaseState to) noexcept {
    if (from == to) {
        return true;
    }
    if (from == CaseState::Resolved) {
        return false;
    }

    switch (from) {
    case CaseState::Unobserved:
        return to == CaseState::Observed || to == CaseState::Resolved;
    case CaseState::Observed:
        return to == CaseState::Reporting || to == CaseState::Reported
            || to == CaseState::Dormant || to == CaseState::Resolved;
    case CaseState::Reporting:
        return to == CaseState::Reported || to == CaseState::Observed
            || to == CaseState::Dormant || to == CaseState::Resolved;
    case CaseState::Reported:
        return to == CaseState::Investigating || to == CaseState::Dormant
            || to == CaseState::Resolved;
    case CaseState::Investigating:
        return to == CaseState::UnknownSuspect || to == CaseState::IdentifiedSuspect
            || to == CaseState::BoloOrWarrant || to == CaseState::Dormant
            || to == CaseState::Resolved;
    case CaseState::UnknownSuspect:
        return to == CaseState::IdentifiedSuspect || to == CaseState::BoloOrWarrant
            || to == CaseState::PursuitOrSearch || to == CaseState::Dormant
            || to == CaseState::Resolved;
    case CaseState::IdentifiedSuspect:
        return to == CaseState::BoloOrWarrant || to == CaseState::PursuitOrSearch
            || to == CaseState::Dormant || to == CaseState::Resolved;
    case CaseState::BoloOrWarrant:
        return to == CaseState::PursuitOrSearch || to == CaseState::Dormant
            || to == CaseState::Resolved;
    case CaseState::PursuitOrSearch:
        return to == CaseState::BoloOrWarrant || to == CaseState::Dormant
            || to == CaseState::Resolved;
    case CaseState::Dormant:
        return to == CaseState::Investigating || to == CaseState::UnknownSuspect
            || to == CaseState::IdentifiedSuspect || to == CaseState::BoloOrWarrant
            || to == CaseState::PursuitOrSearch || to == CaseState::Resolved;
    case CaseState::Resolved:
        return false;
    }
    return false;
}

bool isTerminal(const CaseState state) noexcept {
    return state == CaseState::Resolved;
}

std::string evidenceFingerprint(const EvidenceRecord& evidence) {
    if (!evidence.dedupKey.empty()) {
        return evidence.dedupKey;
    }

    std::ostringstream out;
    out << evidenceSourceName(evidence.source) << '|'
        << evidenceKindName(evidence.kind) << '|'
        << evidence.observedAtMs << '|'
        << evidence.independenceKey << '|'
        << evidence.snapshot.descriptor << '|'
        << std::fixed << std::setprecision(3)
        << evidence.snapshot.location.x << ','
        << evidence.snapshot.location.y << ','
        << evidence.snapshot.location.z << '|';
    if (evidence.snapshot.sourceLogicalId.has_value()) {
        out << *evidence.snapshot.sourceLogicalId;
    }
    out << '|';
    if (evidence.snapshot.relatedVehicleId.has_value()) {
        out << *evidence.snapshot.relatedVehicleId;
    }
    return out.str();
}

std::string_view crimeTypeName(const CrimeType value) noexcept {
    switch (value) {
    case CrimeType::Robbery: return "robbery";
    case CrimeType::ArmedRobbery: return "armed_robbery";
    case CrimeType::Assault: return "assault";
    case CrimeType::Homicide: return "homicide";
    case CrimeType::OfficerAssault: return "officer_assault";
    case CrimeType::VehicleTheft: return "vehicle_theft";
    case CrimeType::PropertyDamage: return "property_damage";
    case CrimeType::Count: break;
    }
    return "unknown";
}

std::string_view crimeSeverityName(const CrimeSeverity value) noexcept {
    switch (value) {
    case CrimeSeverity::Minor: return "minor";
    case CrimeSeverity::Moderate: return "moderate";
    case CrimeSeverity::Serious: return "serious";
    case CrimeSeverity::Major: return "major";
    case CrimeSeverity::Critical: return "critical";
    }
    return "minor";
}

std::string_view caseStateName(const CaseState value) noexcept {
    switch (value) {
    case CaseState::Unobserved: return "unobserved";
    case CaseState::Observed: return "observed";
    case CaseState::Reporting: return "reporting";
    case CaseState::Reported: return "reported";
    case CaseState::Investigating: return "investigating";
    case CaseState::UnknownSuspect: return "unknown_suspect";
    case CaseState::IdentifiedSuspect: return "identified_suspect";
    case CaseState::BoloOrWarrant: return "bolo_or_warrant";
    case CaseState::PursuitOrSearch: return "pursuit_or_search";
    case CaseState::Dormant: return "dormant";
    case CaseState::Resolved: return "resolved";
    }
    return "unobserved";
}

std::string_view suspectKnowledgeName(const SuspectKnowledge value) noexcept {
    switch (value) {
    case SuspectKnowledge::Unknown: return "unknown";
    case SuspectKnowledge::DescriptionOnly: return "description_only";
    case SuspectKnowledge::ProbableIdentity: return "probable_identity";
    case SuspectKnowledge::Identified: return "identified";
    }
    return "unknown";
}

std::string_view caseResolutionName(const CaseResolution value) noexcept {
    switch (value) {
    case CaseResolution::None: return "none";
    case CaseResolution::ExpiredWeakEvidence: return "expired_weak_evidence";
    case CaseResolution::Arrested: return "arrested";
    case CaseResolution::AdministrativelyResolved: return "administratively_resolved";
    }
    return "none";
}

std::string_view evidenceSourceName(const EvidenceSource value) noexcept {
    switch (value) {
    case EvidenceSource::Witness: return "witness";
    case EvidenceSource::Camera: return "camera";
    case EvidenceSource::Officer: return "officer";
    case EvidenceSource::Alarm: return "alarm";
    case EvidenceSource::Vehicle: return "vehicle";
    case EvidenceSource::SceneDiscovery: return "scene_discovery";
    case EvidenceSource::SyntheticDebug: return "synthetic_debug";
    }
    return "witness";
}

std::string_view evidenceKindName(const EvidenceKind value) noexcept {
    switch (value) {
    case EvidenceKind::CrimeObserved: return "crime_observed";
    case EvidenceKind::Face: return "face";
    case EvidenceKind::Mask: return "mask";
    case EvidenceKind::Clothing: return "clothing";
    case EvidenceKind::Weapon: return "weapon";
    case EvidenceKind::Vehicle: return "vehicle";
    case EvidenceKind::Plate: return "plate";
    case EvidenceKind::Direction: return "direction";
    case EvidenceKind::Location: return "location";
    case EvidenceKind::Identity: return "identity";
    case EvidenceKind::Injury: return "injury";
    case EvidenceKind::Body: return "body";
    case EvidenceKind::PropertyDamage: return "property_damage";
    }
    return "crime_observed";
}

std::optional<CrimeType> crimeTypeFromName(const std::string_view value) noexcept {
    constexpr std::array entries{
        std::pair{CrimeType::Robbery, std::string_view{"robbery"}},
        std::pair{CrimeType::ArmedRobbery, std::string_view{"armed_robbery"}},
        std::pair{CrimeType::Assault, std::string_view{"assault"}},
        std::pair{CrimeType::Homicide, std::string_view{"homicide"}},
        std::pair{CrimeType::OfficerAssault, std::string_view{"officer_assault"}},
        std::pair{CrimeType::VehicleTheft, std::string_view{"vehicle_theft"}},
        std::pair{CrimeType::PropertyDamage, std::string_view{"property_damage"}}
    };
    return enumFromName(value, entries);
}

std::optional<CrimeSeverity> crimeSeverityFromName(const std::string_view value) noexcept {
    constexpr std::array entries{
        std::pair{CrimeSeverity::Minor, std::string_view{"minor"}},
        std::pair{CrimeSeverity::Moderate, std::string_view{"moderate"}},
        std::pair{CrimeSeverity::Serious, std::string_view{"serious"}},
        std::pair{CrimeSeverity::Major, std::string_view{"major"}},
        std::pair{CrimeSeverity::Critical, std::string_view{"critical"}}
    };
    return enumFromName(value, entries);
}

std::optional<CaseState> caseStateFromName(const std::string_view value) noexcept {
    constexpr std::array entries{
        std::pair{CaseState::Unobserved, std::string_view{"unobserved"}},
        std::pair{CaseState::Observed, std::string_view{"observed"}},
        std::pair{CaseState::Reporting, std::string_view{"reporting"}},
        std::pair{CaseState::Reported, std::string_view{"reported"}},
        std::pair{CaseState::Investigating, std::string_view{"investigating"}},
        std::pair{CaseState::UnknownSuspect, std::string_view{"unknown_suspect"}},
        std::pair{CaseState::IdentifiedSuspect, std::string_view{"identified_suspect"}},
        std::pair{CaseState::BoloOrWarrant, std::string_view{"bolo_or_warrant"}},
        std::pair{CaseState::PursuitOrSearch, std::string_view{"pursuit_or_search"}},
        std::pair{CaseState::Dormant, std::string_view{"dormant"}},
        std::pair{CaseState::Resolved, std::string_view{"resolved"}}
    };
    return enumFromName(value, entries);
}

std::optional<SuspectKnowledge> suspectKnowledgeFromName(const std::string_view value) noexcept {
    constexpr std::array entries{
        std::pair{SuspectKnowledge::Unknown, std::string_view{"unknown"}},
        std::pair{SuspectKnowledge::DescriptionOnly, std::string_view{"description_only"}},
        std::pair{SuspectKnowledge::ProbableIdentity, std::string_view{"probable_identity"}},
        std::pair{SuspectKnowledge::Identified, std::string_view{"identified"}}
    };
    return enumFromName(value, entries);
}

std::optional<CaseResolution> caseResolutionFromName(const std::string_view value) noexcept {
    constexpr std::array entries{
        std::pair{CaseResolution::None, std::string_view{"none"}},
        std::pair{CaseResolution::ExpiredWeakEvidence, std::string_view{"expired_weak_evidence"}},
        std::pair{CaseResolution::Arrested, std::string_view{"arrested"}},
        std::pair{CaseResolution::AdministrativelyResolved, std::string_view{"administratively_resolved"}}
    };
    return enumFromName(value, entries);
}

std::optional<EvidenceSource> evidenceSourceFromName(const std::string_view value) noexcept {
    constexpr std::array entries{
        std::pair{EvidenceSource::Witness, std::string_view{"witness"}},
        std::pair{EvidenceSource::Camera, std::string_view{"camera"}},
        std::pair{EvidenceSource::Officer, std::string_view{"officer"}},
        std::pair{EvidenceSource::Alarm, std::string_view{"alarm"}},
        std::pair{EvidenceSource::Vehicle, std::string_view{"vehicle"}},
        std::pair{EvidenceSource::SceneDiscovery, std::string_view{"scene_discovery"}},
        std::pair{EvidenceSource::SyntheticDebug, std::string_view{"synthetic_debug"}}
    };
    return enumFromName(value, entries);
}

std::optional<EvidenceKind> evidenceKindFromName(const std::string_view value) noexcept {
    constexpr std::array entries{
        std::pair{EvidenceKind::CrimeObserved, std::string_view{"crime_observed"}},
        std::pair{EvidenceKind::Face, std::string_view{"face"}},
        std::pair{EvidenceKind::Mask, std::string_view{"mask"}},
        std::pair{EvidenceKind::Clothing, std::string_view{"clothing"}},
        std::pair{EvidenceKind::Weapon, std::string_view{"weapon"}},
        std::pair{EvidenceKind::Vehicle, std::string_view{"vehicle"}},
        std::pair{EvidenceKind::Plate, std::string_view{"plate"}},
        std::pair{EvidenceKind::Direction, std::string_view{"direction"}},
        std::pair{EvidenceKind::Location, std::string_view{"location"}},
        std::pair{EvidenceKind::Identity, std::string_view{"identity"}},
        std::pair{EvidenceKind::Injury, std::string_view{"injury"}},
        std::pair{EvidenceKind::Body, std::string_view{"body"}},
        std::pair{EvidenceKind::PropertyDamage, std::string_view{"property_damage"}}
    };
    return enumFromName(value, entries);
}

} // namespace gco::crime
