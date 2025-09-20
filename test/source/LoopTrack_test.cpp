#include <gtest/gtest.h>

#include "LoopTrack.h"

namespace audio_plugin_test
{
TEST (LoopTrack, PrepareToPlay)
{
    LoopTrack loopTrack {};
    loopTrack.prepareToPlay (44100.0, 1, 512, 1);
    auto buffer = loopTrack.getBuffer();
    ASSERT_EQ (buffer.getNumChannels(), 1);
    // expected size round up to multiple of block size
    // 44100 samples at 44100 Hz for 1 sec duration, block size 512 -> 44544 samples
    ASSERT_EQ (buffer.getNumSamples(), 44544);

    auto undoBuffer = loopTrack.getUndoBuffer();
    ASSERT_EQ (undoBuffer.getNumChannels(), 1);
    // expected size round up to multiple of block size
    // 44100 samples at 44100 Hz for 1 sec duration, block size 512 -> 44544 samples
    ASSERT_EQ (undoBuffer.getNumSamples(), 44544);
}
} // namespace audio_plugin_test
