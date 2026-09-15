#include "dialogue/SpeakerPresentation.h"

#include <cassert>
#include <cstdint>
#include <vector>

using namespace gco::dialogue;

int main() {
    assert(voiceSetIdFor(SpeakerRole::Clerk, VoiceGender::Feminine, AgeBand::YoungAdult)
        == "clerk.feminine.young_adult");
    assert(voiceSetIdFor(SpeakerRole::InvestigatingOfficer, VoiceGender::Masculine, AgeBand::OlderAdult)
        == "investigating_officer.masculine.older_adult");
    assert(voiceSetIdFor(SpeakerRole::Clerk, VoiceGender::Unknown, AgeBand::Adult).empty());

    assert(voiceGenderName(VoiceGender::Masculine) == "masculine");
    assert(ageBandName(AgeBand::OlderAdult) == "older_adult");
    assert(speakerRoleName(SpeakerRole::PatrolOfficer) == "patrol_officer");
    assert(speechRegisterName(SpeechRegister::Measured) == "measured");

    const auto invalid = LipSyncEnvelope::fromPcm16({}, 24000, 1);
    assert(invalid.empty());
    assert(invalid.durationMs() == 0);

    std::vector<std::int16_t> silence(2400, 0); // 100 ms at 24 kHz mono.
    const auto silent = LipSyncEnvelope::fromPcm16(silence, 24000, 1, 20);
    assert(!silent.empty());
    assert(silent.durationMs() == 100);
    assert(silent.opennessAt(20) == 0.0f);
    assert(!silent.speakingAt(20));

    std::vector<std::int16_t> voiced;
    voiced.reserve(4800);
    voiced.insert(voiced.end(), 1200, 0);       // 50 ms silence.
    voiced.insert(voiced.end(), 2400, 12000);   // 100 ms voiced energy.
    voiced.insert(voiced.end(), 1200, 0);       // 50 ms silence/release.
    const auto envelope = LipSyncEnvelope::fromPcm16(voiced, 24000, 1, 20);
    assert(!envelope.empty());
    assert(envelope.durationMs() == 200);
    assert(envelope.opennessAt(20) == 0.0f);
    assert(envelope.opennessAt(80) > 0.12f);
    assert(envelope.speakingAt(80));
    assert(envelope.opennessAt(200) == 0.0f);
    assert(!envelope.speakingAt(200));

    // Stereo duration must be based on audio frames, not raw interleaved sample count.
    std::vector<std::int16_t> stereo(4800, 8000); // 100 ms at 24 kHz, two channels.
    const auto stereoEnvelope = LipSyncEnvelope::fromPcm16(stereo, 24000, 2, 20);
    assert(stereoEnvelope.durationMs() == 100);
    assert(stereoEnvelope.speakingAt(40));

    return 0;
}
