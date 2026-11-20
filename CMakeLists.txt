cmake_minimum_required(VERSION 3.24)
project(Sora VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Widgets Network WebEngineCore WebEngineWidgets Concurrent Test)
qt_standard_project_setup()

include(FetchContent)
FetchContent_Declare(Corrosion
  GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
  GIT_TAG v0.5.2
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Corrosion)
corrosion_import_crate(MANIFEST_PATH adblock-ffi/Cargo.toml CRATE_TYPES staticlib PROFILE release)
target_include_directories(sora_adblock INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/adblock-ffi/include)

enable_testing()
add_subdirectory(src)
add_subdirectory(tests)
