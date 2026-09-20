#pragma once
#include "SzxNetwork/HttpClient.h"
namespace SZX::NETWORK::v1::detail
{
auto sendHttpRequest(std::string method, std::string url, HttpRequestOptions options) -> HttpClientResult;
}
