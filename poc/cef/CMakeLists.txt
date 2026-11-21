# Prueba de concepto: una ventana Qt Widgets con una pestaña CEF (tarea 1 del hito CEF).
# cmake -S poc/cef -B build-cef -G Ninja -DCEF_ROOT=<.cef/cef_binary_...> -DCMAKE_PREFIX_PATH=<Qt>
cmake_minimum_required(VERSION 3.21)
project(sora_cef_poc CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# CEF compila con /MT por defecto; Qt usa /MD. Todo el proyecto con /MD.
set(CEF_RUNTIME_LIBRARY_FLAG "/MD" CACHE STRING "" FORCE)
file(TO_CMAKE_PATH "${CEF_ROOT}" CEF_ROOT)
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Subprocesos de CEF (renderer, GPU, red): sin Qt.
add_executable(sora-helper WIN32 helper.cpp compatibility.manifest)
SET_EXECUTABLE_TARGET_PROPERTIES(sora-helper)
add_dependencies(sora-helper libcef_dll_wrapper)
target_link_libraries(sora-helper libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS})

add_executable(sora-cef WIN32 main.cpp compatibility.manifest)
SET_EXECUTABLE_TARGET_PROPERTIES(sora-cef)
add_dependencies(sora-cef libcef_dll_wrapper sora-helper)
target_link_libraries(sora-cef libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS} Qt6::Widgets)
set_target_properties(sora-cef PROPERTIES WIN32_EXECUTABLE OFF) # consola para --bench

# Binarios y recursos de CEF junto al .exe
COPY_FILES(sora-cef "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "$<TARGET_FILE_DIR:sora-cef>")
COPY_FILES(sora-cef "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "$<TARGET_FILE_DIR:sora-cef>")
