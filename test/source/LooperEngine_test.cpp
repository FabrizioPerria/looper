#include <gtest/gtest.h>

#include "engine/LooperEngine.h"

class LooperEngineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        messageManager = juce::MessageManager::getInstance();
        messageManagerLock = std::make_unique<juce::MessageManagerLock> (juce::Thread::getCurrentThread());
        engine = std::make_unique<LooperEngine>();
        engine->prepareToPlay (8000.0, 64, 2, 2); // reduced sample rate and buffer size
    }

    std::unique_ptr<LooperEngine> engine;
    juce::MessageManager* messageManager;
    std::unique_ptr<juce::MessageManagerLock> messageManagerLock;
};

TEST_F (LooperEngineTest, InitialState) { EXPECT_EQ (engine->getNumTracks(), 2); }

TEST_F (LooperEngineTest, VolumeControl)
{
    engine->setTrackVolume (0, 0.5f);
    EXPECT_FLOAT_EQ (engine->getTrackVolume (0), 0.5f);

    engine->setTrackVolume (0, 0.0f);
    EXPECT_FLOAT_EQ (engine->getTrackVolume (0), 0.0f);

    engine->setTrackVolume (0, 1.0f);
    EXPECT_FLOAT_EQ (engine->getTrackVolume (0), 1.0f);
}

TEST_F (LooperEngineTest, MuteUnmute)
{
    EXPECT_FALSE (engine->isTrackMuted (0));

    engine->setTrackMuted (0, true);
    EXPECT_TRUE (engine->isTrackMuted (0));

    engine->setTrackMuted (0, false);
    EXPECT_FALSE (engine->isTrackMuted (0));
}

TEST_F (LooperEngineTest, SoloTrack)
{
    // Solo track 0, should mute track 1
    engine->setTrackSoloed (0, true);

    auto* track0 = engine->getTrackByIndex (0);
    auto* track1 = engine->getTrackByIndex (1);

    EXPECT_TRUE (track0->isSoloed());
    EXPECT_FALSE (track0->isMuted());
    EXPECT_TRUE (track1->isMuted());

    // Unsolo
    engine->setTrackSoloed (0, false);
    EXPECT_FALSE (track0->isSoloed());
    EXPECT_FALSE (track1->isMuted());
}

TEST_F (LooperEngineTest, PlaybackSpeedControl)
{
    engine->setTrackPlaybackSpeed (0, 0.5f);
    EXPECT_FLOAT_EQ (engine->getTrackPlaybackSpeed (0), 0.5f);

    engine->setTrackPlaybackSpeed (0, 2.0f);
    EXPECT_FLOAT_EQ (engine->getTrackPlaybackSpeed (0), 2.0f);
}

TEST_F (LooperEngineTest, PlaybackDirectionControl)
{
    EXPECT_TRUE (engine->isTrackPlaybackForward (0));

    engine->setTrackPlaybackDirectionBackward (0);
    EXPECT_FALSE (engine->isTrackPlaybackForward (0));

    engine->setTrackPlaybackDirectionForward (0);
    EXPECT_TRUE (engine->isTrackPlaybackForward (0));
}

TEST_F (LooperEngineTest, KeepPitchWhenChangingSpeed)
{
    EXPECT_FALSE (engine->getKeepPitchWhenChangingSpeed (0));

    engine->setKeepPitchWhenChangingSpeed (0, true);
    EXPECT_TRUE (engine->getKeepPitchWhenChangingSpeed (0));

    engine->setKeepPitchWhenChangingSpeed (0, false);
    EXPECT_FALSE (engine->getKeepPitchWhenChangingSpeed (0));
}

TEST_F (LooperEngineTest, OverdubGains)
{
    // Just verify it doesn't crash
    engine->setOverdubGainsForTrack (0, 0.7, 1.0);
    engine->setOverdubGainsForTrack (1, 0.5, 0.8);
}
