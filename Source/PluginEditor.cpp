#include "PluginEditor.h"

#include <cmath>
#include <utility>

namespace
{
constexpr auto bodyBackground = 0xff101318;
}

LufsMeterPlusAudioProcessorEditor::LufsMeterPlusAudioProcessorEditor (LufsMeterPlusAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (1250, 690);
    displayedRtaBandDb.fill (-80.0f);
    averageRtaBandDb.fill (-80.0f);
    lastTimerSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;

    titleLabel.setText ("LUFS Meter Plus", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (20.0f));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff111419));
    addAndMakeVisible (titleLabel);

    resetButton.setButtonText ("RESET");
    resetButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffd6d9dd));
    resetButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffc7ccd2));
    resetButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff111419));
    resetButton.onClick = [this]
    {
        audioProcessor.requestMeasurementReset();
        // Clear immediately so the button feels instant; the generation we just
        // bumped is recorded so timerCallback doesn't clear a second time.
        resetGraphState();
        lastResetGeneration = audioProcessor.resetGeneration.load();
    };
    addAndMakeVisible (resetButton);

    configureModeButton (rtaModeButton, "RTA");
    configureModeButton (lufsModeButton, "LUFS");
    rtaModeButton.onClick = [this] { showRtaView = true; updateModeButtons(); repaint (analysisBounds); };
    lufsModeButton.onClick = [this] { showRtaView = false; updateModeButtons(); repaint (analysisBounds); };
    updateModeButtons();

    configureReadout (integratedLufsLabel, "INTEGRATED");
    configureReadout (momentaryLufsLabel, "MOMENTARY");
    configureReadout (shortTermLufsLabel, "SHORT TERM");
    configureReadout (loudnessUnitsLabel, "RANGE");
    configureReadout (truePeakLabel, "TRUE PEAK");
    configureReadout (gainReductionLabel, "GAIN REDUCTION");

    configureSlider (thresholdSlider, thresholdLabel, "Threshold", " dB");
    configureSlider (ceilingSlider, ceilingLabel, "Ceiling", " dB");
    configureSlider (releaseSlider, releaseLabel, "Release", " ms");
    configureComboBox (rtaSlopeComboBox, rtaSlopeLabel, "RTA Slope");

    thresholdAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "threshold", thresholdSlider);
    ceilingAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "ceiling", ceilingSlider);
    releaseAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "release", releaseSlider);
    rtaSlopeAttachment = std::make_unique<ComboBoxAttachment> (audioProcessor.parameters, "rtaSlope", rtaSlopeComboBox);

    startTimerHz (10);
}

void LufsMeterPlusAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff5b5e61));

    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop (40);
    auto footer = bounds.removeFromBottom (64);

    g.setColour (juce::Colour (0xffeceff2));
    g.fillRect (header);
    g.setColour (juce::Colour (0xffc9cdd2));
    g.fillRect (header.removeFromBottom (1));

    g.setColour (juce::Colour (bodyBackground));
    g.fillRect (bounds);

    g.setColour (juce::Colour (0xffeceff2));
    g.fillRect (footer);
    g.setColour (juce::Colour (0xffc9cdd2));
    g.fillRect (footer.removeFromTop (1));

    drawLevelMeter (g, levelMeterBounds);
    drawReadoutStack (g, readoutStackBounds);

    if (showRtaView)
        drawRta (g, analysisBounds);
    else
        drawLufsGraph (g, analysisBounds);

    g.setColour (juce::Colours::white);
    g.fillRect (bottomPanelBounds);
}

void LufsMeterPlusAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (40).reduced (18, 6);
    auto footer = area.removeFromBottom (64).reduced (18, 8);
    auto body = area.reduced (18, 16);

    titleLabel.setBounds (header.withSizeKeepingCentre (360, header.getHeight()));
    resetButton.setBounds (header.removeFromRight (64).withSizeKeepingCentre (60, 22));

    levelMeterBounds = body.removeFromLeft (150);
    body.removeFromLeft (18);
    readoutStackBounds = body.removeFromLeft (164);
    body.removeFromLeft (20);

    analysisBounds = body.reduced (0, 8);
    // Mode buttons overlaid in the top-right corner of the graph
    lufsModeButton.setBounds (analysisBounds.getRight() - 76, analysisBounds.getY() + 8, 68, 22);
    rtaModeButton.setBounds (analysisBounds.getRight() - 76 - 72, analysisBounds.getY() + 8, 68, 22);

    bottomPanelBounds = footer;
    auto controls = bottomPanelBounds.reduced (22, 10);

    auto layoutSlider = [] (juce::Rectangle<int> row, juce::Label& label, juce::Slider& slider)
    {
        label.setBounds (row.removeFromLeft (94));
        slider.setBounds (row);
    };

    auto layoutComboBox = [] (juce::Rectangle<int> row, juce::Label& label, juce::ComboBox& comboBox)
    {
        label.setBounds (row.removeFromLeft (78));
        comboBox.setBounds (row.reduced (0, 2));
    };

    const auto controlWidth = controls.getWidth() / 4;
    layoutSlider (controls.removeFromLeft (controlWidth).withTrimmedRight (12), thresholdLabel, thresholdSlider);
    layoutSlider (controls.removeFromLeft (controlWidth).reduced (6, 0), ceilingLabel, ceilingSlider);
    layoutSlider (controls.removeFromLeft (controlWidth).reduced (6, 0), releaseLabel, releaseSlider);
    layoutComboBox (controls.withTrimmedLeft (12), rtaSlopeLabel, rtaSlopeComboBox);
}

void LufsMeterPlusAudioProcessorEditor::resetGraphState()
{
    shortTermHistory.clear();
    longTermHistory.clear();
    displayedTruePeakDb.fill (-100.0f);
    displayedIntegratedLufs = -70.0f;
    displayedMomentaryLufs = -70.0f;
    displayedShortTermLufs = -70.0f;
    displayedLoudnessRangeLu = 0.0f;
    displayedGainReductionDb = 0.0f;
    displayedRtaBandDb.fill (-80.0f);
    averageRtaBandDb.fill (-80.0f);
    rtaAverageFrameCount = 0;
    graphElapsedSeconds = 0.0;
    lastTimerSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
}

