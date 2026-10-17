cmake_minimum_required(VERSION 3.16)
project(linux-audio-output-selector VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

add_executable(linux-audio-output-selector src/main.cpp)
target_link_libraries(linux-audio-output-selector PRIVATE Qt6::Widgets)
# PS4 toolchains can require PIC even for an executable linked against Qt.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(linux-audio-output-selector PRIVATE -fPIC)
    target_link_options(linux-audio-output-selector PRIVATE -pie)
endif()

install(TARGETS linux-audio-output-selector RUNTIME DESTINATION bin)
configure_file(packaging/linux-audio-output-selector.desktop.in
    linux-audio-output-selector.desktop @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/linux-audio-output-selector.desktop"
    DESTINATION share/applications)

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME system-language
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_locale.py"
            "$<TARGET_FILE:linux-audio-output-selector>")
    endif()
endif()
