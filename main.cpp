#include "cnomlite.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <sstream>

// ANSI Color Utility
class ANSIColor {
public:
    static constexpr const char* RESET = "\033[0m";
    static constexpr const char* RED = "\033[31m";
    static constexpr const char* GREEN = "\033[32m";
    static constexpr const char* BLUE = "\033[34m";
    static constexpr const char* CYAN = "\033[36m";
    static constexpr const char* MAGENTA = "\033[35m";
    static constexpr const char* YELLOW = "\033[33m";
    static constexpr const char* BOLD = "\033[1m";

    static std::string apply(const std::string& text, const char* color) {
        return std::string(color) + text + RESET;
    }
};

void register_command_case_insensitive(
    std::unordered_map<std::string, std::function<void()>>& map,
    const std::string& name,
    const std::function<void()>& command);
void alias(std::unordered_map<std::string, std::function<void()>>& map, const char* existing, const char* alias_name);

namespace cbasic {

// The data stack for CBASIC
std::vector<int> data_stack;

// The environment (dictionary of words/commands)
std::unordered_map<std::string, std::function<void()>> environment;

// Helper: Print the stack contents
void print_stack() {
    std::cout << ANSIColor::apply("Stack: ", ANSIColor::GREEN);
    for (const auto& item : data_stack) {
        std::cout << item << " ";
    }
    std::cout << std::endl;
}

// Basic words for CBASIC
void add() {
    if (data_stack.size() < 2) {
        std::cout << ANSIColor::apply("Error: ADD requires at least two values on the stack.", ANSIColor::RED) << std::endl;
        return;
    }
    int b = data_stack.back(); data_stack.pop_back();
    int a = data_stack.back(); data_stack.pop_back();
    data_stack.push_back(a + b);
}

void subtract() {
    if (data_stack.size() < 2) {
        std::cout << ANSIColor::apply("Error: SUBTRACT requires at least two values on the stack.", ANSIColor::RED) << std::endl;
        return;
    }
    int b = data_stack.back(); data_stack.pop_back();
    int a = data_stack.back(); data_stack.pop_back();
    data_stack.push_back(a - b);
}

void push(int value) {
    data_stack.push_back(value);
}

// Parsing and executing commands
void execute_word(const std::string& word) {
    if (environment.find(word) != environment.end()) {
        environment[word]();
    } else {
        std::cout << ANSIColor::apply("Error: Unknown command '" + word + "'", ANSIColor::RED) << std::endl;
    }
}

void execute_line(const std::string& line) {
    using namespace cnomlite;

    // Define a parser for a word: one or more non-whitespace characters
    auto word_parser = many1(make_parser<char>([](const std::string& input) -> ParseResult<char> {
        if (!input.empty() && !std::isspace(static_cast<unsigned char>(input[0]))) {
            return ParseSuccess<char>{input[0], input.substr(1)};
        }
        return "Expected non-whitespace character.";
    }));

    // Parse the line into words
    auto split_parser = sep_by(map(word_parser, [](const std::vector<char>& chars) {
        return std::string(chars.begin(), chars.end());
    }), whitespace);

    auto result = split_parser(line);
    if (std::holds_alternative<ParseSuccess<std::vector<std::string>>>(result)) {
        auto success = std::get<ParseSuccess<std::vector<std::string>>>(result);

        for (const auto& word : success.value) {
            try {
                // Try to convert to an integer and push it onto the stack
                int value = std::stoi(word);
                push(value);
            } catch (const std::invalid_argument&) {
                // If it's not an integer, treat it as a command
                execute_word(word);
            }
        }
    } else {
        std::cout << ANSIColor::apply("Parse error: ", ANSIColor::RED) << std::get<std::string>(result) << std::endl;
    }
}

} // namespace cbasic

// Startup Banner
void print_startup_banner() {
    std::cout << ANSIColor::apply("========================================", ANSIColor::CYAN) << std::endl;
    std::cout << ANSIColor::apply("        WELCOME TO CBASIC REPL", ANSIColor::GREEN) << std::endl;
    std::cout << ANSIColor::apply("        A Very Cool Experience", ANSIColor::MAGENTA) << std::endl;
    std::cout << ANSIColor::apply("========================================", ANSIColor::CYAN) << std::endl;
    std::cout << ANSIColor::apply("Type 'EXIT' to quit or 'PRINT' to see the stack.", ANSIColor::YELLOW) << std::endl;
    std::cout << std::endl;
}

int main() {
    using namespace cbasic;

    // Initialize the CBASIC environment
    register_command_case_insensitive(environment, "PRINT", print_stack);
    register_command_case_insensitive(environment, "ADD", add);
    register_command_case_insensitive(environment, "SUB", subtract);
    alias(environment, "PRINT", "P");
    alias(environment, "ADD", "+");
    alias(environment, "SUB", "-");


    print_startup_banner();


    std::string line;
    while (true) {
        std::cout << ANSIColor::apply("CBASIC> ", ANSIColor::BLUE) << std::flush;
        std::getline(std::cin, line);

        if (line == "EXIT") {
            std::cout << ANSIColor::apply("Goodbye!", ANSIColor::GREEN) << std::endl;
            break;
        }

        execute_line(line);
    }

    return 0;
}

void register_command_case_insensitive(
    std::unordered_map<std::string, std::function<void()>>& map,
    const std::string& name,
    const std::function<void()>& command)
{
    // Add the original (assume it's case-sensitive by default)
    map[name] = command;

    // Add the lowercase version
    std::string lowercase_name = name;
    std::transform(lowercase_name.begin(), lowercase_name.end(), lowercase_name.begin(), ::tolower);
    map[lowercase_name] = command;

    // Add the uppercase version
    std::string uppercase_name = name;
    std::transform(uppercase_name.begin(), uppercase_name.end(), uppercase_name.begin(), ::toupper);
    map[uppercase_name] = command;
}

// Alias Function
void alias(std::unordered_map<std::string, std::function<void()>>& map, const char* existing, const char* alias_name) {
    std::string existing_str(existing);
    std::string alias_str(alias_name);
    if (map.find(existing_str) != map.end()) {
        map[alias_str] = map[existing_str];
    } else {
        std::cout << ANSIColor::apply("Error: Unknown command '" + existing_str + "'", ANSIColor::RED) << std::endl;
    }
}
