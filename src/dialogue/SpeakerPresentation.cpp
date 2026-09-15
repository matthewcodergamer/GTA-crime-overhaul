#include "SpeakerPresentation.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace gco::dialogue {
namespace {

std::string roleId(const SpeakerRole role) {
    switch (role) {
    case SpeakerRole::Clerk: return "clerk";
    case SpeakerRole::CivilianWitness: return "witness";
    case SpeakerRole::SecurityGuard: return "guard";
    case SpeakerRole::PatrolOfficer: return "patrol_officer";
    case SpeakerRole::InvestigatingOfficer: return "investigating_officer";
    }
    return {};
}

std::string genderId(const VoiceGender gender) {
    switch (gender) {
    case VoiceGender::Masculine: return "masculine";
    case VoiceGender::Feminine: return "feminine";
    case VoiceGender::Unknown: break;
    }
    return {};
}

std::string ageId(const AgeBand ageBand) {
    switch (ageBand) {
    case AgeBand::YoungAdult: return "young_adult";
    case AgeBand::Adult: return "adult";
    case AgeBand::OlderAdult: return "older_adult";
    }
    return {};
}

} // namespace

LipSyncEnvelope LipSyncEnvelope::fromPcm16(
    const std::span<const std::int16_t> interleavedSamples,
    const std::uint32_t sampleRate,
    const std::uint16_t channels,
    const std::uint32_t frameMs,
    const float silenceThreshold) {

    LipSyncEnvelope envelope;
    if (interleavedSamples.empty()
        || sampleRate == 0
        || channels == 0
        || frameMs == 0
        || !std::isfinite(silenceThreshold)
        || silenceThreshold < 0.0f
        || silenceThreshold >= 1.0f) {
        return envelope;
    }

    const std::uint64_t frames = interleavedSamples.size() / channels;
    if (frames == 0) {
        return envelope;
    }

    envelope.durationMs_ = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(
            (frames * 1000ull) / sampleRate,
            std::numeric_limits<std::uint32_t>::max()));

    const std::uint64_t audioFramesPerWindow = std::max<std::uint64_t>(
        1,
        (static_cast<std::uint64_t>(sampleRate) * frameMs) / 1000ull);
    const std::uint64_t samplesPerWindow = audioFramesPerWindow * channels;

    float smoothed = 0.0f;
    for (std::uint64_t sampleStart = 0; sampleStart < interleavedSamples.size(); sampleStart += samplesPerWindow) {
        const std::uint64_t sampleEnd = std::min<std::uint64_t>(
            interleavedSamples.size(),
            sampleStart + samplesPerWindow);
        if (sampleEnd <= sampleStart) {
            break;
        }

        double sumSquares = 0.0;
        for (std::uint64_t index = sampleStart; index < sampleEnd; ++index) {
            const double normalized = static_cast<double>(interleavedSamples[static_cast<std::size_t>(index)]) / 32768.0;
            sumSquares += normalized * normalized;
        }

        const double meanSquare = sumSquares / static_cast<double>(sampleEnd - sampleStart);
        const float rms = static_cast<float>(std::sqrt(meanSquare));
        const float raw = rms <= silenceThreshold
            ? 0.0f
            : std::clamp((rms - silenceThreshold) / std::max(0.001f, 0.32f - silenceThreshold), 0.0f, 1.0f);

        // Fast attack / slower release keeps short consonant gaps from producing visibly
        // frantic mouth flicker while still closing during real pauses.
        if (raw >= smoothed) {
            smoothed = raw;
        } else {
            smoothed = std::max(raw, smoothed * 0.58f);
        }

        const std::uint64_t audioFrameIndex = sampleStart / channels;
        const std::uint64_t atMs64 = (audioFrameIndex * 1000ull) / sampleRate;
        envelope.samples_.push_back(LipSyncSample{
            static_cast<std::uint32_t>(std::min<std::uint64_t>(atMs64, std::numeric_limits<std::uint32_t>::max())),
            std::clamp(smoothed, 0.0f, 1.0f)
        });
    }

    return envelope;
}

float LipSyncEnvelope::opennessAt(const std::uint32_t elapsedMs) const noexcept {
    if (samples_.empty() || elapsedMs >= durationMs_) {
        return 0.0f;
    }

    const auto upper = std::upper_bound(
        samples_.begin(),
        samples_.end(),
        elapsedMs,
        [](const std::uint32_t value, const LipSyncSample& sample) {
            return value < sample.atMs;
        });

    if (upper == samples_.begin()) {
        return upper->openness;
    }
    return std::prev(upper)->openness;
}

bool LipSyncEnvelope::speakingAt(const std::uint32_t elapsedMs, const float threshold) const noexcept {
    if (!std::isfinite(threshold) || threshold < 0.0f || threshold > 1.0f) {
        return false;
    }
    return opennessAt(elapsedMs) >= threshold;
}

std::string voiceSetIdFor(
    const SpeakerRole role,
    const VoiceGender gender,
    const AgeBand ageBand) {

    const std::string roleValue = roleId(role);
    const std::string genderValue = genderId(gender);
    const std::string ageValue = ageId(ageBand);
    if (roleValue.empty() || genderValue.empty() || ageValue.empty()) {
        return {};
    }
    return roleValue + "." + genderValue + "." + ageValue;
}

std::string_view voiceGenderName(const VoiceGender gender) noexcept {
    switch (gender) {
    case VoiceGender::Unknown: return "unknown";
    case VoiceGender::Masculine: return "masculine";
    case VoiceGender::Feminine: return "feminine";
    }
    return "unknown";
}

std::string_view ageBandName(const AgeBand ageBand) noexcept {
    switch (ageBand) {
    case AgeBand::YoungAdult: return "young_adult";
    case AgeBand::Adult: return "adult";
    case AgeBand::OlderAdult: return "older_adult";
    }
    return "adult";
}

std::string_view speakerRoleName(const SpeakerRole role) noexcept {
    switch (role) {
    case SpeakerRole::Clerk: return "clerk";
    case SpeakerRole::CivilianWitness: return "civilian_witness";
    case SpeakerRole::SecurityGuard: return "security_guard";
    case SpeakerRole::PatrolOfficer: return "patrol_officer";
    case SpeakerRole::InvestigatingOfficer: return "investigating_officer";
    }
    return "civilian_witness";
}

std::string_view speechRegisterName(const SpeechRegister speechRegister) noexcept {
    switch (speechRegister) {
    case SpeechRegister::GroundedContemporary: return "grounded_contemporary";
    case SpeechRegister::NeutralProfessional: return "neutral_professional";
    case SpeechRegister::Measured: return "measured";
    }
    return "grounded_contemporary";
}

} // namespace gco::dialogue
