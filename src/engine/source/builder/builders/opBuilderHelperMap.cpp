#include "opBuilderHelperMap.hpp"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <date/date.h>
#include <date/tz.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <re2/re2.h>

#include "syntax.hpp"
#include <baseHelper.hpp>
#include <utils/ipUtils.hpp>
#include <utils/stringUtils.hpp>

namespace
{

constexpr auto TRACE_SUCCESS = "[{}] -> Success";

constexpr auto TRACE_TARGET_NOT_FOUND = "[{}] -> Failure: Target field '{}' reference not found";
constexpr auto TRACE_TARGET_TYPE_NOT_STRING = "[{}] -> Failure: Target field '{}' type is not string";
constexpr auto TRACE_REFERENCE_NOT_FOUND = "[{}] -> Failure: Parameter '{}' reference not found";
constexpr auto TRACE_REFERENCE_TYPE_IS_NOT = "[{}] -> Failure: Parameter '{}' type is not ";

/**
 * @brief Operators supported by the string helpers.
 *
 */
enum class StringOperator
{
    UP,
    LO,
    TR
};

/**
 * @brief Operators supported by the int helpers.
 *
 */
enum class IntOperator
{
    SUM,
    SUB,
    MUL,
    DIV
};

IntOperator strToOp(const helper::base::Parameter& op)
{
    if ("sum" == op.m_value)
    {
        return IntOperator::SUM;
    }
    else if ("sub" == op.m_value)
    {
        return IntOperator::SUB;
    }
    else if ("mul" == op.m_value)
    {
        return IntOperator::MUL;
    }
    else if ("div" == op.m_value)
    {
        return IntOperator::DIV;
    }
    throw std::runtime_error(fmt::format("Operation '{}' not supported", op.m_value));
}

/**
 * @brief Tranform the string in `field` path in the base::Event `e` according to the
 * `op` definition and the `value` or the `refValue`
 *
 * @param definition The transformation definition. i.e : field: +s_[up|lo]/value|$ref
 * @param op The operator to use:
 * - `UP`: Upper case
 * - `LO`: Lower case
 * @return base::Expression
 */
base::Expression opBuilderHelperStringTransformation(const std::string& targetField,
                                                     const std::string& rawName,
                                                     const std::vector<std::string>& rawParameters,
                                                     std::shared_ptr<defs::IDefinitions> definitions,
                                                     StringOperator op)
{
    // Identify references and build JSON pointer paths
    auto parameters {helper::base::processParameters(rawName, rawParameters, definitions)};
    // Assert expected number of parameters
    helper::base::checkParametersSize(rawName, parameters, 1);
    // Format name for the tracer
    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    // Depending on rValue type we store the reference or the string value, string in both
    // cases
    std::string rValue {};
    const helper::base::Parameter rightParameter {parameters[0]};
    const auto rValueType {rightParameter.m_type};
    rValue = rightParameter.m_value;

    // Depending on the operator we return the correct function
    std::function<std::string(const std::string& value)> transformFunction;
    switch (op)
    {
        case StringOperator::UP:
            transformFunction = [](const std::string& value)
            {
                std::string result;
                std::transform(value.begin(), value.end(), std::back_inserter(result), ::toupper);
                return result;
            };
            break;
        case StringOperator::LO:
            transformFunction = [](const std::string& value)
            {
                std::string result;
                std::transform(value.begin(), value.end(), std::back_inserter(result), ::tolower);
                return result;
            };
            break;
        default: break;
    }

    // Tracing messages
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};

    const std::string failureTrace1 {
        fmt::format("[{}] -> Failure: Reference '{}' not found", name, rightParameter.m_value)};

    // Function that implements the helper
    return base::Term<base::EngineOp>::create(
        name,
        [=, targetField = std::move(targetField)](base::Event event) -> base::result::Result<base::Event>
        {
            // We assert that references exists, checking if the optional from Json getter
            // is empty ot not. Then if is a reference we get the value from the event,
            // otherwise we get the value from the parameter

            // REF

            if (helper::base::Parameter::Type::REFERENCE == rValueType)
            {
                const auto resolvedRValue {event->getString(rValue)};

                if (!resolvedRValue.has_value())
                {
                    return base::result::makeFailure(event, failureTrace1);
                }
                else
                {
                    // TODO: should we check the result?
                    auto res {transformFunction(resolvedRValue.value())};
                    event->setString(res, targetField);
                    return base::result::makeSuccess(event, successTrace);
                }
            }
            else
            {
                // TODO: should we check the result?
                const auto res {transformFunction(rValue)};
                event->setString(res, targetField);
                return base::result::makeSuccess(event, successTrace);
            }
        });
}

/**
 * @brief Tranform the int in `field` path in the base::Event `e` according to the
 * `op` definition and the `value` or the `refValue`
 *
 * @param definition The transformation definition. i.e :
 * +int_calculate/[+|-|*|/]/<val1|$ref1>/<.../valN|$refN>/
 * @param op The operator to use:
 * - `SUM`: Sum
 * - `SUB`: Subtract
 * - `MUL`: Multiply
 * - `DIV`: Divide
 * @return base::Expression
 */
base::Expression opBuilderHelperIntTransformation(const std::string& targetField,
                                                  IntOperator op,
                                                  const std::vector<helper::base::Parameter>& parameters,
                                                  std::shared_ptr<defs::IDefinitions> definitions,
                                                  const std::string& name)
{
    std::vector<int> rValueVector {};
    std::vector<std::string> rReferenceVector {};

    // Depending on rValue type we store the reference or the integer value, avoiding
    // iterating again through values inside lambda
    for (const auto& param : parameters)
    {
        int rValue {};
        switch (param.m_type)
        {
            case helper::base::Parameter::Type::VALUE:
                try
                {
                    rValue = std::stoi(param.m_value);
                }
                catch (const std::exception& e)
                {
                    throw std::runtime_error(
                        fmt::format("\"{}\" function: Could not convert parameter \"{}\" to int: {}",
                                    name,
                                    param.m_value,
                                    e.what()));
                }
                if (IntOperator::DIV == op && 0 == rValue)
                {
                    throw std::runtime_error(fmt::format("\"{}\" function: Division by zero", name));
                }

                rValueVector.emplace_back(rValue);
                break;

            case helper::base::Parameter::Type::REFERENCE: rReferenceVector.emplace_back(param.m_value); break;

            default:
                throw std::runtime_error(
                    fmt::format("\"{}\" function: Invalid parameter type of \"{}\"", name, param.m_value));
        }
    }
    // Tracing messages
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};
    const std::string failureTrace1 {fmt::format(TRACE_TARGET_NOT_FOUND, name, targetField)};
    const std::string failureTrace2 {fmt::format(R"([{}] -> Failure: Reference not found: )", name)};
    const std::string failureTrace3 {fmt::format(R"([{}] -> Failure: Parameter is not integer: )", name)};
    const std::string failureTrace4 = fmt::format(R"([{}] -> Failure: Parameter value makes division by zero: )", name);
    const std::string overflowFailureTrace =
        fmt::format(R"([{}] -> Failure: operation result in integer Overflown)", name);
    const std::string underflowFailureTrace =
        fmt::format(R"([{}] -> Failure: operation result in integer Underflown)", name);

    // Depending on the operator we return the correct function
    std::function<int(int l, int r)> transformFunction;
    switch (op)
    {
        case IntOperator::SUM:
            transformFunction = [overflowFailureTrace, underflowFailureTrace](int l, int r)
            {
                if ((r > 0) && (l > INT_MAX - r))
                {
                    throw std::runtime_error(overflowFailureTrace);
                }
                else if ((r < 0) && (l < INT_MIN - r))
                {
                    throw std::runtime_error(underflowFailureTrace);
                }
                else
                {
                    return l + r;
                }
            };
            break;
        case IntOperator::SUB:
            transformFunction = [overflowFailureTrace, underflowFailureTrace](int l, int r)
            {
                if ((r < 0) && (l > INT_MAX + r))
                {
                    throw std::runtime_error(overflowFailureTrace);
                }
                else if ((r > 0) && (l < INT_MIN + r))
                {
                    throw std::runtime_error(underflowFailureTrace);
                }
                else
                {
                    return l - r;
                }
            };
            break;
        case IntOperator::MUL:
            transformFunction = [overflowFailureTrace, underflowFailureTrace](int l, int r)
            {
                if ((r != 0) && (l > INT_MAX / r))
                {
                    throw std::runtime_error(overflowFailureTrace);
                }
                else if ((r != 0) && (l < INT_MIN * r))
                {
                    throw std::runtime_error(underflowFailureTrace);
                }
                else
                {
                    return l * r;
                }
            };
            break;
        case IntOperator::DIV:
            transformFunction = [name, overflowFailureTrace, underflowFailureTrace](int l, int r)
            {
                if (0 == r)
                {
                    throw std::runtime_error(fmt::format(R"("{}" function: Division by zero)", name));
                }
                else
                {
                    return l / r;
                }
            };
            break;
        default: break;
    }

    // Function that implements the helper
    return base::Term<base::EngineOp>::create(
        name,
        [=,
         rValueVector = std::move(rValueVector),
         rReferenceVector = std::move(rReferenceVector),
         targetField = std::move(targetField)](base::Event event) -> base::result::Result<base::Event>
        {
            std::vector<int> auxVector {};
            auxVector.insert(auxVector.begin(), rValueVector.begin(), rValueVector.end());

            const auto lValue {event->getInt(targetField)};
            if (!lValue.has_value())
            {
                return base::result::makeFailure(event, failureTrace1);
            }

            // Iterate throug all references and append them values to the value vector
            for (const auto& rValueItem : rReferenceVector)
            {
                const auto resolvedRValue {event->getInt(rValueItem)};
                if (!resolvedRValue.has_value())
                {
                    return base::result::makeFailure(event,
                                                     (!event->exists(rValueItem)) ? (failureTrace2 + rValueItem)
                                                                                  : (failureTrace3 + rValueItem));
                }
                else
                {
                    if (IntOperator::DIV == op && 0 == resolvedRValue.value())
                    {
                        return base::result::makeFailure(event, failureTrace4 + rValueItem);
                    }

                    auxVector.emplace_back(resolvedRValue.value());
                }
            }

            int res;
            try
            {
                res = std::accumulate(auxVector.begin(), auxVector.end(), lValue.value(), transformFunction);
            }
            catch (const std::runtime_error& e)
            {
                return base::result::makeFailure(event, e.what());
            }

            event->setInt(res, targetField);
            return base::result::makeSuccess(event, successTrace);
        });
}

