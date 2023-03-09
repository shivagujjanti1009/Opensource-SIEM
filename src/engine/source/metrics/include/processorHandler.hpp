#ifndef _PROCESSOR_HANDLER_HPP
#define _PROCESSOR_HANDLER_HPP

#include "chainOfResponsibility.hpp"
#include "metricsContext.hpp"

class ProcessorHandler final : public AbstractHandler<std::shared_ptr<MetricsContext>>
{
public:
    /**
     * @brief Trigger for the processor process.
     *
     * @param data Context of metrics.
     * @return std::shared_ptr<MetricsContext>
     */
    virtual std::shared_ptr<MetricsContext> handleRequest(std::shared_ptr<MetricsContext> data);

private:
    /**
     * @brief Create the processor instance.
     *
     * @param data Context of metrics.
     */
    void create(std::shared_ptr<MetricsContext> data);
};

#endif // _PROCESSOR_HANDLER_HPP
