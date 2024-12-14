#pragma once

#include <functional>
#include <string>
#include <variant>
#include <optional>
#include <cctype>
#include <vector>
#include <iostream>
#include <type_traits>

namespace cnomlite {

// -----------------------------
// ParseResult and ParseSuccess
// -----------------------------
    template <typename T>
    struct ParseSuccess {
        T value;
        std::string remaining;
    };

    template <typename T>
    using ParseResult = std::variant<ParseSuccess<T>, std::string>;

// -----------------------------
// Parser definition
// -----------------------------
// A Parser<T> holds a std::function that takes a string and returns ParseResult<T>.
    template <typename T>
    struct Parser {
        using result_type = T;
        std::function<ParseResult<T>(const std::string&)> f;

        ParseResult<T> operator()(const std::string& input) const {
            return f(input);
        }
    };

// Helper function to build a Parser<T> from a lambda
    template <typename T, typename F>
    Parser<T> make_parser(F&& fn) {
        return Parser<T>{std::function<ParseResult<T>(const std::string&)>(std::forward<F>(fn))};
    }

// -----------------------------
// Basic Parsers
// -----------------------------
    inline auto any_char = make_parser<char>([](const std::string& input) -> ParseResult<char> {
        if (input.empty()) {
            return "Unexpected end of input";
        }
        return ParseSuccess<char>{ input[0], input.substr(1) };
    });

    inline auto char_p(char expected) {
        return make_parser<char>([expected](const std::string& input) -> ParseResult<char> {
            if (!input.empty() && input[0] == expected) {
                return ParseSuccess<char>{ expected, input.substr(1) };
            }
            std::string error = "Expected '";
            error += expected;
            error += "', found '";
            error += (input.empty() ? "EOF" : std::string(1, input[0]));
            error += "'";
            return error;
        });
    }

    inline auto string_p(const std::string& expected) {
        return make_parser<std::string>([expected](const std::string& input) -> ParseResult<std::string> {
            if (input.size() >= expected.size() && input.substr(0, expected.size()) == expected) {
                return ParseSuccess<std::string>{ expected, input.substr(expected.size()) };
            }
            std::string found = input.size() >= expected.size() ? input.substr(0, expected.size()) : input;
            return "Expected \"" + expected + "\", found \"" + found + "\"";
        });
    }

    inline auto digit = make_parser<char>([](const std::string& input) -> ParseResult<char> {
        if (!input.empty() && std::isdigit(static_cast<unsigned char>(input[0]))) {
            return ParseSuccess<char>{ input[0], input.substr(1) };
        }
        std::string error = "Expected digit, found '";
        error += (input.empty() ? "EOF" : std::string(1, input[0]));
        error += "'";
        return error;
    });