std::optional<std::string> hashStringSHA1(std::string& input)
{
    // Sha1 digest len (20) * 2 (hex chars per byte)
    constexpr int OS_SHA1_HEXDIGEST_SIZE = (SHA_DIGEST_LENGTH * 2);
    constexpr int OS_SHA1_ARRAY_SIZE_LEN = OS_SHA1_HEXDIGEST_SIZE + 1;

    char* parameter = NULL;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_size;

    EVP_MD_CTX* ctx = EVP_MD_CTX_create();
    if (!ctx)
    {
        // Failed during hash context creation
        return std::nullopt;
    }

    if (1 != EVP_DigestInit_ex(ctx, EVP_sha1(), NULL))
    {
        // Failed during hash context initialization
        EVP_MD_CTX_destroy(ctx);
        return std::nullopt;
    }

    if (1 != EVP_DigestUpdate(ctx, input.c_str(), input.length()))
    {
        // Failed during hash context update
        return std::nullopt;
    }

    EVP_DigestFinal_ex(ctx, digest, &digest_size);
    EVP_MD_CTX_destroy(ctx);

    // OS_SHA1_Hexdigest(digest, hexdigest);
    char output[OS_SHA1_ARRAY_SIZE_LEN];
    for (size_t n = 0; n < SHA_DIGEST_LENGTH; n++)
    {
        sprintf(&output[n * 2], "%02x", digest[n]);
    }

    return {output};
}

} // namespace

namespace builder::internals::builders
{

using builder::internals::syntax::REFERENCE_ANCHOR;
//*************************************************
//*           String tranform                     *
//*************************************************

// field: +upcase/value|$ref
base::Expression opBuilderHelperStringUP(const std::string& targetField,
                                         const std::string& rawName,
                                         const std::vector<std::string>& rawParameters,
                                         std::shared_ptr<defs::IDefinitions> definitions)
{
    auto expression {
        opBuilderHelperStringTransformation(targetField, rawName, rawParameters, definitions, StringOperator::UP)};
    return expression;
}

// field: +downcase/value|$ref
base::Expression opBuilderHelperStringLO(const std::string& targetField,
                                         const std::string& rawName,
                                         const std::vector<std::string>& rawParameters,
                                         std::shared_ptr<defs::IDefinitions> definitions)
{
    auto expression {
        opBuilderHelperStringTransformation(targetField, rawName, rawParameters, definitions, StringOperator::LO)};
    return expression;
}

// field: +trim/[begin | end | both]/char
base::Expression opBuilderHelperStringTrim(const std::string& targetField,
                                           const std::string& rawName,
                                           const std::vector<std::string>& rawParameters,
                                           std::shared_ptr<defs::IDefinitions> definitions)
{
    // Identify references and build JSON pointer paths
    auto parameters {helper::base::processParameters(rawName, rawParameters, definitions)};
    // Assert expected number of parameters
    helper::base::checkParametersSize(rawName, parameters, 2);
    // Parameter type check
    helper::base::checkParameterType(rawName, parameters[0], helper::base::Parameter::Type::VALUE);
    helper::base::checkParameterType(rawName, parameters[1], helper::base::Parameter::Type::VALUE);
    // Format name for the tracer
    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    // Get trim type
    const char trimType = parameters[0].m_value == "begin"  ? 's'
                          : parameters[0].m_value == "end"  ? 'e'
                          : parameters[0].m_value == "both" ? 'b'
                                                            : '\0';
    if ('\0' == trimType)
    {
        throw std::runtime_error(fmt::format("\"{}\" function: Invalid trim type \"{}\"", name, parameters[0].m_value));
    }

    // get trim char
    std::string trimChar {parameters[1].m_value};
    if (trimChar.size() != 1)
    {
        throw std::runtime_error(fmt::format("'{}' function: Invalid trim char '{}'", name, trimChar));
    }

    // Tracing messages
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};

    const std::string failureTrace1 {fmt::format(TRACE_TARGET_NOT_FOUND, name, targetField)};
    const std::string failureTrace2 {fmt::format(TRACE_TARGET_TYPE_NOT_STRING, name, targetField)};
    const std::string failureTrace3 {fmt::format("[{}] -> Failure: Invalid trim type '{}'", name, trimType)};

    // Return Term
    return base::Term<base::EngineOp>::create(
        name,
        [=, targetField = std::move(targetField)](base::Event event) -> base::result::Result<base::Event>
        {
            // Get field value
            auto resolvedField {event->getString(targetField)};

            // Check if field is a string
            if (!resolvedField.has_value())
            {
                return base::result::makeFailure(event, (!event->exists(targetField)) ? failureTrace1 : failureTrace2);
            }

            // Get string
            std::string strToTrim {resolvedField.value()};

            // Trim
            switch (trimType)
            {
                case 's':
                    // Trim begin
                    strToTrim.erase(0, strToTrim.find_first_not_of(trimChar));
                    break;
                case 'e':
                    // Trim end
                    strToTrim.erase(strToTrim.find_last_not_of(trimChar) + 1);
                    break;
                case 'b':
                    // Trim both
                    strToTrim.erase(0, strToTrim.find_first_not_of(trimChar));
                    strToTrim.erase(strToTrim.find_last_not_of(trimChar) + 1);
                    break;
                default: return base::result::makeFailure(event, failureTrace3); break;
            }

            event->setString(strToTrim, targetField);

            return base::result::makeSuccess(event, successTrace);
        });
}

// field: +concat/string1|$ref1/string2|$ref2
base::Expression opBuilderHelperStringConcat(const std::string& targetField,
                                             const std::string& rawName,
                                             const std::vector<std::string>& rawParameters,
                                             std::shared_ptr<defs::IDefinitions> definitions)
{

    // Identify references and build JSON pointer paths
    auto parameters {helper::base::processParameters(rawName, rawParameters, definitions)};
    // Assert expected number of parameters
    checkParametersMinSize(rawName, parameters, 2);
    // Format name for the tracer
    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    // Tracing messages
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};

    const std::string failureTrace1 {fmt::format("[{}] -> Failure: ", name)};
    const std::string failureTrace2 {fmt::format("[{}] -> Failure: ", name)};

    // Return Term
    return base::Term<base::EngineOp>::create(
        name,
        [=, targetField = std::move(targetField)](base::Event event) -> base::result::Result<base::Event>
        {
            std::string result {};

            for (auto parameter : parameters)
            {
                if (helper::base::Parameter::Type::REFERENCE == parameter.m_type)
                {
                    // Check path exists
                    if (!event->exists(parameter.m_value))
                    {
                        return base::result::makeFailure(
                            event, failureTrace1 + fmt::format("Reference '{}' not found", parameter.m_value));
                    }

                    // Get field value
                    std::string resolvedField {};
                    if (event->isDouble(parameter.m_value))
                    {
                        resolvedField = std::to_string(event->getDouble(parameter.m_value).value());
                    }
                    else if (event->isInt(parameter.m_value))
                    {
                        resolvedField = std::to_string(event->getInt(parameter.m_value).value());
                    }
                    else if (event->isString(parameter.m_value))
                    {
                        resolvedField = event->getString(parameter.m_value).value();
                    }
                    else if (event->isObject(parameter.m_value))
                    {
                        resolvedField = event->str(parameter.m_value).value();
                    }
                    else
                    {
                        return base::result::makeFailure(
                            event,
                            failureTrace2 + fmt::format("Parameter '{}' type cannot be handled", parameter.m_value));
                    }

                    result.append(resolvedField);
                }
                else
                {
                    result.append(parameter.m_value);
                }
            }

            event->setString(result, targetField);

            return base::result::makeSuccess(event, successTrace);
        });
}

