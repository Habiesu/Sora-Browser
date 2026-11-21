add_library(sora_core STATIC
  app/MainWindow.h app/MainWindow.cpp
  app/SingleInstance.h app/SingleInstance.cpp
  app/Browser.h app/Browser.cpp
  app/InternalPages.h app/InternalPages.cpp
  browser/Tab.h browser/Tab.cpp
  browser/TabManager.h browser/TabManager.cpp
  ui/Theme.h ui/Theme.cpp
  ui/Icons.h ui/Icons.cpp
  ui/TabStrip.h ui/TabStrip.cpp
  ui/WindowFrame.h ui/WindowFrame.cpp
  ui/TitleBar.h ui/TitleBar.cpp
  ui/Omnibox.h ui/Omnibox.cpp
  ui/Toolbar.h ui/Toolbar.cpp
  ui/BookmarksBar.h ui/BookmarksBar.cpp
  privacy/AdblockEngine.h privacy/AdblockEngine.cpp
  privacy/AdblockService.h privacy/AdblockService.cpp
  privacy/RequestInterceptor.h privacy/RequestInterceptor.cpp
  privacy/FilterListStore.h privacy/FilterListStore.cpp
)
target_include_directories(sora_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sora_core PUBLIC Qt6::Widgets Qt6::Network Qt6::WebEngineWidgets Qt6::Concurrent sora_adblock)
if(WIN32)
  target_link_libraries(sora_core PUBLIC dwmapi user32)
endif()

# Páginas internas (sora://), dentro del ejecutable.
qt_add_resources(sora_core "pages" PREFIX "/pages" BASE "${CMAKE_SOURCE_DIR}/resources/pages" FILES
  ${CMAKE_SOURCE_DIR}/resources/pages/newtab.html
  ${CMAKE_SOURCE_DIR}/resources/pages/newtab-incognito.html
  ${CMAKE_SOURCE_DIR}/resources/pages/res/sora.css
  ${CMAKE_SOURCE_DIR}/resources/pages/res/sora.svg
)

qt_add_executable(sora WIN32 app/main.cpp)
if(WIN32)
  target_sources(sora PRIVATE app/sora.rc)
endif()
target_link_libraries(sora PRIVATE sora_core)
