#pragma once

#include <JuceHeader.h>
#include <atomic>

struct SelectionSnapshot
{
    int activeTrackIndex = 0;
    int pendingTrackIndex = -1;
    int version = 0;
};

class SelectionStateBridge
{
public:
    SelectionStateBridge() : publishedSnapshot (SelectionSnapshot()) {}
    void publish (int active, int pending)
    {
        snapshot.activeTrackIndex = active;
        snapshot.pendingTrackIndex = pending;
        snapshot.version++;
        publishedSnapshot.store (snapshot);
    }

    SelectionSnapshot getSnapshot() const { return publishedSnapshot.load(); }

private:
    SelectionSnapshot snapshot;
    std::atomic<SelectionSnapshot> publishedSnapshot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionStateBridge)
};
