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
	volatile unsigned char* pos;
	volatile unsigned char* end;

	unsigned char requested_access;
	unsigned char attempted_access;
};

} // namespace

static void attempt_access(void* const p_context)
{
	auto const context = static_cast<context_type volatile*>(p_context);

	auto pos = context->pos;
	auto const end = context->end;

	auto const req = context->requested_access;

	while (pos != end)
	{
		unsigned char value = 0;

		if (req & access_type::r)
		{
			context->attempted_access = access_type::r;
			value = *pos;
		}
		
		if (req & access_type::w)
		{
			context->attempted_access = access_type::w;
			*pos = value;
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

	return protection;
}
