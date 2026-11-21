#include "PluginEditor.h"

namespace
{
    bool isAudioFile (const juce::File& f)
    {
        return f.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
    }

    // Linea que se desvanece hacia los lados, para los detalles en rojo
    void drawFadeLine (juce::Graphics& g, float cx, float y, float halfWidth, float thickness,
                       juce::Colour colour)
    {
        juce::ColourGradient left (juce::Colours::transparentBlack, cx - halfWidth, y, colour, cx, y, false);
        g.setGradientFill (left);
        g.fillRect (cx - halfWidth, y, halfWidth, thickness);

        juce::ColourGradient right (colour, cx, y, juce::Colours::transparentBlack, cx + halfWidth, y, false);
        g.setGradientFill (right);
        g.fillRect (cx, y, halfWidth, thickness);
    }
}

//==============================================================================
// SlotRow
//==============================================================================
SlotRow::SlotRow (TLTriggerAudioProcessor& p, int idx) : processor (p), index (idx)
{
    loadButton.onClick  = [this] { browse(); };
    clearButton.onClick = [this] { processor.clearSlot (index); };
    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearButton);
    refresh();
}

void SlotRow::refresh()
{
    info = processor.getSlotInfo (index);
    repaint();
}

void SlotRow::setDropHighlight (bool shouldHighlight)
{
    if (shouldHighlight != highlight)
    {
        highlight = shouldHighlight;
        repaint();
    }
}

void SlotRow::resized()
{
    auto r = getLocalBounds().reduced (6, 7);
    clearButton.setBounds (r.removeFromRight (26));
    r.removeFromRight (4);
    loadButton.setBounds (r.removeFromRight (50));
    r.removeFromRight (6);
    nameArea = r.withTrimmedLeft (38);
}

void SlotRow::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.f);

    g.setColour (juce::Colour (0xff111215));
    g.fillRoundedRectangle (b, 6.f);
    g.setColour (highlight ? tl::redHi : tl::redDeep);
    g.drawRoundedRectangle (b, 6.f, highlight ? 1.8f : 1.f);

    tl::drawLED (g, { b.getX() + 14.f, b.getCentreY() }, 4.f, info.loaded);

    g.setColour (juce::Colour (0xff9a968b));
    g.setFont (tl::font (13.f, false));
    g.drawText (juce::String (index + 1),
                juce::Rectangle<int> ((int) b.getX() + 24, (int) b.getY(), 14, (int) b.getHeight()),
                juce::Justification::centredLeft, false);

    juce::String text;
    if (info.loaded)       { g.setColour (juce::Colour (0xffe9e4d6)); text = info.text; g.setFont (tl::font (13.5f, true)); }
    else if (info.missing) { g.setColour (tl::redHi);                 text = "missing: " + info.text; g.setFont (tl::font (13.5f, true)); }
    else                   { g.setColour (juce::Colour (0xff77746b)); text = "- empty -"; g.setFont (tl::font (14.f, false)); }

    g.drawText (text, nameArea, juce::Justification::centredLeft, true);
}

void SlotRow::mouseDown (const juce::MouseEvent& e)
{
    // clic en el nombre = preescucha del sample
    if (info.loaded && nameArea.contains (e.getPosition()))
        processor.auditionSlot (index);
}

void SlotRow::browse()
{
    chooser = std::make_unique<juce::FileChooser> ("Select a sample", processor.lastSampleDir,
                                                   "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

    juce::Component::SafePointer<SlotRow> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr) return;
                              const auto f = fc.getResult();
                              if (f.existsAsFile())
                              {
                                  safe->processor.lastSampleDir = f.getParentDirectory();
                                  safe->processor.loadSampleIntoSlot (safe->index, f);
                              }
                          });
}