// field: +join/$<array_reference1>/<separator>
base::Expression opBuilderHelperStringFromArray(const std::string& targetField,
                                                const std::string& rawName,
                                                const std::vector<std::string>& rawParameters,
                                                std::shared_ptr<defs::IDefinitions> definitions)
{
    const auto parameters = helper::base::processParameters(rawName, rawParameters, definitions);
    helper::base::checkParametersSize(rawName, parameters, 2);

    // Check Array reference parameter
    helper::base::checkParameterType(rawName, parameters[0], helper::base::Parameter::Type::REFERENCE);
    const auto arrayName = parameters[0];

    // Check separator parameter
    helper::base::checkParameterType(rawName, parameters[1], helper::base::Parameter::Type::VALUE);
    const auto separator = parameters[1];

    const std::string traceName {helper::base::formatHelperName(rawName, targetField, parameters)};

    // Tracing
    const std::string successTrace {fmt::format(TRACE_SUCCESS, traceName)};

    const std::string failureTrace1 {
        fmt::format("[{}] -> Failure: Array member from '{}' should be a string", traceName, arrayName.m_value)};
    const std::string failureTrace2 {fmt::format(TRACE_REFERENCE_NOT_FOUND, traceName, arrayName.m_value)};
    const std::string failureTrace3 {fmt::format(TRACE_REFERENCE_TYPE_IS_NOT, "array", traceName, arrayName.m_value)};

    // Return Term
    return base::Term<base::EngineOp>::create(
        traceName,
        [=,
         targetField = std::move(targetField),
         separator = std::move(separator.m_value),
         arrayName = std::move(arrayName.m_value)](base::Event event) -> base::result::Result<base::Event>
        {
            // Getting array field, must be a reference
            const auto stringJsonArray = event->getArray(arrayName);
            if (!stringJsonArray.has_value())
            {
                return base::result::makeFailure(event, (!event->exists(arrayName)) ? failureTrace2 : failureTrace3);
            }

            std::vector<std::string> stringArray;
            for (const auto& s_param : stringJsonArray.value())
            {
                if (s_param.isString())
                {
                    const auto strVal = s_param.getString().value();
                    stringArray.emplace_back(std::move(strVal));
                }
                else
                {
                    return base::result::makeFailure(event, failureTrace1);
                }
            }

            // accumulated concation without trailing indexes
            const std::string composedValueString {base::utils::string::join(stringArray, separator)};

            event->setString(composedValueString, targetField);
            return base::result::makeSuccess(event, successTrace);
        });
}

// field: +decode_base16/$<hex_reference>
base::Expression opBuilderHelperStringFromHexa(const std::string& targetField,
                                               const std::string& rawName,
                                               const std::vector<std::string>& rawParameters,
                                               std::shared_ptr<defs::IDefinitions> definitions)
{

    const auto parameters = helper::base::processParameters(rawName, rawParameters, definitions);

    helper::base::checkParametersSize(rawName, parameters, 1);

    helper::base::checkParameterType(rawName, parameters[0], helper::base::Parameter::Type::REFERENCE);

    const auto sourceField = parameters[0];

    const std::string traceName {helper::base::formatHelperName(rawName, targetField, parameters)};

    // Tracing
    const std::string successTrace {fmt::format(TRACE_SUCCESS, traceName)};

    const std::string failureTrace1 {fmt::format(TRACE_REFERENCE_NOT_FOUND, traceName, sourceField.m_value)};
    const std::string failureTrace2 {fmt::format(TRACE_REFERENCE_TYPE_IS_NOT, "array", traceName, sourceField.m_value)};
    const std::string failureTrace3 {
        fmt::format("[{}] -> Failure: Hexa string has not an even quantity of digits", traceName)};
    const std::string failureTrace4 {fmt::format("[{}] -> Failure: ", traceName)};

    // Return Term
    return base::Term<base::EngineOp>::create(
        traceName,
        [=, targetField = std::move(targetField), sourceField = std::move(sourceField)](
            base::Event event) -> base::result::Result<base::Event>
        {
            std::string strHex {};

            // Getting string field from a reference
            const auto refStrHEX = event->getString(sourceField.m_value);
            if (!refStrHEX.has_value())
            {
                return base::result::makeFailure(event,
                                                 (!event->exists(sourceField.m_value)) ? failureTrace1 : failureTrace2);
            }

            strHex = refStrHEX.value();

            const auto lenHex = strHex.length();

            if (lenHex % 2)
            {
                return base::result::makeFailure(event, failureTrace3);
            }

            std::string strASCII {};
            strASCII.resize((lenHex / 2) + 1);

            for (int iHex = 0, iASCII = 0; iHex < lenHex; iHex += 2, iASCII++)
            {
                char* err = nullptr;

                std::string byte = strHex.substr(iHex, 2);
                char chr = (char)strtol(byte.c_str(), &err, 16); // BASE: 16 (Hexa)

                if (err != nullptr && *err != 0)
                {
                    return base::result::makeFailure(
                        event, failureTrace4 + fmt::format("Character '{}' is not a valid hexa digit", err));
                }

                strASCII[iASCII] = chr;
            }

            event->setString(strASCII, targetField);

            return base::result::makeSuccess(event, successTrace);
        });
}

// field: +hex_to_number/$ref
base::Expression opBuilderHelperHexToNumber(const std::string& targetField,
                                            const std::string& rawName,
                                            const std::vector<std::string>& rawParameters,
                                            std::shared_ptr<defs::IDefinitions> definitions)
{
    const auto parameters = helper::base::processParameters(rawName, rawParameters, definitions);
    helper::base::checkParametersSize(rawName, parameters, 1);
    helper::base::checkParameterType(rawName, parameters[0], helper::base::Parameter::Type::REFERENCE);
    const auto sourceField = parameters[0];

    const std::string traceName {helper::base::formatHelperName(rawName, targetField, parameters)};

    // Tracing
    const std::string successTrace {fmt::format(TRACE_SUCCESS, traceName)};

    const std::string failureTrace1 {fmt::format(TRACE_REFERENCE_NOT_FOUND, traceName, sourceField.m_value)};
    const std::string failureTrace2 {
        fmt::format(TRACE_REFERENCE_TYPE_IS_NOT, "string", traceName, sourceField.m_value)};
    const std::string failureTrace3 {fmt::format("[{}] -> Failure: ", traceName)};

    // Return Term
    return base::Term<base::EngineOp>::create(
        traceName,
        [=, targetField = std::move(targetField), sourceField = std::move(sourceField)](
            base::Event event) -> base::result::Result<base::Event>
        {
            // Getting string field from a reference
            const auto refStrHEX = event->getString(sourceField.m_value);
            if (!refStrHEX.has_value())
            {
                return base::result::makeFailure(
                    event,
                    fmt::format((!event->exists(sourceField.m_value)) ? failureTrace1 : failureTrace2,
                                sourceField.m_value));
            }
            std::stringstream ss;
            ss << refStrHEX.value();
            int result;
            ss >> std::hex >> result;
            if (ss.fail() || !ss.eof())
            {
                return base::result::makeFailure(
                    event, failureTrace3 + fmt::format("String '{}' is not a hexadecimal value", refStrHEX.value()));
            }

            event->setInt(result, targetField);
            return base::result::makeSuccess(event, successTrace);
        });
}

