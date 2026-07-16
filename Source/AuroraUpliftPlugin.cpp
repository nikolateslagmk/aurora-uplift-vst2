#include "DistrhoPlugin.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

START_NAMESPACE_DISTRHO

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr uint32_t kVoiceCount = 16;
constexpr uint32_t kMaxUnison = 9;

inline float clampf(const float value, const float lo, const float hi) noexcept
{
    return std::max(lo, std::min(hi, value));
}

inline float midiNoteToHz(const int note) noexcept
{
    return 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
}

inline float fastSaw(const float phase) noexcept
{
    return phase * 2.0f - 1.0f;
}

inline float fastSquare(const float phase) noexcept
{
    return phase < 0.5f ? 1.0f : -1.0f;
}

inline void advancePhase(float& phase, const float increment) noexcept
{
    phase += increment;
    phase -= std::floor(phase);
}

enum ParameterId : uint32_t
{
    pMaster = 0,
    pUnison,
    pDetune,
    pSpread,
    pOsc2Mix,
    pOsc2Semi,
    pOsc2Shape,
    pSub,
    pNoise,
    pDrive,
    pAmpA,
    pAmpD,
    pAmpS,
    pAmpR,
    pCutoff,
    pResonance,
    pFilterEnv,
    pFilterA,
    pFilterD,
    pFilterS,
    pFilterR,
    pVibratoRate,
    pVibratoDepth,
    pVelocity,
    pGateDepth,
    pGateRate,
    pDelayMix,
    pDelayFeedback,
    pDelayDivision,
    pReverbMix,
    pReverbSize,
    pReverbDamp,
    kParameterCount
};

struct ParameterSpec
{
    const char* name;
    const char* symbol;
    const char* unit;
    float min;
    float max;
    float def;
    uint32_t hints;
};

constexpr uint32_t kAuto = kParameterIsAutomatable;
constexpr uint32_t kAutoInt = kParameterIsAutomatable | kParameterIsInteger;
constexpr uint32_t kAutoLog = kParameterIsAutomatable | kParameterIsLogarithmic;

constexpr std::array<ParameterSpec, kParameterCount> kParameterSpecs {{
    {"Master", "master", "", 0.0f, 1.0f, 0.72f, kAuto},
    {"Unison Voices", "unison", "voices", 1.0f, 9.0f, 7.0f, kAutoInt},
    {"Detune", "detune", "cents", 0.0f, 60.0f, 22.0f, kAuto},
    {"Stereo Spread", "spread", "", 0.0f, 1.0f, 0.82f, kAuto},
    {"Osc 2 Mix", "osc2_mix", "", 0.0f, 1.0f, 0.22f, kAuto},
    {"Osc 2 Semitone", "osc2_semi", "st", -24.0f, 24.0f, 12.0f, kAutoInt},
    {"Osc 2 Shape", "osc2_shape", "", 0.0f, 1.0f, 0.25f, kAuto},
    {"Sub", "sub", "", 0.0f, 1.0f, 0.15f, kAuto},
    {"Noise", "noise", "", 0.0f, 0.5f, 0.02f, kAuto},
    {"Drive", "drive", "", 0.5f, 4.0f, 1.25f, kAuto},
    {"Amp Attack", "amp_a", "s", 0.001f, 4.0f, 0.01f, kAutoLog},
    {"Amp Decay", "amp_d", "s", 0.001f, 4.0f, 0.25f, kAutoLog},
    {"Amp Sustain", "amp_s", "", 0.0f, 1.0f, 0.78f, kAuto},
    {"Amp Release", "amp_r", "s", 0.001f, 8.0f, 0.65f, kAutoLog},
    {"Cutoff", "cutoff", "Hz", 40.0f, 20000.0f, 9500.0f, kAutoLog},
    {"Resonance", "resonance", "", 0.05f, 1.0f, 0.18f, kAuto},
    {"Filter Env", "filter_env", "", -1.0f, 1.0f, 0.48f, kAuto},
    {"Filter Attack", "filter_a", "s", 0.001f, 4.0f, 0.01f, kAutoLog},
    {"Filter Decay", "filter_d", "s", 0.001f, 4.0f, 0.45f, kAutoLog},
    {"Filter Sustain", "filter_s", "", 0.0f, 1.0f, 0.28f, kAuto},
    {"Filter Release", "filter_r", "s", 0.001f, 8.0f, 0.55f, kAutoLog},
    {"Vibrato Rate", "vibrato_rate", "Hz", 0.1f, 12.0f, 5.5f, kAutoLog},
    {"Vibrato Depth", "vibrato_depth", "st", 0.0f, 1.0f, 0.0f, kAuto},
    {"Velocity", "velocity", "", 0.0f, 1.0f, 0.35f, kAuto},
    {"Gate Depth", "gate_depth", "", 0.0f, 1.0f, 0.0f, kAuto},
    {"Gate Rate", "gate_rate", "0=1/8 1=1/16 2=1/32", 0.0f, 2.0f, 1.0f, kAutoInt},
    {"Delay Mix", "delay_mix", "", 0.0f, 1.0f, 0.22f, kAuto},
    {"Delay Feedback", "delay_feedback", "", 0.0f, 0.88f, 0.34f, kAuto},
    {"Delay Division", "delay_division", "0=1/16 1=1/8D 2=1/8 3=1/4D 4=1/4", 0.0f, 4.0f, 2.0f, kAutoInt},
    {"Reverb Mix", "reverb_mix", "", 0.0f, 1.0f, 0.28f, kAuto},
    {"Reverb Size", "reverb_size", "", 0.0f, 1.0f, 0.72f, kAuto},
    {"Reverb Damp", "reverb_damp", "", 0.0f, 1.0f, 0.42f, kAuto}
}};

