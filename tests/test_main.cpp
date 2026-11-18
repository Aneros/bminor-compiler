#include "test.hpp"
#include <exception>
#include <iostream>

int main() {
    std::size_t failures = 0;
    for (const auto& testCase : test::cases()) {
        try {
            testCase.run();
            std::cout << "PASS " << testCase.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << testCase.name << ": " << error.what() << '\n';
        }
    }
    std::cout << test::cases().size() - failures << '/' << test::cases().size() << " tests passed\n";
    return failures ? 1 : 0;
}

