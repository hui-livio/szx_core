#include <SzxCrypto/Base64.h>
#include <Result.hpp>

int main()
{
    const auto encoded = SZX::CRYPTO::encodeBase64("szx");
    return encoded == "c3p4" ? 0 : 1;
}

