/* Copyright (C) 2015-2022, Wazuh Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */
#include "opBuilderSCAdecoder.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <tuple>
#include <variant>

#include "syntax.hpp"

#include "baseTypes.hpp"
#include <baseHelper.hpp>
#include <utils/socketInterface/unixDatagram.hpp>
#include <utils/stringUtils.hpp>
#include <wdb/wdb.hpp>

namespace builder::internals::builders
{

namespace sca
{
constexpr auto TYPE_CHECK = "check";
constexpr auto TYPE_SUMMARY = "summary";
constexpr auto TYPE_POLICIES = "policies";
constexpr auto TYPE_DUMP_END = "dump_end";

// SCA event json fields
namespace field
{

enum class Name
{
    CHECK_COMMAND,         ///< checkEvent
    CHECK_COMPLIANCE,      ///< checkEvent
    CHECK_CONDITION,       ///< checkEvent
    CHECK_DESCRIPTION,     ///< checkEvent
    CHECK_DIRECTORY,       ///< checkEvent
    CHECK_FILE,            ///< checkEvent
    CHECK_ID,              ///< checkEvent
    CHECK_PREVIOUS_RESULT, ///< checkEvent
    CHECK_PROCESS,         ///< checkEvent
    CHECK_RATIONALE,       ///< checkEvent
    CHECK_REASON,          ///< checkEvent
    CHECK_REFERENCES,      ///< checkEvent
    CHECK_REGISTRY,        ///< checkEvent
    CHECK_REMEDIATION,     ///< checkEvent
    CHECK_RESULT,          ///< checkEvent
    CHECK_RULES,           ///< checkEvent
    CHECK_STATUS,          ///< checkEvent
    CHECK_TITLE,           ///< checkEvent
    CHECK,                 ///< checkEvent
    DESCRIPTION,           ///< scaninfo
    END_TIME,              ///< scaninfo
    ELEMENTS_SENT,         ///< DumpEvent
    FAILED,                ///< scaninfo
    FILE,                  ///< scaninfo
    FIRST_SCAN,            ///< scaninfo
    FORCE_ALERT,           ///< scaninfo
    HASH_FILE,             ///< scaninfo
    HASH,                  ///< scaninfo
    ID,                    ///< checkEvent
    INVALID,               ///< scaninfo
    NAME,                  ///< scaninfo
    PASSED,                ///< scaninfo
    POLICY_ID,             ///< scaninfo, checkEvent
    POLICY,                ///< checkEvent
    POLICIES,              ///< Policies
    REFERENCES,            ///< scaninfo
    SCAN_ID,               ///< scaninfo
    SCORE,                 ///< scaninfo
    START_TIME,            ///< scaninfo
    TOTAL_CHECKS,          ///< scaninfo
    TYPE,                  ///< checkEvent
};

enum class Type
{
    STRING,
    INT,
    BOOL,
    ARRAY,
    OBJECT
};

const std::string getRawPath(Name field)
{
    switch (field)
    {
        case Name::ID: return "/id";
        case Name::SCAN_ID: return "/scan_id";
        case Name::DESCRIPTION: return "/description";
        case Name::REFERENCES: return "/references";
        case Name::START_TIME: return "/start_time";
        case Name::END_TIME: return "/end_time";
        case Name::PASSED: return "/passed";
        case Name::FAILED: return "/failed";
        case Name::INVALID: return "/invalid";
        case Name::TOTAL_CHECKS: return "/total_checks";
        case Name::SCORE: return "/score";
        case Name::HASH: return "/hash";
        case Name::HASH_FILE: return "/hash_file";
        case Name::FILE: return "/file";
        case Name::NAME: return "/name";
        case Name::FIRST_SCAN: return "/first_scan";
        case Name::FORCE_ALERT: return "/force_alert";
        case Name::POLICY: return "/policy";
        case Name::POLICY_ID: return "/policy_id";
        case Name::POLICIES: return "/policies";
        case Name::CHECK: return "/check";
        case Name::CHECK_ID: return "/check/id";
        case Name::CHECK_TITLE: return "/check/title";
        case Name::CHECK_DESCRIPTION: return "/check/description";
        case Name::CHECK_RATIONALE: return "/check/rationale";
        case Name::CHECK_REMEDIATION: return "/check/remediation";
        case Name::CHECK_REFERENCES: return "/check/references";
        case Name::CHECK_COMPLIANCE: return "/check/compliance";
        case Name::CHECK_CONDITION: return "/check/condition";
        case Name::CHECK_DIRECTORY: return "/check/directory";
        case Name::CHECK_PROCESS: return "/check/process";
        case Name::CHECK_REGISTRY: return "/check/registry";
        case Name::CHECK_COMMAND: return "/check/command";
        case Name::CHECK_RULES: return "/check/rules";
        case Name::CHECK_STATUS: return "/check/status";
        case Name::CHECK_REASON: return "/check/reason";
        case Name::CHECK_RESULT: return "/check/result";
        case Name::CHECK_FILE: return "/check/file";
        case Name::ELEMENTS_SENT: return "/elements_sent";
        case Name::TYPE: return "/type";
        case Name::CHECK_PREVIOUS_RESULT: return "/check/previous_result";
        default: return "";
    }
}

inline std::string getEventPath(Name field, const std::string& scaEventPath)
{
    return std::string {scaEventPath + getRawPath(field)};
}

// Same as getScaPath but every path is prefixed with "/sca"
inline std::string getSCAPath(Name field)
{
    return std::string {"/sca"} + getRawPath(field);
}

/**
 * @brief Copy field from original event to sca event if exist
 *
 * @param field
 * @param event
 * @param scaEventPath
 */
inline void
copyIfExist(Name field, const base::Event& event, const std::string& scaEventPath)
{
    const auto origin = getEventPath(field, scaEventPath);
    if (event->exists(origin))
    {
        event->set(field::getSCAPath(field), origin);
    }
};

/**
 * @brief Copy field from original event to sca event if exist
 *
 * @param field field to copy
 * @param event original event
 * @param scaEventPath sca event path
 */
inline void
csvStr2ArrayIfExist(Name field, base::Event& event, const std::string& scaEventPath)
{
    if (event->exists(field::getEventPath(field, scaEventPath)))
    {
        auto csv = event->getString(field::getEventPath(field, scaEventPath));
        if (csv)
        {
            const auto scaArrayPath = field::getSCAPath(field);
            const auto cvaArray = utils::string::split(csv.value().c_str(), ',');

            event->setArray(scaArrayPath);
            for (auto& csvItem : cvaArray)
            {
                event->appendString(csvItem, scaArrayPath);
            }
        }
    }
};

/**
 * @brief Represents a condition to check.
 *
 * field::name is the the field to check.
 * field::Type is the type of the field to check.
 * bool if true, the field is required.
 */
using conditionToCheck = std::tuple<field::Name, field::Type, bool>;
/**
 * @brief Check array of conditions against a given event
 *
 * Check the types of fields and if they are present when they are mandatory.
 * The conditions are checked against the event in the order they are given.
 * If any condition is not met, the event is not valid and returns false.
 * @param event The event to check against
 * @param eventPath The path to sca event (in event)
 * @param conditions The array of conditions to check against the event.
 * @return true If all conditions are met
 * @return false If any condition is not met
 */
inline bool isValidEvent(const base::Event& event,
                         const std::string& eventPath,
                         const std::vector<conditionToCheck>& conditions)
{
    // Check types and mandatory fields if is necessary. Return false on fail.
    const auto isValidCondition =
        [&event](field::Type type, const std::string& path, bool mandatory)
    {
        if (event->exists(path))
        {
            switch (type)
            {
                case field::Type::STRING: return event->isString(path);
                case field::Type::INT: return event->isInt(path);
                case field::Type::BOOL: return event->isBool(path);
                case field::Type::ARRAY: return event->isArray(path);
                case field::Type::OBJECT: return event->isObject(path);
                default: return false; // TODO Logic error?
            }
        }
        else if (mandatory)
        {
            return false;
        }
        return true;
    };

    for (const auto& [field, type, mandatory] : conditions)
    {
        const auto path = field::getEventPath(field, eventPath);
        if (!isValidCondition(type, path, mandatory))
        {
            return false; // Some condition is not met.
        }
    }

    return true;
};

} // namespace field


/**
 * @brief Get the Rule String from de code rule
 *
 * @param ruleChar The code rule
 * @return std::optional<std::string> The rule string
 */
inline std::optional<std::string> getRuleTypeStr(const char ruleChar)
{
    switch (ruleChar)
    {
        case 'f': return "file";
        case 'd': return "directory";
        case 'r': return "registry";
        case 'c': return "command";
        case 'p': return "process";
        case 'n': return "numeric";
        default: return {};
    }
};

/**
 * @brief Perform a query on the database.
 *
 * Perform a query on wdb and expect a result like:
 * - "not found"
 * - "found ${utilPayload}"
 * @param query The query to perform
 * @param wdb The database to query
 * @return <SearchResult::FOUND, ${utilPayload}> if "found XXX" result received
 * @return <SearchResult::NOT_FOUND, ""> if "not found" result received
 * @return <SearchResult::ERROR, ""> otherwise
 */
std::tuple<SearchResult, std::string>
searchAndParse(const std::string& query, std::shared_ptr<wazuhdb::WazuhDB> wdb)
{
    std::string retStr {};
    SearchResult retCode {SearchResult::ERROR};

    auto [rescode, payload] = wdb->tryQueryAndParseResult(query);

    if (wazuhdb::QueryResultCodes::OK == rescode && payload.has_value())
    {
        if (utils::string::startsWith(payload.value(), "found"))
        {
            retCode = SearchResult::FOUND;
            try
            {
                retStr = payload.value().substr(6); // Remove "found " from the beginning
            }
            catch (const std::out_of_range& e)
            {
                // TODO Log the warning in log? it is a logic error on sca modules
                retCode = SearchResult::ERROR;
            }
        }
        else if (utils::string::startsWith(payload.value(), "not found"))
        {
            retCode = SearchResult::NOT_FOUND;
        }
    }

    return {retCode, retStr};
};

/****************************************************************************************
                                 Check Event info
*****************************************************************************************/

bool isValidCheckEvent(base::Event& event, const std::string& scaEventPath)
{
    // CheckEvent conditions list
    const std::vector<field::conditionToCheck> listFieldConditions = {
        {field::Name::CHECK_COMMAND, field::Type::STRING, false},
        {field::Name::CHECK_COMPLIANCE, field::Type::OBJECT, false},
        {field::Name::CHECK_CONDITION, field::Type::STRING, false},
        {field::Name::CHECK_DESCRIPTION, field::Type::STRING, false},
        {field::Name::CHECK_DIRECTORY, field::Type::STRING, false},
        {field::Name::CHECK_FILE, field::Type::STRING, false},
        {field::Name::CHECK_ID, field::Type::INT, true},
        {field::Name::CHECK_PROCESS, field::Type::STRING, false},
        {field::Name::CHECK_RATIONALE, field::Type::STRING, false},
        {field::Name::CHECK_REASON, field::Type::STRING, false},
        {field::Name::CHECK_REFERENCES, field::Type::STRING, false},
        {field::Name::CHECK_REGISTRY, field::Type::STRING, false},
        {field::Name::CHECK_REMEDIATION, field::Type::STRING, false},
        {field::Name::CHECK_RESULT, field::Type::STRING, false},
        {field::Name::CHECK_RULES, field::Type::ARRAY, false},
        {field::Name::CHECK_TITLE, field::Type::STRING, true},
        {field::Name::CHECK, field::Type::OBJECT, true},
        {field::Name::ID, field::Type::INT, true},
        {field::Name::POLICY_ID, field::Type::STRING, true},
        {field::Name::POLICY, field::Type::STRING, true}};

    if (!field::isValidEvent(event, scaEventPath, listFieldConditions))
    {
        return false;
    }

    /* If status is present then reason should be present
     If result is not present then status should be present */
    bool existResult =
        event->exists(field::getEventPath(field::Name::CHECK_RESULT, scaEventPath));
    bool existReason =
        event->exists(field::getEventPath(field::Name::CHECK_REASON, scaEventPath));
    bool existStatus =
        event->exists(field::getEventPath(field::Name::CHECK_STATUS, scaEventPath));

    if ((!existResult && !existStatus) || (existStatus && !existReason))
    {
        return false;
    }

    return true;
}

void fillCheckEvent(base::Event& event,
                    const std::string& previousResult,
                    const std::string& scaEventPath)
{

    event->setString("check", field::getSCAPath(field::Name::TYPE));

    // Save the previous result
    if (!previousResult.empty())
    {
        event->setString(previousResult.c_str(),
                         field::getSCAPath(field::Name::CHECK_PREVIOUS_RESULT));
    }

    field::copyIfExist(field::Name::ID, event, scaEventPath);
    field::copyIfExist(field::Name::POLICY, event, scaEventPath);
    field::copyIfExist(field::Name::POLICY_ID, event, scaEventPath);

    field::copyIfExist(field::Name::CHECK_ID, event, scaEventPath);
    field::copyIfExist(field::Name::CHECK_TITLE, event, scaEventPath);
    field::copyIfExist(field::Name::CHECK_DESCRIPTION, event, scaEventPath);
    field::copyIfExist(field::Name::CHECK_RATIONALE, event, scaEventPath);
    field::copyIfExist(field::Name::CHECK_REMEDIATION, event, scaEventPath);
    field::copyIfExist(field::Name::CHECK_COMPLIANCE, event, scaEventPath);
    field::copyIfExist(field::Name::CHECK_REFERENCES, event, scaEventPath);
    // field::copyIfExist(field::Name::CHECK_RULES);  TODO: Why not copy this?

    // Optional fields with csv
    field::csvStr2ArrayIfExist(field::Name::CHECK_FILE, event, scaEventPath);
    field::csvStr2ArrayIfExist(field::Name::CHECK_DIRECTORY, event, scaEventPath);
    field::csvStr2ArrayIfExist(field::Name::CHECK_REGISTRY, event, scaEventPath);
    field::csvStr2ArrayIfExist(field::Name::CHECK_PROCESS, event, scaEventPath);
    field::csvStr2ArrayIfExist(field::Name::CHECK_COMMAND, event, scaEventPath);

    if (event->exists(field::getEventPath(field::Name::CHECK_RESULT, scaEventPath)))
    {
        event->set(field::getSCAPath(field::Name::CHECK_RESULT),
                   field::getEventPath(field::Name::CHECK_RESULT, scaEventPath));
    }
    else
    {
        field::copyIfExist(field::Name::CHECK_STATUS, event, scaEventPath);
        field::copyIfExist(field::Name::CHECK_REASON, event, scaEventPath);
    }
}

std::optional<std::string> handleCheckEvent(InfoEventDecode& infoDec)
{

    // Check types of fields and if they are mandatory
    if (!isValidCheckEvent(infoDec.event, infoDec.scaEventPath))
    {
        // TODO: Check this message. exit error
        return "Mandatory fields missing in event";
    }

    /********************
    Find the Check ID in the wazuhdb
    *********************/

    // Mandatory
    const auto checkIDint = infoDec.event->getInt(
        field::getEventPath(field::Name::CHECK_ID, infoDec.scaEventPath));
    const auto checkIDstr = std::to_string(checkIDint.value_or(-1));

    // Return empty string if not found
    auto getStringIfExist = [&infoDec](field::Name field)
    {
        return infoDec.event->getString(field::getEventPath(field, infoDec.scaEventPath))
            .value_or("");
    };
    // Optional
    auto result = getStringIfExist(field::Name::CHECK_RESULT);
    auto status = getStringIfExist(field::Name::CHECK_STATUS);
    auto reason = getStringIfExist(field::Name::CHECK_REASON);

    // Prepare the query to query the policy monitoring and perform the query
    const auto scaQuery =
        fmt::format("agent {} sca query {}", infoDec.agentID, checkIDint.value());


    auto [resultQueyPolicy, previousResult] = searchAndParse(scaQuery, infoDec.wdb);

    // Generate the new query to save or update the policy monitoring
    std::string saveCheckEventQuery {};
    switch (resultQueyPolicy)
    {
        case SearchResult::FOUND:
        {
            const auto id = infoDec.event->getInt(
                field::getEventPath(field::Name::ID, infoDec.scaEventPath));

            saveCheckEventQuery = fmt::format("agent {} sca update {}|{}|{}|{}|{}",
                                             infoDec.agentID,
                                             checkIDint.value(),
                                             result,
                                             status,
                                             reason,
                                             id.value_or(-1));
        }
        break;
        case SearchResult::NOT_FOUND:
        {
            const auto scaStr = infoDec.event->str(infoDec.scaEventPath).value_or("");
            // It not exists, insert
            saveCheckEventQuery =
                fmt::format("agent {} sca insert {}", infoDec.agentID, scaStr);
        }
        break;
        case SearchResult::ERROR:
        default:
        {
            return "Error querying policy monitoring database";
        }
    }

    // Save the policy monitoring
    auto [resultSavePolicy, emptyPayload] =
        infoDec.wdb->tryQueryAndParseResult(saveCheckEventQuery);
    // if (resultSavePolicy != wdb::QueryResult::OK) TODO log warning?

    // Normalize the SCA event and add the previous result if exists
    bool doFillCheckInfo = result.empty() ? (!status.empty() && (previousResult != status))
                                          : (previousResult != result);

    if (doFillCheckInfo)
    {
        fillCheckEvent(infoDec.event, previousResult, infoDec.scaEventPath);
    }

    // If policies stores in the datebase and its a new check, then save the rest
    if (wazuhdb::QueryResultCodes::OK == resultSavePolicy
        && resultQueyPolicy == SearchResult::NOT_FOUND)
    {
        // Saving compliance fields to database for event id
            auto compliacePath =
                field::getEventPath(field::Name::CHECK_COMPLIANCE, infoDec.scaEventPath);
            if (infoDec.event->exists(compliacePath))
            {
                // Mandatory object type for this field (TODO: Add logic error)
                const auto& compliance = infoDec.event->getObject(compliacePath);
                for (const auto& [key, jsonValue] : compliance.value())
                {
                    // TODO Check compliance types, can be stringyfied or not
                    // (implement getNumber as string in json)
                    std::string value {};
                    if (jsonValue.isString())
                    {
                        value = jsonValue.getString().value();
                    }
                    else if (jsonValue.isInt())
                    {
                        value = std::to_string(jsonValue.getInt().value());
                    }
                    else if (jsonValue.isDouble())
                    {
                        value = std::to_string(jsonValue.getDouble().value());
                    }
                    else
                    {
                        return "Error: Expected string for compliance item";
                    }

                    std::string saveComplianceQuery =
                        fmt::format("agent {} sca insert_compliance {}|{}|{}",
                                    infoDec.agentID,
                                    checkIDstr,
                                    key,
                                    value);
                    // Should I warn if ResultCode isn't ok ?
                    infoDec.wdb->tryQueryAndParseResult(saveComplianceQuery);
                }
            }

            // Save rules
            auto rulesPath =
                field::getEventPath(field::Name::CHECK_RULES, infoDec.scaEventPath);
            if (infoDec.event->exists(rulesPath))
            {
                // Mandatory array type for this field (TODO: Add logic error)
                const auto rules = infoDec.event->getArray(rulesPath);
                for (const auto& jsonRule : rules.value())
                {
                    auto rule = jsonRule.getString();
                    if (rule)
                    {
                        auto type = getRuleTypeStr(rule.value()[0]);
                        if (type)
                        {
                            auto saveRulesQuery =
                                fmt::format("agent {} sca insert_rules {}|{}|{}",
                                            infoDec.agentID,
                                            checkIDstr,
                                            type.value(),
                                            rule.value());
                            infoDec.wdb->tryQueryAndParseResult(saveRulesQuery);
                        }
                    }
                }
            }
    }

    return std::nullopt; // Success
}

/****************************************************************************************
                                 Check Event info
*****************************************************************************************/

bool CheckScanInfoJSON(base::Event& event, const std::string& scaEventPath)
{

    // ScanInfo conditions list
    const std::vector<field::conditionToCheck> conditions = {
        {field::Name::POLICY_ID, field::Type::STRING, true},
        {field::Name::SCAN_ID, field::Type::INT, true},
        {field::Name::START_TIME, field::Type::INT, true},   // C dont check this
        {field::Name::END_TIME, field::Type::INT, true},     // C dont check this
        {field::Name::PASSED, field::Type::INT, true},       // C dont check this
        {field::Name::FAILED, field::Type::INT, true},       // C dont check this
        {field::Name::INVALID, field::Type::INT, true},      // C dont check this
        {field::Name::TOTAL_CHECKS, field::Type::INT, true}, // C dont check this
        {field::Name::SCORE, field::Type::INT, true},        // C dont check this
        {field::Name::HASH, field::Type::STRING, true},
        {field::Name::HASH_FILE, field::Type::STRING, true},
        {field::Name::FILE, field::Type::STRING, true},
        {field::Name::DESCRIPTION, field::Type::STRING, false},
        {field::Name::REFERENCES, field::Type::STRING, false},
        {field::Name::NAME, field::Type::STRING, true},
        /*
        '/force_alert' field is "1" (string) or not present on icoming event
        {field::Name::FORCE_ALERT, field::Type::STRING, false},
        '/first_scan' field is a number (0/1) or not present on icoming event
        {field::Name::FIRST_SCAN, field::Type::INT, false},
        */
    };

    return field::isValidEvent(event, scaEventPath, conditions);
}

// TODO: Change, No use parameter as output, returno tuple with optional
std::tuple<SearchResult, std::string> findScanInfo(const std::string& agentId,
                                                   const std::string& policyId,
                                                   std::shared_ptr<wazuhdb::WazuhDB> wdb)
{
    // "Retrieving sha256 hash for policy id: policy_id"
    std::string query = fmt::format("agent {} sca query_scan {}", agentId, policyId);

    auto [resultCode, payload] = wdb->tryQueryAndParseResult(query);
    if (wazuhdb::QueryResultCodes::OK == resultCode && payload)
    {
        if (utils::string::startsWith(payload.value(), "found"))
        {
            return {SearchResult::FOUND, payload.value().substr(6)}; // removing "found "
        }
        else if (utils::string::startsWith(payload.value(), "not found"))
        {
            return {SearchResult::NOT_FOUND, ""};
        }
    }
    return {SearchResult::ERROR, ""};
}

// Returns true on success, false on error
bool SaveScanInfo(base::Event& event,
                  const std::string& scaEventPath,
                  const std::string& agent_id,
                  bool update,
                  std::shared_ptr<wazuhdb::WazuhDB> wdb)
{
    // "Retrieving sha256 hash for policy id: policy_id"
    std::string SaveScanInfoQuery {};

    auto getInt = [&](field::Name field) -> int
    {
        return event->getInt(field::getEventPath(field, scaEventPath)).value();
    };

    auto getString = [&](field::Name field) -> std::string
    {
        return event->getString(field::getEventPath(field, scaEventPath)).value();
    };

    const auto pm_start_scan = getInt(field::Name::START_TIME);
    const auto pm_end_scan = getInt(field::Name::END_TIME);
    const auto scan_id = getInt(field::Name::SCAN_ID);
    const auto pass = getInt(field::Name::PASSED);
    const auto failed = getInt(field::Name::FAILED);
    const auto invalid = getInt(field::Name::INVALID);
    const auto total_checks = getInt(field::Name::TOTAL_CHECKS);
    const auto score = getInt(field::Name::SCORE);

    const auto hash = getString(field::Name::HASH);
    const auto policy_id = getString(field::Name::POLICY_ID);

    // TODO This is a int
    if (!update)
    {
        SaveScanInfoQuery =
            fmt::format("agent {} sca insert_scan_info {}|{}|{}|{}|{}|{}|{}|{}|{}|{}",
                        agent_id,
                        pm_start_scan,
                        pm_end_scan,
                        scan_id,
                        policy_id,
                        pass,
                        failed,
                        invalid,
                        total_checks,
                        score,
                        hash);
    }
    else
    {
        SaveScanInfoQuery = fmt::format(
            "agent {} sca update_scan_info_start {}|{}|{}|{}|{}|{}|{}|{}|{}|{}",
            agent_id,
            policy_id,
            pm_start_scan,
            pm_end_scan,
            scan_id,
            pass,
            failed,
            invalid,
            total_checks,
            score,
            hash);
    }

    auto [queryResult, discartPayload] = wdb->tryQueryAndParseResult(SaveScanInfoQuery);
    return queryResult == wazuhdb::QueryResultCodes::OK;
}

SearchResult findPolicyInfo(base::Event& event,
                            const std::string& agent_id,
                            const std::string& scaEventPath,
                            std::shared_ptr<wazuhdb::WazuhDB> wdb)
{
    // "Find policies IDs for policy '%s', agent id '%s'"
    const auto query = fmt::format(
        "agent {} sca query_policy {}",
        agent_id,
        event->getString(field::getEventPath(field::Name::POLICY_ID, scaEventPath))
            .value());

    auto [code, payload] = wdb->tryQueryAndParseResult(query);

    auto retval = SearchResult::ERROR;
    if (wazuhdb::QueryResultCodes::OK == code && payload)
    {
        if (utils::string::startsWith(payload.value(), "found"))
        {
            retval = SearchResult::FOUND;
        }
        else if (utils::string::startsWith(payload.value(), "not found"))
        {
            retval = SearchResult::NOT_FOUND;
        }
    }

    return retval;
}

// TODO: check return value and implications if the operation fails
bool pushDumpRequest(const std::string& agentId,
                     const std::string& policyId,
                     int firstScan)
{
    // from RequestDBThread I'm assuming there's no chance a manager can be the agent
    // that's why Im using just opening CFGARQUEUE
    const auto msg = fmt::format("{}:sca-dump:{}:{}", agentId, policyId, firstScan);

    base::utils::socketInterface::unixDatagram socketCFFGA(CFGARQUEUE);
    // TODO Check retval, maybe if is ok save in the event?
    bool retval;
    try
    {
        retval =
            socketCFFGA.sendMsg(msg) == base::utils::socketInterface::SendRetval::SUCCESS;
    }
    catch (const std::runtime_error& exception)
    {
        // TODO Log exception ?, we need the tracer here?
        retval = false;
    }

    return retval;
}

// Returns true if the operation was successful, false otherwise
bool SavePolicyInfo(base::Event& event,
                    const std::string& agent_id,
                    const std::string& scaEventPath,
                    std::shared_ptr<wazuhdb::WazuhDB> wdb)
{

    auto getInt = [&](field::Name field) -> int
    {
        return event->getInt(field::getEventPath(field, scaEventPath)).value();
    };

    auto getStringOrNULL = [&](field::Name field) -> std::string
    {
        return event->getString(field::getEventPath(field, scaEventPath))
            .value_or("NULL");
    };

    auto query = fmt::format("agent {} sca insert_policy {}|{}|{}|{}|{}|{}",
                             agent_id,
                             getStringOrNULL(field::Name::NAME),
                             getStringOrNULL(field::Name::FILE),
                             getStringOrNULL(field::Name::POLICY_ID),
                             getStringOrNULL(field::Name::DESCRIPTION),
                             getStringOrNULL(field::Name::REFERENCES),
                             getStringOrNULL(field::Name::HASH_FILE));

    auto [result, payload] = wdb->tryQueryAndParseResult(query);

    return wazuhdb::QueryResultCodes::OK == result;
}

std::tuple<SearchResult, std::string>
FindPolicySHA256(base::Event& event,
                 const std::string& agent_id,
                 const std::string& scaEventPath,
                 std::shared_ptr<wazuhdb::WazuhDB> wdb)
{

    // "Find sha256 for policy X, agent id Y"
    std::string query = fmt::format(
        "agent {} sca query_policy_sha256 {}",
        agent_id,
        event->getString(field::getEventPath(field::Name::POLICY_ID, scaEventPath))
            .value());

    auto [resultCode, payload] = wdb->tryQueryAndParseResult(query);
    if (wazuhdb::QueryResultCodes::OK == resultCode && payload)
    {
        if (utils::string::startsWith(payload.value(), "found"))
        {
            return {SearchResult::FOUND, payload.value().substr(6)}; // removing "found "
        }
        else if (utils::string::startsWith(payload.value(), "not found"))
        {
            return {SearchResult::NOT_FOUND, ""};
        }
    }
    return {SearchResult::ERROR, ""};
}

int deletePolicy(const std::string& agent_id,
                 const std::string& policyId,
                 std::shared_ptr<wazuhdb::WazuhDB> wdb)
{
    // "Deleting policy '%s', agent id '%s'"
    auto query = fmt::format("agent {} sca delete_policy {}", agent_id, policyId);
    auto [resultCode, payload] = wdb->tryQueryAndParseResult(query);

    if (wazuhdb::QueryResultCodes::OK == resultCode)
    {
        return 0;
    }
    else if (wazuhdb::QueryResultCodes::ERROR == resultCode)
    {
        return 1;
    }

    return -1;
}

int deletePolicyCheck(const std::string& agent_id,
                      const std::string& policyId,
                      std::shared_ptr<wazuhdb::WazuhDB> wdb)

{
    // "Deleting check for policy '%s', agent id '%s'"
    auto query = fmt::format("agent {} sca delete_check {}", agent_id, policyId);

    auto [resultCode, payload] = wdb->tryQueryAndParseResult(query);

    if (wazuhdb::QueryResultCodes::OK == resultCode)
    {
        return 0;
    }
    else if (wazuhdb::QueryResultCodes::ERROR == resultCode)
    {
        return 1;
    }

    return -1;
}

std::tuple<SearchResult, std::string>
findCheckResults(const std::string& agentId,
                 const std::string& policyId,
                 std::shared_ptr<wazuhdb::WazuhDB> wdb)
{
    // "Find check results for policy id: %s"
    std::string query = fmt::format("agent {} sca query_results {}", agentId, policyId);

    auto [resultCode, payload] = wdb->tryQueryAndParseResult(query);
    if (wazuhdb::QueryResultCodes::OK == resultCode && payload)
    {
        if (utils::string::startsWith(payload.value(), "found"))
        {
            return {SearchResult::FOUND, payload.value().substr(6)}; // removing found
        }
        else if (utils::string::startsWith(payload.value(), "not found"))
        {
            return {SearchResult::NOT_FOUND, ""};
        }
    }

    return {SearchResult::ERROR, ""};
}

void FillScanInfo(base::Event& event,
                  const std::string& agent_id,
                  const std::string& scaEventPath)
{
    event->setString("summary", field::getSCAPath(field::Name::TYPE));

    // Copy if exists

    field::copyIfExist(field::Name::SCAN_ID, event, scaEventPath);
    // The /name field is renamed to /policy
    event->set(field::getSCAPath(field::Name::POLICY),
               field::getEventPath(field::Name::NAME, scaEventPath));

    field::copyIfExist(field::Name::DESCRIPTION, event, scaEventPath);
    field::copyIfExist(field::Name::POLICY_ID, event, scaEventPath);
    field::copyIfExist(field::Name::PASSED, event, scaEventPath);
    field::copyIfExist(field::Name::FAILED, event, scaEventPath);
    field::copyIfExist(field::Name::INVALID, event, scaEventPath);
    field::copyIfExist(field::Name::TOTAL_CHECKS, event, scaEventPath);
    field::copyIfExist(field::Name::SCORE, event, scaEventPath);
    field::copyIfExist(field::Name::FILE, event, scaEventPath);
}

// - Scan Info Handling - //

std::optional<std::string> handleScanInfo(base::Event& event,
                                          const std::string& agent_id,
                                          const std::string& scaEventPath,
                                          std::shared_ptr<wazuhdb::WazuhDB> wdb)
{
    int alert_data_fill = 0;
    int result_event = 0;

    // Validate the JSON ScanInfo Event
    if (!CheckScanInfoJSON(event, scaEventPath))
    {
        return "fail on CheckScanInfoJSON"; // Fail on check
    }

    // Get the policy ID and the hash from the database
    std::string policyId =
        event->getString(field::getEventPath(field::Name::POLICY_ID, scaEventPath))
            .value();
    std::string eventHash =
        event->getString(field::getEventPath(field::Name::HASH, scaEventPath)).value();
    // Check if should be a force alert and if is the first scan
    bool force_alert =
        event->exists(field::getEventPath(field::Name::FORCE_ALERT, scaEventPath));
    bool first_scan =
        event->exists(field::getEventPath(field::Name::FIRST_SCAN, scaEventPath));

    // Get sha256 hash for policy id
    auto [result_db, hash_scan_info] = findScanInfo(agent_id, policyId, wdb);

    switch (result_db)
    {
        case SearchResult::ERROR:
            //TODO Log error
            break;
        case SearchResult::FOUND:
        {
            const auto separated_hash = utils::string::split(hash_scan_info, ' ');
            // If query fails or hash is not found, storedHash is empty
            const auto& storedHash =
                separated_hash.at(0); // Should I chek qtty of chars? (%64s)
            // Try to update the scan info
            if (SaveScanInfo(event, scaEventPath, agent_id, true, wdb))
            {
                /* Compare hash with previous hash */
                bool diferentHash = (storedHash != eventHash);
                bool newHash = (diferentHash && !first_scan);
                if (force_alert || newHash)
                {
                    FillScanInfo(event, agent_id, scaEventPath);
                }
            }
            /*  TODO Log error
            else return "Error updating policy monitoring database for agent {id}";
            */
           break;
        }
        case SearchResult::NOT_FOUND:
        default:
        {
            // It not exists, insert
            if (SaveScanInfo(event, scaEventPath, agent_id, false, wdb))
            {
                FillScanInfo(event, agent_id, scaEventPath);
                if (first_scan)
                {
                    pushDumpRequest(agent_id, policyId, 1);
                }
            }
            /*  TODO Log error
            else return "Error inserting policy monitoring database for agent {id}";
            */
           break;
        }
    }

    // TODO Changes name and type, shoul be `SearchResult::`
    auto result_db3 = findPolicyInfo(event, agent_id, scaEventPath, wdb);

    // Returns the string of field if exist or "NULL" string if not exist
    auto getStringOrNullStr = [&](field::Name fild) -> std::string
    {
        if (event->exists(field::getEventPath(fild, scaEventPath)))
        {
            return event->getString(field::getEventPath(fild, scaEventPath)).value();
        }
        else
        {
            return "NULL";
        }
    };

    switch (result_db3)
    {
        case SearchResult::ERROR:
            //TODO Log error
            break;
        case SearchResult::NOT_FOUND:
        {
            if (!SavePolicyInfo(event, agent_id, scaEventPath, wdb))
            {
                return "Error storing scan policy monitoring information for {}";
            }
        }
        break;
        case SearchResult::FOUND:
        {
            const auto [rescode, oldHashFile] =
                FindPolicySHA256(event, agent_id, scaEventPath, wdb);

            if (SearchResult::FOUND == rescode)
            {
                const auto eventHashFile = event
                                               ->getString(field::getEventPath(
                                                   field::Name::HASH_FILE, scaEventPath))
                                               .value();
                if (oldHashFile != eventHashFile)
                {
                    if (deletePolicy(agent_id, policyId, wdb) == 0)
                    {
                        deletePolicyCheck(agent_id, policyId, wdb);
                        pushDumpRequest(agent_id, policyId, 1);
                    }
                    // else
                    // {
                    //     debug "Unable to purge DB content for policy '%s'"
                    // }
                }
            }
        }
        break;
    }
    // TODO: change result name
    auto [result_db2, oldEventHash] = findCheckResults(agent_id, policyId, wdb);

    switch (result_db2)
    {
        case SearchResult::FOUND:
            /* Integrity check */
            if (oldEventHash != eventHash)
            {
                // mdebug1("Scan result integrity failed for policy '%s'. Hash from
                // DB:'%s', hash from summary: '%s'. Requesting DB
                // dump.",policy_id->valuestring, wdb_response, hash->valuestring);
                if (!first_scan)
                {
                    pushDumpRequest(agent_id, policyId, 0);
                }
                else
                {
                    pushDumpRequest(agent_id, policyId, 1);
                }
            }
            break;
        case SearchResult::NOT_FOUND:
            /* Empty DB */
            // mdebug1("Check results DB empty for policy '%s'. Requesting DB
            // dump.",policy_id->valuestring);
            if (!first_scan)
            {
                pushDumpRequest(agent_id, policyId, 0);
            }
            else
            {
                pushDumpRequest(agent_id, policyId, 1);
            }
            break;
        default:
            // merror("Error querying policy monitoring database for agent
            // '%s'",lf->agent_id);
            // TODO log error
            break;
    }

    return {};
}

/// Policies Functions ///

std::tuple<SearchResult, std::string>
findPoliciesIds(const std::string& agentId, std::shared_ptr<wazuhdb::WazuhDB> wdb)
{
    // "Find policies IDs for agent id: %s"
    std::string query = fmt::format("agent {} sca query_policies ", agentId);

    std::string policiesIds;

    auto [resultCode, payload] = wdb->tryQueryAndParseResult(query);

    if (wazuhdb::QueryResultCodes::OK == resultCode && payload)
    {
        if (utils::string::startsWith(payload.value(), "found"))
        {
            return {SearchResult::FOUND, payload.value().substr(6)}; // removing found
        }
        else if (utils::string::startsWith(payload.value(), "not found"))
        {
            return {SearchResult::NOT_FOUND, ""};
        }
    }
    return {SearchResult::ERROR, ""};
}

// - Policies Handling - //

std::optional<std::string> handlePoliciesInfo(base::Event& event,
                                              const std::string& agentId,
                                              const std::string& scaEventPath,
                                              std::shared_ptr<wazuhdb::WazuhDB> wdb)
{
    const auto policiesPath = field::getEventPath(field::Name::POLICIES, scaEventPath);
    // Check policies JSON
    if (!event->exists(policiesPath) || !event->isArray(policiesPath))
    {
        return "Error: policies array not found";
    }
    // TODO: add proper Json method to check if array contains value/s
    // This is needed only to check aboved.
    auto policies = event->getArray(policiesPath).value();

    // "Retrieving policies from database."
    auto [resultDb, policiesIds] = findPoliciesIds(agentId, wdb);
    if (SearchResult::ERROR == resultDb)
    {
        return "Error querying policy monitoring database for agent";
    }

    /* For each policy id, look if we have scanned it */
    // policiesIds may be empty, in c is the same
    const auto& policiesList = utils::string::split(policiesIds, ',');

    for (auto& pId : policiesList)
    {
        /* This policy is not being scanned anymore, delete it */
        if (std::find_if(policies.begin(),
                         policies.end(),
                         [&](const auto& policy)
                         {
                             auto pStr = policy.getString();
                             return pStr && pStr.value() == pId;
                         })
            == policies.end())
        {
            // "Policy id doesn't exist: '%s'. Deleting it.", p_id);
            int resultDelete = deletePolicy(agentId, pId, wdb);
            if (resultDelete == 0)
            {
                deletePolicyCheck(agentId, pId, wdb);
            }
            else
            {
                return "Error: Unable to purge DB content for policy";
            }
        }
    }

    return std::nullopt;
}

/// Dump Functions ///

std::tuple<std::optional<std::string>, std::string, std::string>
checkDumpJSON(const base::Event& event, const std::string& scaEventPath)
{

    // ScanInfo conditions list
    const std::vector<field::conditionToCheck> conditions = {
        {field::Name::ELEMENTS_SENT, field::Type::INT, true},
        {field::Name::POLICY_ID, field::Type::STRING, true},
        {field::Name::SCAN_ID, field::Type::STRING, true},
    };

    if (!field::isValidEvent(event, scaEventPath, conditions))
    {
        return {"Malformed JSON", "", ""};
    }
    auto policyId =
        event->getString(getEventPath(field::Name::POLICY_ID, scaEventPath)).value_or("");
    auto scanId =
        event->getString(getEventPath(field::Name::SCAN_ID, scaEventPath)).value();
    return {std::nullopt, std::move(policyId), std::move(scanId)};
}

bool deletePolicyCheckDistinct(const std::string& agentId,
                               const std::string& policyId,
                               const std::string& scanId,
                               std::shared_ptr<wazuhdb::WazuhDB> wdb)
{
    // "Deleting check distinct policy id , agent id "
    auto query = fmt::format(
        "agent {} sca delete_check_distinct {}|{}", agentId, policyId, scanId);

    auto [resultCode, payload] = wdb->tryQueryAndParseResult(query);
    switch (resultCode)
    {
        // If deleted or error we return true
        case wazuhdb::QueryResultCodes::OK:
        case wazuhdb::QueryResultCodes::ERROR: return true;
        // If other result we return false why?
        default: return false;
    }
}

// - Dump Handling - //

// Dump event received (type = dump), if is well formed we proceed to perform
// deletePolicyCheckDistinct and we compare policy hashes
// TODO: When this operations fails ??
std::optional<std::string> handleDumpEvent(base::Event& event,
                                           const std::string& agentId,
                                           const std::string& scaEventPath,
                                           std::shared_ptr<wazuhdb::WazuhDB> wdb)
{
    std::optional<std::string> error;

    // Check dump event JSON fields
    // If all the fields are ok continue, if not do nothing
    auto [checkError, policyId, scanId] = checkDumpJSON(event, scaEventPath);
    if (!checkError)
    {
        // "Deleting check distinct policy id , agent id "
        // Continue always, if rare error log error
        // In the c code it logs the error and continues
        deletePolicyCheckDistinct(agentId, policyId, scanId, wdb);

        // Retreive hash from db
        auto [resultCode, hashCheckResults] = findCheckResults(agentId, policyId, wdb);
        if (SearchResult::FOUND == resultCode)
        {
            // Retreive hash from summary
            auto [scanResultCode, hashScanInfo] = findScanInfo(agentId, policyId, wdb);
            if (SearchResult::FOUND == scanResultCode && hashScanInfo.size() == 64)
            {
                if (hashCheckResults != hashScanInfo)
                {
                    // C Here logs error
                    //                     mdebug1("Scan result integrity failed for
                    //                     policy '%s'. Hash from DB: '%s' hash from
                    //                     summary: '%s'. Requesting DB dump.",
                    pushDumpRequest(agentId, policyId, 0);
                    return fmt::format(
                        "Scan result integrity failed for policy '{}'. Hash from DB: "
                        "'{}' hash from summary: '{}'. Requesting DB dump.",
                        policyId,
                        hashCheckResults,
                        hashScanInfo);
                }
            }
        }
    }

    // If error do nothing
    return std::nullopt;
}

} // namespace sca

// - Helper - //

base::Expression opBuilderSCAdecoder(const std::any& definition)
{
    // Extract parameters from any
    auto [targetField, name, raw_parameters] =
        helper::base::extractDefinition(definition);
    // Identify references and build JSON pointer paths
    auto parameters {helper::base::processParameters(raw_parameters)};
    // Assert expected number of parameters
    helper::base::checkParametersSize(parameters, 2);
    // Parameter type check
    helper::base::checkParameterType(parameters[0],
                                     helper::base::Parameter::Type::REFERENCE);
    helper::base::checkParameterType(parameters[1],
                                     helper::base::Parameter::Type::REFERENCE);

    // Format name for the tracer
    name = helper::base::formatHelperFilterName(name, targetField, parameters);

    // Tracing
    const auto successTrace {fmt::format("[{}] -> Success", name)};

    const auto failureTrace1 {
        fmt::format("[{}] -> Failure: [{}] not found", name, targetField)};
    const auto failureTrace2 {fmt::format(
        "[{}] -> Failure: [{}] is empty or is not an object", name, targetField)};
    const auto failureTrace3 {fmt::format("[{}] -> Failure: ", name)};

    // TODO: we are not doing nothing on buildtime, wazuhdb initializer has 11 refs...
    // EventPaths and mappedPaths can be set in buildtime
    auto wdb = std::make_shared<wazuhdb::WazuhDB>(STREAM_SOCK_PATH);

    // Return Term
    return base::Term<base::EngineOp>::create(
        name,
        [=,
         name = std::string {name},
         targetField = std::move(targetField),
         scaEventPath = parameters[0].m_value,
         agentIdPath = parameters[1].m_value,
         wdb = std::move(wdb)](base::Event event) -> base::result::Result<base::Event>
        {
            std::optional<std::string> error;

            // TODO: this should be checked in the decoder
            if (event->exists(scaEventPath) && event->exists(agentIdPath)
                && event->isString(agentIdPath))
            {
                auto agentId = event->getString(agentIdPath).value();
                auto state = sca::InfoEventDecode {event, agentId, scaEventPath, wdb};

                // TODO: Field type is mandatory and should be checked in the decoder
                auto type = event->getString(scaEventPath + "/type");
                if (!type)
                {
                    // TODO: Change trace message
                    error = failureTrace1;
                }
                // Proccess event with the appropriate handler
                else if (sca::TYPE_CHECK == type.value())
                {
                    error = sca::handleCheckEvent(state);
                }
                else if (sca::TYPE_SUMMARY == type.value())
                {
                    error = sca::handleScanInfo(event, agentId, scaEventPath, wdb);
                }
                else if (sca::TYPE_POLICIES == type.value())
                {
                    error = sca::handlePoliciesInfo(event, agentId, scaEventPath, wdb);
                }
                else if (sca::TYPE_DUMP_END == type.value())
                {
                    error = sca::handleDumpEvent(event, agentId, scaEventPath, wdb);
                }
                // Unknown type value
                else
                {
                    // TODO: Change trace message
                    error = failureTrace2;
                }
            }
            else
            {
                error = failureTrace1;
            }

            // Event is processed, return base::Result accordingly
            // Error is nullopt if no error occurred, otherwise it contains the error
            // message
            // TODO: Is realy needed to set targetField bool? makes more sense that
            // targetField is the sca field, that is where we are mapping the fields
            if (error)
            {
                event->setBool(false, targetField);
                return base::result::makeFailure(event, error.value());
            }
            else
            {
                event->setBool(true, targetField);
                return base::result::makeSuccess(event, successTrace);
            }
        });
}

} // namespace builder::internals::builders
