szx_add_test(
    NAME szx_test_common
    SOURCES
        Defer_test.cc
        Geometry_test.cc
        Result_test.cc
        StringHelper_test.cc
        TimeHelper_test.cc
        UUID_test.cc
    LIBS szx::common
    LABELS unit component common)
