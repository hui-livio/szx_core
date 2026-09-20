#pragma once

#include "JsonHelper.hpp"

namespace SZX::ADMIN::USER
{
/** Apply the same recursive redaction on ingestion and on reads of historical rows. */
inline void redactAuditDetails(SZX::COMMON::Json &value, unsigned depth = 0)
{
    if (depth >= 32)
    {
        value = nullptr;
        return;
    }
    if (value.is_object())
    {
        for (auto it = value.begin(); it != value.end();)
        {
            std::string key;
            for (const unsigned char c : it.key())
            {
                if (c >= 'A' && c <= 'Z')
                    key.push_back(static_cast<char>(c + ('a' - 'A')));
                else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
                    key.push_back(static_cast<char>(c));
            }
            const bool sensitive = key.find("password") != key.npos || key.find("token") != key.npos ||
                key.find("secret") != key.npos || key.find("privatekey") != key.npos || key.find("apikey") != key.npos ||
                key.find("machineid") != key.npos || key.find("hardwareid") != key.npos || key.find("fingerprint") != key.npos ||
                key.find("stack") != key.npos || key.find("traceback") != key.npos || key.find("credential") != key.npos ||
                key == "authorization" || key == "cookie" || key == "setcookie";
            if (sensitive)
                it = value.erase(it);
            else
            {
                redactAuditDetails(it.value(), depth + 1);
                ++it;
            }
        }
    }
    else if (value.is_array())
    {
        for (auto &item : value)
            redactAuditDetails(item, depth + 1);
    }
}
} // namespace SZX::ADMIN::USER
