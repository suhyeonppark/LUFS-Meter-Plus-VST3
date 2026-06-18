cmake_minimum_required(VERSION 3.22)

if (CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")
endif()

project(LufsMeterPlus VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LUFS_COPY_PLUGIN_AFTER_BUILD "Copy the built plug-in to the system plug-in directory after building." ON)

if (DEFINED JUCE_PATH)
    add_subdirectory("${JUCE_PATH}" JUCE)
else()
    find_package(JUCE CONFIG REQUIRED)
endif()

juce_add_plugin(LufsMeterPlus
    COMPANY_NAME "LUFS Meter Plus"
    BUNDLE_ID com.lufsmeterplus.plugin
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${LUFS_COPY_PLUGIN_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE LmPl
    PLUGIN_CODE LmP1
    FORMATS VST3 Standalone
    PRODUCT_NAME "LUFS Meter Plus")

juce_generate_juce_header(LufsMeterPlus)

target_sources(LufsMeterPlus
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/LufsUdpSender.cpp)

target_compile_definitions(LufsMeterPlus
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(LufsMeterPlus
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (APPLE)
    function(lufs_meter_plus_suppress_external_sdk_warnings target_name)
        if (TARGET "${target_name}")
            get_target_property(target_type "${target_name}" TYPE)
            if (target_type STREQUAL "EXECUTABLE"
                    OR target_type STREQUAL "STATIC_LIBRARY"
                    OR target_type STREQUAL "MODULE_LIBRARY"
                    OR target_type STREQUAL "SHARED_LIBRARY"
                    OR target_type STREQUAL "OBJECT_LIBRARY")
                target_compile_options("${target_name}" PRIVATE -Wno-deprecated-declarations)
            endif()
        endif()
    endfunction()

    lufs_meter_plus_suppress_external_sdk_warnings(LufsMeterPlus)
    lufs_meter_plus_suppress_external_sdk_warnings(LufsMeterPlus_Standalone)
    lufs_meter_plus_suppress_external_sdk_warnings(LufsMeterPlus_VST3)
    lufs_meter_plus_suppress_external_sdk_warnings(LufsMeterPlus_vst3_helper)
endif()
