# Parte de CEF común al navegador y a sus subprocesos (sin Qt): esquema sora:// y puente JS.
add_library(sora_engine_app STATIC engine/EngineApp.h engine/EngineApp.cpp)
target_include_directories(sora_engine_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CEF_ROOT})
target_compile_definitions(sora_engine_app PUBLIC NOMINMAX UNICODE _UNICODE)
target_link_libraries(sora_engine_app PUBLIC libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS})
add_dependencies(sora_engine_app libcef_dll_wrapper)

add_library(sora_core STATIC
  app/MainWindow.h app/MainWindow.cpp
  app/SingleInstance.h app/SingleInstance.cpp
  app/Browser.h app/Browser.cpp
  app/InternalPages.h app/InternalPages.cpp
  app/InternalApi.h app/InternalApi.cpp
  app/Settings.h app/Settings.cpp
  browser/Engine.h browser/Engine.cpp
  browser/Profile.h browser/Profile.cpp
  browser/Extensions.h browser/Extensions.cpp
  browser/Tab.h browser/Tab.cpp
  browser/TabManager.h browser/TabManager.cpp
  browser/DownloadManager.h browser/DownloadManager.cpp
  browser/Suggestions.h browser/Suggestions.cpp
  ui/Theme.h ui/Theme.cpp
  ui/Icons.h ui/Icons.cpp
  ui/TabStrip.h ui/TabStrip.cpp
  ui/WindowFrame.h ui/WindowFrame.cpp
  ui/TitleBar.h ui/TitleBar.cpp
  ui/Omnibox.h ui/Omnibox.cpp
  ui/Toolbar.h ui/Toolbar.cpp
  ui/MainMenu.h ui/MainMenu.cpp
  ui/FindBar.h ui/FindBar.cpp
  ui/BookmarksBar.h ui/BookmarksBar.cpp
  data/Database.h data/Database.cpp
  data/HistoryStore.h data/HistoryStore.cpp
  data/BookmarkStore.h data/BookmarkStore.cpp
  privacy/AdblockEngine.h privacy/AdblockEngine.cpp
  privacy/AdblockService.h privacy/AdblockService.cpp
  privacy/FilterListStore.h privacy/FilterListStore.cpp
)
target_include_directories(sora_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sora_core PUBLIC Qt6::Widgets Qt6::Network Qt6::Concurrent Qt6::Sql sora_adblock sora_engine_app)
if(WIN32)
  target_link_libraries(sora_core PUBLIC dwmapi user32)
endif()

# Páginas internas (sora://), dentro del ejecutable.
qt_add_resources(sora_core "adblock" PREFIX "/adblock" BASE "${CMAKE_SOURCE_DIR}/resources/adblock" FILES
  ${CMAKE_SOURCE_DIR}/resources/adblock/youtube.js)
qt_add_resources(sora_core "pages" PREFIX "/pages" BASE "${CMAKE_SOURCE_DIR}/resources/pages" FILES
  ${CMAKE_SOURCE_DIR}/resources/pages/newtab.html
  ${CMAKE_SOURCE_DIR}/resources/pages/newtab-incognito.html
  ${CMAKE_SOURCE_DIR}/resources/pages/history.html
  ${CMAKE_SOURCE_DIR}/resources/pages/bookmarks.html
  ${CMAKE_SOURCE_DIR}/resources/pages/downloads.html
  ${CMAKE_SOURCE_DIR}/resources/pages/settings.html
  ${CMAKE_SOURCE_DIR}/resources/pages/extensions.html
  ${CMAKE_SOURCE_DIR}/resources/pages/res/sora.js
  ${CMAKE_SOURCE_DIR}/resources/pages/res/sora.css
  ${CMAKE_SOURCE_DIR}/resources/pages/res/sora.svg
)

# Subprocesos de CEF (renderer, GPU, red): no cargan Qt.
add_executable(sora-helper WIN32 helper/main.cpp app/sora.rc ${CMAKE_SOURCE_DIR}/resources/compatibility.manifest)
target_link_libraries(sora-helper PRIVATE sora_engine_app)
SET_EXECUTABLE_TARGET_PROPERTIES(sora-helper)

qt_add_executable(sora WIN32 app/main.cpp ${CMAKE_SOURCE_DIR}/resources/compatibility.manifest)
if(WIN32)
  target_sources(sora PRIVATE app/sora.rc)
endif()
target_link_libraries(sora PRIVATE sora_core)
add_dependencies(sora sora-helper)
target_link_options(sora PRIVATE /DELAYLOAD:libcef.dll)
target_link_libraries(sora PRIVATE delayimp)
# Binarios y recursos de CEF junto a sora.exe.
COPY_FILES(sora "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "$<TARGET_FILE_DIR:sora>")
COPY_FILES(sora "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "$<TARGET_FILE_DIR:sora>")
# Los dos ejecutables en la misma carpeta (CEF busca el helper junto a sora.exe).
set_target_properties(sora sora-helper PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
