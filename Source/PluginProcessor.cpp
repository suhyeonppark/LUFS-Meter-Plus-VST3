#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LufsUdpSender.h"

#include <algorithm>
#include <cmath>

void LufsMeterPlusAudioProcessor::Biquad::reset()
{
    z1 = 0.0;
    z2 = 0.0;
}

void LufsMeterPlusAudioProcessor::Biquad::setHighPass(double sampleRate, double frequencyHz, double q)
{
    const auto w0 = juce::MathConstants<double>::twoPi * frequencyHz / sampleRate;
    const auto cosW0 = std::cos(w0);
    const auto alpha = std::sin(w0) / (2.0 * q);
    const auto a0 = 1.0 + alpha;

    b0 = ((1.0 + cosW0) * 0.5) / a0;
    b1 = -(1.0 + cosW0) / a0;
    b2 = ((1.0 + cosW0) * 0.5) / a0;
    a1 = (-2.0 * cosW0) / a0;
    a2 = (1.0 - alpha) / a0;
    reset();
}

void LufsMeterPlusAudioProcessor::Biquad::setHighShelf(double sampleRate, double frequencyHz, double gainDb, double shelfSlope)
{
    const auto amplitude = std::pow(10.0, gainDb / 40.0);
    const auto w0 = juce::MathConstants<double>::twoPi * frequencyHz / sampleRate;
    const auto cosW0 = std::cos(w0);
    const auto sinW0 = std::sin(w0);
    const auto alpha = (sinW0 * 0.5) * std::sqrt((amplitude + (1.0 / amplitude)) * ((1.0 / shelfSlope) - 1.0) + 2.0);
    const auto sqrtA = std::sqrt(amplitude);
    const auto a0 = (amplitude + 1.0) - ((amplitude - 1.0) * cosW0) + (2.0 * sqrtA * alpha);

    b0 = (amplitude * ((amplitude + 1.0) + ((amplitude - 1.0) * cosW0) + (2.0 * sqrtA * alpha))) / a0;
    b1 = (-2.0 * amplitude * ((amplitude - 1.0) + ((amplitude + 1.0) * cosW0))) / a0;
    b2 = (amplitude * ((amplitude + 1.0) + ((amplitude - 1.0) * cosW0) - (2.0 * sqrtA * alpha))) / a0;
    a1 = (2.0 * ((amplitude - 1.0) - ((amplitude + 1.0) * cosW0))) / a0;
    a2 = ((amplitude + 1.0) - ((amplitude - 1.0) * cosW0) - (2.0 * sqrtA * alpha)) / a0;
    reset();
}

float LufsMeterPlusAudioProcessor::Biquad::processSample(float input)
{
    const auto output = (b0 * input) + z1;
    z1 = (b1 * input) - (a1 * output) + z2;
    z2 = (b2 * input) - (a2 * output);
    return static_cast<float>(output);
}

LufsMeterPlusAudioProcessor::LufsMeterPlusAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Parameters", createParameterLayout()),
      rtaFft(rtaFftOrder),
      rtaWindow(rtaFftSize, juce::dsp::WindowingFunction<float>::hann)
{
    thresholdParameter = parameters.getRawParameterValue("threshold");
    ceilingParameter = parameters.getRawParameterValue("ceiling");
    releaseParameter = parameters.getRawParameterValue("release");
    targetLufsParameter = parameters.getRawParameterValue("targetLufs");
    bypassParameter = parameters.getRawParameterValue("bypass");
    rtaSlopeParameter = parameters.getRawParameterValue("rtaSlope");

    resetRta();

    udpSender = std::make_unique<LufsUdpSender>(*this);
}

LufsMeterPlusAudioProcessor::~LufsMeterPlusAudioProcessor() = default;

void LufsMeterPlusAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    limiterGain = 1.0f;

    // Determine power-of-2 oversampling factor that brings rate to >= 384 kHz
    oversamplingFactor = 1;
    while (currentSampleRate * oversamplingFactor < 384000.0 - 1.0)
        oversamplingFactor *= 2;
    oversamplingFactor = juce::jmin(oversamplingFactor, 16);

    const auto log2Factor = static_cast<size_t>(std::round(std::log2(oversamplingFactor)));
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        2, log2Factor, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler->initProcessing(static_cast<size_t>(samplesPerBlock));

    // Lookahead is maintained at the oversampled rate (5 ms)
    const auto effectiveSampleRate = currentSampleRate * oversamplingFactor;
    lookaheadSamples = juce::jlimit(1, maxLookaheadSamples - 1,
        static_cast<int>(std::round(effectiveSampleRate * 0.005)));
    lookaheadWriteIndex = 0;

    integratedBlockSize = juce::jmax(1, static_cast<int>(std::round(currentSampleRate * 0.4)));
    integratedHopSize = juce::jmax(1, static_cast<int>(std::round(currentSampleRate * 0.1)));
    loudnessRangeHopSize = juce::jmax(1, static_cast<int>(std::round(currentSampleRate)));
    loudnessRangeWindowSize = juce::jmax(1, static_cast<int>(std::round(currentSampleRate * 3.0)));
    integratedWindowPowers.assign(static_cast<size_t>(integratedBlockSize), 0.0);
    loudnessRangeWindowPowers.assign(static_cast<size_t>(loudnessRangeWindowSize), 0.0);
    resetMeasurements();
    resetLookaheadDelay();
    resetRta();
    prepareKWeightingFilters();

    const auto nativeLookahead = lookaheadSamples / oversamplingFactor;
    setLatencySamples(static_cast<int>(std::round(oversampler->getLatencyInSamples())) + nativeLookahead);
}

void LufsMeterPlusAudioProcessor::releaseResources()
{
    oversampler.reset();
}

bool LufsMeterPlusAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    const auto &mainInput = layouts.getMainInputChannelSet();
    const auto &mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput != juce::AudioChannelSet::stereo())
        return false;

    return mainOutput == mainInput;
}

void LufsMeterPlusAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &)
{
    juce::ScopedNoDenormals noDenormals;

    if (measurementResetRequested.exchange(false))
        resetMeasurements();

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples             = buffer.getNumSamples();
    const auto processedChannels      = juce::jmin(2, juce::jmin(totalNumInputChannels, totalNumOutputChannels));

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    if (numSamples == 0 || oversampler == nullptr)
        return;

    auto inputPeak = 0.0f;
    for (auto ch = 0; ch < processedChannels; ++ch)
        inputPeak = juce::jmax(inputPeak, buffer.getMagnitude(ch, 0, numSamples));

    const auto bypassed          = bypassParameter   != nullptr && bypassParameter->load()   > 0.5f;
    const auto ceilingDb         = ceilingParameter  != nullptr ? ceilingParameter->load()   : -1.0f;
    const auto thresholdDb       = thresholdParameter != nullptr ? thresholdParameter->load() : -1.0f;
    const auto ceilingLinear     = juce::Decibels::decibelsToGain(ceilingDb);
    const auto driveLinear       = juce::Decibels::decibelsToGain(ceilingDb - thresholdDb);
    const auto releaseMs         = releaseParameter  != nullptr ? releaseParameter->load()   : 120.0f;
    const auto releaseSeconds    = juce::jmax(0.001f, releaseMs * 0.001f);
    const auto effectiveSampleRate = currentSampleRate * oversamplingFactor;
    const auto releaseCoeff      = std::exp(-1.0f / static_cast<float>(effectiveSampleRate * releaseSeconds));

    auto truePeak = inputPeak;

    // ── Limiter at oversampled rate ───────────────────────────────────────
    if (bypassed)
    {
        limiterGain = 1.0f;
    }
    else
    {
        juce::dsp::AudioBlock<float> nativeBlock(buffer);
        auto oversampledBlock = oversampler->processSamplesUp(nativeBlock);
        const auto osNumSamples = static_cast<int>(oversampledBlock.getNumSamples());

        truePeak = 0.0f;

        for (auto s = 0; s < osNumSamples; ++s)
        {
            auto detectorPeak = 0.0f;
            for (auto ch = 0; ch < processedChannels; ++ch)
            {
                const auto x = oversampledBlock.getChannelPointer(static_cast<size_t>(ch))[s];
                detectorPeak = juce::jmax(detectorPeak, std::abs(x * driveLinear));
            }

            const auto targetGain = (detectorPeak > ceilingLinear && detectorPeak > 0.0f)
                                        ? ceilingLinear / detectorPeak : 1.0f;
            limiterGain = targetGain < limiterGain
                            ? targetGain
                            : targetGain + releaseCoeff * (limiterGain - targetGain);

            for (auto ch = 0; ch < processedChannels; ++ch)
            {
                auto* data  = oversampledBlock.getChannelPointer(static_cast<size_t>(ch));
                auto& delay = lookaheadDelayBuffer[static_cast<size_t>(ch)][static_cast<size_t>(lookaheadWriteIndex)];
                const auto delayed = delay;
                delay = data[s] * driveLinear;
                data[s] = delayed * limiterGain;
                truePeak = juce::jmax(truePeak, std::abs(data[s]));
            }
            lookaheadWriteIndex = (lookaheadWriteIndex + 1) % lookaheadSamples;
        }

        oversampler->processSamplesDown(nativeBlock);   // result back in buffer
    }

    // ── Metering at native rate ───────────────────────────────────────────
    auto outputPeak = 0.0f;
    for (auto ch = 0; ch < processedChannels; ++ch)
        outputPeak = juce::jmax(outputPeak, buffer.getMagnitude(ch, 0, numSamples));

    for (auto s = 0; s < numSamples; ++s)
    {
        pushRtaSample(buffer, s, processedChannels);

        auto weightedFramePower = 0.0;
        for (auto ch = 0; ch < processedChannels; ++ch)
        {
            const auto shelfed  = kWeightShelves[static_cast<size_t>(ch)].processSample(buffer.getSample(ch, s));
            const auto weighted = kWeightHighPasses[static_cast<size_t>(ch)].processSample(shelfed);
            weightedFramePower += static_cast<double>(weighted) * static_cast<double>(weighted);
        }

        if (processedChannels > 0)
        {
            pushIntegratedSample(weightedFramePower);
            pushLoudnessRangeSample(weightedFramePower);
        }
    }

    if (processedChannels > 0)
    {
        momentaryPower = integratedWindowFilledSamples > 0
                            ? integratedWindowPower / static_cast<double>(integratedWindowFilledSamples)
                            : 0.0;
        shortTermPower = loudnessRangeWindowFilledSamples > 0
                            ? loudnessRangeWindowPower / static_cast<double>(loudnessRangeWindowFilledSamples)
                            : 0.0;
    }
    else
    {
        momentaryPower = 0.0;
        shortTermPower = 0.0;
    }

    inputPeakDb.store  (juce::Decibels::gainToDecibels(inputPeak,  -100.0f));
    outputPeakDb.store (juce::Decibels::gainToDecibels(outputPeak, -100.0f));
    truePeakDb.store   (juce::Decibels::gainToDecibels(truePeak, -100.0f));
    gainReductionDb.store (bypassed ? 0.0f : juce::jmin(0.0f, juce::Decibels::gainToDecibels(limiterGain, -100.0f)));

    momentaryLufs.store (powerToLufs(momentaryPower));
    shortTermLufs.store (powerToLufs(shortTermPower));
    integratedLufs.store(powerToLufs(gatedIntegratedPower));
    loudnessRangeLu.store(calculateLoudnessRange());
    measurementElapsedSeconds.store(measurementElapsedSeconds.load() + static_cast<double>(numSamples) / currentSampleRate);

    lastAudioTickMs.store (juce::Time::currentTimeMillis());
}

void LufsMeterPlusAudioProcessor::requestMeasurementReset()
{
    integratedLufs.store(-70.0f);
    shortTermLufs.store(-70.0f);
    momentaryLufs.store(-70.0f);
    loudnessRangeLu.store(0.0f);
    measurementResetRequested.store(true);
}

float LufsMeterPlusAudioProcessor::getTargetLufs() const
{
    return targetLufsParameter != nullptr ? targetLufsParameter->load() : -14.0f;
}

