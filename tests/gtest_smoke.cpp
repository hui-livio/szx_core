#include <gtest/gtest.h>
#include <SzxCrypto/Base64.h>

TEST(SzxComponents, Base64RoundTrip)
{
    EXPECT_EQ(SZX::CRYPTO::encodeBase64("gtest"), "Z3Rlc3Q=");
}

