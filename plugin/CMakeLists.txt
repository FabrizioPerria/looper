cmake_minimum_required(VERSION 3.22)

project(looper VERSION 0.1.0)

juce_add_plugin(
  ${PROJECT_NAME}
  COMPANY
  fp
  IS_SYNTH
  false
  NEEDS_MIDI_INPUT
  true
  COPY_PLUGIN_AFTER_BUILD
  true
  NEEDS_MIDI_OUTPUT
  FALSE
  PLUGIN_MANUFACTURER_CODE
  FABP
  PLUGIN_CODE
  LO-1
  FORMATS
  AU
  VST3
  Standalone
  PRODUCT_NAME
  ${PROJECT_NAME}
  COMPANY_NAME
  "fp")

juce_add_binary_data(
  BinaryData
  SOURCES
  resources/mute.svg
  resources/solo.svg
  resources/undo.svg
  resources/redo.svg
  resources/clear.svg
  resources/freeze.svg)

get_target_property(juce_library_code ${PROJECT_NAME}
                    JUCE_GENERATED_SOURCES_DIRECTORY)
juce_generate_juce_header(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES JUCE_LIBRARY_CODE
                                                 ${juce_library_code})
set(juce_binary_data_folder
    "${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_BinaryData/JuceLibraryCode")
set_target_properties(${PROJECT_NAME} PROPERTIES JUCE_BINARY_DATA_FOLDER
                                                 ${juce_binary_data_folder})

target_sources(
  ${PROJECT_NAME}
  PRIVATE source/PluginEditor.cpp source/PluginProcessor.cpp
          source/engine/LoopTrack.cpp source/engine/LooperEngine.cpp
          source/ui/components/WaveformComponent.cpp)

target_include_directories(
  ${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/source
                         ${LIB_DIR}/soundtouch/include)

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE juce::juce_dsp juce::juce_audio_utils juce::juce_gui_basics BinaryData
          SoundTouch
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags
         juce::juce_recommended_warning_flags)

target_compile_definitions(
  ${PROJECT_NAME}
  PUBLIC JUCE_WEB_BROWSER=0 JUCE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
         JUCE_SILENCE_XCODE_15_LINKER_WARNING=1)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
target_link_options(${PROJECT_NAME} PUBLIC
                    "-Wall -Werror -Wl,-weak_reference_mismatches,weak")
