# Looper

A simple audio looper plugin built with JUCE framework.

## Features

- Record audio input
- 4 tracks for looping
- Infinite overdubs on existing loops
- Load backing tracks per track
- Play, stop, and clear loops
- Undo/redo functionality
- Adjustable playback speed and reverse playback
- Mute/Solo per track
- Simple and intuitive user interface
- Low latency performance

## Build

1. Clone the repository:
2. Run ./build.sh
3. Open your Daw of choice and load the plugin.

## Run standalone
App is not signed so, to run mac standalone you need to open a terminal and run xattr -d com.apple.quarantine /Applications/Looper.app
