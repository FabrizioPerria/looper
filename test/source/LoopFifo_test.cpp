#include "LoopFifo.h"
#include <gtest/gtest.h>

class LoopFifoTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a loop FIFO with size 1000 for most tests
        fifo = std::make_unique<LoopFifo> (1000);
    }

    std::unique_ptr<LoopFifo> fifo;
};

TEST_F (LoopFifoTest, Constructor)
{
    LoopFifo f (1000);
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
        fifo->finishedWrite (1, false);
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
        fifo->finishedRead (1, false);
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
    fifo->finishedWrite (100, false);
    EXPECT_EQ (fifo->getWritePos(), 100);

    fifo->finishedWrite (950, false);
    EXPECT_EQ (fifo->getWritePos(), 50); // Should wrap around
}

TEST_F (LoopFifoTest, FinishedWriteOverdub)
{
    // Set different read and write positions
    fifo->finishedWrite (100, false);
    fifo->finishedRead (50, false);

    EXPECT_EQ (fifo->getWritePos(), 100);
    EXPECT_EQ (fifo->getReadPos(), 50);

    // Overdub should sync write pos to read pos
    fifo->finishedWrite (10, true);
    EXPECT_EQ (fifo->getWritePos(), 50);
}

TEST_F (LoopFifoTest, FinishedReadNormal)
{
    fifo->finishedRead (100, false);
    EXPECT_EQ (fifo->getReadPos(), 100);

    fifo->finishedRead (950, false);
    EXPECT_EQ (fifo->getReadPos(), 50); // Should wrap around
}

TEST_F (LoopFifoTest, FinishedReadOverdub)
{
    // Set different read and write positions
    fifo->finishedWrite (100, false);
    fifo->finishedRead (50, false);

    EXPECT_EQ (fifo->getWritePos(), 100);
    EXPECT_EQ (fifo->getReadPos(), 50);

    // Overdub should sync write pos to read pos
    fifo->finishedRead (10, true);
    EXPECT_EQ (fifo->getWritePos(), 60);
    EXPECT_EQ (fifo->getReadPos(), 60);
}