class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void noteOn() noexcept
    {
        stage = Stage::Attack;
        if (value < 0.0f || value > 1.0f)
            value = 0.0f;
    }

    void noteOff() noexcept
    {
        if (stage != Stage::Idle)
        {
            stage = Stage::Release;
            releaseStart = value;
        }
    }

    void reset() noexcept
    {
        stage = Stage::Idle;
        value = 0.0f;
        releaseStart = 0.0f;
    }

    float process(const float sampleRate, const float attack, const float decay,
                  const float sustain, const float release) noexcept
    {
        switch (stage)
        {
        case Stage::Idle:
            value = 0.0f;
            break;
        case Stage::Attack:
            value += 1.0f / std::max(1.0f, attack * sampleRate);
            if (value >= 1.0f)
            {
                value = 1.0f;
                stage = Stage::Decay;
            }
            break;
        case Stage::Decay:
            value -= (1.0f - sustain) / std::max(1.0f, decay * sampleRate);
            if (value <= sustain)
            {
                value = sustain;
                stage = Stage::Sustain;
            }
            break;
        case Stage::Sustain:
            value = sustain;
            break;
        case Stage::Release:
            value -= releaseStart / std::max(1.0f, release * sampleRate);
            if (value <= 0.00001f)
            {
                value = 0.0f;
                stage = Stage::Idle;
            }
            break;
        }
        return value;
    }

    bool isIdle() const noexcept { return stage == Stage::Idle; }

private:
    Stage stage = Stage::Idle;
    float value = 0.0f;
    float releaseStart = 0.0f;
};

struct StateVariableFilter
{
    float ic1 = 0.0f;
    float ic2 = 0.0f;

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    float processLowPass(const float input, const float cutoff,
                         const float resonance, const float sampleRate) noexcept
    {
        const float safeCutoff = clampf(cutoff, 20.0f, sampleRate * 0.45f);
        const float g = std::tan(kPi * safeCutoff / sampleRate);
        const float k = 2.0f - 1.9f * clampf(resonance, 0.0f, 1.0f);
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float v1 = a1 * (ic1 + g * (input - ic2));
        const float v2 = ic2 + g * v1;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }
};

struct Voice
{
    bool active = false;
    bool held = false;
    int note = -1;
    float velocity = 0.0f;
    uint64_t age = 0;
    std::array<float, kMaxUnison> phases {};
    float phase2 = 0.0f;
    float subPhase = 0.0f;
    float vibratoPhase = 0.0f;
    uint32_t noiseState = 0x12345678u;
    Envelope ampEnv;
    Envelope filterEnv;
    StateVariableFilter filterL;
    StateVariableFilter filterR;