double LufsMeterPlusAudioProcessor::getMeasurementElapsedSeconds() const
{
    return measurementElapsedSeconds.load();
}

bool LufsMeterPlusAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor *LufsMeterPlusAudioProcessor::createEditor()
{
    return new LufsMeterPlusAudioProcessorEditor(*this);
}

const juce::String LufsMeterPlusAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool LufsMeterPlusAudioProcessor::acceptsMidi() const
{
    return false;
}

bool LufsMeterPlusAudioProcessor::producesMidi() const
{
    return false;
}

bool LufsMeterPlusAudioProcessor::isMidiEffect() const
{
    return false;
}

double LufsMeterPlusAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LufsMeterPlusAudioProcessor::getNumPrograms()
{
    return 1;
}

int LufsMeterPlusAudioProcessor::getCurrentProgram()
{
    return 0;
}

void LufsMeterPlusAudioProcessor::setCurrentProgram(int)
{
}

const juce::String LufsMeterPlusAudioProcessor::getProgramName(int)
{
    return {};
}

void LufsMeterPlusAudioProcessor::changeProgramName(int, const juce::String &)
{
}

void LufsMeterPlusAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    if (auto state = parameters.copyState(); auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void LufsMeterPlusAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout LufsMeterPlusAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"threshold", 1},
        "Threshold",
        juce::NormalisableRange<float>{-24.0f, 0.0f, 0.1f},
        -1.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"ceiling", 1},
        "Ceiling",
        juce::NormalisableRange<float>{-12.0f, 0.0f, 0.1f},
        -1.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"release", 1},
        "Release",
        juce::NormalisableRange<float>{10.0f, 1000.0f, 1.0f},
        120.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"targetLufs", 1},
        "Target LUFS",
        juce::NormalisableRange<float>{-24.0f, -6.0f, 0.1f},
        -14.0f,
        juce::AudioParameterFloatAttributes().withLabel("LUFS")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bypass", 1},
        "Bypass",
        false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"rtaSlope", 1},
        "RTA Slope",
        juce::StringArray{"0 dB/oct", "3 dB/oct", "4.5 dB/oct", "6 dB/oct"},
        1));

    return {params.begin(), params.end()};
}

float LufsMeterPlusAudioProcessor::powerToLufs(double power)
{
    if (power <= 0.0)
        return -70.0f;

    return juce::jmax(-70.0f, static_cast<float>(-0.691 + 10.0 * std::log10(power)));
}

double LufsMeterPlusAudioProcessor::lufsToPower(double lufs)
{
    return std::pow(10.0, (lufs + 0.691) / 10.0);
}

void LufsMeterPlusAudioProcessor::pushIntegratedBlock(double meanPower)
{
    integratedBlockPowers[integratedBlockWriteIndex] = meanPower;
    integratedBlockWriteIndex = (integratedBlockWriteIndex + 1) % integratedBlockPowers.size();
    integratedBlockCount = juce::jmin(integratedBlockCount + 1, integratedBlockPowers.size());
    gatedIntegratedPower = calculateGatedIntegratedPower();
}

void LufsMeterPlusAudioProcessor::pushIntegratedSample(double weightedMeanPower)
{
    if (integratedWindowPowers.empty())
        return;

    const auto oldPower = integratedWindowPowers[integratedWindowWriteIndex];
    integratedWindowPowers[integratedWindowWriteIndex] = weightedMeanPower;
    integratedWindowWriteIndex = (integratedWindowWriteIndex + 1) % integratedWindowPowers.size();

    if (integratedWindowFilledSamples < integratedBlockSize)
    {
        ++integratedWindowFilledSamples;
        integratedWindowPower += weightedMeanPower;
    }
    else
    {
        integratedWindowPower += weightedMeanPower - oldPower;
    }

    ++integratedHopSampleCount;

    if (integratedWindowFilledSamples >= integratedBlockSize && integratedHopSampleCount >= integratedHopSize)
    {
        integratedHopSampleCount = 0;
        pushIntegratedBlock(integratedWindowPower / static_cast<double>(integratedWindowFilledSamples));
    }
}

