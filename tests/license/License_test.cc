#include "SzxLicense/License.h"
#include "../../components/license/src/Issuer.h"

#include "SzxCrypto/Rsa.h"

#include <gtest/gtest.h>


using namespace SZX::LICENSE;

TEST(LicenseTest, RequestIssueAndVerifyRoundTrip)
{
    RequestOptions request;
    request.productId = "szx.crypto-tool";
    request.appVersion = "1.2.3";
    request.requestedFeatures = {"crypto.symmetric", "crypto.asymmetric"};
    request.customFields["customer"] = "acme";
    auto requestJson = createRequest(request);
    ASSERT_TRUE(requestJson.ok) << requestJson.message;

    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;
    IssueOptions issueOptions;
    issueOptions.licenseId = "lic-test-001";
    issueOptions.issuer = "szx-internal";
    issueOptions.features = request.requestedFeatures;
    issueOptions.customFields["customer"] = "acme";
    auto license = issue(requestJson.value, issueOptions, keys.value.privateKey, keys.value.publicKey);
    ASSERT_TRUE(license.ok) << license.message;

    auto verified = verify(license.value, keys.value.publicKey, request.productId);
    ASSERT_TRUE(verified.ok) << verified.message;
    EXPECT_EQ(verified.value.licenseId, issueOptions.licenseId);
    EXPECT_TRUE(hasFeature(verified.value, "crypto.symmetric"));
    EXPECT_EQ(verified.value.customFields.at("customer"), "acme");
}

TEST(LicenseTest, RejectsTamperingAndWrongProduct)
{
    RequestOptions request{.productId = "product-a", .appVersion = "1.0"};
    auto requestJson = createRequest(request);
    ASSERT_TRUE(requestJson.ok);
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok);
    IssueOptions options{.licenseId = "lic-1", .issuer = "issuer"};
    auto license = issue(requestJson.value, options, keys.value.privateKey, keys.value.publicKey);
    ASSERT_TRUE(license.ok);
    auto tampered = license.value;
    tampered[20] ^= 1;
    EXPECT_FALSE(verify(tampered, keys.value.publicKey).ok);
    EXPECT_EQ(verify(license.value, keys.value.publicKey, "product-b").error, static_cast<int>(LicenseError::WrongProduct));

    auto otherKeys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(otherKeys.ok);
    auto mismatched = issue(requestJson.value, options, otherKeys.value.privateKey, keys.value.publicKey);
    EXPECT_EQ(mismatched.error, static_cast<int>(LicenseError::SignatureInvalid));
}

TEST(LicenseTest, SupportsFiniteAndPerpetualExpiry)
{
    RequestOptions request{.productId = "product-a", .appVersion = "1.0"};
    auto requestJson = createRequest(request);
    ASSERT_TRUE(requestJson.ok);
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok);

    IssueOptions perpetual{.licenseId = "perpetual", .issuer = "issuer"};
    auto perpetualLicense = issue(requestJson.value, perpetual, keys.value.privateKey, keys.value.publicKey);
    ASSERT_TRUE(perpetualLicense.ok);
    EXPECT_TRUE(verify(perpetualLicense.value, keys.value.publicKey).ok);

    IssueOptions expired{.licenseId = "expired", .issuer = "issuer", .notBeforeUtc = 1, .expiresAtUtc = 2};
    auto expiredLicense = issue(requestJson.value, expired, keys.value.privateKey, keys.value.publicKey);
    ASSERT_TRUE(expiredLicense.ok);
    EXPECT_EQ(verify(expiredLicense.value, keys.value.publicKey).error, static_cast<int>(LicenseError::Expired));
}

TEST(LicenseTest, RejectsInvalidRequestVersionAndExpiryRange)
{
    RequestOptions request{.productId = "product-a", .appVersion = "1.0"};
    auto requestJson = createRequest(request);
    ASSERT_TRUE(requestJson.ok);
    auto unsupported = requestJson.value;
    const auto versionPos = unsupported.find("\"schema_version\":1");
    ASSERT_NE(versionPos, std::string::npos);
    unsupported[versionPos + 17] = '2';
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok);
    IssueOptions options{.licenseId = "bad-range", .issuer = "issuer", .notBeforeUtc = 100, .expiresAtUtc = 100};
    EXPECT_EQ(issue(requestJson.value, options, keys.value.privateKey, keys.value.publicKey).error,
              static_cast<int>(LicenseError::InvalidArgument));
    EXPECT_EQ(issue(unsupported, IssueOptions{.licenseId = "bad-version", .issuer = "issuer"}, keys.value.privateKey,
                    keys.value.publicKey)
                  .error,
              static_cast<int>(LicenseError::Unsupported));
}