    inline auto whitespace_char = make_parser<char>([](const std::string& input) -> ParseResult<char> {
        if (!input.empty() && std::isspace(static_cast<unsigned char>(input[0]))) {
            return ParseSuccess<char>{ input[0], input.substr(1) };
        }
        std::string error = "Expected whitespace, found '";
        error += (input.empty() ? "EOF" : std::string(1, input[0]));
        error += "'";
        return error;
    });

// -----------------------------
// Combinators
// -----------------------------

// map: Transform the result of a parser
    template <typename ParserA, typename F>
    auto map(ParserA p, F f) {
        using A = typename ParserA::result_type;
        using B = std::invoke_result_t<F,A>;
        return make_parser<B>([p,f](const std::string& input) -> ParseResult<B> {
            auto r = p(input);
            if (auto ps = std::get_if<ParseSuccess<A>>(&r)) {
                return ParseSuccess<B>{ f(ps->value), ps->remaining };
            }
            return std::get<std::string>(r);
        });
    }

// bind: Chains parsers, second depends on first result
    template <typename ParserA, typename F>
    auto bind(ParserA p, F f) {
        using A = typename ParserA::result_type;
        using ParserB = std::invoke_result_t<F,A>;
        using B = typename ParserB::result_type;
        return make_parser<B>([p,f](const std::string& input) -> ParseResult<B> {
            auto r = p(input);
            if (auto ps = std::get_if<ParseSuccess<A>>(&r)) {
                auto next = f(ps->value);
                return next(ps->remaining);
            }
            return std::get<std::string>(r);
        });
    }

// sequence: run first, then second
    template <typename ParserA, typename ParserB>
    auto sequence(ParserA p1, ParserB p2) {
        using A = typename ParserA::result_type;
        using B = typename ParserB::result_type;
        return make_parser<std::pair<A,B>>([p1,p2](const std::string& input) -> ParseResult<std::pair<A,B>> {
            auto r1 = p1(input);
            if (auto ps1 = std::get_if<ParseSuccess<A>>(&r1)) {
                auto r2 = p2(ps1->remaining);
                if (auto ps2 = std::get_if<ParseSuccess<B>>(&r2)) {
                    return ParseSuccess<std::pair<A,B>>{{ps1->value, ps2->value}, ps2->remaining};
                }
                return std::get<std::string>(r2);
            }
            return std::get<std::string>(r1);
        });
    }

// choice: try multiple parsers, return first success
    template <typename T>
    auto choice(const std::vector<Parser<T>>& parsers) {
        return make_parser<T>([parsers](const std::string& input) -> ParseResult<T> {
            std::string errors;
            for (auto& parser : parsers) {
                auto r = parser(input);
                if (std::holds_alternative<ParseSuccess<T>>(r)) {
                    return r;
                }
                errors += std::get<std::string>(r) + " | ";
            }
            if (!errors.empty()) {
                errors = errors.substr(0, errors.size() - 3);
            }
            return errors.empty() ? std::string("No alternatives matched") : errors;
        });
    }

// many: zero or more occurrences
    template <typename ParserT>
    auto many(ParserT p) {
        using T = typename ParserT::result_type;
        return make_parser<std::vector<T>>([p](const std::string& input) -> ParseResult<std::vector<T>> {
            std::vector<T> results;
            std::string remaining = input;
            while (true) {
                auto r = p(remaining);
                if (auto ps = std::get_if<ParseSuccess<T>>(&r)) {
                    results.push_back(ps->value);
                    remaining = ps->remaining;
                } else {
                    break;
                }
            }
            return ParseSuccess<std::vector<T>>{results, remaining};
        });
    }

// many1: one or more occurrences
    template <typename ParserT>
    auto many1(ParserT p) {
        using T = typename ParserT::result_type;
        return make_parser<std::vector<T>>([p](const std::string& input) -> ParseResult<std::vector<T>> {
            auto r = many(p)(input);
            if (auto ps = std::get_if<ParseSuccess<std::vector<T>>>(&r)) {
                if (ps->value.empty()) {
                    return std::string("Expected at least one occurrence");
                }
                return r;
            }
            return r;
        });
    }

// optional_p: zero or one occurrence
    template <typename ParserT>
    auto optional_p(ParserT p) {
        using T = typename ParserT::result_type;
        return make_parser<std::optional<T>>([p](const std::string& input) -> ParseResult<std::optional<T>> {
            auto r = p(input);
            if (auto ps = std::get_if<ParseSuccess<T>>(&r)) {
                return ParseSuccess<std::optional<T>>{ps->value, ps->remaining};
            }
            // no consumption on failure
            return ParseSuccess<std::optional<T>>{std::nullopt, input};
        });
    }

// sep_by: zero or more occurrences separated by a separator
    template <typename ParserT, typename SepParser>
    auto sep_by(ParserT element, SepParser separator) {
        using T = typename ParserT::result_type;
        return make_parser<std::vector<T>>([element,separator](const std::string& input) -> ParseResult<std::vector<T>> {
            std::vector<T> results;
            std::string remaining = input;
            while (true) {
                auto elem_r = element(remaining);
                if (auto ps_elem = std::get_if<ParseSuccess<T>>(&elem_r)) {
                    results.push_back(ps_elem->value);
                    remaining = ps_elem->remaining;
                    auto sep_r = separator(remaining);
                    if (std::holds_alternative<ParseSuccess<typename SepParser::result_type>>(sep_r)) {
                        auto ps_sep = std::get_if<ParseSuccess<typename SepParser::result_type>>(&sep_r);
                        remaining = ps_sep->remaining;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
            return ParseSuccess<std::vector<T>>{results, remaining};
        });
    }

// -----------------------------
// Utility and Higher-level Parsers
// -----------------------------
    inline auto whitespace = many(whitespace_char);

    template <typename ParserT>
    auto skip_ws(ParserT p) {
        using T = typename ParserT::result_type;
        // bind whitespace, then ignore its result and return p
        return bind(whitespace, [p](const std::vector<char>&) {
            return p;
        });
    }

// integer parser: one or more digits -> int
    inline auto integer_p = map(many1(digit), [](const std::vector<char>& digits) -> int {
        int value = 0;
        for (char c : digits) {
            value = value * 10 + (c - '0');
        }
        return value;
    });

#ifdef CNOMLITE_EXAMPLE

    int main() {
        using namespace cnomlite;

        // Parser for '+' with optional whitespace around it
        auto plus_p = map(
                sequence(
                        skip_ws(char_p('+')),
                        skip_ws(integer_p)
                ),
                [](const std::pair<char,int>& p) -> int {
                    return p.second;
                }
        );

        // Expression parser: integer + integer -> sum
        auto expr_p = make_parser<int>([&](const std::string& input) -> ParseResult<int> {
            auto seq = sequence(integer_p, plus_p);
            auto result = seq(input);
            if (std::holds_alternative<ParseSuccess<std::pair<int,int>>>(result)) {
                auto s = std::get<ParseSuccess<std::pair<int,int>>>(result);
                return ParseSuccess<int>{ s.value.first + s.value.second, s.remaining };
            }
            return std::get<std::string>(result);
        });

        std::vector<std::string> test_inputs = {
                "123+456",
                "  789 +  10 ",
                "42+",
                "+100",
                "abc+def"
        };

        for (const auto& input : test_inputs) {
            std::cout << "Parsing: \"" << input << "\"\n";
            auto r = expr_p(input);
            if (auto ps = std::get_if<ParseSuccess<int>>(&r)) {
                std::cout << "Parsed result: " << ps->value << "\n";
                std::cout << "Remaining: \"" << ps->remaining << "\"\n";
            } else {
                std::cout << "Parse error: " << std::get<std::string>(r) << "\n";
            }
            std::cout << "------------------------\n";
        }

        // Example: parse comma-separated integers
        auto comma = skip_ws(char_p(','));
        auto int_list = sep_by(integer_p, comma);

        auto list_result = int_list("10, 20, 30,40");
        if (auto ps = std::get_if<ParseSuccess<std::vector<int>>>(&list_result)) {
            std::cout << "Parsed integers:";
            for (auto num : ps->value) {
                std::cout << " " << num;
            }
            std::cout << "\nRemaining: \"" << ps->remaining << "\"\n";
        } else {
            std::cout << "Parse error: " << std::get<std::string>(list_result) << "\n";
        }

        return 0;
    }

#endif // CNOMLITE_EXAMPLE

} // namespace cnomlite
