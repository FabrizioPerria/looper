#pragma once

constexpr int NUM_TRACKS = 4;

constexpr float FREEZE_BUFFER_DURATION_SECONDS = 0.5f;
constexpr int MAX_GRAINS = 64;
constexpr int GRAIN_LENGTH = 16384;
constexpr int GRAIN_SPACING = 512;
constexpr int WINDOW_SIZE = 2048;
constexpr int WINDOW_SIZE_1 = WINDOW_SIZE - 1;
constexpr int MOD_TABLE_SIZE = 1024;
constexpr int MOD_TABLE_MASK = MOD_TABLE_SIZE - 1;

constexpr float MOD_RATE = 0.04f;
constexpr float PITCH_MOD_DEPTH = 0.005f;
constexpr float AMP_MOD_DEPTH = 0.01f;
