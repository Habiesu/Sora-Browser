add_library(sora_core STATIC
  app/MainWindow.h app/MainWindow.cpp
  browser/Tab.h browser/Tab.cpp
  browser/TabManager.h browser/TabManager.cpp
  ui/Theme.h ui/Theme.cpp
  ui/Icons.h ui/Icons.cpp
  ui/TabStrip.h ui/TabStrip.cpp
  privacy/AdblockEngine.h privacy/AdblockEngine.cpp
  privacy/AdblockService.h privacy/AdblockService.cpp
  privacy/RequestInterceptor.h privacy/RequestInterceptor.cpp
  privacy/FilterListStore.h privacy/FilterListStore.cpp
)
target_include_directories(sora_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sora_core PUBLIC Qt6::Widgets Qt6::Network Qt6::WebEngineWidgets Qt6::Concurrent sora_adblock)

qt_add_executable(sora WIN32 app/main.cpp)
target_link_libraries(sora PRIVATE sora_core)
