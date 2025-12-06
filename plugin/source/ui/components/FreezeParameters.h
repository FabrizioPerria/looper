#pragma once

struct FreezeParameters
{
    float grainLengthMs = 150.0f;
    int grainSpacing = 256;
    int maxGrains = 64;
    float positionSpread = 0.3f;
    float modRate = 0.04f;
    float pitchModDepth = 0.005f;
    float ampModDepth = 0.01f;
    float grainRandomness = 0.3f;
};