    void start(const int newNote, const float newVelocity, const uint64_t newAge) noexcept
    {
        active = true;
        held = true;
        note = newNote;
        velocity = newVelocity;
        age = newAge;
        for (uint32_t i = 0; i < kMaxUnison; ++i)
            phases[i] = std::fmod(0.137f * static_cast<float>(i + 1) + 0.013f * static_cast<float>(newNote), 1.0f);
        phase2 = 0.19f;
        subPhase = 0.41f;
        vibratoPhase = 0.0f;
        noiseState ^= static_cast<uint32_t>(newNote * 2654435761u);
        filterL.reset();
        filterR.reset();
        ampEnv.noteOn();
        filterEnv.noteOn();
    }

    void release() noexcept
    {
        held = false;
        ampEnv.noteOff();
        filterEnv.noteOff();
    }

    float noise() noexcept
    {
        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        return static_cast<float>(static_cast<int32_t>(noiseState)) / 2147483648.0f;
    }
};

struct DelayLine
{
    std::vector<float> data;
    uint32_t write = 0;

    void resize(const uint32_t size)
    {
        data.assign(std::max<uint32_t>(2, size), 0.0f);
        write = 0;
    }

    void clear() noexcept
    {
        std::fill(data.begin(), data.end(), 0.0f);
        write = 0;
    }

    float read(const float delaySamples) const noexcept
    {
        if (data.empty())
            return 0.0f;
        float position = static_cast<float>(write) - delaySamples;
        while (position < 0.0f)
            position += static_cast<float>(data.size());
        const uint32_t i0 = static_cast<uint32_t>(position) % static_cast<uint32_t>(data.size());
        const uint32_t i1 = (i0 + 1) % static_cast<uint32_t>(data.size());
        const float frac = position - std::floor(position);
        return data[i0] + (data[i1] - data[i0]) * frac;
    }

    void push(const float sample) noexcept
    {
        if (data.empty())
            return;
        data[write] = sample;
        write = (write + 1) % static_cast<uint32_t>(data.size());
    }
};

struct Comb
{
    DelayLine line;
    float filterStore = 0.0f;

    void resize(const uint32_t size) { line.resize(size); filterStore = 0.0f; }
    void clear() noexcept { line.clear(); filterStore = 0.0f; }

    float process(const float input, const float feedback, const float damp) noexcept
    {
        const float delayed = line.read(static_cast<float>(line.data.size() - 1));
        filterStore = delayed * (1.0f - damp) + filterStore * damp;
        line.push(input + filterStore * feedback);
        return delayed;
    }
};

} // namespace

class AuroraUpliftPlugin final : public Plugin
{
public:
    AuroraUpliftPlugin()
        : Plugin(kParameterCount, 0, 0)
    {
        for (uint32_t i = 0; i < kParameterCount; ++i)
            fParameters[i] = kParameterSpecs[i].def;
    }

protected:
    const char* getLabel() const override { return "AuroraUplift"; }
    const char* getDescription() const override { return "Uplifting trance supersaw synthesizer"; }
    const char* getMaker() const override { return "Alza Audio"; }
    const char* getHomePage() const override { return "https://github.com/nikolateslagmk/aurora-uplift-vst3"; }
    const char* getLicense() const override { return "Project source license + DPF attribution"; }
    uint32_t getVersion() const override { return d_version(1, 0, 0); }
    int64_t getUniqueId() const override { return d_cconst('A', 'U', 'P', '2'); }

    void initParameter(const uint32_t index, Parameter& parameter) override
    {
        if (index >= kParameterCount)
            return;
        const ParameterSpec& spec = kParameterSpecs[index];
        parameter.hints = spec.hints;
        parameter.name = spec.name;
        parameter.symbol = spec.symbol;
        parameter.unit = spec.unit;
        parameter.ranges.min = spec.min;
        parameter.ranges.max = spec.max;
        parameter.ranges.def = spec.def;
    }

    float getParameterValue(const uint32_t index) const override
    {
        return index < kParameterCount ? fParameters[index] : 0.0f;
    }

