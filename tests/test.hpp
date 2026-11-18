#pragma once

#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace test {
struct Case { std::string name; std::function<void()> run; };
inline std::vector<Case>& cases() { static std::vector<Case> registry; return registry; }
struct Register {
    Register(std::string name, std::function<void()> run) {
        cases().push_back({std::move(name), std::move(run)});
    }
};
inline void check(bool okay, const char* expression, const char* file, int line) {
    if (!okay) {
        std::ostringstream message;
        message << file << ':' << line << ": " << expression;
        throw std::runtime_error(message.str());
    }
}
} // namespace test

#define TEST(name) static void name(); static test::Register name##_case(#name, name); static void name()
#define CHECK(expression) test::check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

