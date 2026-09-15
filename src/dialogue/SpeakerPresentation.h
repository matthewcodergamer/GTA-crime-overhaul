#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace gco::dialogue {

enum class VoiceGender : std::uint8_t {
    Unknown,
    Masculine,
    Feminine
};

enum class AgeBand : std::uint8_t {
    YoungAdult,
    Adult,
    OlderAdult
};

enum class SpeakerRole : std::uint8_t {
    Clerk,
    CivilianWitness,
    SecurityGuard,
    PatrolOfficer,
    InvestigatingOfficer
};

enum class SpeechRegister : std::uint8_t {
    GroundedContemporary,
    NeutralProfessional,
    Measured
};

// Presentation-only traits. These may alter line selection, cadence and voice choice,
// but they never alter evidence truth or case state.
struct SpeakerPresentationProfile final {
    std::string profileId;
    VoiceGender voiceGender = VoiceGender::Unknown;
    AgeBand ageBand = AgeBand::Adult;
    SpeechRegister speechRegister = SpeechRegister::GroundedContemporary;
};

struct LipSyncSample final {
    std::uint32_t atMs = 0;
    float openness = 0.0f;
};

// Provider-neutral mouth activity derived from the actual PCM that is played. A GTA
// presenter may map this envelope to a validated facial animation, while another
// presenter could map it to visemes. Subtitle duration is deliberately not used.
class LipSyncEnvelope final {
public:
    [[nodiscard]] static LipSyncEnvelope fromPcm16(
        std::span<const std::int16_t> interleavedSamples,
        std::uint32_t sampleRate,
        std::uint16_t channels,
        std::uint32_t frameMs = 40,
        float silenceThreshold = 0.018f);

    [[nodiscard]] bool empty() const noexcept { return samples_.empty(); }
    [[nodiscard]] std::uint32_t durationMs() const noexcept { return durationMs_; }
    [[nodiscard]] std::span<const LipSyncSample> samples() const noexcept { return samples_; }
    [[nodiscard]] float opennessAt(std::uint32_t elapsedMs) const noexcept;
    [[nodiscard]] bool speakingAt(std::uint32_t elapsedMs, float threshold = 0.12f) const noexcept;

private:
    std::vector<LipSyncSample> samples_;
    std::uint32_t durationMs_ = 0;
};

[[nodiscard]] std::string voiceSetIdFor(
    SpeakerRole role,
    VoiceGender gender,
    AgeBand ageBand);

[[nodiscard]] std::string_view voiceGenderName(VoiceGender gender) noexcept;
[[nodiscard]] std::string_view ageBandName(AgeBand ageBand) noexcept;
[[nodiscard]] std::string_view speakerRoleName(SpeakerRole role) noexcept;
[[nodiscard]] std::string_view speechRegisterName(SpeechRegister speechRegister) noexcept;

} // namespace gco::dialogue
