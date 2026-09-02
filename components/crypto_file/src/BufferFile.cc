#include "SzxCryptoFile/BufferFile.h"
#include "SzxCryptoService/CryptoService.h"
#include <array>
#include <cstring>
#include <utility>
namespace SZX::CRYPTO_FILE {
namespace {
constexpr std::size_t kNonceSize = 12;
constexpr std::size_t kTagSize = 16;
constexpr std::size_t kHeaderSize = 24;
constexpr std::uint32_t kMaxPayloadSize = 0xffffffffU;
constexpr std::array<std::uint8_t, 5> kAad{'S','Z','X','E',1};
constexpr int kInvalidContainer = 1900;
auto fail(const char* m) -> SZX::COMMON::Result<std::vector<std::uint8_t>> { return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(kInvalidContainer,m); }
auto validHeader(std::span<const std::uint8_t> c) -> bool
{
    return c.size() >= kHeaderSize + kTagSize && std::memcmp(c.data(), "SZXE", 4) == 0 && c[4] == 1 &&
           c[5] == 0 && c[6] == 0 && c[7] == 0;
}
auto readLength(std::span<const std::uint8_t> c) -> std::uint32_t { return static_cast<std::uint32_t>(c[20]) | (static_cast<std::uint32_t>(c[21])<<8) | (static_cast<std::uint32_t>(c[22])<<16) | (static_cast<std::uint32_t>(c[23])<<24); }
auto validLength(std::span<const std::uint8_t> c, std::uint32_t n) -> bool
{
    return n <= c.size() - kHeaderSize - kTagSize && c.size() == kHeaderSize + static_cast<std::size_t>(n) + kTagSize;
}
}
auto encryptBuffer(SZX::CRYPTO_SERVICE::ICryptoService &service,std::span<const std::uint8_t> key,std::span<const std::uint8_t> plaintext)->SZX::COMMON::Result<std::vector<std::uint8_t>> { if(plaintext.size()>kMaxPayloadSize)return fail("SZXE 明文超过单块容器限制。"); auto e=service.encrypt(key,plaintext,kAad); if(!e.ok)return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(e.error,e.message); std::vector<std::uint8_t> out{'S','Z','X','E',1,0,0,0}; out.insert(out.end(),e.value.nonce.begin(),e.value.nonce.end()); std::uint32_t n=static_cast<std::uint32_t>(e.value.ciphertext.size()); for(int i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>((n>>(8*i))&255)); out.insert(out.end(),e.value.ciphertext.begin(),e.value.ciphertext.end()); out.insert(out.end(),e.value.tag.begin(),e.value.tag.end()); return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(out)); }
auto encryptBuffer(std::span<const std::uint8_t> key,std::span<const std::uint8_t> plaintext)->SZX::COMMON::Result<std::vector<std::uint8_t>> { SZX::CRYPTO_SERVICE::CryptoService service; return encryptBuffer(service,key,plaintext); }
auto decryptBuffer(SZX::CRYPTO_SERVICE::ICryptoService &service,std::span<const std::uint8_t> key,std::span<const std::uint8_t> c)->SZX::COMMON::Result<std::vector<std::uint8_t>> { if(!validHeader(c))return fail("SZXE 容器头无效。"); auto n=readLength(c); if(!validLength(c,n))return fail("SZXE 容器长度无效。"); SZX::CRYPTO_SERVICE::CryptoCiphertext cipher{std::vector<std::uint8_t>(c.begin()+8,c.begin()+8+kNonceSize),std::vector<std::uint8_t>(c.begin()+kHeaderSize,c.begin()+kHeaderSize+n),std::vector<std::uint8_t>(c.begin()+kHeaderSize+n,c.end())}; return service.decrypt(key,cipher,kAad); }
auto decryptBuffer(std::span<const std::uint8_t> key,std::span<const std::uint8_t> c)->SZX::COMMON::Result<std::vector<std::uint8_t>> { SZX::CRYPTO_SERVICE::CryptoService service; return decryptBuffer(service,key,c); }
auto verifyBuffer(SZX::CRYPTO_SERVICE::ICryptoService &service,std::span<const std::uint8_t> key,std::span<const std::uint8_t> container)->SZX::COMMON::Result<void> { auto r=decryptBuffer(service,key,container); return r.ok?SZX::COMMON::Result<void>::Success():SZX::COMMON::Result<void>::Failure(r.error,r.message); }
auto verifyBuffer(std::span<const std::uint8_t> key,std::span<const std::uint8_t> container)->SZX::COMMON::Result<void> { SZX::CRYPTO_SERVICE::CryptoService service; return verifyBuffer(service,key,container); }
auto inspectBuffer(std::span<const std::uint8_t> c)->SZX::COMMON::Result<ContainerInfo> { if(!validHeader(c))return SZX::COMMON::Result<ContainerInfo>::Failure(kInvalidContainer,"SZXE 容器头无效。"); auto n=readLength(c); if(!validLength(c,n))return SZX::COMMON::Result<ContainerInfo>::Failure(kInvalidContainer,"SZXE 容器长度无效。"); return SZX::COMMON::Result<ContainerInfo>::Success({1,n}); }
}
