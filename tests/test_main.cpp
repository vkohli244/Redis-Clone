#include "test_support.h"

#include <csignal>
#include <exception>
#include <iostream>

int main() {
  std::signal(SIGPIPE, SIG_IGN);
  std::size_t failures = 0;

  for (const TestCase &test : test_registry()) {
    try {
      test.function();
      std::cout << "[PASS] " << test.name << '\n';
    } catch (const std::exception &error) {
      ++failures;
      std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
    } catch (...) {
      ++failures;
      std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
    }
  }

  std::cout << test_registry().size() - failures << "/" << test_registry().size()
            << " tests passed\n";
  return failures == 0 ? 0 : 1;
}
