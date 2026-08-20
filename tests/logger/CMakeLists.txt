szx_add_test(
    NAME szx_test_logger
    SOURCES Logger_test.cc
    LIBS szx::logger
    COPY_RUNTIME
    LABELS unit component logger)
