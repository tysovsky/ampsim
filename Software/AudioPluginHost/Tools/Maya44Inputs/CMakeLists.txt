# Reads or sets an ESI MAYA44 eX card's input modes from the command line, through the same
# code as the host's settings. Built along with the host when AUDIOPLUGINHOST_BUILD_TOOLS is ON
# (Windows only); see Main.cpp for usage.

add_executable(Maya44Inputs
    Main.cpp
    ../../Source/Audio/Maya44Inputs.cpp)

target_compile_features(Maya44Inputs PRIVATE cxx_std_17)
target_link_libraries(Maya44Inputs PRIVATE setupapi)
