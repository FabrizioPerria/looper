#include "engine/LoopLifo.h"
#include <gtest/gtest.h>

class LoopLifoTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        lifo = std::make_unique<LoopLifo>();
        lifo->prepareToPlay (5); // 5 undo slots
    }

    std::unique_ptr<LoopLifo> lifo;
};

TEST_F (LoopLifoTest, Constructor)
{
    LoopLifo l;
    l.prepareToPlay (3);
    EXPECT_EQ (l.getCapacity(), 3);
    EXPECT_EQ (l.getWritePos(), 0);
    EXPECT_EQ (l.getActiveLayers(), 0);
}

TEST_F (LoopLifoTest, PrepareToWrite)
{
    int start1, size1, start2, size2;
    lifo->prepareToWrite (1, start1, size1, start2, size2);

    EXPECT_EQ (start1, 0);
    EXPECT_EQ (size1, 1);
    EXPECT_EQ (start2, 0);
    EXPECT_EQ (size2, 0);
}

TEST_F (LoopLifoTest, FinishedWriteIncrementsWritePos)
{
    int start1, size1, start2, size2;
    lifo->prepareToWrite (1, start1, size1, start2, size2);
    lifo->finishedWrite (1, false);

    EXPECT_EQ (lifo->getWritePos(), 1);
    EXPECT_EQ (lifo->getActiveLayers(), 1);

    lifo->prepareToWrite (1, start1, size1, start2, size2);
    lifo->finishedWrite (1, false);

    EXPECT_EQ (lifo->getWritePos(), 2);
    EXPECT_EQ (lifo->getActiveLayers(), 2);
}

TEST_F (LoopLifoTest, WrapAroundWritePos)
{
    for (int i = 0; i < 5; ++i)
    {
        int s1, sz1, s2, sz2;
        lifo->prepareToWrite (1, s1, sz1, s2, sz2);
        lifo->finishedWrite (1, false);
    }

    EXPECT_EQ (lifo->getWritePos(), 0);
    EXPECT_EQ (lifo->getActiveLayers(), 5);
}

TEST_F (LoopLifoTest, PrepareToReadNewestLayer)
{
    // Push 3 layers
    for (int i = 0; i < 3; ++i)
        lifo->finishedWrite (1, false);

    int start1, size1, start2, size2;
    lifo->prepareToRead (1, start1, size1, start2, size2);

    EXPECT_EQ (size1, 1);
    EXPECT_EQ (start1, 2); // newest layer index
    EXPECT_EQ (size2, 0);
}

TEST_F (LoopLifoTest, FinishedReadDecrementsActiveLayers)
{
    // Push 3 layers
    for (int i = 0; i < 3; ++i)
        lifo->finishedWrite (1, false);

    int start1, size1, start2, size2;
    lifo->prepareToRead (1, start1, size1, start2, size2);
    lifo->finishedRead (1, false);

    EXPECT_EQ (lifo->getActiveLayers(), 2);
    EXPECT_EQ (lifo->getWritePos(), 2); // writePos moves back after pop

    // Pop again
    lifo->prepareToRead (1, start1, size1, start2, size2);
    lifo->finishedRead (1, false);

    EXPECT_EQ (lifo->getActiveLayers(), 1);
    EXPECT_EQ (lifo->getWritePos(), 1);
}

TEST_F (LoopLifoTest, PopEmptyReturnsZeroSize)
{
    int start1, size1, start2, size2;
    lifo->prepareToRead (1, start1, size1, start2, size2);

    EXPECT_EQ (size1, 0);
    EXPECT_EQ (size2, 0);
}

TEST_F (LoopLifoTest, ReadWithNoActiveLayersDoesNothing)
{
    int start1, size1, start2, size2;
    lifo->prepareToRead (1, start1, size1, start2, size2);
    lifo->finishedRead (1, false); // should do nothing

    EXPECT_EQ (lifo->getActiveLayers(), 0);
    EXPECT_EQ (lifo->getWritePos(), 0);
}

// prepareToRead branch: activeLayers == 0
TEST_F (LoopLifoTest, PrepareToReadEmptyStack)
{
    int start1, size1, start2, size2;
    lifo->prepareToRead (1, start1, size1, start2, size2);

    EXPECT_EQ (size1, 0);
    EXPECT_EQ (size2, 0);
}

// finishedRead branch: activeLayers == 0
TEST_F (LoopLifoTest, FinishedReadEmptyStack)
{
    int start1, size1, start2, size2;

    // Attempt to pop with empty stack
    lifo->prepareToRead (1, start1, size1, start2, size2);
    lifo->finishedRead (1, false);

    EXPECT_EQ (lifo->getActiveLayers(), 0);
    EXPECT_EQ (lifo->getWritePos(), 0);
}

// prepareToRead and finishedRead branch: non-empty stack
TEST_F (LoopLifoTest, PrepareAndFinishedReadNonEmptyStack)
{
    int start1, size1, start2, size2;

    // Push a layer
    lifo->prepareToWrite (1, start1, size1, start2, size2);
    lifo->finishedWrite (1, false);

    // Pop the layer
    lifo->prepareToRead (1, start1, size1, start2, size2);
    EXPECT_EQ (size1, 1);
    EXPECT_EQ (size2, 0);
    EXPECT_EQ (start1, 0);

    lifo->finishedRead (1, false);
    EXPECT_EQ (lifo->getActiveLayers(), 0);
    EXPECT_EQ (lifo->getWritePos(), 0);
}

// ensure wraparound branches are touched
TEST_F (LoopLifoTest, WrapAroundPushAndPop)
{
    int start1, size1, start2, size2;

    // Fill stack completely
    for (int i = 0; i < 3; ++i)
    {
        lifo->prepareToWrite (1, start1, size1, start2, size2);
        lifo->finishedWrite (1, false);
    }

    EXPECT_EQ (lifo->getActiveLayers(), 3);

    // Pop all layers
    for (int i = 0; i < 3; ++i)
    {
        lifo->prepareToRead (1, start1, size1, start2, size2);
        lifo->finishedRead (1, false);
    }

    EXPECT_EQ (lifo->getActiveLayers(), 0);
    EXPECT_EQ (lifo->getWritePos(), 0);
}
