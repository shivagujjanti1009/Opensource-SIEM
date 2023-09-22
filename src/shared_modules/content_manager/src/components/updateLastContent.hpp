/*
 * Wazuh content manager
 * Copyright (C) 2015, Wazuh Inc.
 * May 02, 2023.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#ifndef _UPDATE_LAST_CONTENT_HPP
#define _UPDATE_LAST_CONTENT_HPP

#include "updaterContext.hpp"
#include "utils/chainOfResponsability.hpp"
#include "utils/timeHelper.h"
#include <memory>

/**
 * @class UpdateLastContent
 *
 * @brief Class in charge of updating the content version as a step of a chain of responsibility.
 *
 */
class UpdateLastContent final : public AbstractHandler<std::shared_ptr<UpdaterContext>>
{
private:
    /**
     * @brief Update the content version.
     *
     * @param context updater context.
     */
    void update(const UpdaterContext& context) const
    {
        try
        {
            context.spUpdaterBaseContext->spRocksDB->put(Utils::getCompactTimestamp(std::time(nullptr)),
                                                         std::to_string(context.currentOffset));
        }
        catch (const std::exception& e)
        {
            std::ostringstream errorMsg;
            errorMsg << "UpdateLastContent - Error updating the content version: " << e.what();
            throw std::runtime_error(errorMsg.str());
        }
    }

public:
    /**
     * @brief Update the content version.
     *
     * @param context updater context.
     * @return std::shared_ptr<UpdaterContext>
     */
    std::shared_ptr<UpdaterContext> handleRequest(std::shared_ptr<UpdaterContext> context) override
    {

        update(*context);

        return AbstractHandler<std::shared_ptr<UpdaterContext>>::handleRequest(context);
    }
};

#endif // _UPDATE_LAST_CONTENT_HPP