//==============================================================================
// MainPanel
//==============================================================================
MainPanel::MainPanel (TLTriggerAudioProcessor& p)
    : processor (p), inMeter ("INPUT"), outMeter ("OUTPUT")
{
    setLookAndFeel (&lnf);
    background = makeBackground();

    using R = juce::Rectangle<int>;

    // columna izquierda
    setupKnob (knobs[kThreshold], "threshold", "THRESHOLD", R (60,  82, 180, 198));
    setupKnob (knobs[kDynamics],  "dynamics",  "DYNAMICS",  R (30, 328, 110, 128));
    setupKnob (knobs[kRetrig],    "retrig",    "RETRIG",    R (160, 328, 110, 128));
    setupKnob (knobs[kLowCut],    "lowcut",    "LOW CUT",   R (30, 488, 110, 128));
    setupKnob (knobs[kHighCut],   "highcut",   "HIGH CUT",  R (160, 488, 110, 128));

    // columna derecha
    setupKnob (knobs[kOutput],  "output",  "OUTPUT",   R (860,  82, 180, 198));
    setupKnob (knobs[kOrig],    "orig",    "ORIGINAL", R (830, 328, 110, 128));
    setupKnob (knobs[kSamples], "samples", "SAMPLES",  R (960, 328, 110, 128));
    setupKnob (knobs[kTune],    "tune",    "TUNE",     R (830, 488, 110, 128));

    // vumetros
    inMeter.setBounds  (300, 60, 245, 175);
    outMeter.setBounds (555, 60, 245, 175);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    // lista de samples
    for (int i = 0; i < TriggerEngine::maxSlots; ++i)
    {
        slotRows[(size_t) i] = std::make_unique<SlotRow> (processor, i);
        const int col = i / 4, row = i % 4;
        slotRows[(size_t) i]->setBounds (308 + col * 244, 278 + row * 46, 240, 46);
        addAndMakeVisible (*slotRows[(size_t) i]);
    }

    // modo de disparo
    const char* modeNames[3] = { "VELOCITY", "ROUND ROBIN", "LAYER" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeButtons[(size_t) i];
        b.setButtonText (modeNames[i]);
        b.getProperties().set ("led", true);
        b.setBounds (330 + i * 150, 502, 140, 30);
        b.onClick = [this, i] { modeAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    modeAttachment = std::make_unique<juce::ParameterAttachment> (
        *processor.apvts.getParameter ("mode"),
        [this] (float v)
        {
            const int m = juce::roundToInt (v);
            for (int i = 0; i < 3; ++i)
                modeButtons[(size_t) i].setToggleState (i == m, juce::dontSendNotification);
        });
    modeAttachment->sendInitialUpdate();

    // tamaño de la ventana: 80 / 100 / 120 %
    const int percents[3] = { 80, 100, 120 };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = sizeButtons[(size_t) i];
        b.setButtonText (juce::String (percents[i]) + "%");
        b.setBounds (934 + i * 50, 14, 46, 24);
        b.onClick = [this, i, percents]
        {
            setSizeSelection (percents[i]);
            if (onSizeChange) onSizeChange (percents[i]);
        };
        addAndMakeVisible (b);
    }

    // fase
    phaseButton.setBounds (958, 540, 130, 30);
    addAndMakeVisible (phaseButton);
    phaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "phase", phaseButton);

    // lampara de disparo
    lamp.setBounds (516, 549, 68, 68);
    addAndMakeVisible (lamp);

    setSize (designW, designH);
    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MainPanel::setSizeSelection (int percent)
{
    const int percents[3] = { 80, 100, 120 };
    for (int i = 0; i < 3; ++i)
        sizeButtons[(size_t) i].setToggleState (percents[i] == percent, juce::dontSendNotification);
}

void MainPanel::setupKnob (Knob& k, const juce::String& paramId, const juce::String& name,
                           juce::Rectangle<int> rect)
{
    k.name = name;
    k.rect = rect;

    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                  juce::MathConstants<float>::pi * 2.8f, true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    k.slider.setMouseDragSensitivity (200);
    k.slider.setBounds (rect);
    addAndMakeVisible (k.slider);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, paramId, k.slider);
}

