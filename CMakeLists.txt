cmake_minimum_required(VERSION 3.22)
project(MixbusWebViewClickRepro VERSION 1.0.0 LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is fetched at configure time so this repro is self-contained. Pinned to the
# version we observed the bug with; any recent JUCE 7/8 reproduces it.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(WebViewClickRepro
    COMPANY_NAME             "Repro"
    PLUGIN_MANUFACTURER_CODE  Rpro
    PLUGIN_CODE               Wvcr
    FORMATS                   VST3 Standalone
    PRODUCT_NAME             "WebView Click Repro"
    BUNDLE_ID                "com.repro.webviewclickrepro"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE)   # auto-installs the VST3 to ~/Library/Audio/Plug-Ins

target_sources(WebViewClickRepro PRIVATE
    Source/Plugin.cpp
    Source/NativeMouseProbe.mm)

target_compile_definitions(WebViewClickRepro PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(WebViewClickRepro
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