void LufsMeterPlusAudioProcessorEditor::timerCallback()
{
    // A reset may have arrived remotely (monitor app over UDP) without going
    // through the button; clear the graph history when the generation changes.
    const auto resetGen = audioProcessor.resetGeneration.load();
    if (resetGen != lastResetGeneration)
    {
        lastResetGeneration = resetGen;
        resetGraphState();
    }

    const auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    graphElapsedSeconds += juce::jmax (0.0, nowSeconds - lastTimerSeconds);
    lastTimerSeconds = nowSeconds;

    const auto longTerm = audioProcessor.integratedLufs.load();
    const auto momentary = audioProcessor.momentaryLufs.load();
    const auto shortTerm = audioProcessor.shortTermLufs.load();
    const auto targetLufs = audioProcessor.getTargetLufs();
    const auto alertColour = juce::Colour (0xffff5a5f);
    const auto normalColour = juce::Colours::white;

    displayedTruePeakDb[0] = smoothMeterValue (displayedTruePeakDb[0], audioProcessor.truePeakDb[0].load());
    displayedTruePeakDb[1] = smoothMeterValue (displayedTruePeakDb[1], audioProcessor.truePeakDb[1].load());
    displayedIntegratedLufs = smoothMeterValue (displayedIntegratedLufs, longTerm);
    displayedMomentaryLufs = smoothMeterValue (displayedMomentaryLufs, momentary);
    displayedShortTermLufs = smoothMeterValue (displayedShortTermLufs, shortTerm);
    displayedLoudnessRangeLu += (audioProcessor.loudnessRangeLu.load() - displayedLoudnessRangeLu) * 0.25f;
    displayedGainReductionDb += (audioProcessor.gainReductionDb.load() - displayedGainReductionDb) * 0.35f;

    auto hasRtaSignal = false;

    for (auto band = 0; band < LufsMeterPlusAudioProcessor::rtaBandCount; ++band)
    {
        const auto target = audioProcessor.rtaBandDb[static_cast<size_t> (band)].load();
        if (target > -78.0f)
            hasRtaSignal = true;
    }

    for (auto band = 0; band < LufsMeterPlusAudioProcessor::rtaBandCount; ++band)
    {
        const auto target = audioProcessor.rtaBandDb[static_cast<size_t> (band)].load();
        auto& displayed = displayedRtaBandDb[static_cast<size_t> (band)];
        auto& average = averageRtaBandDb[static_cast<size_t> (band)];

        const auto response = target > displayed ? 0.78f : 0.52f;
        displayed += (target - displayed) * response;

        if (hasRtaSignal)
        {
            if (rtaAverageFrameCount == 0)
                average = target;
            else
                average += (target - average) / static_cast<float> (rtaAverageFrameCount + 1);
        }
    }

    if (hasRtaSignal)
        rtaAverageFrameCount = juce::jmin (rtaAverageFrameCount + 1, 36000);

    integratedLufsLabel.setText (formatDb (displayedIntegratedLufs, " LUFS"), juce::dontSendNotification);
    momentaryLufsLabel.setText (formatDb (displayedMomentaryLufs, " LUFS"), juce::dontSendNotification);
    shortTermLufsLabel.setText (formatDb (displayedShortTermLufs, " LUFS"), juce::dontSendNotification);
    loudnessUnitsLabel.setText (formatDb (displayedLoudnessRangeLu, " LU"), juce::dontSendNotification);
    
    const auto truePeakMax = juce::jmax (displayedTruePeakDb[0], displayedTruePeakDb[1]);
    truePeakLabel.setText (formatDb (truePeakMax, " dBFS"), juce::dontSendNotification);
    
    gainReductionLabel.setText (formatDb (displayedGainReductionDb, " dB"), juce::dontSendNotification);
    integratedLufsLabel.setColour (juce::Label::textColourId, longTerm > targetLufs ? alertColour : normalColour);
    momentaryLufsLabel.setColour (juce::Label::textColourId, momentary > targetLufs ? alertColour : normalColour);
    shortTermLufsLabel.setColour (juce::Label::textColourId, shortTerm > targetLufs ? alertColour : normalColour);

    shortTermHistory.push_back (shortTerm);
    longTermHistory.push_back (longTerm);

    repaint (levelMeterBounds.expanded (8));
    repaint (targetInfoBounds.expanded (2));
    repaint (readoutStackBounds.expanded (4));
    repaint (analysisBounds.expanded (2));
}

void LufsMeterPlusAudioProcessorEditor::configureReadout (juce::Label& label, const juce::String&)
{
    label.setFont (juce::FontOptions (23.0f));
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (label);
}

void LufsMeterPlusAudioProcessorEditor::configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& name, const juce::String& suffix)
{
    label.setText (name, juce::dontSendNotification);
    label.setFont (juce::FontOptions (12.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff111419));
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 18);
    slider.setTextValueSuffix (suffix);
    slider.setColour (juce::Slider::trackColourId, juce::Colour (0xff5479b7));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff111419));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff111419));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);
}

void LufsMeterPlusAudioProcessorEditor::configureComboBox (juce::ComboBox& comboBox, juce::Label& label, const juce::String& name)
{
    label.setText (name, juce::dontSendNotification);
    label.setFont (juce::FontOptions (12.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff111419));
    addAndMakeVisible (label);

    comboBox.addItem ("0 dB/oct", 1);
    comboBox.addItem ("3 dB/oct", 2);
    comboBox.addItem ("4.5 dB/oct", 3);
    comboBox.addItem ("6 dB/oct", 4);
    comboBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::white);
    comboBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xff111419));
    comboBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    comboBox.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff111419));
    addAndMakeVisible (comboBox);
}

