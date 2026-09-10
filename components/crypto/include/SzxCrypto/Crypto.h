/**
 * @file Crypto.h
 * @brief SZX 加密公共接口的便捷聚合头文件。
 *
 * 包含 Base64、Hash/HMAC、Fernet、随机数、安全字节容器和 RSA 接口。
 */

#pragma once

#include "SzxCrypto/Aead.h"
#include "SzxCrypto/Argon2.h"
#include "SzxCrypto/Base64.h"
#include "SzxCrypto/Fernet.h"
#include "SzxCrypto/Hash.h"
#include "SzxCrypto/Kdf.h"
#include "SzxCrypto/Random.h"
#include "SzxCrypto/Rsa.h"
#include "SzxCrypto/SecureBytes.h"
