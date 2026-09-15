#pragma once

#include "DispatchDirector.h"
#include "dialogue/InvestigationDialogue.h"
#include "witness/WitnessDirector.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace gco::investigation {

struct InvestigationTuning final {
    std::size_t maxWitnessesPerCase = 3;
    std::uint64_t turnDelayMs = 2800;
    float vehicleBoloMinimumConfidence = 0.35f;
    float plateBoloMinimumConfidence = 0.45f;
};

class InvestigationDirector final {
public:
    InvestigationDirector(
        crime::CrimeRegistry& registry,
        crime::CrimeDirector& crimeDirector,
        witness::WitnessDirector& witnessDirector,
        DispatchDirector& dispatchDirector,
        platform::IPoliceInvestigationAdapter& adapter,
        EventBus& events,
        InvestigationTuning tuning = {});
    ~InvestigationDirector();

    void initialize();
    void shutdown();
    void tickFiveHz(std::uint64_t nowMs, bool gameplayAllowed);

    [[nodiscard]] bool casePersistenceDirty() const noexcept { return casePersistenceDirty_; }
    void clearCasePersistenceDirty() noexcept { casePersistenceDirty_ = false; }
    [[nodiscard]] std::string debugSummary() const;

private:
    struct KnownWitness final {
        LogicalId caseId = 0;
        platform::PedHandle ped = 0;
        std::string sourceKey;
    };

    struct ActiveInterview final {
        LogicalId caseId = 0;
        platform::PedHandle witnessPed = 0;
        std::string sourceKey;
        InterviewSourceSummary summary{};
        dialogue::InterviewPlan plan{};
        std::size_t turnIndex = 0;
        std::uint64_t nextTurnAtMs = 0;
    };

    void onWitnessReported(const RuntimeEvent& event);
    void cacheLiveWitnesses(LogicalId caseId);
    bool tryStartInterview(LogicalId caseId, std::uint64_t nowMs);
    void tickActiveInterview(std::uint64_t nowMs);
    void abortActiveInterview(const char* reason);
    void completeActiveInterview(std::uint64_t nowMs);
    void commitInterviewFacts(LogicalId caseId, const InterviewSourceSummary& summary, std::uint64_t nowMs);
    [[nodiscard]] static PresentationStyle presentationFor(const dialogue::ConversationTurn& turn) noexcept;
    [[nodiscard]] static std::string interviewKey(LogicalId caseId, std::string_view sourceKey);

    crime::CrimeRegistry& registry_;
    crime::CrimeDirector& crimeDirector_;
    witness::WitnessDirector& witnessDirector_;
    DispatchDirector& dispatchDirector_;
    platform::IPoliceInvestigationAdapter& adapter_;
    EventBus& events_;
    InvestigationTuning tuning_;

    EventBus::SubscriptionId witnessReportedSub_ = 0;
    std::vector<KnownWitness> knownWitnesses_;
    std::unordered_set<std::string> interviewed_;
    std::optional<ActiveInterview> active_;
    bool initialized_ = false;
    bool casePersistenceDirty_ = false;
};

} // namespace gco::investigation
