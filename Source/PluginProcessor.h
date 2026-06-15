#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

class LufsUdpSender;

class LufsMeterPlusAudioProcessor final : public juce::AudioProcessor
{
public:
    LufsMeterPlusAudioProcessor();
    ~LufsMeterPlusAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void requestMeasurementReset();
    float getTargetLufs() const;
    double getMeasurementElapsedSeconds() const;

    static constexpr int rtaBandCount = 31;

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float> integratedLufs { -70.0f };
    std::atomic<float> shortTermLufs { -70.0f };
    std::atomic<float> momentaryLufs { -70.0f };
    std::atomic<float> loudnessRangeLu { 0.0f };
    std::array<std::atomic<float>, 2> truePeakDb {};
    std::atomic<float> gainReductionDb { 0.0f };
    std::array<std::atomic<float>, 2> inputPeakDb {};
    std::array<std::atomic<float>, 2> outputPeakDb {};
    std::atomic<double> measurementElapsedSeconds { 0.0 };
    std::array<std::atomic<float>, rtaBandCount> rtaBandDb {};

    // Heartbeat written from processBlock(); read by LufsUdpSender to gate sending.
    std::atomic<juce::int64> lastAudioTickMs { 0 };

    // Bumped on every requestMeasurementReset() (GUI button or remote UDP). The
    // editor watches this to clear its graph history regardless of reset source.
    std::atomic<juce::uint32> resetGeneration { 0 };

private:
    struct Biquad
    {
        void reset();
        void setHighPass (double sampleRate, double frequencyHz, double q);
        void setHighShelf (double sampleRate, double frequencyHz, double gainDb, double shelfSlope);
        float processSample (float input);

        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
        double z1 = 0.0;
        double z2 = 0.0;
    };

    std::atomic<float>* thresholdParameter = nullptr;
    std::atomic<float>* ceilingParameter = nullptr;
    std::atomic<float>* releaseParameter = nullptr;
    std::atomic<float>* targetLufsParameter = nullptr;
    std::atomic<float>* bypassParameter = nullptr;
    std::atomic<float>* rtaSlopeParameter = nullptr;
    std::atomic<bool> measurementResetRequested { false };

    static constexpr size_t maxIntegratedBlocks = 7200;
    static constexpr size_t maxLoudnessRangeBlocks = 7200;
    static constexpr int maxLookaheadSamples = 4096;
    static constexpr int rtaFftOrder = 12;
    static constexpr int rtaFftSize = 1 << rtaFftOrder;

    double currentSampleRate = 44100.0;
    float limiterGain = 1.0f;
    int lookaheadSamples = 0;
    int lookaheadWriteIndex = 0;
    double momentaryPower = 0.0;
    double shortTermPower = 0.0;
    double integratedBlockPower = 0.0;
    int integratedBlockSampleCount = 0;
    int integratedBlockSize = 0;
    int integratedHopSampleCount = 0;
    int integratedHopSize = 0;
    int loudnessRangeHopSampleCount = 0;
    int loudnessRangeHopSize = 0;
    int loudnessRangeWindowSize = 0;
    size_t integratedBlockWriteIndex = 0;
    size_t integratedBlockCount = 0;
    size_t loudnessRangeBlockWriteIndex = 0;
    size_t loudnessRangeBlockCount = 0;
    double gatedIntegratedPower = 0.0;
    double integratedWindowPower = 0.0;
    double loudnessRangeWindowPower = 0.0;
    std::array<double, maxIntegratedBlocks> integratedBlockPowers {};
    std::array<double, maxLoudnessRangeBlocks> loudnessRangePowers {};
    std::vector<double> integratedWindowPowers;
    std::vector<double> loudnessRangeWindowPowers;
    size_t integratedWindowWriteIndex = 0;
    int integratedWindowFilledSamples = 0;
    size_t loudnessRangeWindowWriteIndex = 0;
    int loudnessRangeWindowFilledSamples = 0;
    std::array<std::array<float, maxLookaheadSamples>, 2> lookaheadDelayBuffer {};
    std::array<Biquad, 2> kWeightShelves;
    std::array<Biquad, 2> kWeightHighPasses;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int oversamplingFactor = 1;

    std::unique_ptr<LufsUdpSender> udpSender;

    juce::dsp::FFT rtaFft;
    juce::dsp::WindowingFunction<float> rtaWindow;
    std::array<std::array<float, rtaFftSize>, 2> rtaFifo {};
    std::array<float, rtaFftSize * 2> rtaFftData {};
    int rtaFifoIndex = 0;

    void pushIntegratedBlock (double meanPower);
    void pushIntegratedSample (double weightedMeanPower);
    void pushLoudnessRangeSample (double weightedMeanPower);
    void pushLoudnessRangeBlock (double meanPower);
    double calculateGatedIntegratedPower() const;
    float calculateLoudnessRange() const;
    void resetMeasurements();
    void prepareKWeightingFilters();
    void resetLookaheadDelay();
    void resetRta();
    void pushRtaSample (const juce::AudioBuffer<float>& buffer, int sampleIndex, int channelCount);
    void calculateRtaBands();
    static float powerToLufs (double power);
    static double lufsToPower (double lufs);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LufsMeterPlusAudioProcessor)
};
