#include "engine/LoopFifo.h"
#include <gtest/gtest.h>

class LoopFifoTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a loop FIFO with size 1000 for most tests
        fifo = std::make_unique<LoopFifo>();
        fifo->prepareToPlay (1000);
    }

    std::unique_ptr<LoopFifo> fifo;
};

TEST_F (LoopFifoTest, Constructor)
{
    LoopFifo f;
    f.prepareToPlay (1000);
    EXPECT_EQ (f.getMusicalLength(), 1000);
    EXPECT_EQ (f.getReadPos(), 0);
    EXPECT_EQ (f.getWritePos(), 0);
}

TEST_F (LoopFifoTest, SetMusicalLength)
{
    fifo->setMusicalLength (500);
    EXPECT_EQ (fifo->getMusicalLength(), 500);
}

TEST_F (LoopFifoTest, PrepareToWriteNoWraparound)
{
    int start1, size1, start2, size2;
    fifo->prepareToWrite (100, start1, size1, start2, size2);

    EXPECT_EQ (start1, 0);
    EXPECT_EQ (size1, 100);
    EXPECT_EQ (start2, 0);
    EXPECT_EQ (size2, 0);
}

TEST_F (LoopFifoTest, PrepareToWriteWithWraparound)
{
    // Set write position near the end
    for (int i = 0; i < 900; i++)
    {
        fifo->finishedWrite (1, false, false);
    }

    int start1, size1, start2, size2;
    fifo->prepareToWrite (200, start1, size1, start2, size2);

    EXPECT_EQ (start1, 900);
    EXPECT_EQ (size1, 100); // Space until end
    EXPECT_EQ (start2, 0);
    EXPECT_EQ (size2, 100); // Remaining wraps to start
}

TEST_F (LoopFifoTest, PrepareToReadNoWraparound)
{
    int start1, size1, start2, size2;
    fifo->prepareToRead (100, start1, size1, start2, size2);

    EXPECT_EQ (start1, 0);
    EXPECT_EQ (size1, 100);
    EXPECT_EQ (start2, 0);
    EXPECT_EQ (size2, 0);
}

TEST_F (LoopFifoTest, PrepareToReadWithWraparound)
{
    // Set read position near the end
    for (int i = 0; i < 900; i++)
    {
        fifo->finishedRead (1, 1.0f, false);
    }

    int start1, size1, start2, size2;
    fifo->prepareToRead (200, start1, size1, start2, size2);

    EXPECT_EQ (start1, 900);
    EXPECT_EQ (size1, 100); // Space until end
    EXPECT_EQ (start2, 0);
    EXPECT_EQ (size2, 100); // Remaining wraps to start
}

TEST_F (LoopFifoTest, FinishedWriteNormal)
{
    fifo->finishedWrite (100, false, false);
    EXPECT_EQ (fifo->getWritePos(), 100);

    fifo->finishedWrite (950, false, false);
    EXPECT_EQ (fifo->getWritePos(), 50); // Should wrap around
}

TEST_F (LoopFifoTest, FinishedWriteOverdubWithSync)
{
    // Set different read and write positions
    fifo->finishedWrite (100, false, false);
    fifo->finishedRead (50, 1.0f, false);

    EXPECT_EQ (fifo->getWritePos(), 100);
    EXPECT_EQ (fifo->getReadPos(), 50);

    // Overdub with sync should sync write pos to read pos
    fifo->finishedWrite (10, true, true);
    EXPECT_EQ (fifo->getWritePos(), 50);
}

TEST_F (LoopFifoTest, FinishedReadNormal)
{
    fifo->finishedRead (100, 1.0f, false);
    EXPECT_EQ (fifo->getReadPos(), 100);

    fifo->finishedRead (950, 1.0f, false);
    EXPECT_EQ (fifo->getReadPos(), 50); // Should wrap around
}

TEST_F (LoopFifoTest, FinishedReadOverdub)
{
    // Set different read and write positions
    fifo->finishedWrite (100, false, false);
    fifo->finishedRead (50, 1.0f, false);

    EXPECT_EQ (fifo->getWritePos(), 100);
    EXPECT_EQ (fifo->getReadPos(), 50);

    // Overdub should sync write pos to read pos
    fifo->finishedRead (10, 1.0f, true);
    EXPECT_EQ (fifo->getWritePos(), 60);
    EXPECT_EQ (fifo->getReadPos(), 60);
}

TEST_F (LoopFifoTest, GetReverseReadIndex)
{
    // Set read position to 500
    fifo->finishedRead (500, 1.0f, false);
    
    // Test reverse indexing
    EXPECT_EQ (fifo->getReverseReadIndex (0), 500);
    EXPECT_EQ (fifo->getReverseReadIndex (1), 499);
    EXPECT_EQ (fifo->getReverseReadIndex (100), 400);
    
    // Test wrap-around (negative indices)
    EXPECT_EQ (fifo->getReverseReadIndex (600), 900); // 500 - 600 = -100, wraps to 900
}

TEST_F (LoopFifoTest, GetLastPlaybackRate)
{
    EXPECT_FLOAT_EQ (fifo->getLastPlaybackRate(), 1.0f);
    
    fifo->finishedRead (10, 0.5f, false);
    EXPECT_FLOAT_EQ (fifo->getLastPlaybackRate(), 0.5f);
    
    fifo->finishedRead (10, -1.0f, false);
    EXPECT_FLOAT_EQ (fifo->getLastPlaybackRate(), -1.0f);
}

TEST_F (LoopFifoTest, WrapAroundFlag)
{
    EXPECT_TRUE (fifo->getWrapAround());
    
    fifo->setWrapAround (false);
    EXPECT_FALSE (fifo->getWrapAround());
    
    fifo->setWrapAround (true);
    EXPECT_TRUE (fifo->getWrapAround());
}

TEST_F (LoopFifoTest, ExactReadPosition)
{
    EXPECT_DOUBLE_EQ (fifo->getExactReadPos(), 0.0);
    
    // Test fractional read position with playback rate
    fifo->finishedRead (10, 0.5f, false);
    EXPECT_DOUBLE_EQ (fifo->getExactReadPos(), 5.0);
    
    fifo->finishedRead (20, 1.5f, false);
    EXPECT_DOUBLE_EQ (fifo->getExactReadPos(), 35.0);
}

TEST_F (LoopFifoTest, NegativePlaybackRate)
{
    // Start at position 500
    fifo->finishedRead (500, 1.0f, false);
    EXPECT_EQ (fifo->getReadPos(), 500);
    
    // Go backwards with negative rate
    fifo->finishedRead (100, -1.0f, false);
    EXPECT_EQ (fifo->getReadPos(), 400);
    
    // Wrap around backwards
    fifo->finishedRead (500, -1.0f, false);
    EXPECT_EQ (fifo->getReadPos(), 900); // Should wrap to end
}

TEST_F (LoopFifoTest, Clear)
{
    fifo->finishedWrite (100, false, false);
    fifo->finishedRead (50, 1.0f, false);
    
    fifo->clear();
    
    EXPECT_EQ (fifo->getWritePos(), 0);
    EXPECT_EQ (fifo->getReadPos(), 0);
    EXPECT_EQ (fifo->getMusicalLength(), 0);
}