// field: +replace/substring/new_substring
base::Expression opBuilderHelperStringReplace(const std::string& targetField,
                                              const std::string& rawName,
                                              const std::vector<std::string>& rawParameters,
                                              std::shared_ptr<defs::IDefinitions> definitions)
{
    // Identify references and build JSON pointer paths
    auto parameters {helper::base::processParameters(rawName, rawParameters, definitions)};
    // Assert expected number of parameters
    helper::base::checkParametersSize(rawName, parameters, 2);
    // Format name for the tracer
    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    const auto paramOldSubstr = parameters[0];
    if (paramOldSubstr.m_value.empty())
    {
        throw std::runtime_error(fmt::format("'{}' function: First parameter (substring) cannot be empty", name));
    }
    const auto paramNewSubstr = parameters[1];

    // Tracing messages
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};

    const std::string failureTrace1 {fmt::format(TRACE_TARGET_NOT_FOUND, name, targetField)};
    const std::string failureTrace2 {fmt::format(TRACE_TARGET_TYPE_NOT_STRING, name, targetField)};
    const std::string failureTrace3 {fmt::format("[{}] -> Failure: Target field '{}' is empty", name, targetField)};

    // Return Term
    return base::Term<base::EngineOp>::create(
        name,
        [=,
         targetField = std::move(targetField),
         paramOldSubstr = std::move(paramOldSubstr),
         paramNewSubstr = std::move(paramNewSubstr)](base::Event event) -> base::result::Result<base::Event>
        {
            if (!event->exists(targetField))
            {
                return base::result::makeFailure(event, failureTrace1);
            }

            // Get field value
            std::optional<std::string> resolvedField {event->getString(targetField)};

            // Check if field is a string
            if (!resolvedField.has_value())
            {
                return base::result::makeFailure(event, failureTrace2);
            }

            // Check if field is a string
            if (resolvedField.value().empty())
            {
                return base::result::makeFailure(event, failureTrace3);
            }

            auto newString {resolvedField.value()};

            std::string oldSubstring {paramOldSubstr.m_value};
            if (helper::base::Parameter::Type::REFERENCE == paramOldSubstr.m_type)
            {
                resolvedField = event->getString(paramOldSubstr.m_value);

                // Check if field is a string
                if (!resolvedField.has_value())
                {
                    return base::result::makeFailure(event, failureTrace1);
                }

                // Check if field is a string
                if (resolvedField.value().empty())
                {
                    return base::result::makeFailure(event, failureTrace2);
                }

                oldSubstring = resolvedField.value();
            }

            std::string newSubstring {paramNewSubstr.m_value};
            if (helper::base::Parameter::Type::REFERENCE == paramNewSubstr.m_type)
            {
                resolvedField = event->getString(paramNewSubstr.m_value);

                // Check if field is a string
                if (!resolvedField.has_value())
                {
                    return base::result::makeFailure(event, failureTrace1);
                }

                // Check if field is a string
                if (resolvedField.value().empty())
                {
                    return base::result::makeFailure(event, failureTrace2);
                }

                newSubstring = resolvedField.value();
            }

            size_t start_pos = 0;
            while ((start_pos = newString.find(oldSubstring, start_pos)) != std::string::npos)
            {
                newString.replace(start_pos, oldSubstring.length(), newSubstring);
                start_pos += newSubstring.length();
            }

            event->setString(newString, targetField);

            return base::result::makeSuccess(event, successTrace);
        });
}

//*************************************************
//*           Int tranform                        *
//*************************************************

// field: +int_calculate/[+|-|*|/]/<val1|$ref1>/.../<valN|$refN>
base::Expression opBuilderHelperIntCalc(const std::string& targetField,
                                        const std::string& rawName,
                                        const std::vector<std::string>& rawParameters,
                                        std::shared_ptr<defs::IDefinitions> definitions)
{
    // Identify references and build JSON pointer paths
    auto parameters {helper::base::processParameters(rawName, rawParameters, definitions)};
    // Assert expected number of parameters
    checkParametersMinSize(rawName, parameters, 2);
    // Format name for the tracer
    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);
    const auto op {strToOp(parameters[0])};
    // TODO: check if there's a better way to do this
    //  remove operation parameter in order to handle all the params equally
    parameters.erase(parameters.begin());

    auto expression {opBuilderHelperIntTransformation(targetField, op, parameters, definitions, name)};
    return expression;
}

//*************************************************
//*           Regex tranform                      *
//*************************************************

// field: +regex_extract/_field/regexp/
base::Expression opBuilderHelperRegexExtract(const std::string& targetField,
                                             const std::string& rawName,
                                             const std::vector<std::string>& rawParameters,
                                             std::shared_ptr<defs::IDefinitions> definitions)
{
    // Identify references and build JSON pointer paths
    auto parameters {helper::base::processParameters(rawName, rawParameters, definitions)};
    // Assert expected number of parameters
    helper::base::checkParametersSize(rawName, parameters, 2);
    // Parameter type check
    helper::base::checkParameterType(rawName, parameters[0], helper::base::Parameter::Type::REFERENCE);
    helper::base::checkParameterType(rawName, parameters[1], helper::base::Parameter::Type::VALUE);
    // Format name for the tracer
    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    std::string map_field {parameters[0].m_value};

    auto regex_ptr = std::make_shared<RE2>(parameters[1].m_value);
    if (!regex_ptr->ok())
    {
        throw std::runtime_error(fmt::format(
            "\"{}\" function: Error compiling regex \"{}\": {}", name, parameters[1].m_value, regex_ptr->error()));
    }

    // Tracing
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};

    const std::string failureTrace1 {fmt::format(TRACE_REFERENCE_NOT_FOUND, name, map_field)};
    const std::string failureTrace2 {fmt::format(TRACE_REFERENCE_TYPE_IS_NOT, "string", name, map_field)};
    const std::string failureTrace3 {fmt::format("[{}] -> Failure: Regex did not match", name)};

    // Return Term
    return base::Term<base::EngineOp>::create(
        name,
        [=, targetField = std::move(targetField)](base::Event event) -> base::result::Result<base::Event>
        {
            // TODO Remove try catch
            const auto resolvedField = event->getString(map_field);

            if (!resolvedField.has_value())
            {
                return base::result::makeFailure(event, (!event->exists(map_field)) ? failureTrace1 : failureTrace2);
            }

            std::string match {};
            if (RE2::PartialMatch(resolvedField.value(), *regex_ptr, &match))
            {
                event->setString(match, targetField);

                return base::result::makeSuccess(event, successTrace);
            }

            return base::result::makeFailure(event, failureTrace3);
        });
}

//*************************************************
//*           Array tranform                      *
//*************************************************

// field: array_append_unique($ref|literal)
// field: array_append($ref|literal)
HelperBuilder getBuilderArrayAppend(bool unique, std::shared_ptr<schemf::ISchema> schema)
{
    return [=](const std::string& targetField,
               const std::string& rawName,
               const std::vector<std::string>& rawParameters,
               std::shared_ptr<defs::IDefinitions> definitions)
    {
        auto parameters = helper::base::processParameters(rawName, rawParameters, definitions);

        checkParametersMinSize(rawName, parameters, 1);

        const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

        // Validate schema types and homogeneities
        if (schema->hasField(targetField) && schema->getType(targetField) != json::Json::Type::Array)
        {
            throw std::runtime_error(
                fmt::format("\"{}\" function: Target field \"{}\" is not an array", name, targetField));
        }

        bool first = true;
        json::Json::Type type;
        for (auto& param : parameters)
        {
            if (first)
            {
                if (param.m_type == helper::base::Parameter::Type::REFERENCE)
                {
                    if (!schema->hasField(param.m_value))
                    {
                        continue;
                    }
                    else
                    {
                        type = schema->getType(param.m_value);
                        first = false;
                    }
                }
                else
                {
                    json::Json value;
                    try
                    {
                        value = json::Json(param.m_value.c_str());
                    }
                    catch (const std::exception& e)
                    {
                        type = json::Json::Type::String;
                        first = false;
                        continue;
                    }

                    type = value.type();
                    first = false;
                }
            }

            if (param.m_type == helper::base::Parameter::Type::REFERENCE)
            {
                if (!schema->hasField(param.m_value))
                {
                    continue;
                }
                else if (schema->getType(param.m_value) != type)
                {
                    throw std::runtime_error(
                        fmt::format("\"{}\" function: Parameter \"{}\" type does not match the first parameter type",
                                    name,
                                    param.m_value));
                }
            }
            else
            {
                json::Json value;
                json::Json::Type valueType;
                try
                {
                    value = json::Json(param.m_value.c_str());
                }
                catch (const std::exception& e)
                {
                    valueType = json::Json::Type::String;
                    if (valueType != type)
                    {
                        throw std::runtime_error(fmt::format(
                            "\"{}\" function: Parameter \"{}\" type does not match the first parameter type",
                            name,
                            param.m_value));
                    }
                    continue;
                }

                if (value.type() != type)
                {
                    throw std::runtime_error(
                        fmt::format("\"{}\" function: Parameter \"{}\" type does not match the first parameter type",
                                    name,
                                    param.m_value));
                }
            }
        }

        // Tracing
        const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};

        const std::string failureTrace {fmt::format("[{}] -> Failure: ", name)};
        const std::string failureSuffix1 {"Parameter '{}' reference not found"};
        const std::string failureSuffix2 {"Parameter '{}' type is not a string"};
        const std::string failureSuffix3 {"Parameter '{}' type unexpected"};

        // Return result
        return base::Term<base::EngineOp>::create(
            name,
            [=, targetField = std::move(targetField)](base::Event event) -> base::result::Result<base::Event>
            {
                // Get target array, needed for check uniqueness
                std::vector<json::Json> targetArray {};
                if (unique)
                {
                    auto array = event->getArray(targetField);
                    if (array)
                    {
                        targetArray = std::move(array.value());
                    }
                }

                // Append values
                for (const auto& parameter : parameters)
                {
                    // Get value to append
                    json::Json value;
                    if (parameter.m_type == helper::base::Parameter::Type::VALUE)
                    {
                        try
                        {
                            value = json::Json(parameter.m_value.c_str());
                        }
                        catch (const std::exception& e)
                        {
                            value.setString(parameter.m_value);
                        }
                    }
                    // REFERENCE
                    else
                    {
                        // Best effort
                        auto result = event->getJson(parameter.m_value);
                        if (result)
                        {
                            value = std::move(result.value());
                        }
                        else
                        {
                            continue;
                        }
                    }

                    // Append value, we append on both arrays to check for uniqueness
                    // TODO improve json so it exposes a contains method
                    if (unique)
                    {
                        if (std::find(targetArray.begin(), targetArray.end(), value) == targetArray.end())
                        {
                            targetArray.emplace_back(value);
                        }
                        else
                        {
                            continue;
                        }
                    }

                    event->appendJson(value, targetField);
                }
                return base::result::makeSuccess(event, successTrace);
            });
    };
}

