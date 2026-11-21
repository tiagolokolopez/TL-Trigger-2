#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

//==============================================================================
struct SampleData
{
    juce::AudioBuffer<float> audio;   // 1 o 2 canales
    double sampleRate = 44100.0;
    juce::String name;
    juce::String path;
};

using SamplePtr = std::shared_ptr<const SampleData>;

//==============================================================================
// Filtro biquad de 2o orden (Butterworth) sin asignaciones de memoria.
class Biquad
{
public:
    void reset() noexcept { z1 = z2 = 0.f; }
    void setLowPass  (double fs, double f) noexcept { calc (fs, f, false); }
    void setHighPass (double fs, double f) noexcept { calc (fs, f, true);  }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    void calc (double fs, double f, bool high) noexcept
    {
        f = juce::jlimit (10.0, fs * 0.45, f);
        const double w0    = juce::MathConstants<double>::twoPi * f / fs;
        const double cosw  = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * 0.70710678);
        const double a0    = 1.0 + alpha;

        double nb0, nb1, nb2;
        if (high) { nb0 = (1.0 + cosw) * 0.5; nb1 = -(1.0 + cosw); nb2 = nb0; }
        else      { nb0 = (1.0 - cosw) * 0.5; nb1 =  (1.0 - cosw); nb2 = nb0; }

        b0 = (float) (nb0 / a0);
        b1 = (float) (nb1 / a0);
        b2 = (float) (nb2 / a0);
        a1 = (float) (-2.0 * cosw / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f, z1 = 0.f, z2 = 0.f;
};

//==============================================================================
class TriggerEngine
{
public:
    static constexpr int    maxSlots    = 8;
    static constexpr int    maxVoices   = 32;
    static constexpr double lookaheadMs = 4.0;   // el plugin reporta esta latencia al host (PDC)

    struct Params
    {
        float thresholdDb = -24.f;
        float dynamics    = 1.f;      // 0..1
        float retrigMs    = 20.f;
        float lowCutHz    = 20.f;
        float highCutHz   = 20000.f;
        float tuneSemis   = 0.f;
        float originalDb  = 0.f;      // <= -59.5 => silencio
        float samplesDb   = 0.f;
        float outputDb    = 0.f;
        int   mode        = 0;        // 0 velocity, 1 round robin, 2 layer
        bool  invertPhase = false;
    };

    //--------------------------------------------------------------------------
    void prepare (double sr)
    {
        sampleRate = sr;
        lookahead  = juce::jmax (1, juce::roundToInt (sr * lookaheadMs * 0.001));
        delayL.assign ((size_t) lookahead, 0.f);
        delayR.assign ((size_t) lookahead, 0.f);
        writePos = 0;

        envRelease = std::exp (-1.0f / (float) (0.020 * sr));
        meterCoeff = 1.0f - std::exp (-1.0f / (float) (0.065 * sr));

        gOrig.reset (sr, 0.02);
        gSamp.reset (sr, 0.02);
        gOut.reset  (sr, 0.02);

        reset();
    }

    void reset()
    {
        for (auto& v : voices) { v.active = false; v.data.reset(); }
        hp.reset();
        lp.reset();
        env = peak = 0.f;
        armed = true;
        scanning = false;
        cooldown = scanRemaining = 0;
        inMs = outMs = 0.f;
        snapGains = true;
        lastLow = lastHigh = -1.f;
        std::fill (delayL.begin(), delayL.end(), 0.f);
        std::fill (delayR.begin(), delayR.end(), 0.f);
    }

    int getLatencySamples() const noexcept { return lookahead; }

    //--------------------------------------------------------------------------
    void setSlot (int index, SamplePtr data)
    {
        if (index < 0 || index >= maxSlots) return;
        SamplePtr old;
        {
            const juce::SpinLock::ScopedLockType sl (slotLock);
            old = std::move (slots[(size_t) index]);
            slots[(size_t) index] = std::move (data);
        }
        // 'old' se libera aqui, fuera del lock
    }

    SamplePtr getSlot (int index) const
    {
        if (index < 0 || index >= maxSlots) return {};
        const juce::SpinLock::ScopedLockType sl (slotLock);
        return slots[(size_t) index];
    }

    void audition (int slot) noexcept { auditionRequest.store (slot); }

