cmake_minimum_required(VERSION 3.24)
project(Sora VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent Sql Test)
qt_standard_project_setup()
add_compile_definitions(SORA_VERSION="${PROJECT_VERSION}")

# Motor: CEF (scripts/fetch-cef.py lo deja en .cef/ y apunta la versión en .cef/current.txt).
if(NOT CEF_ROOT AND EXISTS "${CMAKE_SOURCE_DIR}/.cef/current.txt")
  file(READ "${CMAKE_SOURCE_DIR}/.cef/current.txt" _cef)
  string(STRIP "${_cef}" _cef)
  set(CEF_ROOT "${CMAKE_SOURCE_DIR}/.cef/${_cef}")
endif()
file(TO_CMAKE_PATH "${CEF_ROOT}" CEF_ROOT)
set(CEF_RUNTIME_LIBRARY_FLAG "/MD" CACHE STRING "" FORCE) # el mismo runtime que Qt
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
# FindCEF vacía CMAKE_CXX_FLAGS*: sin esto Sora se compilaba sin /O2 ni NDEBUG (y saltaban los DCHECK de CEF).
set(_sora_cxx "${CMAKE_CXX_FLAGS}")
set(_sora_cxx_release "${CMAKE_CXX_FLAGS_RELEASE}")
set(_sora_cxx_debug "${CMAKE_CXX_FLAGS_DEBUG}")
find_package(CEF REQUIRED)
set(CMAKE_CXX_FLAGS "${_sora_cxx}")
set(CMAKE_CXX_FLAGS_RELEASE "${_sora_cxx_release}")
set(CMAKE_CXX_FLAGS_DEBUG "${_sora_cxx_debug}")
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

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
