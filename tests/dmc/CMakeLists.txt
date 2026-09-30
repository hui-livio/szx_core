szx_add_test(
    NAME test_modules_dmc
    SOURCES DmcController_test.cc
    LIBS szx::dmc szx::dmc_c
    INCLUDES "${CMAKE_SOURCE_DIR}/components/dmc/include")