    //--------------------------------------------------------------------------
    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh      = buffer.getNumChannels();
        if (numCh < 1 || numSamples < 1) return;

        float* L = buffer.getWritePointer (0);
        float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

        // ---- parametros del bloque ------------------------------------------
        curThrDb       = p.thresholdDb;
        curVelWindowDb = juce::jlimit (12.f, 36.f, -curThrDb);
        curDynamics    = p.dynamics;
        curMode        = p.mode;
        curInvert      = p.invertPhase;
        curRate        = std::pow (2.0, (double) p.tuneSemis / 12.0);

        const float thr   = juce::Decibels::decibelsToGain (curThrDb);
        const float rearm = thr * 0.35f;
        const int retrigSamples = juce::jmax (1, (int) (p.retrigMs * 0.001 * sampleRate));

        if (p.lowCutHz != lastLow)   { hp.setHighPass (sampleRate, p.lowCutHz);  lastLow  = p.lowCutHz;  }
        if (p.highCutHz != lastHigh) { lp.setLowPass  (sampleRate, p.highCutHz); lastHigh = p.highCutHz; }
        const bool useHP = p.lowCutHz  > 25.f;
        const bool useLP = p.highCutHz < 19000.f;

        auto toGain = [] (float db) { return db <= -59.5f ? 0.f : juce::Decibels::decibelsToGain (db); };
        const float tO = toGain (p.originalDb), tS = toGain (p.samplesDb), tOut = toGain (p.outputDb);
        if (snapGains)
        {
            gOrig.setCurrentAndTargetValue (tO);
            gSamp.setCurrentAndTargetValue (tS);
            gOut.setCurrentAndTargetValue (tOut);
            snapGains = false;
        }
        else
        {
            gOrig.setTargetValue (tO);
            gSamp.setTargetValue (tS);
            gOut.setTargetValue (tOut);
        }

        // ---- preescucha pedida desde la interfaz ----------------------------
        const int req = auditionRequest.exchange (-1);
        if (req >= 0 && req < maxSlots)
            if (auto s = getSlot (req))
                startVoice (s, 1.0f, curRate);

        // ---- bucle principal ------------------------------------------------
        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = L[i];
            const float inR = R != nullptr ? R[i] : inL;

            inMs += meterCoeff * (0.5f * (inL * inL + inR * inR) - inMs);

            // señal original retrasada 'lookahead' muestras (compensada por el host)
            const float dryL = delayL[(size_t) writePos];
            const float dryR = delayR[(size_t) writePos];
            delayL[(size_t) writePos] = inL;
            delayR[(size_t) writePos] = inR;
            if (++writePos >= lookahead) writePos = 0;

            // detector
            float d = 0.5f * (inL + inR);
            if (useHP) d = hp.process (d);
            if (useLP) d = lp.process (d);
            const float a = std::abs (d);
            env = juce::jmax (a, env * envRelease);

            if (cooldown > 0) --cooldown;

            if (scanning)
            {
                peak = juce::jmax (peak, a);
                if (--scanRemaining <= 0)
                {
                    scanning = false;
                    fireTrigger (peak);          // el sample arranca alineado con el transitorio original
                    cooldown = retrigSamples;
                }
            }
            else if (! armed)
            {
                if (env < rearm && cooldown == 0)
                    armed = true;
            }
            else if (cooldown == 0 && a >= thr)
            {
                scanning = true;
                scanRemaining = lookahead;
                peak = a;
                armed = false;
            }

            // voces
            float sL = 0.f, sR = 0.f;
            for (auto& v : voices)
            {
                if (! v.active) continue;

                const auto& buf = v.data->audio;
                const int len = buf.getNumSamples();
                const int i0  = (int) v.pos;
                if (i0 >= len) { v.active = false; v.data.reset(); continue; }

                const int   i1   = juce::jmin (i0 + 1, len - 1);
                const float frac = (float) (v.pos - (double) i0);
                const float* c0  = buf.getReadPointer (0);
                const float* c1  = buf.getNumChannels() > 1 ? buf.getReadPointer (1) : c0;

                sL += v.gain * (c0[i0] + frac * (c0[i1] - c0[i0]));
                sR += v.gain * (c1[i0] + frac * (c1[i1] - c1[i0]));
                v.pos += v.inc;
            }

