add_library(sora_core STATIC
  app/MainWindow.h app/MainWindow.cpp
  app/SingleInstance.h app/SingleInstance.cpp
  app/Browser.h app/Browser.cpp
  app/InternalPages.h app/InternalPages.cpp
  app/InternalApi.h app/InternalApi.cpp
  app/Settings.h app/Settings.cpp
  browser/Tab.h browser/Tab.cpp
  browser/TabManager.h browser/TabManager.cpp
  browser/DownloadManager.h browser/DownloadManager.cpp
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
  privacy/RequestInterceptor.h privacy/RequestInterceptor.cpp
  privacy/FilterListStore.h privacy/FilterListStore.cpp
)
target_include_directories(sora_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sora_core PUBLIC Qt6::Widgets Qt6::Network Qt6::WebEngineWidgets Qt6::Concurrent Qt6::PrintSupport Qt6::Sql Qt6::WebChannel sora_adblock)
if(WIN32)
  target_link_libraries(sora_core PUBLIC dwmapi user32)
endif()

# Páginas internas (sora://), dentro del ejecutable.
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

qt_add_executable(sora WIN32 app/main.cpp)
if(WIN32)
  target_sources(sora PRIVATE app/sora.rc)
endif()
target_link_libraries(sora PRIVATE sora_core)
