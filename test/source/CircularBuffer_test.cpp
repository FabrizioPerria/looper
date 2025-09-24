#include <gtest/gtest.h>

#include "CircularBuffer.h"

namespace audio_plugin_test
{
TEST (CircularBufferTest, AllocateInPowerOfTwo)
{
    {
        CircularBuffer<float> buffer (0);
        EXPECT_EQ (buffer.getCapacity(), 1);
    }
    {
        CircularBuffer<float> buffer (1);
        EXPECT_EQ (buffer.getCapacity(), 1);
    }
    {
        CircularBuffer<float> buffer (2);
        EXPECT_EQ (buffer.getCapacity(), 2);
    }
    for (int i = 3; i <= 20; ++i) // test up to 1M
    {
        auto n = 1 << i;
        {
            CircularBuffer<float> buffer (n);
            EXPECT_EQ (buffer.getCapacity(), n);
        }

        {
            CircularBuffer<float> buffer2 (n - 1);
            EXPECT_EQ (buffer2.getCapacity(), n);
        }

        {
            CircularBuffer<float> buffer3 (n + 1);
            EXPECT_EQ (buffer3.getCapacity(), n * 2);
        }
    }
}

} // namespace audio_plugin_test
