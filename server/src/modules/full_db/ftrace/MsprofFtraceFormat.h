/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
#ifndef PROFILER_SERVER_MSPROFFTRACEFORMAT_H
#define PROFILER_SERVER_MSPROFFTRACEFORMAT_H

#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace Dic::Module::FullDb::FtraceFormat {
// Only bounded integer constants and these operators are accepted. No C text is executed.
class ConstantExpression {
  public:
    struct Value {
        uint64_t number = 0;
        std::optional<uint64_t> beforeSubtractOne;
    };

    explicit ConstantExpression(std::string_view text) : input(text) {}
    std::optional<Value> Parse() {
        Value result;
        if (input.size() > MAX_LENGTH || !Or(result)) {
            return std::nullopt;
        }
        Space();
        return !failed && position == input.size() ? std::optional<Value>{result} : std::nullopt;
    }

  private:
    static constexpr size_t MAX_LENGTH = 4096;
    static constexpr size_t MAX_TOKENS = 256;
    static constexpr size_t MAX_DEPTH = 16;
    std::string_view input;
    size_t position = 0;
    size_t tokens = 0;
    size_t depth = 0;
    bool failed = false;

    void Space() {
        while (position < input.size() && std::isspace(static_cast<unsigned char>(input[position]))) {
            ++position;
        }
    }
    bool Take(std::string_view token) {
        Space();
        if (input.substr(position, token.size()) != token) {
            return false;
        }
        // Logical operators are outside this constant-expression grammar.
        if (token.size() == 1 && (token == "|" || token == "&") && position + 1 < input.size() &&
            input[position + 1] == token[0]) {
            failed = true;
            return false;
        }
        position += token.size();
        failed = failed || ++tokens > MAX_TOKENS;
        return !failed;
    }
    bool Or(Value &value) {
        if (!And(value)) {
            return false;
        }
        while (Take("|")) {
            Value right;
            if (!And(right)) {
                return false;
            }
            value = {value.number | right.number, std::nullopt};
        }
        return !failed;
    }
    bool And(Value &value) {
        if (!Shift(value)) {
            return false;
        }
        while (Take("&")) {
            Value right;
            if (!Shift(right)) {
                return false;
            }
            value = {value.number & right.number, std::nullopt};
        }
        return !failed;
    }
    bool Shift(Value &value) {
        if (!Sum(value)) {
            return false;
        }
        for (;;) {
            const bool left = Take("<<");
            if (!left && !Take(">>")) {
                return !failed;
            }
            Value right;
            if (!Sum(right) || right.number >= 64 ||
                (left && value.number > (std::numeric_limits<uint64_t>::max() >> right.number))) {
                return false;
            }
            value = {left ? value.number << right.number : value.number >> right.number, std::nullopt};
        }
    }
    bool Sum(Value &value) {
        if (!Primary(value)) {
            return false;
        }
        for (;;) {
            const bool plus = Take("+");
            if (!plus && !Take("-")) {
                return !failed;
            }
            Value right;
            if (!Primary(right) || (plus && value.number > std::numeric_limits<uint64_t>::max() - right.number) ||
                (!plus && value.number < right.number)) {
                return false;
            }
            const auto previous = value.number;
            value = {plus ? previous + right.number : previous - right.number,
                !plus && right.number == 1 ? std::optional<uint64_t>{previous} : std::nullopt};
        }
    }
    bool Primary(Value &value) {
        if (Take("(")) {
            if (++depth > MAX_DEPTH || !Or(value) || !Take(")")) {
                return false;
            }
            --depth;
            return true;
        }
        Space();
        if (++tokens > MAX_TOKENS || position >= input.size() ||
            !std::isdigit(static_cast<unsigned char>(input[position]))) {
            return false;
        }
        unsigned base = 10;
        if (input[position] == '0') {
            base = 8;
            if (position + 1 < input.size() && (input[position + 1] == 'x' || input[position + 1] == 'X')) {
                base = 16;
                position += 2;
            }
        }
        uint64_t number = 0;
        size_t digits = 0;
        while (position < input.size()) {
            const unsigned char character = static_cast<unsigned char>(input[position]);
            unsigned digit;
            if (character >= '0' && character <= '9') {
                digit = character - '0';
            } else if (character >= 'a' && character <= 'f') {
                digit = character - 'a' + 10;
            } else if (character >= 'A' && character <= 'F') {
                digit = character - 'A' + 10;
            } else {
                break;
            }
            if (digit >= base || number > (std::numeric_limits<uint64_t>::max() - digit) / base) {
                return false;
            }
            number = number * base + digit;
            ++position;
            ++digits;
        }
        if (digits == 0) {
            return false;
        }
        const auto suffixStart = position;
        while (position < input.size() &&
            (input[position] == 'u' || input[position] == 'U' || input[position] == 'l' || input[position] == 'L')) {
            ++position;
            if (position - suffixStart > 3) {
                return false;
            }
        }
        std::string suffix(input.substr(suffixStart, position - suffixStart));
        for (auto &character : suffix) {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        if (!suffix.empty() && suffix != "u" && suffix != "l" && suffix != "ul" && suffix != "lu" && suffix != "ll" &&
            suffix != "ull" && suffix != "llu") {
            return false;
        }
        value = {number, std::nullopt};
        return true;
    }
};

inline bool PowerOfTwo(uint64_t value) { return value != 0 && (value & (value - 1)) == 0; }

inline std::optional<uint64_t> PreemptMask(const std::string &format) {
    if (format.size() > 1024 * 1024) {
        return std::nullopt;
    }
    std::optional<uint64_t> result;
    size_t cursor = 0;
    while ((cursor = format.find("prev_state", cursor)) != std::string::npos) {
        const auto nameStart = cursor;
        cursor += std::string_view("prev_state").size();
        if ((nameStart > 0 &&
                (std::isalnum(static_cast<unsigned char>(format[nameStart - 1])) || format[nameStart - 1] == '_')) ||
            (cursor < format.size() &&
                (std::isalnum(static_cast<unsigned char>(format[cursor])) || format[cursor] == '_'))) {
            continue;
        }
        auto skipSpace = [&](size_t &position) {
            while (position < format.size() && std::isspace(static_cast<unsigned char>(format[position]))) {
                ++position;
            }
        };
        skipSpace(cursor);
        if (cursor >= format.size() || format[cursor] != '&' ||
            (cursor + 1 < format.size() && format[cursor + 1] == '&')) {
            continue;
        }
        ++cursor;
        skipSpace(cursor);
        if (cursor >= format.size() || format[cursor] != '(') {
            continue;
        }
        const auto start = cursor;
        size_t nesting = 0;
        do {
            if (format[cursor] == '(') {
                ++nesting;
            } else if (format[cursor] == ')') {
                --nesting;
            }
            ++cursor;
        } while (cursor < format.size() && nesting != 0 && cursor - start <= 4096);
        if (nesting != 0 || cursor - start > 4096) {
            return std::nullopt;
        }
        const auto parsed = ConstantExpression(std::string_view(format).substr(start, cursor - start)).Parse();
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        auto suffix = cursor;
        skipSpace(suffix);
        if (suffix < format.size() && std::string_view("+-*/%|&<>^").find(format[suffix]) != std::string_view::npos) {
            // Do not mistake a parenthesized prefix for the complete bit mask.
            return std::nullopt;
        }
        std::optional<uint64_t> candidate = parsed->beforeSubtractOne;
        if (!candidate.has_value()) {
            if (suffix < format.size() && format[suffix++] == '?') {
                skipSpace(suffix);
                if (format.compare(suffix, 3, "\"+\"") == 0) {
                    candidate = parsed->number;
                }
            }
        }
        if (candidate.has_value()) {
            if (!PowerOfTwo(*candidate) || (result.has_value() && result != candidate)) {
                return std::nullopt;
            }
            result = candidate;
        }
    }
    return result;
}
}
#endif
