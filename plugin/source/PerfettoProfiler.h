#pragma once
#include "juce_core/juce_core.h"
#include <JuceHeader.h>
#include <chrono>
#include <fstream>
#include <vector>

// Simple Perfetto-compatible JSON trace writer
// View output at: https://ui.perfetto.dev/
class PerfettoProfiler
{
public:
    struct TraceEvent
    {
        std::string name;
        char phase; // 'B' for begin, 'E' for end
        uint64_t timestamp;
        uint32_t threadId;
        uint32_t processId;
    };

    static PerfettoProfiler& getInstance()
    {
        static PerfettoProfiler instance;
        return instance;
    }

    void beginEvent (const std::string& name)
    {
        uint64_t ts = getMicroseconds();
        juce::ScopedLock lock (mutex);
        events.push_back ({ name, 'B', ts, getThreadId(), getProcessId() });
    }

    void endEvent (const std::string& name)
    {
        uint64_t ts = getMicroseconds();
        juce::ScopedLock lock (mutex);
        events.push_back ({ name, 'E', ts, getThreadId(), getProcessId() });
    }

    void writeTraceFile (const juce::File& outputFile)
    {
        juce::ScopedLock lock (mutex);

        std::ofstream file (outputFile.getFullPathName().toStdString());
        if (! file.is_open()) return;

        file << "[\n";

        for (size_t i = 0; i < events.size(); ++i)
        {
            const auto& e = events[i];
            file << "  {\"name\":\"" << e.name << "\","
                 << "\"ph\":\"" << e.phase << "\","
                 << "\"ts\":" << e.timestamp << ","
                 << "\"pid\":" << e.processId << ","
                 << "\"tid\":" << e.threadId << "}";

            if (i < events.size() - 1) file << ",\n";
        }

        file << "\n]\n";
        file.close();

        std::cout << "Trace written to: " << outputFile.getFullPathName() << "\n";
        std::cout << "Open in browser: https://ui.perfetto.dev/\n";
    }

    void reset()
    {
        juce::ScopedLock lock (mutex);
        events.clear();
    }

    size_t getEventCount() const
    {
        juce::ScopedLock lock (mutex);
        return events.size();
    }

private:
    PerfettoProfiler() = default;

    uint64_t getMicroseconds()
    {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::time_point_cast<std::chrono::microseconds> (now).time_since_epoch().count();
    }

    uint32_t getThreadId()
    {
        return static_cast<uint32_t> (std::hash<std::thread::id> {}(std::this_thread::get_id()));
    }

    uint32_t getProcessId()
    {
        return static_cast<uint32_t> (5);
    }

    std::vector<TraceEvent> events;
    mutable juce::CriticalSection mutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerfettoProfiler)
};

class PerfettoScope
{
public:
    explicit PerfettoScope (const std::string& name) : name (name)
    {
        PerfettoProfiler::getInstance().beginEvent (name);
    }

    ~PerfettoScope()
    {
        PerfettoProfiler::getInstance().endEvent (name);
    }

private:
    std::string name;
};

#define PERFETTO_FUNCTION() PerfettoScope _perfetto_##__LINE__ (__FUNCTION__)
#define PERFETTO_SCOPE(name) PerfettoScope _perfetto_##__LINE__ (name)
