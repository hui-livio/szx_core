#include <gtest/gtest.h>

#include "FeatureManager.h"

#include <memory>
#include <stdexcept>

namespace
{
class ThrowingProvider final : public SZX::FEATURE::IFeatureProvider
{
public:
    SZX::FEATURE::FeatureLoadResult load() override
    {
        throw std::runtime_error("load failed");
    }
    SZX::FEATURE::FeaturePersistResult save(const SZX::FEATURE::FeatureMap &) override
    {
        return SZX::FEATURE::FeaturePersistResult::Success({});
    }
};
} // namespace

TEST(FeatureManagerTest, SetValueAndSnapshotAdvanceRevision)
{
    SZX::FEATURE::FeatureManager manager;
    EXPECT_TRUE(manager.setEnabled("inspection.enabled", true).ok);
    const auto snapshot = manager.snapshot();
    EXPECT_EQ(snapshot.revision, 1U);
    EXPECT_TRUE(manager.isEnabled("inspection.enabled"));
}

TEST(FeatureManagerTest, ProviderExceptionsAreContained)
{
    SZX::FEATURE::FeatureManager manager;
    const auto result = manager.setProvider(std::make_unique<ThrowingProvider>());
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, 1);
}
