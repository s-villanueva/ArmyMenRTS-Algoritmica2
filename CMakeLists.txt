cmake_minimum_required(VERSION 3.16)
project(ArmyMenRTS_Playable CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp src/*/*.cpp)

# --- Executable target
add_executable(ArmyMenRTS ${SOURCES})

# --- Link SFML
target_link_libraries(ArmyMenRTS PRIVATE
        sfml-graphics
        sfml-window
        sfml-system
)

# --- Optional: copy SFML DLLs/libs next to the binary on build (for Windows users)
# if(WIN32)
#     add_custom_command(TARGET ArmyMenRTS POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         $<TARGET_FILE_DIR:ArmyMenRTS>/sfml-*.dll
#         $<TARGET_FILE_DIR:ArmyMenRTS>)
# endif()
