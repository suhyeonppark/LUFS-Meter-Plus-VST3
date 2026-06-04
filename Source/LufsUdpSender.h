#pragma once

#include <JuceHeader.h>

class LufsMeterPlusAudioProcessor;

class LufsUdpSender final : private juce::Timer,
                           private juce::Thread
{
public:
    explicit LufsUdpSender (LufsMeterPlusAudioProcessor& processor);
    ~LufsUdpSender() override;

    void setEnabled (bool shouldBeEnabled) noexcept { enabled.store (shouldBeEnabled); }
    bool isEnabled() const noexcept                 { return enabled.load(); }

    void setDestination (const juce::String& newHost, int newPort);

    static constexpr const char* kDefaultHost      = "255.255.255.255";
    static constexpr int         kDefaultPort      = 49152;
    static constexpr int         kSendIntervalMs   = 1000;
    static constexpr bool        kDefaultEnabled   = true;

    // Port the monitor app sends control commands (e.g. reset) to.
    static constexpr int         kControlPort      = 49153;

    // If processBlock heartbeat is older than this, we treat the plugin as idle and skip sending.
    static constexpr juce::int64 kHeartbeatStaleMs = 2000;

private:
    void timerCallback() override;

    // juce::Thread — blocks on the control socket waiting for reset commands.
    void run() override;
    static juce::String formatField (float value);
    juce::String buildJson (float momentary, float shortTerm, float integrated) const;

    LufsMeterPlusAudioProcessor& processor;
    juce::DatagramSocket socket { true };

    std::atomic<bool> enabled { kDefaultEnabled };
    juce::CriticalSection destinationLock;
    juce::String hostAddress { kDefaultHost };
    int port { kDefaultPort };

    bool socketReady = false;
    bool warnedSendFailure = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LufsUdpSender)
};
