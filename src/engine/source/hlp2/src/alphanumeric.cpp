#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include <hlp/base.hpp>
#include <hlp/hlp.hpp>
#include <hlp/parsec.hpp>
#include <json/json.hpp>

namespace hlp
{
parsec::Parser<json::Json> getAlphanumericParser(const std::string& name, Stop, Options lst)
{
    if (!lst.empty())
    {
        throw std::runtime_error("alphanumeric parser doesn't accept parameters");
    }

    return [name](std::string_view text, int index)
    {
        auto res = internal::eofError<json::Json>(text, index);
        if (res.has_value())
        {
            return res.value();
        }

        auto end = std::find_if(text.begin() + index, text.end(), [](char const& c) { return !std::isalnum(c); });

        auto endPos = end - text.begin();

        if (end == text.end())
        {
            json::Json alphaNumeric;
            alphaNumeric.setString(std::string {text.substr(index, endPos - index)});

            return parsec::makeSuccess<json::Json>(std::move(alphaNumeric), endPos);
        }
        else
        {
            return parsec::makeError<json::Json>(fmt::format("{}: Expected alphanumeric input", name), index);
        }
    };
}
} // namespace hlp
