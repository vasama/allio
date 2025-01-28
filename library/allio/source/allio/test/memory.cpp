#include <allio/test/memory.hpp>

using namespace allio;

namespace allio::test {

bool catch_access_violation(void(*function)(void*), void* context);

} // namespace allio::test

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

} // namespace

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

	return protection | protection::none;
}
