#include "HTTPRequest.hpp"
#include "UNIXSocketRequest.hpp"
#include "contentManager.hpp"
#include "contentRegister.hpp"
#include "defs.h"
#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <thread>

/*
 * @brief Configuration parameters for the content provider.
 *
 * @topicName: Name of the topic.
 * @interval: Interval in seconds to execute the content provider.
 * @ondemand: If true, the content provider will be executed on demand.
 * @configData: Configuration data to create the orchestration of the content provider.
 * @contentSource: Source of the content.
 * @compressionType: Compression type of the content.
 * @versionedContent: Type of versioned content. If false, the content must not be versioned.
 * @deleteDownloadedContent: If true, the downloaded content will be deleted.
 * @url: URL where the content is located.
 * @outputFolder: if defined, the content will be downloaded to this folder.
 * @dataFormat: Format of the content downloaded or after decompression.
 * @contentFileName: Name for the downloaded file (unless using the 'offline' or 'file' contentSource).
 * @offset (integer): Api offset used to override (if greater) the one set on the database.
 */
static const nlohmann::json CONFIG_PARAMETERS =
    R"(
        {
            "topicName": "test",
            "interval": 10,
            "ondemand": true,
            "configData":
            {
                "contentSource": "api",
                "compressionType": "raw",
                "versionedContent": "false",
                "deleteDownloadedContent": true,
                "url": "https://jsonplaceholder.typicode.com/todos/1",
                "outputFolder": "/tmp/testProvider",
                "dataFormat": "json",
                "contentFileName": "example.json",
                "databasePath": "/tmp/content_updater/rocksdb",
                "offset": 0
            }
        }
        )"_json;

// Enable/Disable logging verbosity.
static const auto VERBOSE {true};

// Enable/Disable the offset update process execution.
static const auto OFFSET_UPDATE {false};
static const auto OFFSET_UPDATE_VALUE {100000};

/**
 * @brief Log function callback used on the Content Manager test tool.
 *
 * @param logLevel Log level.
 * @param tag Log tag.
 * @param file File from where the logger is called.
 * @param line Line from where the logger is called.
 * @param func Function from where the logger is called.
 * @param message Message to log.
 */
void logFunction(const int logLevel,
                 const std::string& tag,
                 const std::string& file,
                 const int line,
                 const std::string& func,
                 const std::string& message)
{
    auto pos {file.find_last_of('/')};
    if (pos != std::string::npos)
    {
        pos++;
    }
    const auto fileName {file.substr(pos, file.size() - pos)};

    // Set log level tag.
    static const std::map<int, std::string> LOG_LEVEL_TAGS {{LOGLEVEL_DEBUG_VERBOSE, "DEBUG_VERBOSE"},
                                                            {LOGLEVEL_DEBUG, "DEBUG"},
                                                            {LOGLEVEL_INFO, "INFO"},
                                                            {LOGLEVEL_WARNING, "WARNING"},
                                                            {LOGLEVEL_ERROR, "ERROR"},
                                                            {LOGLEVEL_CRITICAL, "CRITICAL"}};
    const auto levelTag {"[" + LOG_LEVEL_TAGS.at(logLevel) + "]"};

    if (logLevel == LOGLEVEL_ERROR || logLevel == LOGLEVEL_CRITICAL)
    {
        // Error logs.
        std::cerr << tag << ":" << levelTag << ": " << message.c_str() << std::endl;
    }
    else if (logLevel == LOGLEVEL_INFO || logLevel == LOGLEVEL_WARNING)
    {
        // Info and warning logs.
        std::cout << tag << ":" << levelTag << ": " << message.c_str() << std::endl;
    }
    else
    {
        // Debug logs.
        if (VERBOSE)
        {
            std::cout << tag << ":" << levelTag << ":" << fileName << ":" << line << " " << func << ": "
                      << message.c_str() << std::endl;
        }
    }
}

/**
 * @brief Performs a PUT query to the on-demand manager, requesting an offset update.
 *
 * @param topicName Name of the topic.
 */
void runOffsetUpdate(const std::string& topicName)
{
    nlohmann::json data;
    data["offset"] = OFFSET_UPDATE_VALUE;
    data["topicName"] = topicName;
    const auto putUrl {"http://localhost/offset"};
    UNIXSocketRequest::instance().put(
        HttpUnixSocketURL(ONDEMAND_SOCK, putUrl),
        data,
        [](const std::string& msg) { std::cout << msg << std::endl; },
        [](const std::string& msg, const long responseCode) { std::cout << msg << ": " << responseCode << std::endl; });
}

int main()
{
    auto& instance = ContentModule::instance();
    const std::string topic_name = CONFIG_PARAMETERS.at("topicName").get<std::string>();

    // Server
    instance.start(logFunction);

    // Client -> Vulnerability detector
    ContentRegister registerer {topic_name, CONFIG_PARAMETERS};
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Run offset update if specified.
    if (OFFSET_UPDATE)
    {
        runOffsetUpdate(topic_name);
    }

    // OnDemand request
    std::string url = "http://localhost/ondemand/" + topic_name + "?offset=-1";
    UNIXSocketRequest::instance().get(
        HttpUnixSocketURL(ONDEMAND_SOCK, url),
        [](const std::string& msg) { std::cout << msg << std::endl; },
        [](const std::string& msg, const long responseCode) { std::cout << msg << ": " << responseCode << std::endl; });

    std::this_thread::sleep_for(std::chrono::seconds(60));

    // Stop server
    instance.stop();

    return 0;
}