    void setParameterValue(const uint32_t index, const float value) override
    {
        if (index >= kParameterCount)
            return;
        const ParameterSpec& spec = kParameterSpecs[index];
        fParameters[index] = clampf(value, spec.min, spec.max);
    }

    void activate() override
    {
        configureForSampleRate(static_cast<float>(getSampleRate()));
        resetAll();
    }

    void deactivate() override
    {
        resetAll();
    }

    void run(const float**, float** outputs, const uint32_t frames,
             const MidiEvent* midiEvents, const uint32_t midiEventCount) override
    {
        float* const outL = outputs[0];
        float* const outR = outputs[1];
        std::memset(outL, 0, sizeof(float) * frames);
        std::memset(outR, 0, sizeof(float) * frames);

        const float currentRate = static_cast<float>(getSampleRate());
        if (std::abs(currentRate - fSampleRate) > 0.5f)
            configureForSampleRate(currentRate);

        double bpm = 138.0;
        const TimePosition& timePosition = getTimePosition();
        if (timePosition.bbt.valid && timePosition.bbt.beatsPerMinute > 20.0)
            bpm = timePosition.bbt.beatsPerMinute;
        bpm = std::max(40.0, std::min(300.0, bpm));

        uint32_t eventIndex = 0;
        for (uint32_t frame = 0; frame < frames; ++frame)
        {
            while (eventIndex < midiEventCount && midiEvents[eventIndex].frame <= frame)
            {
                handleMidi(midiEvents[eventIndex]);
                ++eventIndex;
            }

            float left = 0.0f;
            float right = 0.0f;
            for (Voice& voice : fVoices)
            {
                if (!voice.active)
                    continue;
                renderVoice(voice, left, right);
            }

            left = std::tanh(left * fParameters[pDrive]);
            right = std::tanh(right * fParameters[pDrive]);

            applyGate(left, right, bpm);
            applyDelay(left, right, bpm);
            applyReverb(left, right);

            const float master = fParameters[pMaster];
            outL[frame] = clampf(left * master, -1.0f, 1.0f);
            outR[frame] = clampf(right * master, -1.0f, 1.0f);
        }
    }

private:
    void configureForSampleRate(const float rate)
    {
        fSampleRate = std::max(8000.0f, rate);
        fDelayL.resize(static_cast<uint32_t>(fSampleRate * 4.0f) + 8u);
        fDelayR.resize(static_cast<uint32_t>(fSampleRate * 4.0f) + 8u);

        constexpr std::array<float, 4> timesL {{0.0297f, 0.0371f, 0.0411f, 0.0437f}};
        constexpr std::array<float, 4> timesR {{0.0307f, 0.0383f, 0.0427f, 0.0451f}};
        for (uint32_t i = 0; i < 4; ++i)
        {
            fCombsL[i].resize(static_cast<uint32_t>(fSampleRate * timesL[i]) + 2u);
            fCombsR[i].resize(static_cast<uint32_t>(fSampleRate * timesR[i]) + 2u);
        }
    }

    void resetAll() noexcept
    {
        for (Voice& voice : fVoices)
        {
            voice.active = false;
            voice.held = false;
            voice.ampEnv.reset();
            voice.filterEnv.reset();
            voice.filterL.reset();
            voice.filterR.reset();
        }
        fDelayL.clear();
        fDelayR.clear();
        for (Comb& comb : fCombsL) comb.clear();
        for (Comb& comb : fCombsR) comb.clear();
        fPitchBend = 0.0f;
        fGatePhase = 0.0;
        fSmoothedGate = 1.0f;
    }

    void handleMidi(const MidiEvent& event) noexcept
    {
        if (event.size == 0)
            return;
        const uint8_t* const data = event.data;
        const uint8_t status = data[0] & 0xF0u;

        if (status == 0x90u && event.size >= 3)
        {
            const int note = data[1] & 0x7F;
            const int velocity = data[2] & 0x7F;
            if (velocity == 0)
                noteOff(note);
            else
                noteOn(note, static_cast<float>(velocity) / 127.0f);
        }
        else if (status == 0x80u && event.size >= 3)
        {
            noteOff(data[1] & 0x7F);
        }
        else if (status == 0xE0u && event.size >= 3)
        {
            const int value = static_cast<int>(data[1]) | (static_cast<int>(data[2]) << 7);
            fPitchBend = (static_cast<float>(value) - 8192.0f) / 8192.0f * 2.0f;
        }
        else if (status == 0xB0u && event.size >= 3 && data[1] == 123u)
        {
            for (Voice& voice : fVoices)
                if (voice.active) voice.release();
        }
    }

