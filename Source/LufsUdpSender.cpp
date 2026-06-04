#include "LufsUdpSender.h"
#include "PluginProcessor.h"

#include <cmath>

LufsUdpSender::LufsUdpSender(LufsMeterPlusAudioProcessor &p)
    : juce::Thread("LufsResetListener"), processor(p)
{
    // Bind to the IPv4 wildcard so we can sendto() unicast, subnet broadcast,
    // or the limited broadcast address 255.255.255.255 from a single socket.
    socketReady = socket.bindToPort(0, "0.0.0.0");

    if (!socketReady)
        juce::Logger::writeToLog("LufsUdpSender: bindToPort(0) failed; UDP export disabled.");

    startTimer(kSendIntervalMs);

    // Listen for control commands (reset) from the monitor app.
    startThread();
}

LufsUdpSender::~LufsUdpSender()
{
    stopTimer();
    signalThreadShouldExit();
    stopThread(1000);
    socket.shutdown();
}

void LufsUdpSender::run()
{
    // Separate socket bound to the fixed control port so the app knows where to
    // send. Port reuse lets multiple plugin instances coexist without bind errors
    // (the OS may deliver a command to only one of them, which is fine for reset).
    juce::DatagramSocket controlSocket { true };
    controlSocket.setEnablePortReuse(true);

    if (!controlSocket.bindToPort(kControlPort))
    {
        juce::Logger::writeToLog("LufsUdpSender: control bindToPort(" + juce::String(kControlPort)
                                 + ") failed; remote reset disabled.");
        return;
    }

    char buffer[512];

    while (!threadShouldExit())
    {
        // Wake up periodically so threadShouldExit() is checked even with no traffic.
        const int ready = controlSocket.waitUntilReady(true, 200);
        if (ready < 0)
            break; // socket error
        if (ready == 0)
            continue; // timeout, re-check exit flag

        juce::String senderIp;
        int senderPort = 0;
        const int numBytes = controlSocket.read(buffer, sizeof(buffer) - 1, false, senderIp, senderPort);
        if (numBytes <= 0)
            continue;

        buffer[numBytes] = 0;
        // Brace-init to avoid the most-vexing-parse (message read as a function decl).
        const juce::String message { juce::CharPointer_UTF8 (buffer) };

        const auto parsed = juce::JSON::parse(message);
        if (parsed.isObject() && parsed.getProperty("type", juce::var()).toString() == "reset")
            processor.requestMeasurementReset();
    }

    controlSocket.shutdown();
}

void LufsUdpSender::setDestination(const juce::String &newHost, int newPort)
{
    const juce::ScopedLock lock(destinationLock);
    hostAddress = newHost;
    port = newPort;
    warnedSendFailure = false;
}

juce::String LufsUdpSender::formatField(float value)
{
    if (!std::isfinite(value))
        return "null";

    return juce::String(value, 2);
}

juce::String LufsUdpSender::buildJson(float momentary, float shortTerm, float integrated) const
{
    juce::String json;
    json.preallocateBytes(160);

    json << "{\"type\":\"lufs\","
         << "\"momentary\":" << formatField(momentary) << ','
         << "\"shortTerm\":" << formatField(shortTerm) << ','
         << "\"integrated\":" << formatField(integrated) << ','
         << "\"ts\":" << juce::Time::currentTimeMillis()
         << '}';

    return json;
}

void LufsUdpSender::timerCallback()
{
    if (!enabled.load() || !socketReady)
        return;

    const auto lastTickMs = processor.lastAudioTickMs.load();
    if (lastTickMs <= 0)
        return; // never processed audio yet

    const auto now = juce::Time::currentTimeMillis();
    if (now - lastTickMs > kHeartbeatStaleMs)
        return; // host (OBS) is not currently feeding audio to the plugin

    const auto momentary = processor.momentaryLufs.load();
    const auto shortTerm = processor.shortTermLufs.load();
    const auto integrated = processor.integratedLufs.load();

    const auto payload = buildJson(momentary, shortTerm, integrated);
    const auto utf8 = payload.toRawUTF8();
    const auto bytes = static_cast<int>(payload.getNumBytesAsUTF8());

    juce::String currentHost;
    int currentPort = 0;
    {
        const juce::ScopedLock lock(destinationLock);
        currentHost = hostAddress;
        currentPort = port;
    }

    const auto written = socket.write(currentHost, currentPort, utf8, bytes);

    if (written < 0)
    {
        if (!warnedSendFailure)
        {
            warnedSendFailure = true;
            juce::Logger::writeToLog("LufsUdpSender: write() failed to " + currentHost + ":" + juce::String(currentPort) + " (further failures suppressed).");
        }
    }
    else
    {
        warnedSendFailure = false;
    }
}
