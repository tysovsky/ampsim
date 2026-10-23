# Lists a plugin's parameters, for writing its definition file in PluginUIDefinitions.
# Built along with the host when AUDIOPLUGINHOST_BUILD_TOOLS is ON; see Main.cpp for usage.

juce_add_console_app(ParamDump)

target_sources(ParamDump PRIVATE Main.cpp)

target_compile_definitions(ParamDump PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(ParamDump PRIVATE
    juce::juce_audio_processors
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