void LufsMeterPlusAudioProcessorEditor::configureModeButton (juce::TextButton& button, const juce::String& text)
{
    button.setButtonText (text);
    button.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff242b34));
    button.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff5479b7));
    button.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffcbd3dd));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    button.setClickingTogglesState (false);
    addAndMakeVisible (button);
}

void LufsMeterPlusAudioProcessorEditor::drawLevelMeter (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (bounds.isEmpty())
        return;

    auto labelArea = bounds.removeFromBottom (40);
    auto meter = bounds.reduced (14, 16);
    const auto channelLabelRow = meter.removeFromTop (20);
    const auto scale = meter.removeFromLeft (38);

    // Two stereo bars (L and R) with a small gap so they read as two channels
    // without drifting too far apart.
    const auto barWidth = 20;
    const auto barSpacing = 0;
    const auto totalBarWidth = barWidth * 2 + barSpacing;
    auto barAreaCenter = meter.withWidth (totalBarWidth).withCentre ({ meter.getCentreX(), meter.getCentreY() });
    
    // Left channel bar
    auto barAreaL = barAreaCenter.removeFromLeft (barWidth);
    auto innerBarAreaL = barAreaL.reduced (2, 4);
    const auto truePeakL = juce::jlimit (-60.0f, 0.0f, displayedTruePeakDb[0]);
    const auto normalizedL = juce::jmap (truePeakL, -60.0f, 0.0f, 0.0f, 1.0f);
    const auto fillHeightL = static_cast<float> (innerBarAreaL.getHeight()) * normalizedL;
    
    // Right channel bar (skip spacing)
    barAreaCenter.removeFromLeft (barSpacing);
    auto barAreaR = barAreaCenter.removeFromLeft (barWidth);
    auto innerBarAreaR = barAreaR.reduced (2, 4);
    const auto truePeakR = juce::jlimit (-60.0f, 0.0f, displayedTruePeakDb[1]);
    const auto normalizedR = juce::jmap (truePeakR, -60.0f, 0.0f, 0.0f, 1.0f);
    const auto fillHeightR = static_cast<float> (innerBarAreaR.getHeight()) * normalizedR;

    g.setColour (juce::Colour (bodyBackground));
    g.fillRect (bounds);

    for (auto db : { -3, -9, -15, -21, -30, -42, -54 })
    {
        const auto y = juce::jmap (static_cast<float> (db), -60.0f, 0.0f, static_cast<float> (barAreaL.getBottom()), static_cast<float> (barAreaL.getY()));
        g.setColour (juce::Colour (0x4538414c));
        g.drawHorizontalLine (static_cast<int> (std::round (y)), static_cast<float> (scale.getRight() - 5), static_cast<float> (barAreaR.getRight() + 5));
    }

    g.setFont (juce::FontOptions (10.5f));

    for (auto db : { 0, -6, -12, -18, -24, -36, -48, -60 })
    {
        const auto y = juce::jmap (static_cast<float> (db), -60.0f, 0.0f, static_cast<float> (barAreaL.getBottom()), static_cast<float> (barAreaL.getY()));
        g.setColour (db >= -6 ? juce::Colour (0xffc4ccd5) : juce::Colour (0xff8d98a5));
        g.drawText (juce::String (db), scale.getX(), static_cast<int> (std::round (y)) - 8, scale.getWidth() - 8, 16, juce::Justification::centredRight);
        g.setColour (db >= -6 ? juce::Colour (0xff4b5664) : juce::Colour (0xff38414c));
        g.drawHorizontalLine (static_cast<int> (std::round (y)), static_cast<float> (scale.getRight() - 8), static_cast<float> (barAreaR.getRight() + 8));
    }

    // Draw left channel bar background and fill
    g.setColour (juce::Colour (0xff080b0f));
    g.fillRoundedRectangle (barAreaL.toFloat(), 3.0f);
    g.setColour (juce::Colour (0xff202733));
    g.fillRoundedRectangle (innerBarAreaL.toFloat(), 2.0f);

    auto fillL = innerBarAreaL.toFloat();
    fillL.setY (static_cast<float> (innerBarAreaL.getBottom()) - fillHeightL);
    fillL.setHeight (fillHeightL);
    const auto fillColourL = truePeakL >= -1.0f ? juce::Colour (0xffff5a5f)
                           : truePeakL >= -6.0f ? juce::Colour (0xffffcf5a)
                                                : juce::Colour (0xff56a5ff);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff56a5ff),
                                            fillL.getCentreX(),
                                            fillL.getBottom(),
                                            fillColourL,
                                            fillL.getCentreX(),
                                            fillL.getY(),
                                            false));
    g.fillRoundedRectangle (fillL, 2.0f);
    
    // Draw right channel bar background and fill
    g.setColour (juce::Colour (0xff080b0f));
    g.fillRoundedRectangle (barAreaR.toFloat(), 3.0f);
    g.setColour (juce::Colour (0xff202733));
    g.fillRoundedRectangle (innerBarAreaR.toFloat(), 2.0f);

    auto fillR = innerBarAreaR.toFloat();
    fillR.setY (static_cast<float> (innerBarAreaR.getBottom()) - fillHeightR);
    fillR.setHeight (fillHeightR);
    const auto fillColourR = truePeakR >= -1.0f ? juce::Colour (0xffff5a5f)
                           : truePeakR >= -6.0f ? juce::Colour (0xffffcf5a)
                                                : juce::Colour (0xff56a5ff);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff56a5ff),
                                            fillR.getCentreX(),
                                            fillR.getBottom(),
                                            fillColourR,
                                            fillR.getCentreX(),
                                            fillR.getY(),
                                            false));
    g.fillRoundedRectangle (fillR, 2.0f);

    g.setColour (juce::Colour (0x3388929e));
    for (auto db : { -12, -24, -36, -48 })
    {
        const auto y = juce::jmap (static_cast<float> (db), -60.0f, 0.0f, static_cast<float> (innerBarAreaL.getBottom()), static_cast<float> (innerBarAreaL.getY()));
        g.drawHorizontalLine (static_cast<int> (std::round (y)), static_cast<float> (innerBarAreaL.getX()), static_cast<float> (innerBarAreaR.getRight()));
    }

    g.setColour (juce::Colour (0xffe6ebf2));
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("L", barAreaL.withY (channelLabelRow.getY()).withHeight (channelLabelRow.getHeight()), juce::Justification::centred);
    g.drawText ("R", barAreaR.withY (channelLabelRow.getY()).withHeight (channelLabelRow.getHeight()), juce::Justification::centred);
    g.setColour (juce::Colour (0xffaab3bf));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("dBFS", labelArea.withX (barAreaL.getX()).withWidth (barAreaR.getRight() - barAreaL.getX()).withHeight (16), juce::Justification::centred);
}