    void noteOn(const int note, const float velocity) noexcept
    {
        Voice* selected = nullptr;
        for (Voice& voice : fVoices)
        {
            if (!voice.active)
            {
                selected = &voice;
                break;
            }
        }
        if (selected == nullptr)
        {
            selected = &*std::min_element(fVoices.begin(), fVoices.end(),
                [](const Voice& a, const Voice& b) { return a.age < b.age; });
        }
        selected->start(note, velocity, ++fVoiceAge);
    }

    void noteOff(const int note) noexcept
    {
        for (Voice& voice : fVoices)
            if (voice.active && voice.note == note && voice.held)
                voice.release();
    }

    void renderVoice(Voice& voice, float& outL, float& outR) noexcept
    {
        const float amp = voice.ampEnv.process(fSampleRate,
            fParameters[pAmpA], fParameters[pAmpD], fParameters[pAmpS], fParameters[pAmpR]);
        const float filterEnvelope = voice.filterEnv.process(fSampleRate,
            fParameters[pFilterA], fParameters[pFilterD], fParameters[pFilterS], fParameters[pFilterR]);

        if (voice.ampEnv.isIdle())
        {
            voice.active = false;
            return;
        }

        const float vibrato = std::sin(kTwoPi * voice.vibratoPhase) * fParameters[pVibratoDepth];
        advancePhase(voice.vibratoPhase, fParameters[pVibratoRate] / fSampleRate);
        const float baseHz = midiNoteToHz(voice.note) * std::pow(2.0f, (fPitchBend + vibrato) / 12.0f);

        const uint32_t unison = static_cast<uint32_t>(clampf(std::round(fParameters[pUnison]), 1.0f, 9.0f));
        const float detune = fParameters[pDetune];
        const float spread = fParameters[pSpread];
        float left = 0.0f;
        float right = 0.0f;

        for (uint32_t i = 0; i < unison; ++i)
        {
            const float position = unison == 1 ? 0.0f : (static_cast<float>(i) / static_cast<float>(unison - 1) * 2.0f - 1.0f);
            const float hz = baseHz * std::pow(2.0f, (position * detune) / 1200.0f);
            const float saw = fastSaw(voice.phases[i]);
            advancePhase(voice.phases[i], hz / fSampleRate);

            const float pan = position * spread;
            const float angle = (pan + 1.0f) * (kPi * 0.25f);
            left += saw * std::cos(angle);
            right += saw * std::sin(angle);
        }

        const float normalization = 1.0f / std::sqrt(static_cast<float>(unison));
        left *= normalization;
        right *= normalization;

        const float osc2Hz = baseHz * std::pow(2.0f, fParameters[pOsc2Semi] / 12.0f);
        const float osc2Saw = fastSaw(voice.phase2);
        const float osc2Square = fastSquare(voice.phase2);
        const float osc2 = osc2Saw + (osc2Square - osc2Saw) * fParameters[pOsc2Shape];
        advancePhase(voice.phase2, osc2Hz / fSampleRate);
        left += osc2 * fParameters[pOsc2Mix] * 0.72f;
        right += osc2 * fParameters[pOsc2Mix] * 0.72f;

        const float sub = std::sin(kTwoPi * voice.subPhase) * fParameters[pSub];
        advancePhase(voice.subPhase, baseHz * 0.5f / fSampleRate);
        const float noise = voice.noise() * fParameters[pNoise];
        left += sub + noise;
        right += sub - noise * 0.35f;

        const float envOctaves = fParameters[pFilterEnv] * filterEnvelope * 6.0f;
        const float cutoff = fParameters[pCutoff] * std::pow(2.0f, envOctaves);
        left = voice.filterL.processLowPass(left, cutoff, fParameters[pResonance], fSampleRate);
        right = voice.filterR.processLowPass(right, cutoff, fParameters[pResonance], fSampleRate);

        const float velocityGain = (1.0f - fParameters[pVelocity]) + voice.velocity * fParameters[pVelocity];
        const float gain = amp * velocityGain * 0.16f;
        outL += left * gain;
        outR += right * gain;
    }