// field: +merge_recursive/$field
base::Expression opBuilderHelperMergeRecursively(const std::string& targetField,
                                                 const std::string& rawName,
                                                 const std::vector<std::string>& rawParameters,
                                                 std::shared_ptr<defs::IDefinitions> definitions)
{
    auto parameters = helper::base::processParameters(rawName, rawParameters, definitions);
    helper::base::checkParametersSize(rawName, parameters, 1);
    helper::base::checkParameterType(rawName, parameters[0], helper::base::Parameter::Type::REFERENCE);

    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    // Tracing
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};

    const std::string failureTrace1 {fmt::format(TRACE_REFERENCE_NOT_FOUND, name, parameters[0].m_value)};
    const std::string failureTrace2 {fmt::format(TRACE_TARGET_NOT_FOUND, name, targetField)};
    const std::string failureTrace3 {fmt::format("[{}] -> Failure: Fields type do not match", name)};
    const std::string failureTrace4 {fmt::format("[{}] -> Failure: Fields type not supported", name)};

    // Return result
    return base::Term<base::EngineOp>::create(
        name,
        [=, targetField = std::move(targetField), fieldReference = std::move(parameters[0].m_value)](
            base::Event event) -> base::result::Result<base::Event>
        {
            // Check target and reference field exists
            if (!event->exists(fieldReference))
            {
                return base::result::makeFailure(event, failureTrace1);
            }

            if (!event->exists(targetField))
            {
                return base::result::makeFailure(event, failureTrace2);
            }

            // Check fields types
            const auto targetType = event->type(targetField);
            if (targetType != event->type(fieldReference))
            {
                return base::result::makeFailure(event, failureTrace3);
            }
            if (targetType != json::Json::Type::Array && targetType != json::Json::Type::Object)
            {
                return base::result::makeFailure(event, failureTrace4);
            }

            // Merge
            event->merge(json::RECURSIVE, fieldReference, targetField);

            return base::result::makeSuccess(event, successTrace);
        });
}

// event: +erase_custom_fields
HelperBuilder getOpBuilderHelperEraseCustomFields(std::shared_ptr<schemf::ISchema> schema)
{
    return [schema](const std::string& targetField,
                    const std::string& rawName,
                    const std::vector<std::string>& rawParameters,
                    std::shared_ptr<defs::IDefinitions> definitions) -> base::Expression
    {
        auto parameters = helper::base::processParameters(rawName, rawParameters, definitions);
        helper::base::checkParametersSize(rawName, parameters, 0);

        const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

        // Tracing
        const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};

        // Function that check if a field is a custom field
        auto isCustomField = [schema](const std::string& path) -> bool
        {
            // Check if field is a custom field
            return !schema->hasField(path);
        };

        // Return result
        return base::Term<base::EngineOp>::create(
            name,
            [isCustomField, targetField, successTrace](base::Event event) -> base::result::Result<base::Event>
            {
                // Erase custom fields
                event->eraseIfKey(isCustomField, false, targetField);
                return base::result::makeSuccess(event, successTrace);
            });
    };
}

// field: +split/$field/[,| | ...]
base::Expression opBuilderHelperAppendSplitString(const std::string& targetField,
                                                  const std::string& rawName,
                                                  const std::vector<std::string>& rawParameters,
                                                  std::shared_ptr<defs::IDefinitions> definitions)
{
    auto parameters = helper::base::processParameters(rawName, rawParameters, definitions);
    helper::base::checkParametersSize(rawName, parameters, 2);
    helper::base::checkParameterType(rawName, parameters[0], helper::base::Parameter::Type::REFERENCE);
    helper::base::checkParameterType(rawName, parameters[1], helper::base::Parameter::Type::VALUE);
    if (parameters[1].m_value.size() != 1)
    {
        throw std::runtime_error(fmt::format(
            "\"{}\" function: Separator \"{}\" should be one character long", rawName, parameters[1].m_value.size()));
    }

    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    // Tracing
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};

    const std::string failureTrace1 {fmt::format(TRACE_REFERENCE_NOT_FOUND, name, parameters[0].m_value)};
    const std::string failureTrace2 {fmt::format(TRACE_REFERENCE_TYPE_IS_NOT, "string", name, parameters[0].m_value)};

    // Return result
    return base::Term<base::EngineOp>::create(
        name,
        [=,
         targetField = std::move(targetField),
         fieldReference = std::move(parameters[0].m_value),
         separator = std::move(parameters[1].m_value[0])](base::Event event) -> base::result::Result<base::Event>
        {
            const auto resolvedReference = event->getString(fieldReference);
            if (!resolvedReference.has_value())
            {
                return base::result::makeFailure(event,
                                                 (!event->exists(fieldReference)) ? failureTrace1 : failureTrace2);
            }

            const auto splitted = base::utils::string::split(resolvedReference.value(), separator);

            for (const auto& value : splitted)
            {
                event->appendString(value, targetField);
            }

            return base::result::makeSuccess(event, successTrace);
        });
}

base::Expression opBuilderHelperMerge(const std::string& targetField,
                                      const std::string& rawName,
                                      const std::vector<std::string>& rawParameters,
                                      std::shared_ptr<defs::IDefinitions> definitions)
{
    auto parameters = helper::base::processParameters(rawName, rawParameters, definitions);
    helper::base::checkParametersSize(rawName, parameters, 1);
    helper::base::checkParameterType(rawName, parameters[0], helper::base::Parameter::Type::REFERENCE);

    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    // Tracing
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};

    const std::string failureTrace1 {fmt::format(TRACE_REFERENCE_NOT_FOUND, name, parameters[0].m_value)};
    const std::string failureTrace2 {fmt::format(TRACE_TARGET_NOT_FOUND, name, targetField)};
    const std::string failureTrace3 {fmt::format("[{}] -> Failure: Fields type do not match", name)};
    const std::string failureTrace4 {fmt::format("[{}] -> Failure: Fields type not supported", name)};

    // Return result
    return base::Term<base::EngineOp>::create(
        name,
        [=, targetField = std::move(targetField), fieldReference = std::move(parameters[0].m_value)](
            base::Event event) -> base::result::Result<base::Event>
        {
            // Check target and reference field exists
            if (!event->exists(fieldReference))
            {
                return base::result::makeFailure(event, failureTrace1);
            }

            if (!event->exists(targetField))
            {
                return base::result::makeFailure(event, failureTrace2);
            }

            // Check fields types
            auto targetType = event->type(targetField);
            if (targetType != event->type(fieldReference))
            {
                return base::result::makeFailure(event, failureTrace3);
            }
            if (targetType != json::Json::Type::Array && targetType != json::Json::Type::Object)
            {
                return base::result::makeFailure(event, failureTrace4);
            }

            // Merge
            event->merge(json::NOT_RECURSIVE, fieldReference, targetField);

            return base::result::makeSuccess(event, successTrace);
        });
}

//*************************************************
//*             JSON tranform                     *
//*************************************************