void LufsMeterPlusAudioProcessorEditor::drawReadoutStack (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto rowHeight = 59;
    auto rows = bounds.reduced (0, 6);

    struct Readout
    {
        juce::Label* value;
        const char* title;
        juce::Colour colour;
    };

    const Readout readouts[] =
    {
        { &momentaryLufsLabel, "MOMENTARY", juce::Colour (0xff303740) },
        { &shortTermLufsLabel, "SHORT TERM", juce::Colour (0xff303740) },
        { &integratedLufsLabel, "INTEGRATED", juce::Colour (0xff4f8eaa) },
        { &loudnessUnitsLabel, "LOUDNESS RANGE", juce::Colour (0xff303740) },
        { &truePeakLabel, "TRUE PEAK", juce::Colour (0xff303740) },
        { &gainReductionLabel, "GAIN REDUCTION", juce::Colour (0xff303740) }
    };

    for (const auto& readout : readouts)
    {
        auto row = rows.removeFromTop (rowHeight);
        g.setColour (readout.colour);
        g.fillRect (row.reduced (0, 1));
        readout.value->setBounds (row.removeFromTop (36));

        g.setColour (juce::Colour (0xffdce3ec));
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (readout.title, row, juce::Justification::centred);
    }

    targetInfoBounds = rows.removeFromTop (26).withTrimmedTop (8);
    const auto targetText = "Target " + juce::String (audioProcessor.getTargetLufs(), 1) + " LUFS";
    const auto iconDiameter = 10.0f;
    const auto textWidth = 108.0f;
    const auto groupWidth = iconDiameter + 5.0f + textWidth;
    const auto iconX = static_cast<float> (targetInfoBounds.getCentreX()) - (groupWidth * 0.5f);
    const auto iconY = static_cast<float> (targetInfoBounds.getCentreY()) - (iconDiameter * 0.5f);

    g.setColour (juce::Colour (0x88aeb7c2));
    g.drawEllipse (iconX, iconY, iconDiameter, iconDiameter, 1.0f);
    g.setFont (juce::FontOptions (7.0f));
    g.drawText ("i", static_cast<int> (std::round (iconX)), targetInfoBounds.getY(), static_cast<int> (iconDiameter), targetInfoBounds.getHeight(), juce::Justification::centred);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText (targetText,
                static_cast<int> (std::round (iconX + iconDiameter + 5.0f)),
                targetInfoBounds.getY(),
                static_cast<int> (textWidth),
                targetInfoBounds.getHeight(),
                juce::Justification::centredLeft);

}