            // mezcla
            const float og = gOrig.getNextValue(), sg = gSamp.getNextValue(), outg = gOut.getNextValue();
            if (R != nullptr)
            {
                const float oL = (dryL * og + sL * sg) * outg;
                const float oR = (dryR * og + sR * sg) * outg;
                L[i] = oL;
                R[i] = oR;
                outMs += meterCoeff * (0.5f * (oL * oL + oR * oR) - outMs);
            }
            else
            {
                const float o = (dryL * og + 0.5f * (sL + sR) * sg) * outg;
                L[i] = o;
                outMs += meterCoeff * (o * o - outMs);
            }
        }

        inputLevel.store  (std::sqrt (inMs));
        outputLevel.store (std::sqrt (outMs));
    }

    //--------------------------------------------------------------------------
    // Lecturas para la interfaz
    std::atomic<float>        inputLevel   { 0.f };
    std::atomic<float>        outputLevel  { 0.f };
    std::atomic<juce::uint32> triggerCount { 0 };

private:
    struct Voice
    {
        SamplePtr data;
        double pos = 0.0, inc = 1.0;
        float gain = 0.f;
        bool active = false;
        juce::uint32 age = 0;
    };

    void startVoice (const SamplePtr& data, float gain, double rateMul)
    {
        Voice* target = nullptr;
        for (auto& v : voices)
            if (! v.active) { target = &v; break; }

        if (target == nullptr)   // robar la voz mas antigua
        {
            target = &voices[0];
            for (auto& v : voices)
                if (v.age < target->age) target = &v;
        }

        target->data   = data;
        target->pos    = 0.0;
        target->inc    = (data->sampleRate / sampleRate) * rateMul;
        target->gain   = gain;
        target->active = true;
        target->age    = ++voiceCounter;
    }

    void fireTrigger (float pk)
    {
        const float peakDb = juce::Decibels::gainToDecibels (pk, -100.f);
        float vel = juce::jlimit (0.f, 1.f, (peakDb - curThrDb) / curVelWindowDb);
        vel = 1.f - curDynamics * (1.f - vel);

        std::array<SamplePtr, maxSlots> loaded;
        int n = 0;
        {
            const juce::SpinLock::ScopedLockType sl (slotLock);
            for (auto& s : slots)
                if (s != nullptr) loaded[(size_t) n++] = s;
        }

        triggerCount.fetch_add (1);
        if (n == 0) return;

        const float velRangeDb = (curMode == 0 && n > 1) ? 9.f : 18.f;
        const float g = juce::Decibels::decibelsToGain (-velRangeDb * (1.f - vel)) * (curInvert ? -1.f : 1.f);

        if (curMode == 0)
        {
            const int idx = juce::jmin (n - 1, (int) (vel * (float) n));
            startVoice (loaded[(size_t) idx], g, curRate);
        }
        else if (curMode == 1)
        {
            startVoice (loaded[(size_t) (rrIndex % n)], g, curRate);
            ++rrIndex;
        }
        else
        {
            for (int i = 0; i < n; ++i)
                startVoice (loaded[(size_t) i], g, curRate);
        }
    }

    //--------------------------------------------------------------------------
    double sampleRate = 44100.0;
    int lookahead = 1, writePos = 0;
    std::vector<float> delayL, delayR;

    Biquad hp, lp;
    float lastLow = -1.f, lastHigh = -1.f;

    float env = 0.f, peak = 0.f, envRelease = 0.99f, meterCoeff = 0.001f;
    bool armed = true, scanning = false;
    int cooldown = 0, scanRemaining = 0;

    float inMs = 0.f, outMs = 0.f;

    juce::SmoothedValue<float> gOrig, gSamp, gOut;
    bool snapGains = true;

    float curThrDb = -24.f, curVelWindowDb = 24.f, curDynamics = 1.f;
    int   curMode = 0;
    bool  curInvert = false;
    double curRate = 1.0;

    std::array<Voice, maxVoices> voices;
    juce::uint32 voiceCounter = 0;
    int rrIndex = 0;

    mutable juce::SpinLock slotLock;
    std::array<SamplePtr, maxSlots> slots;
    std::atomic<int> auditionRequest { -1 };
};
