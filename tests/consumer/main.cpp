#include <SzxCrypto/Base64.h>
#include <szx/camera/CameraTypes.h>
#include <szx/logger/Logger.h>

int main()
{
    SZX::LOGGER::set_level(SZX::LOGGER::LogLevel::Off);
    return SZX::CRYPTO::encodeBase64("consumer") == "Y29uc3VtZXI=" ? 0 : 1;
}
