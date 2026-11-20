function(sora_add_test name)
  qt_add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE sora_core Qt6::Test)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;SORA_TEST_DATA=${CMAKE_CURRENT_SOURCE_DIR}/data")
endfunction()

sora_add_test(tst_mainwindow)
sora_add_test(tst_adblockengine)
sora_add_test(tst_requestinterceptor)
sora_add_test(tst_filterliststore)
sora_add_test(tst_theme)