// field: +delete
base::Expression opBuilderHelperDeleteField(const std::string& targetField,
                                            const std::string& rawName,
                                            const std::vector<std::string>& rawParameters,
                                            std::shared_ptr<defs::IDefinitions> definitions)
{
    // Identify references and build JSON pointer paths
    auto parameters {helper::base::processParameters(rawName, rawParameters, definitions)};
    // Assert expected number of parameters
    helper::base::checkParametersSize(rawName, parameters, 0);
    // Format name for the tracer
    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    // Tracing messages
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};

    const std::string failureTrace1 {fmt::format("[{}] -> Failure: ", name)};
    const std::string failureTrace2 {
        fmt::format("[{}] -> Failure: Target field '{}' could not be erased", name, targetField)};

    // Return Term
    return base::Term<base::EngineOp>::create(
        name,
        [=, targetField = std::move(targetField)](base::Event event) -> base::result::Result<base::Event>
        {
            bool result {false};
            try
            {
                result = event->erase(targetField);
            }
            catch (const std::exception& e)
            {
                return base::result::makeFailure(event, failureTrace1 + e.what());
            }

            if (result)
            {
                return base::result::makeSuccess(event, successTrace);
            }
            else
            {
                return base::result::makeFailure(event, failureTrace2);
            }
        });
}

// field: +rename/$sourceField
base::Expression opBuilderHelperRenameField(const std::string& targetField,
                                            const std::string& rawName,
                                            const std::vector<std::string>& rawParameters,
                                            std::shared_ptr<defs::IDefinitions> definitions)
{
    // Identify references and build JSON pointer paths
    auto parameters {helper::base::processParameters(rawName, rawParameters, definitions)};
    // Assert expected number and type of parameters
    helper::base::checkParametersSize(rawName, parameters, 1);
    auto sourceField = parameters[0];
    helper::base::checkParameterType(rawName, sourceField, helper::base::Parameter::Type::REFERENCE);
    // Format name for the tracer
    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    // Tracing messages
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};

    const std::string failureTrace1 {
        fmt::format("[{}] -> Failure: Target field '{}' could not be set: ", name, targetField)};
    const std::string failureTrace2 {fmt::format(TRACE_REFERENCE_NOT_FOUND, name, sourceField.m_value)};
    const std::string failureTrace3 {fmt::format("[{}] -> Failure: ", name)};
    const std::string failureTrace4 {
        fmt::format("[{}] -> Failure: Target field '{}' could not be erased", name, targetField)};

    return base::Term<base::EngineOp>::create(
        name,
        [=, targetField = std::move(targetField), sourceField = std::move(sourceField)](
            base::Event event) -> base::result::Result<base::Event>
        {
            if (event->exists(sourceField.m_value))
            {
                try
                {
                    event->set(targetField, sourceField.m_value);
                }
                catch (const std::exception& e)
                {
                    return base::result::makeFailure(event, failureTrace1 + e.what());
                }
            }
            else
            {
                return base::result::makeFailure(event, failureTrace2);
            }

            bool result {false};
            try
            {
                result = event->erase(sourceField.m_value);
            }
            catch (const std::exception& e)
            {
                return base::result::makeFailure(event, failureTrace3 + e.what());
            }

            if (result)
            {
                return base::result::makeSuccess(event, successTrace);
            }
            else
            {
                return base::result::makeFailure(event, failureTrace4);
            }
        });
}

//*************************************************
//*              IP tranform                      *
//*************************************************
// field: +s_IPVersion/$ip_field
base::Expression opBuilderHelperIPVersionFromIPStr(const std::string& targetField,
                                                   const std::string& rawName,
                                                   const std::vector<std::string>& rawParameters,
                                                   std::shared_ptr<defs::IDefinitions> definitions)
{
    auto parameters {helper::base::processParameters(rawName, rawParameters, definitions)};

    // Check parameters
    helper::base::checkParametersSize(rawName, parameters, 1);
    helper::base::checkParameterType(rawName, parameters[0], helper::base::Parameter::Type::REFERENCE);

    // Tracing
    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};

    const std::string failureTrace1 {fmt::format(TRACE_REFERENCE_NOT_FOUND, name, parameters[0].m_value)};
    const std::string failureTrace2 {fmt::format(TRACE_REFERENCE_TYPE_IS_NOT, "string", name, parameters[0].m_value)};
    const std::string failureTrace3 {fmt::format("[{}] -> Failure: ", name)
                                     + "The string \"{}\" is not a valid IP address"};

    // Return result
    return base::Term<base::EngineOp>::create(
        name,
        [=, targetField = std::move(targetField), name = std::move(name), ipStrPath = std::move(parameters[0].m_value)](
            base::Event event) -> base::result::Result<base::Event>
        {
            const auto strIP = event->getString(ipStrPath);

            if (!strIP)
            {
                return base::result::makeFailure(event, (!event->exists(ipStrPath)) ? failureTrace1 : failureTrace2);
            }

            if (utils::ip::checkStrIsIPv4(strIP.value()))
            {
                event->setString("IPv4", targetField);
            }
            else if (utils::ip::checkStrIsIPv6(strIP.value()))
            {
                event->setString("IPv6", targetField);
            }
            else
            {
                return base::result::makeFailure(
                    event, failureTrace3 + fmt::format("The string '{}' is not a valid IP address", strIP.value()));
            }
            return base::result::makeSuccess(event, successTrace);
        });
}

//*************************************************
//*              Time tranform                    *
//*************************************************

// field: + system_epoch
base::Expression opBuilderHelperEpochTimeFromSystem(const std::string& targetField,
                                                    const std::string& rawName,
                                                    const std::vector<std::string>& rawParameters,
                                                    std::shared_ptr<defs::IDefinitions> definitions)
{
    auto parameters = helper::base::processParameters(rawName, rawParameters, definitions);

    // Check parameters
    helper::base::checkParametersSize(rawName, parameters, 0);

    // Tracing
    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};
    const std::string failureTrace {fmt::format("[{}] -> Failure: Value overflow", name)};

    // Return result
    return base::Term<base::EngineOp>::create(
        name,
        [=, targetField = std::move(targetField)](base::Event event) -> base::result::Result<base::Event>
        {
            auto sec =
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                    .count();
            // TODO: Delete this and dd SetInt64 or SetIntAny to JSON class, get
            // Number of any type (fix concat helper)
            if (sec > std::numeric_limits<int>::max())
            {
                return base::result::makeFailure(event, failureTrace);
            }
            event->setInt(sec, targetField);
            return base::result::makeSuccess(event, successTrace);
        });
}

// field: +date_from_epoch/<$epoch_field_ref>|epoch_field
base::Expression opBuilderHelperDateFromEpochTime(const std::string& targetField,
                                                  const std::string& rawName,
                                                  const std::vector<std::string>& rawParameters,
                                                  std::shared_ptr<defs::IDefinitions> definitions,
                                                  std::shared_ptr<schemf::ISchema> schema)
{
    auto parameters = helper::base::processParameters(rawName, rawParameters, definitions);

    // Check parameters
    helper::base::checkParametersSize(rawName, parameters, 1);

    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    // If target field (key) is a schema field, the value should be a string
    if (schema->hasField(DotPath::fromJsonPath(targetField))
        && (schema->getType(DotPath::fromJsonPath(targetField)) != json::Json::Type::String))
    {
        throw std::runtime_error(
            fmt::format("Engine helper builder: [{}] failed schema validation: Target field '{}' value is not a string",
                        name,
                        targetField));
    }

    // Tracing
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};
    const std::string failureTrace1 {fmt::format(TRACE_REFERENCE_NOT_FOUND, name, parameters[0].m_value)};
    const std::string failureTrace2 {fmt::format(TRACE_REFERENCE_TYPE_IS_NOT, "number", name, parameters[0].m_value)};
    const std::string failureTrace3 {fmt::format("[{}] -> Failure: Value overflow", name)};
    const std::string failureTrace4 {fmt::format("[{}] -> Failure: Couldn't create int from parameter", name)};

    // Return result
    return base::Term<base::EngineOp>::create(
        name,
        [=, targetField = std::move(targetField), parameter = std::move(parameters[0])](
            base::Event event) -> base::result::Result<base::Event>
        {
            int64_t IntResolvedParameter;
            // Check parameter
            if (helper::base::Parameter::Type::REFERENCE == parameter.m_type)
            {
                const auto paramValue = event->getInt64(parameter.m_value);
                if (paramValue.has_value())
                {
                    IntResolvedParameter = paramValue.value();
                }
                else
                {
                    const auto paramValue = event->getInt(parameter.m_value);
                    if (!paramValue.has_value())
                    {
                        return base::result::makeFailure(
                            event, (!event->exists(parameter.m_value) ? failureTrace1 : failureTrace2));
                    }
                    IntResolvedParameter = paramValue.value();
                }
            }
            else
            {
                try
                {
                    IntResolvedParameter = std::stoi(parameter.m_value);
                }
                catch (const std::exception& e)
                {
                    return base::result::makeFailure(event, failureTrace4);
                }
            }

            if (IntResolvedParameter < 0 || IntResolvedParameter > std::numeric_limits<int64_t>::max())
            {
                return base::result::makeFailure(event, failureTrace3);
            }

            date::sys_time<std::chrono::seconds> tp {std::chrono::seconds {IntResolvedParameter}};
            auto result = date::format("%Y-%m-%dT%H:%M:%SZ", tp);

            event->setString(result, targetField);
            return base::result::makeSuccess(event, successTrace);
        });
}

