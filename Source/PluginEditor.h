#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>

class LufsMeterPlusAudioProcessorEditor final
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:
    explicit LufsMeterPlusAudioProcessorEditor (LufsMeterPlusAudioProcessor&);
    ~LufsMeterPlusAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void configureReadout (juce::Label& label, const juce::String& name);
    void configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& name, const juce::String& suffix);
    void configureComboBox (juce::ComboBox& comboBox, juce::Label& label, const juce::String& name);
    void configureModeButton (juce::TextButton& button, const juce::String& text);
    void drawLevelMeter (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawReadoutStack (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawRta (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawLufsGraph (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawGraphGrid (juce::Graphics& g, juce::Rectangle<int> bounds);
    void updateModeButtons();
    static float smoothMeterValue (float current, float target);
    static juce::String formatDb (float value, const juce::String& suffix);
    static juce::String formatElapsedTime (double seconds);

    LufsMeterPlusAudioProcessor& audioProcessor;

    juce::Label titleLabel;
    juce::TextButton resetButton;
    juce::TextButton rtaModeButton;
    juce::TextButton lufsModeButton;

    juce::Label integratedLufsLabel;
    juce::Label momentaryLufsLabel;
    juce::Label shortTermLufsLabel;
    juce::Label loudnessUnitsLabel;
    juce::Label truePeakLabel;
    juce::Label gainReductionLabel;

    juce::Slider thresholdSlider;
    juce::Slider ceilingSlider;
    juce::Slider releaseSlider;
    juce::ComboBox rtaSlopeComboBox;

    juce::Label thresholdLabel;
    juce::Label ceilingLabel;
    juce::Label releaseLabel;
    juce::Label rtaSlopeLabel;

    std::vector<float> shortTermHistory;
    std::vector<float> longTermHistory;
    float displayedTruePeakDb = -100.0f;
    float displayedIntegratedLufs = -70.0f;
    float displayedMomentaryLufs = -70.0f;
    float displayedShortTermLufs = -70.0f;
    float displayedLoudnessRangeLu = 0.0f;
    float displayedGainReductionDb = 0.0f;
    std::array<float, LufsMeterPlusAudioProcessor::rtaBandCount> displayedRtaBandDb {};
    std::array<float, LufsMeterPlusAudioProcessor::rtaBandCount> averageRtaBandDb {};
    int rtaAverageFrameCount = 0;
    bool showRtaView = false;
    double graphElapsedSeconds = 0.0;
    double lastTimerSeconds = 0.0;

    juce::Rectangle<int> levelMeterBounds;
    juce::Rectangle<int> targetInfoBounds;
    juce::Rectangle<int> readoutStackBounds;
    juce::Rectangle<int> analysisBounds;
    juce::Rectangle<int> bottomPanelBounds;

    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> ceilingAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<ComboBoxAttachment> rtaSlopeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LufsMeterPlusAudioProcessorEditor)
};
