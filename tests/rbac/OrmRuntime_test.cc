#include "../../RBAC/szx_admin/src/infra/OrmRuntime.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace SZX::ADMIN::INFRA::DETAIL;

TEST(OrmRuntimeTest, SuppressesOnlyScopedThreadAndRestoresAfterException)
{
    testing::internal::CaptureStdout();
    try
    {
        const OrmDiagnosticsScope diagnostics;
        std::cout << "secret-driver-error" << std::endl;
        {
            const OrmDiagnosticsScope nested;
            std::cout << "nested-secret" << std::endl;
        }
        std::cout << "still-secret" << std::endl;
        std::jthread writer([] { std::cout << "other-thread-visible" << std::endl; });
        writer.join();
        throw std::runtime_error("unwind");
    } catch (const std::runtime_error &)
    {
    }
    std::cout << "business-output-visible" << std::endl;
    const auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output.find("secret"), std::string::npos);
    EXPECT_NE(output.find("other-thread-visible"), std::string::npos);
    EXPECT_NE(output.find("business-output-visible"), std::string::npos);
    EXPECT_TRUE(std::cout.good());
}

TEST(OrmRuntimeTest, GateExcludesOtherThreadsAndReleasesOnUnwind)
{
    std::atomic_bool entered{};
    try
    {
        std::lock_guard transaction(ormMutex());
        // The same thread's ORM calls must be able to enter its transaction gate.
        std::lock_guard operation(ormMutex());
        std::jthread contender(
            [&]
            {
                std::unique_lock lock(ormMutex(), std::defer_lock);
                entered = lock.try_lock_for(std::chrono::milliseconds(20));
            });
        contender.join();
        EXPECT_FALSE(entered);
        throw std::runtime_error("unwind");
    } catch (const std::runtime_error &)
    {
    }
    std::jthread successor(
        [&]
        {
            std::unique_lock lock(ormMutex(), std::defer_lock);
            entered = lock.try_lock_for(std::chrono::seconds(1));
        });
    successor.join();
    EXPECT_TRUE(entered);
}
