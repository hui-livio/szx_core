szx_add_test(
    NAME szx_test_crypto
    SOURCES Crypto_test.cc
    LIBS szx::crypto
    COPY_RUNTIME
    LABELS unit component crypto)