    void applyGate(float& left, float& right, const double bpm) noexcept
    {
        const float depth = fParameters[pGateDepth];
        if (depth <= 0.0001f)
            return;

        static constexpr std::array<float, 16> pattern {{
            1.00f, 0.18f, 0.82f, 0.28f,
            1.00f, 0.38f, 0.72f, 0.18f,
            1.00f, 0.22f, 0.90f, 0.35f,
            1.00f, 0.48f, 0.76f, 0.14f
        }};

        const int rate = static_cast<int>(std::round(fParameters[pGateRate]));
        const double stepsPerBeat = rate == 0 ? 2.0 : (rate == 1 ? 4.0 : 8.0);
        const double stepsPerSecond = bpm / 60.0 * stepsPerBeat;
        fGatePhase += stepsPerSecond / static_cast<double>(fSampleRate);
        fGatePhase -= std::floor(fGatePhase / 16.0) * 16.0;
        const uint32_t step = static_cast<uint32_t>(fGatePhase) & 15u;
        const float target = (1.0f - depth) + pattern[step] * depth;
        const float smoothing = 1.0f - std::exp(-1.0f / (0.004f * fSampleRate));
        fSmoothedGate += (target - fSmoothedGate) * smoothing;
        left *= fSmoothedGate;
        right *= fSmoothedGate;
    }

    void applyDelay(float& left, float& right, const double bpm) noexcept
    {
        static constexpr std::array<double, 5> divisions {{0.25, 0.375, 0.5, 0.75, 1.0}};
        const int index = static_cast<int>(clampf(std::round(fParameters[pDelayDivision]), 0.0f, 4.0f));
        const float delaySamples = static_cast<float>((60.0 / bpm) * divisions[static_cast<uint32_t>(index)] * fSampleRate);
        const float wetL = fDelayL.read(delaySamples);
        const float wetR = fDelayR.read(delaySamples * 1.007f);
        const float feedback = fParameters[pDelayFeedback];
        const float mix = fParameters[pDelayMix];

        fDelayL.push(left + wetR * feedback);
        fDelayR.push(right + wetL * feedback);
        left = left * (1.0f - mix) + wetL * mix;
        right = right * (1.0f - mix) + wetR * mix;
    }

    void applyReverb(float& left, float& right) noexcept
    {
        const float mix = fParameters[pReverbMix] * 0.58f;
        if (mix <= 0.0001f)
            return;
        const float feedback = 0.62f + fParameters[pReverbSize] * 0.31f;
        const float damp = 0.08f + fParameters[pReverbDamp] * 0.82f;
        float wetL = 0.0f;
        float wetR = 0.0f;
        const float input = (left + right) * 0.24f;
        for (uint32_t i = 0; i < 4; ++i)
        {
            wetL += fCombsL[i].process(input + right * 0.05f, feedback, damp);
            wetR += fCombsR[i].process(input + left * 0.05f, feedback, damp);
        }
        wetL *= 0.25f;
        wetR *= 0.25f;
        left = left * (1.0f - mix) + wetL * mix;
        right = right * (1.0f - mix) + wetR * mix;
    }

    std::array<float, kParameterCount> fParameters {};
    std::array<Voice, kVoiceCount> fVoices {};
    uint64_t fVoiceAge = 0;
    float fSampleRate = 44100.0f;
    float fPitchBend = 0.0f;
    DelayLine fDelayL;
    DelayLine fDelayR;
    std::array<Comb, 4> fCombsL;
    std::array<Comb, 4> fCombsR;
    double fGatePhase = 0.0;
    float fSmoothedGate = 1.0f;
};

Plugin* createPlugin()
{
    return new AuroraUpliftPlugin();
}

END_NAMESPACE_DISTRHO
