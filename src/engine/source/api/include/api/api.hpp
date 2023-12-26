#ifndef _API_API_HPP
#define _API_API_HPP

#include "registry.hpp"

#include <json/json.hpp>
#include <rbac/irbac.hpp>

namespace api
{
/**
 * @brief Class that handles all endpoints and exposes Server functionality.
 *
 * @note This class is thread-safe but the handlers must be thread-safe.

 */
class Api
{

private:
    std::shared_ptr<api::Registry> m_registry; ///< Registry of handlers
    std::weak_ptr<rbac::IRBAC> m_rbac;         ///< RBAC instance

public:
    /**
     * @brief Construct a new Api object
     *
     */
    Api()
        : m_registry(std::make_shared<api::Registry>())
        , m_rbac(std::weak_ptr<rbac::IRBAC>()) {};

    /**
     * @brief Construct a new Api object
     *
     * @param rbac RBAC instance to use in the API
     */
    Api(std::weak_ptr<rbac::IRBAC> rbac)
        : m_registry(std::make_shared<api::Registry>())
        , m_rbac(rbac) {};
    /**
     * @brief Register a handler for a command
     *
     * @param command Command to register
     * @param handler Handler to register
     * @return true if the handler was registered
     * @return false if the handler was not registered (command already registered)
     */
    bool registerHandler(const std::string& command, const HandlerAsync& handler)
    {
        return m_registry->registerHandler(command, handler);
    }

    /**
     * @brief Converts a synchronous handler to an asynchronous handler.
     *
     * This static method takes a synchronous handler and converts it to an asynchronous handler.
     *
     * @param handlerSync Synchronous handler to convert
     * @return Handler Asynchronous handler
     */
    static HandlerAsync convertToHandlerAsync(const HandlerSync& handlerSync)
    {
        return [=](const wpRequest& request, std::function<void(const wpResponse&)> callback)
        {
            auto response = handlerSync(request);
            callback(response);
        };
    }

    /**
     * @brief Processes a request and invokes a callback function with the response.
     *
     * This method takes a JSON-formatted request message, processes it, and generates
     * a response. The response is then passed to the provided callback function.
     *
     * @param message The JSON-formatted request message.
     * @param callbackFn A callback function that will be invoked with the generated response.
     *
     */
    void processRequest(const std::string& message, std::function<void(const wpResponse&)> callbackFn)
    {
        wpResponse wresponse {};
        json::Json jrequest {};

        try
        {
            jrequest = json::Json {message.c_str()};
        }
        catch (const std::exception& e)
        {
            wresponse = base::utils::wazuhProtocol::WazuhResponse::invalidJsonRequest();
            callbackFn(wresponse);
            return;
        }

        try
        {
            wpRequest wrequest {jrequest};
            if (wrequest.isValid())
            {
                m_registry->getHandler(wrequest.getCommand().value())(wrequest, callbackFn);
            }
            else
            {
                wresponse = base::utils::wazuhProtocol::WazuhResponse::invalidRequest(wrequest.error().value());
                callbackFn(wresponse);
            }
        }
        catch (const std::exception& e)
        {
            LOG_DEBUG("Exception in Api::processRequest: %s", e.what());
            wresponse = base::utils::wazuhProtocol::WazuhResponse::unknownError();
            callbackFn(wresponse);
        }
    }

    /**
     * @brief Get the RBAC instance
     *
     * @return std::weak_ptr<rbac::IRBAC> RBAC instance
     */
    std::weak_ptr<rbac::IRBAC> getRBAC() const { return m_rbac; }
};
} // namespace api

#endif // _API_API_HPP