HelperBuilder getOpBuilderHelperDateFromEpochTime(std::shared_ptr<schemf::ISchema> schema)
{
    return [schema](const std::string& targetField,
                    const std::string& rawName,
                    const std::vector<std::string>& rawParameters,
                    std::shared_ptr<defs::IDefinitions> definitions)
    {
        return opBuilderHelperDateFromEpochTime(targetField, rawName, rawParameters, definitions, schema);
    };
}

//*************************************************
//*              Checksum and hash                *
//*************************************************

// field: +sha1/<string1>|<string_reference1>
base::Expression opBuilderHelperHashSHA1(const std::string& targetField,
                                         const std::string& rawName,
                                         const std::vector<std::string>& rawParameters,
                                         std::shared_ptr<defs::IDefinitions> definitions)
{
    const auto parameters = helper::base::processParameters(rawName, rawParameters, definitions);

    // Assert expected minimun number of parameters
    helper::base::checkParametersSize(rawName, parameters, 1);
    // Format name for the tracer
    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    // Tracing
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};
    const std::string failureTrace1 {fmt::format(TRACE_REFERENCE_NOT_FOUND, name, parameters[0].m_value)};
    const std::string failureTrace2 {fmt::format(TRACE_REFERENCE_TYPE_IS_NOT, "string", name, parameters[0].m_value)};
    const std::string failureTrace3 {fmt::format("[{}] -> Failure: Couldn't create HASH from string", name)};

    // Return Term
    return base::Term<base::EngineOp>::create(
        name,
        [=, targetField = std::move(targetField), parameter = std::move(parameters[0])](
            base::Event event) -> base::result::Result<base::Event>
        {
            std::string resolvedParameter;
            // Check parameter
            if (helper::base::Parameter::Type::REFERENCE == parameter.m_type)
            {
                const auto paramValue = event->getString(parameter.m_value);
                if (!paramValue.has_value())
                {
                    return base::result::makeFailure(
                        event, (!event->exists(parameter.m_value) ? failureTrace1 : failureTrace2));
                }
                resolvedParameter = paramValue.value();
            }
            else
            {
                resolvedParameter = parameter.m_value;
            }

            const auto resultHash = hashStringSHA1(resolvedParameter);
            if (!resultHash.has_value())
            {
                return base::result::makeFailure(event, failureTrace3);
            }
            event->setString(resultHash.value(), targetField);
            return base::result::makeSuccess(event, successTrace);
        });
}

//*************************************************
//*                  bit functions                *
//*************************************************
base::Expression opBuilderHelperBitmaskToTable(const std::string& targetField,
                                               const std::string& rawName,
                                               const std::vector<std::string>& rawParameters,
                                               std::shared_ptr<defs::IDefinitions> definitions,
                                               std::shared_ptr<schemf::ISchema> schema)
{

    const auto parameters = helper::base::processParameters(rawName, rawParameters, definitions, false);
    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);
    const std::string throwTrace {"Engine bitmask32 to table builder: "};

    // Verify parameters size
    helper::base::checkParametersMinSize(rawName, parameters, 2);

    // Check isMSB
    bool isMSB = (parameters.size() == 3 && parameters[2].m_value == "MSB");
    if (parameters.size() == 3 && parameters[2].m_value != "LSB" && !isMSB)
    {
        throw std::runtime_error(
            fmt::format(throwTrace + "Expected LSB or MSB as third parameter, got {}", parameters[2].m_value));
    }

    // Verify the schema fields
    if (schema->hasField(targetField) && schema->getType(targetField) != json::Json::Type::Array)
    {
        throw std::runtime_error(throwTrace + "failed schema validation: Target field must be an array.");
    }

    if (parameters[0].m_type == helper::base::Parameter::Type::REFERENCE && schema->hasField(parameters[0].m_value)
        && schema->getType(parameters[0].m_value) != json::Json::Type::Object)
    {
        throw std::runtime_error(throwTrace + "failed schema validation: Map field must be an object.");
    }

    if (parameters[1].m_type == helper::base::Parameter::Type::REFERENCE && schema->hasField(parameters[1].m_value))
    {
        auto type = schema->getType(parameters[1].m_value);
        if (type != json::Json::Type::String && type != json::Json::Type::Number)
        {
            throw std::runtime_error(throwTrace + "failed schema validation: Mask field must be a string or number.");
        }
    }

    // Tracing
    const std::string successTrace {fmt::format(TRACE_SUCCESS, name)};
    const std::string failureTrace1 {fmt::format("[{}] -> Failure: Expected number as mask", name)};
    const std::string failureTrace2 {
        fmt::format("[{}] -> Failure: Expected same type for all values on the bit map", name)};
    const std::string failureTrace3 {fmt::format(
        "[{}] -> Failure: Reference  to mask '{}' not found or not a string or number", name, parameters[1].m_value)};

    /*****************************
     *  MAP
     ***************************** */
    std::map<uint32_t, json::Json> buildedMap;
    if (parameters[0].m_type == helper::base::Parameter::Type::VALUE)
    {
        json::Json jDefinition;

        try
        {
            jDefinition = json::Json {parameters[0].m_value.c_str()};
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(
                fmt::format("{}Expected object as first parameter, got {}", throwTrace, parameters[0].m_value));
        }

        if (!jDefinition.isObject())
        {
            throw std::runtime_error(
                fmt::format("{}Expected object as first parameter, got {}", throwTrace, parameters[0].m_value));
        }

        auto jMap = jDefinition.getObject().value();
        json::Json::Type mapValueType {};
        bool isTypeSet = false;

        for (auto& [key, value] : jMap)
        {
            uint32_t index = 0;

            if (key.empty())
            {
                throw std::runtime_error("Empty key provided");
            }

            if (key[0] == '-')
            {
                throw std::runtime_error(fmt::format(
                    "{}Expected positive number as decimal/hexa (0x)/binary (0b) as key, got {}", throwTrace, key));
            }

            size_t pos = 0;
            int base = (key.compare(0, 2, "0x") == 0)   ? (pos = 2, 16)
                       : (key.compare(0, 2, "0b") == 0) ? (pos = 2, 2)
                                                        : 10;

            try
            {
                index = std::stoul(key.substr(pos), nullptr, base);
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error(
                    fmt::format("{}Expected number as decimal/hexa (0x)/binary (0b) as key, got {}", throwTrace, key));
            }

            if (!isTypeSet)
            {
                mapValueType = value.type();
                isTypeSet = true;
            }
            else if (mapValueType != value.type())
            {
                throw std::runtime_error(fmt::format("{}Expected same type for all values on the bit map", throwTrace));
            }

            buildedMap[index] = std::move(value);
        }
    }

    // Function that get the value from the builded map, returns nullopt if not found
    auto getValueFromBuildedMap = [buildedMap](const uint32_t pos) -> std::optional<json::Json>
    {
        auto it = buildedMap.find(pos);
        if (it != buildedMap.end())
        {
            return it->second;
        }
        return std::nullopt;
    };

    // Function that get the value from the event map, returns nullopt if not found (Dont check types)
    auto getValueFromEventMap =
        [](const base::Event& event, const uint32_t pos, const std::string& refMap) -> std::optional<json::Json>
    {
        auto path = refMap + "/" + std::to_string(pos);
        const auto value = event->getJson(path);
        if (value.has_value())
        {
            return value.value();
        }
        return std::nullopt;
    };

    /*****************************
     *  Mask
     ***************************** */
    // If mask is a definition/value, then get the mask
    uint32_t paramMask = 0;
    if (helper::base::Parameter::Type::VALUE == parameters[1].m_type)
    {
        try
        {
            paramMask = std::stoul(parameters[1].m_value, nullptr, 16) & 0x0000FFFF;
        }
        catch (const std::exception&)
        {
            throw std::runtime_error(
                fmt::format(throwTrace + "Expected number as mask, got {}", parameters[1].m_value));
        }
    }
    // If mask is a reference, then use the lambda to get the mask, if not found or not a number, return string failure
    auto getMaskFromRef = [failureTrace1,
                           failureTrace3](const base::Event& event,
                                          const std::string& srcMask) -> std::variant<uint32_t, std::string>
    {
        uint32_t mask = 0;
        // If is a string
        const auto maskStr = event->getString(srcMask);
        if (maskStr.has_value())
        {
            try
            {
                mask = std::stoul(maskStr.value(), nullptr, 16) & 0x0000FFFF;
                return mask;
            }
            catch (const std::exception&)
            {
                return failureTrace1;
            }
        }

        // If is a number 32 bits
        const auto maskInt = event->getInt(srcMask);
        if (maskInt.has_value())
        {
            mask = maskInt.value() & 0x0000FFFF;
            return mask;
        }

        // If is a number 64 bits
        const auto maskInt64 = event->getInt64(srcMask);
        if (maskInt64.has_value())
        {
            mask = maskInt64.value() & 0x0000FFFF;
            return mask;
        }

        return failureTrace3;
    };

    // Return Term
    return base::Term<base::EngineOp>::create(
        name,
        [targetField = std::move(targetField),
         srcMap = std::move(parameters[0]),
         maskField = std::move(parameters[1]),
         isMSB,
         paramMask,
         getMaskFromRef,
         getValueFromBuildedMap,
         getValueFromEventMap,
         successTrace,
         failureTrace2](const base::Event& event) -> base::result::Result<base::Event>
        {
            // Get mask in hexa or decimal
            uint32_t mask = paramMask;
            if (maskField.m_type == helper::base::Parameter::Type::REFERENCE)
            {
                auto res = getMaskFromRef(event, maskField.m_value);
                if (std::holds_alternative<std::string>(res))
                {
                    return base::result::makeFailure(event, std::move(std::get<std::string>(res)));
                }
                mask = std::get<uint32_t>(res);
            }

            bool usePrebuildedMap = srcMap.m_type == helper::base::Parameter::Type::VALUE;

            json::Json::Type mapValueType;
            bool isTypeSet = false;

            for (uint32_t pos = 0; pos <= 31; ++pos)
            {
                uint32_t bitPos = isMSB ? (0x1 << 31) >> pos : 0x1 << pos;

                if (bitPos & mask)
                {
                    // Determine the appropriate map and get the value.
                    auto value = usePrebuildedMap ? getValueFromBuildedMap(0x1 << pos)
                                                  : getValueFromEventMap(event, 0x1 << pos, srcMap.m_value);

                    if (value.has_value())
                    {
                        if (!isTypeSet)
                        {
                            mapValueType = value.value().type();
                            isTypeSet = true;
                        }
                        else if (mapValueType != value.value().type())
                        {
                            return base::result::makeFailure(event, failureTrace2);
                        }
                        event->appendJson(*value, targetField);
                    }
                }
            }

            return base::result::makeSuccess(event, successTrace);
        });
}