void LufsMeterPlusAudioProcessor::pushLoudnessRangeSample(double weightedMeanPower)
{
    if (loudnessRangeWindowPowers.empty())
        return;

    const auto oldPower = loudnessRangeWindowPowers[loudnessRangeWindowWriteIndex];
    loudnessRangeWindowPowers[loudnessRangeWindowWriteIndex] = weightedMeanPower;
    loudnessRangeWindowWriteIndex = (loudnessRangeWindowWriteIndex + 1) % loudnessRangeWindowPowers.size();

    if (loudnessRangeWindowFilledSamples < loudnessRangeWindowSize)
    {
        ++loudnessRangeWindowFilledSamples;
        loudnessRangeWindowPower += weightedMeanPower;
    }
    else
    {
        loudnessRangeWindowPower += weightedMeanPower - oldPower;
    }

    ++loudnessRangeHopSampleCount;

    if (loudnessRangeWindowFilledSamples >= loudnessRangeWindowSize && loudnessRangeHopSampleCount >= loudnessRangeHopSize)
    {
        loudnessRangeHopSampleCount = 0;
        pushLoudnessRangeBlock(loudnessRangeWindowPower / static_cast<double>(loudnessRangeWindowFilledSamples));
    }
}

void LufsMeterPlusAudioProcessor::pushLoudnessRangeBlock(double meanPower)
{
    loudnessRangePowers[loudnessRangeBlockWriteIndex] = meanPower;
    loudnessRangeBlockWriteIndex = (loudnessRangeBlockWriteIndex + 1) % loudnessRangePowers.size();
    loudnessRangeBlockCount = juce::jmin(loudnessRangeBlockCount + 1, loudnessRangePowers.size());
}

double LufsMeterPlusAudioProcessor::calculateGatedIntegratedPower() const
{
    if (integratedBlockCount == 0)
        return 0.0;

    const auto absoluteGatePower = lufsToPower(-70.0);
    auto absoluteGatedPowerSum = 0.0;
    auto absoluteGatedBlockCount = 0;

    for (size_t i = 0; i < integratedBlockCount; ++i)
    {
        const auto blockPower = integratedBlockPowers[i];

        if (blockPower > absoluteGatePower)
        {
            absoluteGatedPowerSum += blockPower;
            ++absoluteGatedBlockCount;
        }
    }

    if (absoluteGatedBlockCount == 0)
        return 0.0;

    const auto preliminaryPower = absoluteGatedPowerSum / static_cast<double>(absoluteGatedBlockCount);
    const auto relativeGateLufs = static_cast<double>(powerToLufs(preliminaryPower)) - 10.0;
    const auto relativeGatePower = lufsToPower(juce::jmax(-70.0, relativeGateLufs));
    auto relativeGatedPowerSum = 0.0;
    auto relativeGatedBlockCount = 0;

    for (size_t i = 0; i < integratedBlockCount; ++i)
    {
        const auto blockPower = integratedBlockPowers[i];

        if (blockPower > relativeGatePower)
        {
            relativeGatedPowerSum += blockPower;
            ++relativeGatedBlockCount;
        }
    }

    if (relativeGatedBlockCount == 0)
        return preliminaryPower;

    return relativeGatedPowerSum / static_cast<double>(relativeGatedBlockCount);
}

