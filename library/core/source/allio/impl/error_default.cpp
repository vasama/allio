#include <allio/error.hpp>

#include <allio/impl/inplace_vector.hpp>

#include <vsm/exceptions.hpp>

#if allio_config_std_stacktrace
#	include <stacktrace>
#endif

#include <cstdio>
#include <cstdlib>

void allio::unrecoverable_error(
	std::error_code const error,
	std::source_location const location) noexcept
{
	static constexpr char format[] =
		"allio encountered an unrecoverable error:\n"
		"  code:     0x%08x (%s)\n"
		"  message:  %s%s\n"
		"  location: %s:%u\n"
		"%s%s";

	auto const print_error_1 = [&](
		char const* const message_1,
		char const* const message_2,
		char const* const stack_trace_1,
		char const* const stack_trace_2)
	{
		std::fprintf(
			stderr,
			format,
			static_cast<unsigned>(error.value()),
			error.category().name(),
			message_1,
			message_2,
			location.file_name(),
			static_cast<unsigned>(location.line()),
			stack_trace_1,
			stack_trace_2);
	};

	auto const print_error_2 = [&](
		char const* const stack_trace_1,
		char const* const stack_trace_2)
	{
		static constexpr char threw_exception[] =
			"?\n            message() threw an exception: ";

		vsm_except_try
		{
			print_error_1(
				error.message().c_str(),
				"",
				stack_trace_1,
				stack_trace_2);
		}
		vsm_except_catch (std::exception const& e)
		{
			print_error_1(
				threw_exception,
				e.what(),
				stack_trace_1,
				stack_trace_2);
		}
		vsm_except_catch (...)
		{
			print_error_1(
				threw_exception,
				"?",
				stack_trace_1,
				stack_trace_2);
		}
	};

#if allio_config_std_stacktrace
	{
		static constexpr char threw_exception[] =
			"\nstd::stacktrace threw an exception: "

		vsm_except_try
		{
			print_error_2("\n", std::to_string(std::stacktrace::current()).c_str());
		}
		vsm_except_catch (std::exception const& e)
		{
			print_error_2(threw_exception, e.what());
		}
		vsm_except_catch (...)
		{
			print_error_2(threw_exception, "?");
		}
	}
#else
	print_error_2("", "");
#endif

	std::terminate();
}
