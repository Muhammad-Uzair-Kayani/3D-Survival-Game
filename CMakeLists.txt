cmake_minimum_required(VERSION 3.20)
project(AdaptiveSocialSurvivalTPS LANGUAGES CXX)

option(BUILD_PYTHON_PLUGIN "Embed the Python policy plugin in GameRuntime" OFF)

add_library(survival_core STATIC
    cpp/core/src/game_session.cpp
)
add_library(Survival::Core ALIAS survival_core)

target_include_directories(survival_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp/core/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(survival_core PUBLIC cxx_std_20)
set_target_properties(survival_core PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

add_executable(GameRuntime cpp/runtime/main.cpp)
target_link_libraries(GameRuntime PRIVATE Survival::Core)
target_compile_features(GameRuntime PRIVATE cxx_std_20)

if(BUILD_PYTHON_PLUGIN)
    find_package(Python REQUIRED COMPONENTS Interpreter Development.Embed)
    find_package(pybind11 CONFIG REQUIRED)

    target_sources(GameRuntime PRIVATE cpp/runtime/python_policy.cpp)
    target_link_libraries(GameRuntime PRIVATE pybind11::embed)
    target_compile_definitions(GameRuntime PRIVATE
        SURVIVAL_ENABLE_PYTHON_PLUGIN
        "SURVIVAL_PYTHON_MODULE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/python\""
    )
endif()
