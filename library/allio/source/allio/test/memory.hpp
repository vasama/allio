#pragma once

#include <allio/memory.hpp>

namespace allio::test {

[[nodiscard]] protection test_memory_protection(void* base, size_t size);

[[nodiscard]] bool write_noop_executable_code(void* base, size_t size);

} // namespace allio::test
