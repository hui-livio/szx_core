#include "SzxCryptoService/CryptoService.h"
#include <gtest/gtest.h>
#include <vector>
using namespace SZX::CRYPTO_SERVICE;
TEST(CryptoServiceTest, ReportsCapabilities) { CryptoService service; auto c=service.capabilities(); EXPECT_TRUE(c.aes256Gcm); EXPECT_TRUE(c.rsaOaepSha256); EXPECT_TRUE(c.rsaPssSha256); EXPECT_TRUE(c.pbkdf2Sha256); }
TEST(CryptoServiceTest, KeepsStableInternalFailureCode) { EXPECT_EQ(static_cast<int>(CryptoServiceError::InternalFailure), 1600); }
TEST(CryptoServiceTest, FakeTracksCallsAndPropagatesErrors) { FakeCryptoService fake; fake.supported.aes256Gcm=true; auto r=fake.encrypt(std::vector<std::uint8_t>(32), std::vector<std::uint8_t>{1}); EXPECT_FALSE(r.ok); EXPECT_EQ(fake.encryptCalls,1U); auto d=fake.decrypt({}, {}, {}); EXPECT_FALSE(d.ok); EXPECT_EQ(fake.decryptCalls,1U); EXPECT_TRUE(fake.capabilities().aes256Gcm); }
TEST(CryptoServiceTest, EncryptsAndDecryptsThroughFacade) { CryptoService service; std::vector<std::uint8_t> key(32,7), data{1,2,3}; auto e=service.encrypt(key,data);
#if defined(_WIN32)
 ASSERT_TRUE(e.ok) << e.message; auto d=service.decrypt(key,e.value); ASSERT_TRUE(d.ok) << d.message; EXPECT_EQ(d.value,data);
#else
 EXPECT_FALSE(e.ok);
#endif
}
