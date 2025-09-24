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

TEST (CircularBufferTest, PushPop)
{
    CircularBuffer<float> buffer (8); // capacity 8
    std::vector<float> out (8, 0.0f);

    std::vector<float> in1 { 1.0f, 2.0f, 3.0f };
    buffer.pushBlock (in1.data(), in1.size());

    buffer.popBlock (out.data(), in1.size());
    EXPECT_EQ (out[0], 1.0f);
    EXPECT_EQ (out[1], 2.0f);
    EXPECT_EQ (out[2], 3.0f);

    std::vector<float> in2 { 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f };
    buffer.pushBlock (in2.data(), in2.size());

    buffer.popBlock (out.data(), in2.size());
    EXPECT_EQ (out[0], 4.0f);
    EXPECT_EQ (out[1], 5.0f);
    EXPECT_EQ (out[2], 6.0f);
    EXPECT_EQ (out[3], 7.0f);
    EXPECT_EQ (out[4], 8.0f);
    EXPECT_EQ (out[5], 9.0f);

    std::vector<float> in3 { 10.0f, 11.0f, 12.0f, 13.0f };
    buffer.pushBlock (in3.data(), in3.size());

    buffer.popBlock (out.data(), in3.size());
    EXPECT_EQ (out[0], 10.0f);
    EXPECT_EQ (out[1], 11.0f);
    EXPECT_EQ (out[2], 12.0f);
    EXPECT_EQ (out[3], 13.0f);
}

TEST (CircularBufferTest, WrapAround)
{
    CircularBuffer<float> buffer (4); // capacity 4
    std::vector<float> out (4, 0.0f);

    std::vector<float> in1 { 1.0f, 2.0f, 3.0f };
    buffer.pushBlock (in1.data(), in1.size());

    buffer.popBlock (out.data(), in1.size());
    EXPECT_EQ (out[0], 1.0f);
    EXPECT_EQ (out[1], 2.0f);
    EXPECT_EQ (out[2], 3.0f);

    std::vector<float> in2 { 4.0f, 5.0f, 6.0f };
    buffer.pushBlock (in2.data(), in2.size());

    buffer.popBlock (out.data(), in2.size());
    EXPECT_EQ (out[0], 4.0f);
    EXPECT_EQ (out[1], 5.0f);
    EXPECT_EQ (out[2], 6.0f);

    std::vector<float> in3 { 7.0f, 8.0f, 9.0f };
    buffer.pushBlock (in3.data(), in3.size());

    buffer.popBlock (out.data(), in3.size());
    EXPECT_EQ (out[0], 7.0f);
    EXPECT_EQ (out[1], 8.0f);
    EXPECT_EQ (out[2], 9.0f);
}

} // namespace audio_plugin_test
