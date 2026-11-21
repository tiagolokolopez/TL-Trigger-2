#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TLTriggerAudioProcessor::createLayout()
{
    using namespace juce;
    using Attr = AudioParameterFloatAttributes;

    AudioProcessorValueTreeState::ParameterLayout layout;

    auto dbStr   = [] (float v, int) { return String (v, 1) + " dB"; };
    auto dbVal   = [] (const String& t) { return t.getFloatValue(); };
    auto gainStr = [] (float v, int) { return v <= -59.5f ? String ("-inf") : String (v, 1) + " dB"; };
    auto gainVal = [] (const String& t) { return t.containsIgnoreCase ("inf") ? -60.f : t.getFloatValue(); };
    auto hzStr   = [] (float v, int) { return v >= 1000.f ? String (v / 1000.f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto hzVal   = [] (const String& t) { return t.containsIgnoreCase ("k") ? t.getFloatValue() * 1000.f : t.getFloatValue(); };
    auto msStr   = [] (float v, int) { return String (roundToInt (v)) + " ms"; };
    auto msVal   = [] (const String& t) { return t.getFloatValue(); };
    auto pctStr  = [] (float v, int) { return String (roundToInt (v)) + " %"; };
    auto stStr   = [] (float v, int) { return String (v, 2) + " st"; };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "threshold", 1 }, "Threshold",
                    NormalisableRange<float> (-60.f, 0.f, 0.1f), -24.f,
                    Attr().withStringFromValueFunction (dbStr).withValueFromStringFunction (dbVal)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "dynamics", 1 }, "Dynamics",
                    NormalisableRange<float> (0.f, 100.f, 1.f), 100.f,
                    Attr().withStringFromValueFunction (pctStr).withValueFromStringFunction (msVal)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "retrig", 1 }, "Retrig",
                    NormalisableRange<float> (5.f, 250.f, 1.f, 0.5f), 20.f,
                    Attr().withStringFromValueFunction (msStr).withValueFromStringFunction (msVal)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lowcut", 1 }, "Detect Low Cut",
                    NormalisableRange<float> (20.f, 5000.f, 1.f, 0.3f), 20.f,
                    Attr().withStringFromValueFunction (hzStr).withValueFromStringFunction (hzVal)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "highcut", 1 }, "Detect High Cut",
                    NormalisableRange<float> (200.f, 20000.f, 1.f, 0.3f), 20000.f,
                    Attr().withStringFromValueFunction (hzStr).withValueFromStringFunction (hzVal)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "tune", 1 }, "Tune",
                    NormalisableRange<float> (-12.f, 12.f, 0.01f), 0.f,
                    Attr().withStringFromValueFunction (stStr).withValueFromStringFunction (msVal)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "orig", 1 }, "Original",
                    NormalisableRange<float> (-60.f, 6.f, 0.1f), 0.f,
                    Attr().withStringFromValueFunction (gainStr).withValueFromStringFunction (gainVal)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "samples", 1 }, "Samples",
                    NormalisableRange<float> (-60.f, 6.f, 0.1f), 0.f,
                    Attr().withStringFromValueFunction (gainStr).withValueFromStringFunction (gainVal)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                    NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f,
                    Attr().withStringFromValueFunction (dbStr).withValueFromStringFunction (dbVal)));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                    StringArray { "Velocity", "Round Robin", "Layer" }, 0));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "phase", 1 }, "Invert Phase", false));

    return layout;
}

//==============================================================================
TLTriggerAudioProcessor::TLTriggerAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();

    pThreshold = apvts.getRawParameterValue ("threshold");
    pDynamics  = apvts.getRawParameterValue ("dynamics");
    pRetrig    = apvts.getRawParameterValue ("retrig");
    pLowCut    = apvts.getRawParameterValue ("lowcut");
    pHighCut   = apvts.getRawParameterValue ("highcut");
    pTune      = apvts.getRawParameterValue ("tune");
    pOrig      = apvts.getRawParameterValue ("orig");
    pSamples   = apvts.getRawParameterValue ("samples");
    pOutput    = apvts.getRawParameterValue ("output");
    pMode      = apvts.getRawParameterValue ("mode");
    pPhase     = apvts.getRawParameterValue ("phase");
}

bool TLTriggerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void TLTriggerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
}

void TLTriggerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    TriggerEngine::Params p;
    p.thresholdDb = pThreshold->load();
    p.dynamics    = pDynamics->load() * 0.01f;
    p.retrigMs    = pRetrig->load();
    p.lowCutHz    = pLowCut->load();
    p.highCutHz   = pHighCut->load();
    p.tuneSemis   = pTune->load();
    p.originalDb  = pOrig->load();
    p.samplesDb   = pSamples->load();
    p.outputDb    = pOutput->load();
    p.mode        = (int) pMode->load();
    p.invertPhase = pPhase->load() > 0.5f;

    engine.process (buffer, p);
}

//==============================================================================
std::shared_ptr<SampleData> TLTriggerAudioProcessor::readSample (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return nullptr;

    const auto maxLen = (juce::int64) (reader->sampleRate * 15.0);   // maximo 15 s por sample
    const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);
    if (len <= 0) return nullptr;

    auto data = std::make_shared<SampleData>();
    data->audio.setSize (juce::jmin (2, (int) reader->numChannels), len);
    reader->read (&data->audio, 0, len, 0, true, true);
    data->sampleRate = reader->sampleRate;
    data->name = file.getFileNameWithoutExtension();
    data->path = file.getFullPathName();
    return data;
}

bool TLTriggerAudioProcessor::loadSampleIntoSlot (int slot, const juce::File& file)
{
    if (slot < 0 || slot >= TriggerEngine::maxSlots) return false;

    if (auto data = readSample (file))
    {
        engine.setSlot (slot, SamplePtr (data));
        apvts.state.setProperty ("slot" + juce::String (slot), file.getFullPathName(), nullptr);
        ++slotsVersion;
        return true;
    }
    return false;
}

void TLTriggerAudioProcessor::clearSlot (int slot)
{
    if (slot < 0 || slot >= TriggerEngine::maxSlots) return;
    engine.setSlot (slot, nullptr);
    apvts.state.removeProperty ("slot" + juce::String (slot), nullptr);
    ++slotsVersion;
}

TLTriggerAudioProcessor::SlotInfo TLTriggerAudioProcessor::getSlotInfo (int slot) const
{
    SlotInfo info;
    if (auto s = engine.getSlot (slot))
    {
        info.text = s->name;
        info.loaded = true;
        return info;
    }

    const auto path = apvts.state.getProperty ("slot" + juce::String (slot)).toString();
    if (path.isNotEmpty())
    {
        info.text = juce::File (path).getFileNameWithoutExtension();
        info.missing = true;
    }
    return info;
}

void TLTriggerAudioProcessor::reloadSlotsFromState()
{
    for (int i = 0; i < TriggerEngine::maxSlots; ++i)
    {
        const auto path = apvts.state.getProperty ("slot" + juce::String (i)).toString();
        SamplePtr sample;

        if (path.isNotEmpty())
        {
            const juce::File f (path);
            if (f.existsAsFile())
                sample = SamplePtr (readSample (f));
        }
        engine.setSlot (i, sample);
    }
    ++slotsVersion;
}

//==============================================================================
void TLTriggerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TLTriggerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            reloadSlotsFromState();
        }
    }
}

juce::AudioProcessorEditor* TLTriggerAudioProcessor::createEditor()
{
    return new TLTriggerAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TLTriggerAudioProcessor();
}