juce::Image MainPanel::makeBackground() const
{
    juce::Image img (juce::Image::RGB, designW, designH, true);
    juce::Graphics g (img);

    juce::ColourGradient base (juce::Colour (0xff202126), 0.f, 0.f,
                               juce::Colour (0xff0e0f11), 0.f, (float) designH, false);
    g.setGradientFill (base);
    g.fillAll();

    // acabado cepillado
    juce::Random rng (1234);
    for (int y = 0; y < designH; ++y)
    {
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black)
                         .withAlpha (rng.nextFloat() * 0.035f));
        g.drawHorizontalLine (y, 0.f, (float) designW);
    }

    // orejas de rack
    g.setColour (juce::Colours::black);
    for (int y : { 50, 580 })
    {
        g.fillRoundedRectangle (14.f, (float) y, 28.f, 9.f, 4.5f);
        g.fillRoundedRectangle ((float) designW - 42.f, (float) y, 28.f, 9.f, 4.5f);
    }

    g.setColour (juce::Colour (0xff000000));
    g.drawRect (0, 0, designW, designH, 2);

    // filetes rojos arriba y abajo
    drawFadeLine (g, (float) designW * 0.5f, 3.f, (float) designW * 0.5f, 1.5f, tl::redMid);
    drawFadeLine (g, (float) designW * 0.5f, (float) designH - 5.f, (float) designW * 0.5f, 1.5f, tl::redDark);

    return img;
}

void MainPanel::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);

    // titulo en Acidic, con sombra y resplandor rojo
    const juce::Rectangle<int> titleArea (300, 4, 500, 48);
    g.setFont (tl::titleFont (46.f));
    g.setColour (tl::redDark.withAlpha (0.6f));
    g.drawText ("TL TRIGGER", titleArea.translated (0, 3), juce::Justification::centred, false);
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawText ("TL TRIGGER", titleArea.translated (2, 2), juce::Justification::centred, false);
    g.setColour (tl::cream);
    g.drawText ("TL TRIGGER", titleArea, juce::Justification::centred, false);

    drawFadeLine (g, 550.f, 54.f, 200.f, 1.5f, tl::red);

    // selector de tamaño
    g.setFont (tl::font (12.5f, true).withExtraKerningFactor (0.05f));
    g.setColour (juce::Colour (0xff9a968b));
    g.drawText ("WINDOW SIZE", juce::Rectangle<int> (806, 14, 122, 24), juce::Justification::centredRight, false);

    // nombres de las perillas
    g.setFont (tl::font (14.f, true).withExtraKerningFactor (0.05f));
    g.setColour (juce::Colour (0xffbdb8aa));
    for (const auto& k : knobs)
        g.drawText (k.name, k.rect.getX(), k.rect.getY() - 21, k.rect.getWidth(), 18,
                    juce::Justification::centred, false);

    // panel de samples
    const juce::Rectangle<float> panel (300.f, 252.f, 500.f, 224.f);
    g.setColour (juce::Colour (0xff0a0a0c));
    g.fillRoundedRectangle (panel, 8.f);
    g.setColour (tl::redDark.withAlpha (0.7f));
    g.drawRoundedRectangle (panel, 8.f, 1.2f);

    g.setFont (tl::font (13.f, false));
    g.setColour (juce::Colour (0xffa29e92));
    g.drawText ("SAMPLES  -  DROP AUDIO FILES HERE  -  CLICK A NAME TO PREVIEW",
                juce::Rectangle<int> (310, 255, 480, 20), juce::Justification::centred, false);

    g.setFont (tl::font (13.f, true).withExtraKerningFactor (0.06f));
    g.setColour (juce::Colour (0xffbdb8aa));
    g.drawText ("TRIGGER MODE", juce::Rectangle<int> (300, 481, 500, 18), juce::Justification::centred, false);

    // rotulos inferiores en Acidic, tono rojo
    g.setFont (tl::titleFont (21.f));
    const juce::Rectangle<int> leftText (310, 568, 200, 30), rightText (596, 568, 199, 30);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawText ("DRUM TRIGGER",    leftText.translated (1, 2),  juce::Justification::centredLeft,  false);
    g.drawText ("SAMPLE REPLACER", rightText.translated (1, 2), juce::Justification::centredRight, false);
    g.setColour (tl::redMid);
    g.drawText ("DRUM TRIGGER",    leftText,  juce::Justification::centredLeft,  false);
    g.drawText ("SAMPLE REPLACER", rightText, juce::Justification::centredRight, false);
}

