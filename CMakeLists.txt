cmake_minimum_required(VERSION 3.22)
project(catcrusher VERSION 0.0.1 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.16-security GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)
FetchContent_Declare(cat_ui GIT_REPOSITORY https://github.com/SpeakingColors/cat-ui GIT_TAG main)
FetchContent_MakeAvailable(cat_ui)

juce_add_plugin(CatCrusher
    COMPANY_NAME "SpeakingColors"
    PLUGIN_MANUFACTURER_CODE Spco
    PLUGIN_CODE Ccru
    FORMATS VST3 Standalone
    PRODUCT_NAME "Cat Crusher"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE)
target_sources(CatCrusher PRIVATE src/common/FxBase.cpp src/common/FxEditor.cpp src/Plugins.cpp src/crusher/CatCrusher.cpp)
target_include_directories(CatCrusher PRIVATE src)
target_compile_definitions(CatCrusher PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(CatCrusher PRIVATE cat_ui juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Fat Cat (dynamics-preserving saturator). Lives here until it gets its own repo.
juce_add_plugin(FatCat
    COMPANY_NAME "SpeakingColors"
    PLUGIN_MANUFACTURER_CODE Spco
    PLUGIN_CODE Fcat
    FORMATS VST3 Standalone
    PRODUCT_NAME "Fat Cat"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE)
target_sources(FatCat PRIVATE src/common/FxBase.cpp src/common/FxEditor.cpp src/PluginsFat.cpp src/fatcat/FatCat.cpp)
target_include_directories(FatCat PRIVATE src)
target_compile_definitions(FatCat PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(FatCat PRIVATE cat_ui juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
