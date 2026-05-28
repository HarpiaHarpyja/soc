cmake_minimum_required(VERSION 3.14)

project(battery_soc VERSION 0.1.0 LANGUAGES CXX)

add_library(battery_soc
    src/BatterySoc.cpp
)

target_include_directories(battery_soc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(battery_soc PUBLIC cxx_std_11)

option(BATTERY_SOC_BUILD_TESTS "Build battery_soc tests" ON)
option(BATTERY_SOC_BUILD_EXAMPLES "Build battery_soc examples" ON)

if(BATTERY_SOC_BUILD_EXAMPLES)
    add_executable(battery_soc_example examples/basic_usage.cpp)
    target_link_libraries(battery_soc_example PRIVATE battery_soc)
endif()

if(BATTERY_SOC_BUILD_TESTS)
    enable_testing()

    add_executable(battery_soc_tests tests/test_battery_soc.cpp)
    target_link_libraries(battery_soc_tests PRIVATE battery_soc)

    add_executable(battery_soc_profile_simulation tests/simulate_1800mah_profile.cpp)
    target_link_libraries(battery_soc_profile_simulation PRIVATE battery_soc)

    add_test(NAME battery_soc_tests COMMAND battery_soc_tests)
    add_test(NAME battery_soc_profile_simulation COMMAND battery_soc_profile_simulation)
endif()
