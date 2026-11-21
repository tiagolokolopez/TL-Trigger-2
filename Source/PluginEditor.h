#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/TLLookAndFeel.h"
#include "UI/Widgets.h"

//==============================================================================
// Una fila de la lista de samples: LED, numero, nombre, LOAD y X.
class SlotRow : public juce::Component
{
public:
    SlotRow (TLTriggerAudioProcessor& p, int idx);

    void refresh();
    void setDropHighlight (bool shouldHighlight);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void browse();

    TLTriggerAudioProcessor& processor;
    const int index;

    juce::TextButton loadButton { "LOAD" }, clearButton { "X" };
    TLTriggerAudioProcessor::SlotInfo info;
    juce::Rectangle<int> nameArea;
    bool highlight = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
// Panel de tamaño fijo (1100 x 640). El editor lo escala con una transformacion.
class MainPanel : public juce::Component,
                  public juce::FileDragAndDropTarget,
                  private juce::Timer
{
public:
    static constexpr int designW = 1100;
    static constexpr int designH = 640;

    explicit MainPanel (TLTriggerAudioProcessor&);
    ~MainPanel() override;

    void paint (juce::Graphics&) override;

    // El editor se entera cuando el usuario elige 80 / 100 / 120 %
    std::function<void (int)> onSizeChange;
    void setSizeSelection (int percent);

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    enum KnobId { kThreshold, kDynamics, kRetrig, kLowCut, kHighCut,
                  kOutput, kOrig, kSamples, kTune, kNumKnobs };

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::String name;
        juce::Rectangle<int> rect;
    };

    void setupKnob (Knob& k, const juce::String& paramId, const juce::String& name, juce::Rectangle<int> rect);
    juce::Image makeBackground() const;
    int  slotIndexAt (juce::Point<int> p) const;
    void highlightSlot (int idx);
    void timerCallback() override;

    juce::SharedResourcePointer<tl::FontHolder> fonts;   // debe crearse antes que el resto
    TLTriggerAudioProcessor& processor;
    TLLookAndFeel lnf;
    juce::Image background;

    std::array<Knob, kNumKnobs> knobs;
    VUMeter inMeter, outMeter;
    JewelLamp lamp;
    std::array<std::unique_ptr<SlotRow>, TriggerEngine::maxSlots> slotRows;

    std::array<juce::TextButton, 3> modeButtons;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;

    std::array<juce::TextButton, 3> sizeButtons;

    juce::ToggleButton phaseButton { "INVERT PHASE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> phaseAttachment;

    juce::uint32 lastTriggerCount = 0;
    int lastSlotsVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};

//==============================================================================
class TLTriggerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit TLTriggerAudioProcessorEditor (TLTriggerAudioProcessor&);
    ~TLTriggerAudioProcessorEditor() override = default;

    void resized() override;

private:
    void applyScale (int percent);

    TLTriggerAudioProcessor& audioProcessor;
    MainPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TLTriggerAudioProcessorEditor)
};