//==============================================================================
int MainPanel::slotIndexAt (juce::Point<int> p) const
{
    for (int i = 0; i < TriggerEngine::maxSlots; ++i)
        if (slotRows[(size_t) i]->getBounds().contains (p))
            return i;
    return -1;
}

void MainPanel::highlightSlot (int idx)
{
    for (int i = 0; i < TriggerEngine::maxSlots; ++i)
        slotRows[(size_t) i]->setDropHighlight (i == idx);
}

bool MainPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (isAudioFile (juce::File (f)))
            return true;
    return false;
}

void MainPanel::fileDragMove (const juce::StringArray&, int x, int y)
{
    highlightSlot (slotIndexAt ({ x, y }));
}

void MainPanel::fileDragExit (const juce::StringArray&)
{
    highlightSlot (-1);
}

void MainPanel::filesDropped (const juce::StringArray& files, int x, int y)
{
    highlightSlot (-1);

    int slot = slotIndexAt ({ x, y });
    if (slot < 0)   // soltado fuera de la lista: primer hueco libre
    {
        for (int i = 0; i < TriggerEngine::maxSlots; ++i)
            if (! processor.getSlotInfo (i).loaded) { slot = i; break; }
        if (slot < 0) slot = 0;
    }

    for (const auto& path : files)
    {
        const juce::File f (path);
        if (! isAudioFile (f)) continue;
        if (slot >= TriggerEngine::maxSlots) break;
        if (processor.loadSampleIntoSlot (slot, f)) ++slot;
    }
}

//==============================================================================
void MainPanel::timerCallback()
{
    inMeter.setLevel  (processor.engine.inputLevel.load());
    outMeter.setLevel (processor.engine.outputLevel.load());

    const auto tc = processor.engine.triggerCount.load();
    if (tc != lastTriggerCount)
    {
        lastTriggerCount = tc;
        lamp.flash();
    }
    lamp.tick();

    const int v = processor.getSlotsVersion();
    if (v != lastSlotsVersion)
    {
        lastSlotsVersion = v;
        for (auto& row : slotRows) row->refresh();
    }
}

//==============================================================================
// Editor
//==============================================================================
TLTriggerAudioProcessorEditor::TLTriggerAudioProcessorEditor (TLTriggerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (false, false);

    const int percent = audioProcessor.getUiScalePercent();
    panel.setSizeSelection (percent);
    panel.onSizeChange = [this] (int newPercent)
    {
        audioProcessor.setUiScalePercent (newPercent);
        applyScale (newPercent);
    };

    applyScale (percent);
}

// 120 % = tamaño de diseño (1100 x 640); 100 % y 80 % son proporcionales
void TLTriggerAudioProcessorEditor::applyScale (int percent)
{
    const float s = (float) percent / 120.f;
    setSize (juce::roundToInt ((float) MainPanel::designW * s),
             juce::roundToInt ((float) MainPanel::designH * s));
}

void TLTriggerAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) MainPanel::designW;
    panel.setTransform (juce::AffineTransform::scale (s));
    panel.setBounds (0, 0, MainPanel::designW, MainPanel::designH);
}
