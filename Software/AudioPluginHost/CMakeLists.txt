# ==============================================================================
#
#  This file is part of the JUCE library.
#  Copyright (c) 2022 - Raw Material Software Limited
#
#  JUCE is an open source library subject to commercial or open-source
#  licensing.
#
#  By using JUCE, you agree to the terms of both the JUCE 7 End-User License
#  Agreement and JUCE Privacy Policy.
#
#  End User License Agreement: www.juce.com/juce-7-licence
#  Privacy Policy: www.juce.com/juce-privacy-policy
#
#  Or: You may also use this code under the terms of the GPL v3 (see
#  www.gnu.org/licenses).
#
#  JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
#  EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
#  DISCLAIMED.
#
# ==============================================================================

cmake_minimum_required(VERSION 3.22)

project(AudioPluginHost VERSION 1.0.0)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# ==============================================================================
# Dependencies are downloaded at configure time, so the project builds from a
# clean checkout. JUCE is pinned to the version the host was developed against.

include(FetchContent)

FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/7.0.12.tar.gz
    URL_HASH SHA256=94e3b35e1990cd67f59736bb5e1eff1cfd9590b16fea82696f309a8f59452434)

FetchContent_Declare(pugixml
    URL https://github.com/zeux/pugixml/releases/download/v1.14/pugixml-1.14.tar.gz
    URL_HASH SHA256=2f10e276870c64b1db6809050a75e11a897a8d7456c4be5c6b2e35a11168a015)

FetchContent_MakeAvailable(JUCE pugixml)

# ASIO support needs the Steinberg ASIO SDK, which isn't redistributed here.
# Download it from Steinberg and configure with -DASIO_SDK_DIR=<path to the SDK>.
set(ASIO_SDK_DIR "" CACHE PATH "Path to the Steinberg ASIO SDK (enables the ASIO device type)")

# Developer tools in Tools/ (e.g. ParamDump, for mapping a plugin's parameters)
option(AUDIOPLUGINHOST_BUILD_TOOLS "Also build the developer tools in Tools/" OFF)

# ==============================================================================
juce_add_gui_app(AudioPluginHost
    BUNDLE_ID                       com.juce.pluginhost
    ICON_BIG                        "${CMAKE_CURRENT_SOURCE_DIR}/Source/JUCEAppIcon.png"
    MICROPHONE_PERMISSION_ENABLED   TRUE
    PLUGINHOST_AU                   TRUE)

juce_generate_juce_header(AudioPluginHost)

target_sources(AudioPluginHost PRIVATE
    Source/HostStartup.cpp
    Source/Plugins/ARAPlugin.cpp
    Source/Plugins/IOConfigurationWindow.cpp
    Source/Plugins/InternalPlugins.cpp
    Source/Plugins/PluginGraph.cpp
    Source/Plugins/PluginModules.cpp
    Source/UI/AudioSettingsPopup.cpp
    Source/UI/ConnectorComponent.cpp
    Source/UI/GraphDocumentComponent.cpp
    Source/UI/GraphEditorPanel.cpp
    Source/UI/MainHostWindow.cpp
    Source/UI/PinComponent.cpp
    Source/UI/PluginComponent.cpp
    Source/UI/PluginControlsPanel.cpp
    Source/UI/ThemeManager.cpp
    Source/UI/PluginControls/ArchetypeGojiraa.cpp
    Source/UI/PluginControls/ControlXmlParser.cpp
    Source/UI/PluginControls/DarkGlass.cpp
    Source/UI/PluginControls/PluginControl.cpp)

juce_add_binary_data(AudioPluginHostData SOURCES
    ${juce_SOURCE_DIR}/examples/Assets/cassette_recorder.wav
    ${juce_SOURCE_DIR}/examples/Assets/cello.wav
    ${juce_SOURCE_DIR}/examples/Assets/guitar_amp.wav
    ${juce_SOURCE_DIR}/examples/Assets/proaudio.path
    ${juce_SOURCE_DIR}/examples/Assets/reverb_ir.wav
    ${juce_SOURCE_DIR}/examples/Assets/singing.ogg)

target_compile_definitions(AudioPluginHost PRIVATE
    JUCE_ALSA=1
    JUCE_CONTENT_SHARING=1
    JUCE_DIRECTSOUND=1
    JUCE_DISABLE_CAUTIOUS_PARAMETER_ID_CHECKING=1
    JUCE_PLUGINHOST_LADSPA=1
    JUCE_PLUGINHOST_LV2=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_ARA=0
    JUCE_USE_CAMERA=0
    JUCE_USE_CDBURNER=0
    JUCE_USE_CDREADER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=1
    JUCE_VST3_HOST_CROSS_PLATFORM_UID=1
    JUCE_WASAPI=1
    JUCE_WEB_BROWSER=0
    PIP_JUCE_EXAMPLES_DIRECTORY_STRING="${juce_SOURCE_DIR}/examples"
    # This is a temporary workaround to allow builds to complete on Xcode 15.
    # Add -Wl,-ld_classic to the OTHER_LDFLAGS build setting if you need to
    # deploy to older versions of macOS/iOS.
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1)

if(WIN32 AND ASIO_SDK_DIR)
    target_include_directories(AudioPluginHost PRIVATE "${ASIO_SDK_DIR}/common")
    target_compile_definitions(AudioPluginHost PRIVATE JUCE_ASIO=1)
elseif(WIN32)
    message(STATUS "ASIO_SDK_DIR not set: building without ASIO support")
endif()

# The app reads plugin definitions (control panels, modules) from a folder next to it
add_custom_command(TARGET AudioPluginHost POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/PluginUIDefinitions"
            "$<TARGET_FILE_DIR:AudioPluginHost>/PluginUIDefinitions")

target_link_libraries(AudioPluginHost PRIVATE
    AudioPluginHostData
    pugixml::pugixml
    juce::juce_audio_utils
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(AUDIOPLUGINHOST_BUILD_TOOLS)
    add_subdirectory(Tools/ParamDump)
endif()
