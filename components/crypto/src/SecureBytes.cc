#include "SzxCrypto/SecureBytes.h"

#include <openssl/crypto.h>
#include <utility>

namespace SZX::CRYPTO
{

struct SecureBytes::Impl
{
    std::vector<std::uint8_t> bytes;
};

SecureBytes::SecureBytes() = default;

SecureBytes::SecureBytes(std::vector<std::uint8_t> bytes) :
    impl_(std::make_unique<Impl>(Impl{std::move(bytes)}))
{
}

SecureBytes::SecureBytes(SecureBytes &&other) noexcept :
    impl_(std::move(other.impl_))
{
}

auto SecureBytes::operator=(SecureBytes &&other) noexcept -> SecureBytes &
{
    if (this != &other)
    {
        clear();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

SecureBytes::~SecureBytes() noexcept
{
    clear();
}

auto SecureBytes::size() const noexcept -> std::size_t
{
    return impl_ ? impl_->bytes.size() : 0;
}
auto SecureBytes::empty() const noexcept -> bool
{
    return size() == 0;
}
auto SecureBytes::data() const noexcept -> const std::uint8_t *
{
    return impl_ ? impl_->bytes.data() : nullptr;
}
auto SecureBytes::span() const noexcept -> std::span<const std::uint8_t>
{
    return {data(), size()};
}

auto SecureBytes::take() && noexcept -> std::vector<std::uint8_t>
{
    if (!impl_)
        return {};
    return std::move(impl_->bytes);
}

void SecureBytes::clear() noexcept
{
    if (!impl_)
        return;
    if (!impl_->bytes.empty())
        OPENSSL_cleanse(impl_->bytes.data(), impl_->bytes.size());
    impl_->bytes.clear();
}

} // namespace SZX::CRYPTO
