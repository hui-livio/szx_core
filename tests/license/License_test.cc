#include "SzxLicense/License.h"
#include "SzxLicense/Issuer.h"

#include "SzxCrypto/Rsa.h"
#include "nlohmann/json.hpp"
#include "FileHelper.hpp"

#include <filesystem>
#include <gtest/gtest.h>


using namespace SZX::LICENSE;

TEST(LicenseTest, CollectsProductScopedMachineFactors)
{
    auto factors = collectMachineFactors("product-a");
    ASSERT_TRUE(factors.ok) << factors.message;
    for (const auto &factor : factors.value)
    {
        EXPECT_FALSE(factor.type.empty());
        EXPECT_EQ(factor.hash.size(), 64U);
    }

    auto request = createRequest(RequestOptions{.productId = "product-a", .appVersion = "1.0"});
    ASSERT_TRUE(request.ok) << request.message;
    EXPECT_NE(request.value.find("machine_factors"), std::string::npos);
}

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

TEST(LicenseTest, RejectsLicenseForDifferentMachineFactors)
{
    RequestOptions request{.productId = "product-a", .appVersion = "1.0"};
    auto requestJson = createRequest(request);
    ASSERT_TRUE(requestJson.ok);
    std::size_t hashPos = 0;
    std::size_t hashCount = 0;
    while ((hashPos = requestJson.value.find("\"hash\":\"", hashPos)) != std::string::npos)
    {
        requestJson.value.replace(hashPos + 8, 64, 64, '0');
        hashPos += 8 + 64;
        ++hashCount;
    }
    ASSERT_GT(hashCount, 0U);
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok);
    IssueOptions options{.licenseId = "machine-bound", .issuer = "issuer"};
    auto license = issue(requestJson.value, options, keys.value.privateKey, keys.value.publicKey);
    ASSERT_TRUE(license.ok);
    EXPECT_EQ(verify(license.value, keys.value.publicKey).error, static_cast<int>(LicenseError::MachineMismatch));
}

TEST(LicenseTest, RejectsIssueRequestWithoutMachineFactors)
{
    RequestOptions request{.productId = "product-a", .appVersion = "1.0"};
    auto requestJson = createRequest(request);
    ASSERT_TRUE(requestJson.ok);
    auto json = nlohmann::json::parse(requestJson.value);
    json.erase("machine_factors");
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok);
    IssueOptions options{.licenseId = "unbound", .issuer = "issuer"};
    auto license = issue(json.dump(), options, keys.value.privateKey, keys.value.publicKey);
    EXPECT_EQ(license.error, static_cast<int>(LicenseError::InvalidArgument));
}

TEST(LicenseTest, RequestFileIsEncryptedAndSupportsPathBasedFlow)
{
    const auto requestPath = std::filesystem::temp_directory_path() / "szx-license-test.szxreq";
    const auto licensePath = std::filesystem::temp_directory_path() / "szx-license-test.szxlic";
    std::filesystem::remove(requestPath);
    std::filesystem::remove(requestPath.string() + ".tmp");
    std::filesystem::remove(licensePath);

    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok);
    RequestOptions request{.productId = "product-a", .appVersion = "1.0"};
    auto created = createRequestFile(request, requestPath.string(), keys.value.publicKey);
    ASSERT_TRUE(created.ok) << created.message;
    auto encrypted = SZX::COMMON::FileHelper::ReadBytes(requestPath);
    ASSERT_TRUE(encrypted);
    EXPECT_EQ(std::search(encrypted->begin(), encrypted->end(), request.productId.begin(), request.productId.end()), encrypted->end());

    auto tampered = *encrypted;
    tampered.back() ^= 1;
    ASSERT_TRUE(SZX::COMMON::FileHelper::WriteBytes(requestPath, tampered, false));
    IssueOptions options{.licenseId = "file-flow", .issuer = "issuer"};
    EXPECT_FALSE(issueFile(requestPath.string(), options, keys.value.privateKey, keys.value.publicKey).ok);

    std::filesystem::remove(requestPath);
    ASSERT_TRUE(createRequestFile(request, requestPath.string(), keys.value.publicKey).ok);
    auto license = issueFile(requestPath.string(), options, keys.value.privateKey, keys.value.publicKey);
    ASSERT_TRUE(license.ok) << license.message;
    ASSERT_TRUE(SZX::COMMON::FileHelper::WriteBytes(licensePath, license.value, false));
    auto verified = verifyFile(licensePath.string(), keys.value.publicKey, request.productId);
    EXPECT_TRUE(verified.ok) << verified.message;

    std::filesystem::remove(requestPath);
    std::filesystem::remove(licensePath);
}
