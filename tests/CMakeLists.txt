function(sora_add_test name)
  qt_add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE sora_core Qt6::Test)
  set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}") # junto a libcef.dll
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;SORA_TEST_DATA=${CMAKE_CURRENT_SOURCE_DIR}/data")
endfunction()

sora_add_test(tst_adblockengine)
sora_add_test(tst_filterliststore)
sora_add_test(tst_theme)
sora_add_test(tst_icons)
sora_add_test(tst_tabstrip)
sora_add_test(tst_singleinstance)
sora_add_test(tst_mainmenu)
sora_add_test(tst_stores)
sora_add_test(tst_suggestions)

# Pendientes de portar a CEF (necesitan el motor arrancado dentro de la prueba): tst_mainwindow,
# tst_requestinterceptor, tst_tabmanager, tst_windowframe, tst_browser, tst_internalpages, tst_youtube
# y tst_nativewindow. Se quedan fuera de la compilación hasta entonces.
