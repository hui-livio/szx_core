#include <szx/dmc/DmcController.hpp>

#include "SimulatedDmcController.hpp"
#if defined(SZX_DMC_HAS_LTDMC)
    #include "LTDMCE3032Controller.hpp"
#endif

#include <exception>
#include <memory>
#include <new>
#include <utility>

namespace SZX::DMC::v1
{

auto DmcController::create(DmcConfig config) -> SZX::COMMON::Result<std::unique_ptr<DmcController>>
{
    return DmcControllerFactory::create(std::move(config));
}

auto DmcControllerFactory::create(DmcConfig config) -> SZX::COMMON::Result<std::unique_ptr<DmcController>>
{
    try
    {
        if (config.compareQueueCapacity == 0 || config.inputCount > 64 || config.outputCount > 64)
            return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Failure(static_cast<int>(DmcError::InvalidArgument),
                                                                                "Invalid DMC configuration.");
        if (config.profileId.empty())
            return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Failure(static_cast<int>(DmcError::InvalidArgument),
                                                                                "DMC profileId is required.");
        const auto validButton = [&config](const std::optional<DmcButtonInput> &button)
        {
            return !button || (button->input < config.inputCount && button->input < 64);
        };
        if (!validButton(config.startButton) || !validButton(config.resetButton) || !validButton(config.emergencyStopButton))
            return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Failure(
                static_cast<int>(DmcError::InvalidArgument), "DMC button input is outside the configured IO range.");
        if ((config.startButton && config.resetButton && config.startButton->input == config.resetButton->input) ||
            (config.startButton && config.emergencyStopButton &&
             config.startButton->input == config.emergencyStopButton->input) ||
            (config.resetButton && config.emergencyStopButton && config.resetButton->input == config.emergencyStopButton->input))
            return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Failure(static_cast<int>(DmcError::InvalidArgument),
                                                                                "DMC button inputs must be distinct.");
        switch (config.model)
        {
        case DmcModel::LeiSaiLTDMC:
            if (config.profileId != "E3032")
                return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Failure(static_cast<int>(DmcError::InvalidArgument),
                                                                                    "LeiSai DMC profileId is not registered.");
            if (config.simulation)
                return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Success(createSimulatedController(std::move(config)));
#if defined(SZX_DMC_HAS_LTDMC)
            return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Success(
                std::make_unique<LTDMCE3032Controller>(std::move(config)));
#else
            if (config.allowSimulationFallback)
            {
                config.simulation = true;
                return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Success(createSimulatedController(std::move(config)));
            }
            return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Failure(static_cast<int>(DmcError::DriverUnavailable),
                                                                                "The LTDMC hardware adapter is unavailable.");
#endif
        case DmcModel::Custom:
            return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Failure(
                static_cast<int>(DmcError::InvalidArgument), "The custom DMC implementation is not registered.");
        default:
            return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Failure(static_cast<int>(DmcError::InvalidArgument),
                                                                                "The DMC model is not registered.");
        }
    } catch (const std::bad_alloc &)
    {
        return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Failure(static_cast<int>(DmcError::ResourceExhausted),
                                                                            "DMC creation exhausted memory.");
    } catch (const std::exception &error)
    {
        return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Failure(static_cast<int>(DmcError::UnexpectedFailure),
                                                                            error.what());
    } catch (...)
    {
        return SZX::COMMON::Result<std::unique_ptr<DmcController>>::Failure(static_cast<int>(DmcError::UnexpectedFailure),
                                                                            "DMC creation failed.");
    }
}

} // namespace SZX::DMC::v1