void LufsMeterPlusAudioProcessorEditor::drawGraphGrid (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour (juce::Colour (bodyBackground));
    g.fillRect (bounds);

    // Mirror drawLufsGraph's plot area exactly so the gridlines and dB labels
    // line up with the plotted data and the target line.
    const auto plotTop = static_cast<float> (bounds.getY() + 32);
    const auto plotBottom = static_cast<float> (bounds.getBottom() - 32);
    const auto lineLeft = static_cast<float> (bounds.getX() + 54);
    const auto lineRight = static_cast<float> (bounds.getRight() - 18);

    for (auto db : { 0, -3, -6, -9, -18, -23, -27, -36, -45, -54 })
    {
        const auto y = juce::jmap (static_cast<float> (db), -60.0f, 0.0f, plotBottom, plotTop);
        g.setColour (juce::Colour (0xff27303a));
        g.drawHorizontalLine (static_cast<int> (std::round (y)), lineLeft, lineRight);
        g.setColour (juce::Colour (0xff88929e));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::String (db), bounds.getX() + 12, static_cast<int> (std::round (y)) - 8, 34, 16, juce::Justification::centredRight);
    }
}

void LufsMeterPlusAudioProcessorEditor::drawRta (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour (juce::Colour (bodyBackground));
    g.fillRect (bounds);

    auto plotBounds = bounds.withTrimmedLeft (54).withTrimmedRight (18).reduced (0, 32).toFloat();
    const auto floorDb = -80.0f;
    const auto ceilingDb = 0.0f;
    const auto minFrequency = 20.0f;
    const auto maxFrequency = 20000.0f;
    const auto frequencyRatio = maxFrequency / minFrequency;

    g.setColour (juce::Colour (0xff27303a));
    for (auto db : { 0, -10, -20, -30, -40, -50, -60, -70, -80 })
    {
        const auto y = juce::jmap (static_cast<float> (db), floorDb, ceilingDb, plotBounds.getBottom(), plotBounds.getY());
        g.drawHorizontalLine (static_cast<int> (std::round (y)), plotBounds.getX(), plotBounds.getRight());
        g.setColour (juce::Colour (0xff88929e));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::String (db), bounds.getX() + 12, static_cast<int> (std::round (y)) - 8, 34, 16, juce::Justification::centredRight);
        g.setColour (juce::Colour (0xff27303a));
    }

    for (const auto frequency : { 20.0f, 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        const auto x = juce::jmap (std::log10 (frequency), std::log10 (minFrequency), std::log10 (maxFrequency),
                                   plotBounds.getX(), plotBounds.getRight());
        g.drawVerticalLine (static_cast<int> (std::round (x)), plotBounds.getY(), plotBounds.getBottom());
    }

    auto makeSpectrumPath = [plotBounds, floorDb, ceilingDb, minFrequency, maxFrequency, frequencyRatio] (const auto& bands)
    {
        std::array<juce::Point<float>, LufsMeterPlusAudioProcessor::rtaBandCount> points {};

        for (auto band = 0; band < LufsMeterPlusAudioProcessor::rtaBandCount; ++band)
        {
            const auto bandCentreRatio = (static_cast<float> (band) + 0.5f) / static_cast<float> (LufsMeterPlusAudioProcessor::rtaBandCount);
            const auto frequency = minFrequency * std::pow (frequencyRatio, bandCentreRatio);
            const auto x = juce::jmap (std::log10 (frequency), std::log10 (minFrequency), std::log10 (maxFrequency),
                                       plotBounds.getX(), plotBounds.getRight());
            const auto y = juce::jmap (juce::jlimit (floorDb, ceilingDb, bands[static_cast<size_t> (band)]),
                                       floorDb, ceilingDb, plotBounds.getBottom(), plotBounds.getY());
            points[static_cast<size_t> (band)] = { x, y };
        }

        juce::Path path;
        path.startNewSubPath (points.front());

        for (auto band = 0; band < LufsMeterPlusAudioProcessor::rtaBandCount - 1; ++band)
        {
            const auto current = points[static_cast<size_t> (band)];
            const auto next = points[static_cast<size_t> (band + 1)];
            const auto previous = band > 0 ? points[static_cast<size_t> (band - 1)] : current;
            const auto afterNext = band < LufsMeterPlusAudioProcessor::rtaBandCount - 2 ? points[static_cast<size_t> (band + 2)] : next;

            const auto control1 = current + ((next - previous) / 6.0f);
            const auto control2 = next - ((afterNext - current) / 6.0f);
            path.cubicTo (control1, control2, next);
        }

        return std::pair<juce::Path, std::array<juce::Point<float>, LufsMeterPlusAudioProcessor::rtaBandCount>> { path, points };
    };

    auto [averagePath, averagePoints] = makeSpectrumPath (averageRtaBandDb);
    auto [spectrumPath, points] = makeSpectrumPath (displayedRtaBandDb);

    auto fillPath = spectrumPath;
    fillPath.lineTo (points.back().x, plotBounds.getBottom());
    fillPath.lineTo (points.front().x, plotBounds.getBottom());
    fillPath.closeSubPath();

    juce::ColourGradient fillGradient (juce::Colour (0xddf0d64a), plotBounds.getCentreX(), plotBounds.getY(),
                                       juce::Colour (0x22394a2a), plotBounds.getCentreX(), plotBounds.getBottom(),
                                       false);
    fillGradient.addColour (0.28, juce::Colour (0xc8c6d64f));
    fillGradient.addColour (0.62, juce::Colour (0x8f6d9448));
    g.setGradientFill (fillGradient);
    g.fillPath (fillPath);

    g.setColour (juce::Colour (0xffffe36a));
    g.strokePath (spectrumPath, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (juce::Colour (0x99f7f0a0));
    g.strokePath (spectrumPath, juce::PathStrokeType (0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (rtaAverageFrameCount > 0)
    {
        g.setColour (juce::Colour (0xcc080b0f));
        g.strokePath (averagePath, juce::PathStrokeType (3.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (0xffdce8f4));
        g.strokePath (averagePath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (0x99dce8f4));
        for (auto band = 0; band < LufsMeterPlusAudioProcessor::rtaBandCount; band += 3)
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (averagePoints[static_cast<size_t> (band)]));
    }

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (juce::Colour (0xff88929e));
    for (const auto& label : { std::pair<float, const char*> { 20.0f, "20" },
                               { 100.0f, "100" },
                               { 1000.0f, "1k" },
                               { 10000.0f, "10k" },
                               { 20000.0f, "20k" } })
    {
        const auto x = juce::jmap (std::log10 (label.first), std::log10 (20.0f), std::log10 (20000.0f),
                                   static_cast<float> (plotBounds.getX()), static_cast<float> (plotBounds.getRight()));
        g.drawText (label.second, static_cast<int> (std::round (x)) - 18, bounds.getBottom() - 24, 36, 16, juce::Justification::centred);
    }
}

void LufsMeterPlusAudioProcessorEditor::drawLufsGraph (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    drawGraphGrid (g, bounds);

    auto plotBounds = bounds.withTrimmedLeft (54).withTrimmedRight (18).reduced (0, 32).toFloat();
    const auto count = static_cast<int> (shortTermHistory.size());
    const auto targetLufs = audioProcessor.getTargetLufs();
    const auto targetY = juce::jmap (juce::jlimit (-60.0f, 0.0f, targetLufs), -60.0f, 0.0f, plotBounds.getBottom(), plotBounds.getY());

    g.setColour (juce::Colour (0x88aeb7c2));
    g.drawHorizontalLine (static_cast<int> (std::round (targetY)), plotBounds.getX(), plotBounds.getRight());
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String (targetLufs, 1) + " LUFS",
                static_cast<int> (plotBounds.getX() + 6.0f),
                static_cast<int> (targetY) - 14,
                82,
                12,
                juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (juce::Colour (0xff88929e));
    for (const auto& tick : { 0.0, 0.333, 0.666, 1.0 })
    {
        const auto x = juce::jmap (static_cast<float> (tick), 0.0f, 1.0f, plotBounds.getX(), plotBounds.getRight());
        g.drawText (formatElapsedTime (graphElapsedSeconds * tick), static_cast<int> (std::round (x)) - 29, bounds.getBottom() - 24, 58, 16, juce::Justification::centred);
    }

    if (count < 2)
        return;

    auto makePath = [count, plotBounds] (const std::vector<float>& values)
    {
        juce::Path path;
        for (auto i = 0; i < count; ++i)
        {
            const auto value = juce::jlimit (-60.0f, 0.0f, values[static_cast<size_t> (i)]);
            const auto x = juce::jmap (static_cast<float> (i), 0.0f, static_cast<float> (count - 1), plotBounds.getX(), plotBounds.getRight());
            const auto y = juce::jmap (value, -60.0f, 0.0f, plotBounds.getBottom(), plotBounds.getY());
            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }
        return path;
    };

    // Integrated (long-term) filled area
    g.setColour (juce::Colour (0x555479b7));
    auto area = makePath (longTermHistory);
    area.lineTo (plotBounds.getRight(), plotBounds.getBottom());
    area.lineTo (plotBounds.getX(), plotBounds.getBottom());
    area.closeSubPath();
    g.fillPath (area);

    // Integrated line — solid, distinct colour
    g.setColour (juce::Colour (0xff5b8fd4));
    g.strokePath (makePath (longTermHistory), juce::PathStrokeType (2.0f));

    // Short-term line — white, drawn on top so it reads clearly
    g.setColour (juce::Colours::white);
    g.strokePath (makePath (shortTermHistory), juce::PathStrokeType (1.5f));
}

void LufsMeterPlusAudioProcessorEditor::updateModeButtons()
{
    rtaModeButton.setToggleState (showRtaView, juce::dontSendNotification);
    lufsModeButton.setToggleState (! showRtaView, juce::dontSendNotification);
}

float LufsMeterPlusAudioProcessorEditor::smoothMeterValue (float current, float target)
{
    const auto coefficient = target > current ? 0.55f : 0.18f;
    return current + ((target - current) * coefficient);
}

juce::String LufsMeterPlusAudioProcessorEditor::formatDb (float value, const juce::String& suffix)
{
    if (value <= -99.9f)
        return "-inf" + suffix;

    return juce::String (value, 1) + suffix;
}

juce::String LufsMeterPlusAudioProcessorEditor::formatElapsedTime (double seconds)
{
    const auto totalSeconds = juce::jmax (0, static_cast<int> (std::floor (seconds + 0.5)));
    const auto minutes = totalSeconds / 60;
    const auto remainingSeconds = totalSeconds % 60;

    if (minutes <= 0)
        return juce::String (remainingSeconds) + "s";

    return juce::String (minutes) + ":" + juce::String (remainingSeconds).paddedLeft ('0', 2);
}
