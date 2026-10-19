szx_add_test(
    NAME test_modules_dmc
    SOURCES DmcController_test.cc
    LIBS szx::dmc
    INCLUDES "${CMAKE_SOURCE_DIR}/components/dmc/include")
