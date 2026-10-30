#include <allio/testing/memory.hpp>

#include <vsm/exceptions.hpp>

#include <Windows.h>

using namespace allio;
using namespace allio::test;

void test::setup_executable_memory_test(void* const base, size_t const size)
{
}

protection test::test_executable_memory(void* const base, size_t const size)
{
	return test_memory_protection(base, size);
}

protection test::test_memory_protection(void* const base, size_t const size)
{
	MEMORY_BASIC_INFORMATION information;
	if (VirtualQuery(base, &information, sizeof(information)) >= sizeof(information))
	{
		switch (information.Protect)
		{
		case PAGE_NOACCESS:
			return protection::none;

		case PAGE_READONLY:
			return protection::read;

		case PAGE_READWRITE:
			return protection::read_write;

		case PAGE_WRITECOPY:
			return protection::read_write;

		case PAGE_EXECUTE:
			return protection::execute;

		case PAGE_EXECUTE_READ:
			return protection::execute | protection::read;

		case PAGE_EXECUTE_READWRITE:
			return protection::execute | protection::read_write;

		case PAGE_EXECUTE_WRITECOPY:
			return protection::execute | protection::read_write;
		}
	}

	vsm_except_throw_or_terminate(std::runtime_error("Unable to query memory protection."));
}
