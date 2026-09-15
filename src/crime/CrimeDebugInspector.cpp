#include "CrimeDebugInspector.h"

#include <iomanip>
#include <sstream>

namespace gco::crime {

std::string CrimeDebugInspector::formatRegistry(const CrimeRegistry& registry) {
    std::ostringstream out;
    out << "CrimeRegistry cases=" << registry.caseCount()
        << " crimes=" << registry.crimeCount();

    for (const CaseFile* file : registry.cases()) {
        out << "\n" << formatCase(registry, file->id);
    }
    return out.str();
}

std::string CrimeDebugInspector::formatCase(
    const CrimeRegistry& registry,
    const LogicalId caseId) {

    const CaseFile* file = registry.findCase(caseId);
    if (file == nullptr) {
        return "Case " + std::to_string(caseId) + " <missing>";
    }

    std::ostringstream out;
    out << "Case " << file->id
        << " state=" << caseStateName(file->state)
        << " severity=" << crimeSeverityName(file->severity)
        << " suspect=" << suspectKnowledgeName(file->suspectKnowledge)
        << " identity=" << std::fixed << std::setprecision(2) << file->identityConfidence
        << " warrant=" << (file->activePersonWarrant ? "yes" : "no")
        << " vehicleBOLO=" << (file->activeVehicleBolo ? "yes" : "no")
        << " immediate={active:" << (file->immediate.active ? "yes" : "no")
        << ",reportPending:" << (file->immediate.reportPending ? "yes" : "no")
        << ",pursuit:" << (file->immediate.pursuitActive ? "yes" : "no")
        << ",level:" << static_cast<unsigned>(file->immediate.tacticalLevel) << "}"
        << " crimes=" << file->crimeIds.size()
        << " evidence=" << file->evidence.size()
        << " resolution=" << caseResolutionName(file->resolution);

    if (!file->incidentKey.empty()) {
        out << " incident=" << file->incidentKey;
    }

    for (const CrimeEvent* event : registry.crimesForCase(caseId)) {
        out << "\n  Crime " << event->id
            << " type=" << crimeTypeName(event->type)
            << " severity=" << crimeSeverityName(event->severity)
            << " at=" << event->occurredAtMs
            << " loc=(" << event->location.x << ',' << event->location.y << ',' << event->location.z << ')';
        if (!event->location.zoneTag.empty()) out << " zone=" << event->location.zoneTag;
        if (event->businessId.has_value()) out << " businessId=" << *event->businessId;
    }

    for (const auto& evidence : file->evidence) {
        out << "\n  Evidence " << evidenceSourceName(evidence.source)
            << '/' << evidenceKindName(evidence.kind)
            << " confidence=" << std::fixed << std::setprecision(2) << evidence.confidence
            << " at=" << evidence.observedAtMs;
        if (!evidence.independenceKey.empty()) out << " sourceFamily=" << evidence.independenceKey;
        if (!evidence.snapshot.descriptor.empty()) out << " value=\"" << evidence.snapshot.descriptor << '\"';
    }

    return out.str();
}

} // namespace gco::crime
