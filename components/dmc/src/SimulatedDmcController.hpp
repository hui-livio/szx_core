#pragma once

#include <szx/dmc/DmcController.hpp>

namespace SZX::DMC::v1
{

/** @brief Internal simulation controller creation entry point. */
auto createSimulatedController(DmcConfig config) -> std::unique_ptr<DmcController>;

} // namespace SZX::DMC::v1