HelperBuilder getOpBuilderHelperBitmaskToTable(std::shared_ptr<schemf::ISchema> schema)
{
    return [schema](const std::string& targetField,
                    const std::string& rawName,
                    const std::vector<std::string>& rawParameters,
                    std::shared_ptr<defs::IDefinitions> definitions)
    {
        return opBuilderHelperBitmaskToTable(targetField, rawName, rawParameters, definitions, schema);
    };
}

//*************************************************
//*                  Definition                   *
//*************************************************

base::Expression opBuilderHelperGetValue(const std::string& targetField,
                                         const std::string& rawName,
                                         const std::vector<std::string>& rawParameters,
                                         std::shared_ptr<defs::IDefinitions> definitions,
                                         std::shared_ptr<schemf::ISchema> schema,
                                         bool isMerge)
{
    auto parameters {helper::base::processParameters(rawName, rawParameters, definitions, false)};
    helper::base::checkParametersSize(rawName, parameters, 2);
    helper::base::checkParameterType(rawName, parameters[1], helper::base::Parameter::Type::REFERENCE);

    const auto name = helper::base::formatHelperName(rawName, targetField, parameters);

    // If key field is a schema field, the value should be a string
    if (schema->hasField(DotPath::fromJsonPath(parameters[1].m_value))
        && (schema->getType(DotPath::fromJsonPath(parameters[1].m_value)) != json::Json::Type::String))
    {
        throw std::runtime_error(
            fmt::format("Engine helper builder: [{}] failed schema validation: Field '{}' value is not a string",
                        name,
                        parameters[1].m_value));
    }

    std::optional<json::Json> definitionObject {std::nullopt};

    if (helper::base::Parameter::Type::VALUE == parameters[0].m_type)
    {
        try
        {
            definitionObject = json::Json {parameters[0].m_value.c_str()};
        }
        catch (std::runtime_error& e)
        {
            throw std::runtime_error(fmt::format(
                "Engine helper builder: [{}] Definition '{}' has an invalid type", name, parameters[0].m_value));
        }

        if (!definitionObject->isObject())
        {
            throw std::runtime_error(fmt::format(
                "Engine helper builder: [{}] Definition '{}' is not an object", name, parameters[0].m_value));
        }
    }

    // Tracing
    const std::string successTrace {fmt::format("[{}] -> Success", name)};

    const std::string failureTrace1 {
        fmt::format("[{}] -> Failure: Reference '{}' not found", name, parameters[1].m_value)};
    const std::string failureTrace2 {
        fmt::format("[{}] -> Failure: Parameter '{}' not found", name, parameters[0].m_value)};
    const std::string failureTrace3 {
        fmt::format("[{}] -> Failure: Parameter '{}' has an invalid type", name, parameters[0].m_value)};
    const std::string failureTrace4 {
        fmt::format("[{}] -> Failure: Parameter '{}' is not an object", name, parameters[0].m_value)};
    const std::string failureTrace5 {
        fmt::format("[{}] -> Failure: Object '{}' does not contain '{}'", name, parameters[0].m_value, targetField)};
    const std::string failureTrace6 {
        fmt::format("[{}] -> Failure: fields dont match type or type is not supported (array or object)", name)};

    // Return Term
    return base::Term<base::EngineOp>::create(
        name,
        [=,
         targetField = std::move(targetField),
         parameter = std::move(parameters[0]),
         key = std::move(parameters[1].m_value),
         definitionObject = std::move(definitionObject)](base::Event event) -> base::result::Result<base::Event>
        {
            // Get key
            std::string resolvedKey;
            const auto value = event->getString(key);
            if (value)
            {
                resolvedKey = value.value();
            }
            else
            {
                return base::result::makeFailure(event, failureTrace1);
            }

            auto pointerPath = json::Json::formatJsonPath(resolvedKey);

            // Get object
            std::optional<json::Json> resolvedValue {std::nullopt};

            if (helper::base::Parameter::Type::REFERENCE == parameter.m_type)
            {
                // Parameter is a reference
                const auto resolvedJson = event->getJson(parameter.m_value);
                if (!resolvedJson.has_value())
                {
                    return base::result::makeFailure(
                        event, (!event->exists(parameter.m_value)) ? failureTrace2 : failureTrace3);
                }
                if (!resolvedJson->isObject())
                {
                    return base::result::makeFailure(event, failureTrace4);
                }
                resolvedValue = resolvedJson->getJson(pointerPath);
            }
            else
            {
                // Parameter is a definition
                resolvedValue = definitionObject->getJson(pointerPath);
            }

            // Check if object contains the key
            if (!resolvedValue.has_value())
            {
                return base::result::makeFailure(event, failureTrace5);
            }

            if (!isMerge)
            {
                event->set(targetField, resolvedValue.value());
            }
            else
            {
                try
                {
                    event->merge(json::NOT_RECURSIVE, resolvedValue.value(), targetField);
                }
                catch (std::runtime_error& e)
                {
                    return base::result::makeFailure(event, failureTrace6);
                }
                return base::result::makeSuccess(event, successTrace);
            }

            return base::result::makeSuccess(event, successTrace);
        });
}

// <field>: +get_value/$<definition_object>|$<object_reference>/$<key>
HelperBuilder getOpBuilderHelperGetValue(std::shared_ptr<schemf::ISchema> schema)
{
    return [schema](const std::string& targetField,
                    const std::string& rawName,
                    const std::vector<std::string>& rawParameters,
                    std::shared_ptr<defs::IDefinitions> definitions)
    {
        return opBuilderHelperGetValue(targetField, rawName, rawParameters, definitions, schema);
    };
}

// <field>: +merge_value/$<definition_object>|$<object_reference>/$<key>
HelperBuilder getOpBuilderHelperMergeValue(std::shared_ptr<schemf::ISchema> schema)
{
    return [schema](const std::string& targetField,
                    const std::string& rawName,
                    const std::vector<std::string>& rawParameters,
                    std::shared_ptr<defs::IDefinitions> definitions)
    {
        return opBuilderHelperGetValue(targetField, rawName, rawParameters, definitions, schema, true);
    };
}

} // namespace builder::internals::builders
