cmake_minimum_required(VERSION 3.24)
project(MojaveIsoNative VERSION 0.6.0 LANGUAGES CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Fallout New Vegas requires a Windows Win32 (x86) build.")
endif()
add_library(MojaveIsoNative SHARED src/plugin.cpp)
target_compile_features(MojaveIsoNative PRIVATE cxx_std_20)
target_compile_definitions(MojaveIsoNative PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(MojaveIsoNative PRIVATE /W4 /EHsc)
target_link_libraries(MojaveIsoNative PRIVATE d3d9 user32 gdi32 shell32 ole32 uuid)
set_target_properties(MojaveIsoNative PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(MojaveIsoLaunch WIN32 src/launcher.cpp)
target_compile_features(MojaveIsoLaunch PRIVATE cxx_std_20)
target_compile_definitions(MojaveIsoLaunch PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MojaveIsoLaunch PRIVATE user32 shell32 ole32 uuid)
set_target_properties(MojaveIsoLaunch PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(MojaveIsoWheelTests tests/wheel_input_tests.cpp)
target_compile_features(MojaveIsoWheelTests PRIVATE cxx_std_20)

add_executable(MojaveIsoCameraTests tests/camera_tests.cpp)
target_compile_features(MojaveIsoCameraTests PRIVATE cxx_std_20)

add_executable(MojaveIsoPathTests tests/pathfinder_tests.cpp)
target_compile_features(MojaveIsoPathTests PRIVATE cxx_std_20)

add_executable(MojaveIsoCombatTests tests/combat_tests.cpp)
target_compile_features(MojaveIsoCombatTests PRIVATE cxx_std_20)

add_executable(MojaveIsoFollowerTests tests/route_follower_tests.cpp)
target_compile_features(MojaveIsoFollowerTests PRIVATE cxx_std_20)

add_executable(MojaveIsoNavmeshTests tests/navmesh_tests.cpp)
target_compile_features(MojaveIsoNavmeshTests PRIVATE cxx_std_20)

add_executable(MojaveIsoDamageTests tests/damage_number_tests.cpp)
target_compile_features(MojaveIsoDamageTests PRIVATE cxx_std_20)

add_executable(MojaveIsoDisplayTests tests/display_settings_tests.cpp)
target_compile_features(MojaveIsoDisplayTests PRIVATE cxx_std_20)
target_link_libraries(MojaveIsoDisplayTests PRIVATE shell32 ole32 uuid)

add_executable(MojaveIsoLightingTests tests/lighting_distance_tests.cpp)
target_compile_features(MojaveIsoLightingTests PRIVATE cxx_std_20)

add_executable(MojaveIsoContextTests tests/context_menu_tests.cpp)
target_compile_features(MojaveIsoContextTests PRIVATE cxx_std_20)
