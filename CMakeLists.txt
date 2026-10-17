cmake_minimum_required(VERSION 3.16)
project(ps4-audio-selector VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

add_executable(ps4-audio-selector src/main.cpp)
target_link_libraries(ps4-audio-selector PRIVATE Qt6::Widgets)
# PS4 toolchains can require PIC even for an executable linked against Qt.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ps4-audio-selector PRIVATE -fPIC)
    target_link_options(ps4-audio-selector PRIVATE -pie)
endif()

install(TARGETS ps4-audio-selector RUNTIME DESTINATION bin)
configure_file(packaging/ps4-audio-selector.desktop.in
    ps4-audio-selector.desktop @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ps4-audio-selector.desktop"
    DESTINATION share/applications)

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME system-language
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_locale.py"
            "$<TARGET_FILE:ps4-audio-selector>")
    endif()
endif()
