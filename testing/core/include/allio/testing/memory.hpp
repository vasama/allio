#pragma once

#include <allio/memory.hpp>

namespace allio::test {

void setup_executable_memory_test(void* base, size_t size);
protection test_executable_memory(void* base, size_t size);
protection test_memory_protection(void* base, size_t size);

} // namespace allio::test
