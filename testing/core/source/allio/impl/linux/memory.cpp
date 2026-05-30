#include <allio/test/memory.hpp>

#include <cstring>

using namespace allio;
using namespace allio::test;

namespace {

enum access_type : unsigned char
{
	r = 1 << 0,
	w = 1 << 1,
};

struct context_type
{
	unsigned char volatile* pos;
	unsigned char volatile* end;

	unsigned char requested_access;
	unsigned char attempted_access;
};

static void attempt_copy(unsigned char volatile* const src, unsigned char volatile* const dst)
{
	// If you encounter an access violation here while running unit tests under a debugger, note
	// that the access violation is intended and is caught by a previously installed signal handler
	// or structured exception handler. To avoid executing test cases thus affected, use the Catch2
	// ~[hardware_exception] command line argument to filter them out.
	*dst = *src;
}

static void attempt_access(void* const p_context)
{
	auto const context = static_cast<context_type volatile*>(p_context);

	unsigned char volatile* pos = context->pos;
	unsigned char volatile* const end = context->end;

	unsigned char const requested_access = context->requested_access;

	while (pos != end)
	{
		unsigned char value[1] = {};

		if (requested_access & access_type::r)
		{
			context->attempted_access = access_type::r;
			attempt_copy(pos, value);
		}

		if (requested_access & access_type::w)
		{
			context->attempted_access = access_type::w;
			attempt_copy(value, pos);
		}

		context->pos = ++pos;
	}
}

static void attempt_execute(void* const function)
{
	reinterpret_cast<void(*)()>(function)();
}

} // namespace

__attribute__((section("__allio_executable_memory_test")))
extern "C" void allio_test_executable_memory()
{
}

__attribute__((section("__allio_executable_memory_test")))
extern "C" void allio_test_executable_memory_end()
{
}

extern "C" void __stop___allio_executable_memory_test();

void test::setup_executable_memory_test(void* const base, size_t const size)
{
	auto const begin = reinterpret_cast<std::byte const*>(allio_test_executable_memory);
	auto const end_1 = reinterpret_cast<std::byte const*>(allio_test_executable_memory_end);
	auto const end_2 = reinterpret_cast<std::byte const*>(__stop___allio_executable_memory_test);

	size_t const code_size = std::max(end_1, end_2) - begin;

	if (size < code_size)
	{
		vsm_except_throw_or_terminate(std::runtime_error("Unable to set up executable region."));
	}

	std::memcpy(base, begin, code_size);
}

protection test::test_executable_memory(void* const base, size_t const size)
{
	if (catch_access_violation(attempt_execute, base))
	{
		return protection::execute | test_memory_protection(base, size);
	}
	else
	{
		return protection::none;
	}
}

protection test::test_memory_protection(void* const base, size_t const size)
{
	protection protection = protection::read_write;

	context_type context =
	{
		.pos = reinterpret_cast<unsigned char*>(base),
		.end = reinterpret_cast<unsigned char*>(base) + size,
		.requested_access = access_type::r | access_type::w,
	};

	while (context.requested_access)
	{
		if (catch_access_violation(attempt_access, &context))
		{
			break;
		}

		switch (context.attempted_access)
		{
		case access_type::r:
			protection &= ~protection::read;
			break;

		case access_type::w:
			protection &= ~protection::write;
			break;
		}

		context.requested_access &= ~context.attempted_access;
	}

	return protection;
}