float LufsMeterPlusAudioProcessor::calculateLoudnessRange() const
{
    if (loudnessRangeBlockCount < 2)
        return 0.0f;

    const auto absoluteGatePower = lufsToPower(-70.0);
    auto absoluteGatedPowerSum = 0.0;
    auto absoluteGatedBlockCount = 0;

    for (size_t i = 0; i < loudnessRangeBlockCount; ++i)
    {
        const auto blockPower = loudnessRangePowers[i];

        if (blockPower > absoluteGatePower)
        {
            absoluteGatedPowerSum += blockPower;
            ++absoluteGatedBlockCount;
        }
    }

    if (absoluteGatedBlockCount < 2)
        return 0.0f;

    const auto relativeGatePower = lufsToPower(static_cast<double>(powerToLufs(absoluteGatedPowerSum / static_cast<double>(absoluteGatedBlockCount))) - 20.0);
    std::vector<float> gatedLoudnessValues;
    gatedLoudnessValues.reserve(static_cast<size_t>(absoluteGatedBlockCount));

    for (size_t i = 0; i < loudnessRangeBlockCount; ++i)
    {
        const auto blockPower = loudnessRangePowers[i];

        if (blockPower > absoluteGatePower && blockPower > relativeGatePower)
            gatedLoudnessValues.push_back(powerToLufs(blockPower));
    }

    if (gatedLoudnessValues.size() < 2)
        return 0.0f;

    std::sort(gatedLoudnessValues.begin(), gatedLoudnessValues.end());

    const auto percentile = [&gatedLoudnessValues](float ratio)
    {
        const auto maxIndex = static_cast<float>(gatedLoudnessValues.size() - 1);
        const auto position = juce::jlimit(0.0f, maxIndex, ratio * maxIndex);
        const auto lowerIndex = static_cast<size_t>(std::floor(position));
        const auto upperIndex = static_cast<size_t>(std::ceil(position));
        const auto fraction = position - static_cast<float>(lowerIndex);

        return gatedLoudnessValues[lowerIndex] + ((gatedLoudnessValues[upperIndex] - gatedLoudnessValues[lowerIndex]) * fraction);
    };

    return juce::jmax(0.0f, percentile(0.95f) - percentile(0.10f));
}

void LufsMeterPlusAudioProcessor::resetMeasurements()
{
    momentaryPower = 0.0;
    shortTermPower = 0.0;
    integratedBlockPower = 0.0;
    integratedBlockSampleCount = 0;
    integratedHopSampleCount = 0;
    loudnessRangeHopSampleCount = 0;
    integratedBlockWriteIndex = 0;
    integratedBlockCount = 0;
    loudnessRangeBlockWriteIndex = 0;
    loudnessRangeBlockCount = 0;
    gatedIntegratedPower = 0.0;
    integratedWindowPower = 0.0;
    loudnessRangeWindowPower = 0.0;
    integratedBlockPowers.fill(0.0);
    loudnessRangePowers.fill(0.0);
    std::fill(integratedWindowPowers.begin(), integratedWindowPowers.end(), 0.0);
    std::fill(loudnessRangeWindowPowers.begin(), loudnessRangeWindowPowers.end(), 0.0);
    integratedWindowWriteIndex = 0;
    integratedWindowFilledSamples = 0;
    loudnessRangeWindowWriteIndex = 0;
    loudnessRangeWindowFilledSamples = 0;

    for (auto &filter : kWeightShelves)
        filter.reset();

    for (auto &filter : kWeightHighPasses)
        filter.reset();

    integratedLufs.store(-70.0f);
    shortTermLufs.store(-70.0f);
    momentaryLufs.store(-70.0f);
    loudnessRangeLu.store(0.0f);
    measurementElapsedSeconds.store(0.0);
}

void LufsMeterPlusAudioProcessor::prepareKWeightingFilters()
{
    for (auto channel = 0; channel < 2; ++channel)
    {
        kWeightShelves[static_cast<size_t>(channel)].setHighShelf(currentSampleRate, 1681.974450955533, 4.0, 1.0);
        kWeightHighPasses[static_cast<size_t>(channel)].setHighPass(currentSampleRate, 38.13547087602444, 0.5);
    }
}

void LufsMeterPlusAudioProcessor::resetLookaheadDelay()
{
    for (auto &channelBuffer : lookaheadDelayBuffer)
        channelBuffer.fill(0.0f);

    lookaheadWriteIndex = 0;
}

void LufsMeterPlusAudioProcessor::resetRta()
{
    for (auto& channelFifo : rtaFifo)
        channelFifo.fill(0.0f);

    rtaFftData.fill(0.0f);
    rtaFifoIndex = 0;

    for (auto &band : rtaBandDb)
        band.store(-100.0f);
}

