#pragma once

#include <JuceHeader.h>
#include "TriggerEngine.h"

class TLTriggerAudioProcessor : public juce::AudioProcessor
{
public:
    TLTriggerAudioProcessor();
    ~TLTriggerAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TL TRIGGER"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Gestion de samples (hilo de interfaz)
    struct SlotInfo
    {
        juce::String text;
        bool loaded  = false;
        bool missing = false;
    };

    bool loadSampleIntoSlot (int slot, const juce::File& file);
    void clearSlot (int slot);
    SlotInfo getSlotInfo (int slot) const;
    int  getSlotsVersion() const noexcept { return slotsVersion.load(); }
    void auditionSlot (int slot) { engine.audition (slot); }

    // Tamaño de la ventana: 80, 100 o 120 (%). Se guarda con el estado del plugin.
    int getUiScalePercent() const
    {
        const int v = (int) apvts.state.getProperty ("uiScale", 100);
        return (v == 80 || v == 100 || v == 120) ? v : 100;
    }
    void setUiScalePercent (int pct) { apvts.state.setProperty ("uiScale", pct, nullptr); }

    juce::File lastSampleDir;

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "TLTRIGGER", createLayout() };
    TriggerEngine engine;

private:
    std::shared_ptr<SampleData> readSample (const juce::File& file);
    void reloadSlotsFromState();

    juce::AudioFormatManager formatManager;
    std::atomic<int> slotsVersion { 0 };

    std::atomic<float> *pThreshold = nullptr, *pDynamics = nullptr, *pRetrig = nullptr,
                       *pLowCut = nullptr, *pHighCut = nullptr, *pTune = nullptr,
                       *pOrig = nullptr, *pSamples = nullptr, *pOutput = nullptr,
                       *pMode = nullptr, *pPhase = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TLTriggerAudioProcessor)
};