void LufsMeterPlusAudioProcessor::pushRtaSample(const juce::AudioBuffer<float>& buffer, int sampleIndex, int channelCount)
{
    const auto channelsToAnalyse = juce::jlimit(0, 2, channelCount);

    for (auto ch = 0; ch < 2; ++ch)
    {
        const auto sample = ch < channelsToAnalyse ? buffer.getSample(ch, sampleIndex) : 0.0f;
        rtaFifo[static_cast<size_t>(ch)][static_cast<size_t>(rtaFifoIndex)] = sample;
    }

    ++rtaFifoIndex;

    if (rtaFifoIndex >= rtaFftSize)
    {
        rtaFifoIndex = 0;
        calculateRtaBands();
    }
}

void LufsMeterPlusAudioProcessor::calculateRtaBands()
{
    const auto binHz = static_cast<float>(currentSampleRate) / static_cast<float>(rtaFftSize);
    const auto maxFrequency = juce::jmin(20000.0f, static_cast<float>(currentSampleRate * 0.5));
    const auto minFrequency = 20.0f;
    const auto frequencyRatio = maxFrequency / minFrequency;
    const std::array<float, 4> slopeChoices { 0.0f, 3.0f, 4.5f, 6.0f };
    const auto slopeIndex = rtaSlopeParameter != nullptr
                                ? juce::jlimit(0, static_cast<int>(slopeChoices.size()) - 1,
                                               static_cast<int>(std::round(rtaSlopeParameter->load())))
                                : 1;
    const auto slopeDbPerOctave = slopeChoices[static_cast<size_t>(slopeIndex)];
    constexpr auto slopeReferenceHz = 1000.0f;
    std::array<float, (rtaFftSize / 2) + 1> binPower {};

    for (const auto& channelFifo : rtaFifo)
    {
        rtaFftData.fill(0.0f);
        std::copy(channelFifo.begin(), channelFifo.end(), rtaFftData.begin());
        rtaWindow.multiplyWithWindowingTable(rtaFftData.data(), rtaFftSize);
        rtaFft.performFrequencyOnlyForwardTransform(rtaFftData.data());

        for (auto bin = 1; bin <= rtaFftSize / 2; ++bin)
        {
            const auto magnitude = rtaFftData[static_cast<size_t>(bin)];
            binPower[static_cast<size_t>(bin)] += magnitude * magnitude;
        }
    }

    for (auto band = 0; band < rtaBandCount; ++band)
    {
        const auto lowHz = minFrequency * std::pow(frequencyRatio, static_cast<float>(band) / static_cast<float>(rtaBandCount));
        const auto highHz = minFrequency * std::pow(frequencyRatio, static_cast<float>(band + 1) / static_cast<float>(rtaBandCount));
        const auto centreHz = std::sqrt(lowHz * highHz);
        const auto firstBin = juce::jlimit(1, rtaFftSize / 2, static_cast<int>(std::ceil(lowHz / binHz)));
        const auto lastBin = juce::jlimit(firstBin, rtaFftSize / 2, static_cast<int>(std::floor(highHz / binHz)));

        auto sumSquares = 0.0f;
        auto binCount = 0;

        for (auto bin = firstBin; bin <= lastBin; ++bin)
        {
            sumSquares += binPower[static_cast<size_t>(bin)] * 0.5f;
            ++binCount;
        }

        const auto rmsMagnitude = binCount > 0 ? std::sqrt(sumSquares / static_cast<float>(binCount)) / (static_cast<float>(rtaFftSize) * 0.5f) : 0.0f;
        const auto slopeOffsetDb = slopeDbPerOctave * std::log2(centreHz / slopeReferenceHz);
        const auto bandDb = juce::jlimit(-100.0f, 12.0f, juce::Decibels::gainToDecibels(rmsMagnitude, -100.0f) + slopeOffsetDb);
        const auto smoothedDb = (rtaBandDb[static_cast<size_t>(band)].load() * 0.78f) + (bandDb * 0.22f);
        rtaBandDb[static_cast<size_t>(band)].store(smoothedDb);
    }
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new LufsMeterPlusAudioProcessor();
}
