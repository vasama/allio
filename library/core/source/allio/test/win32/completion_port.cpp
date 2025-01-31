#include <allio/impl/win32/completion_port.hpp>
#include <allio/impl/win32/event.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/nothrow/pipe.hpp>
#include <allio/test/spawn.hpp>
#include <allio/test/win32/completion_port.hpp>
#include <allio/win32/kernel_error.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::win32;

namespace {

TEST_CASE("Synchronous I/O with attached overlapped handle", "[windows][iocp]")
{
	//TODO: The completion port is not being signaled...
	//bool const pass_event = GENERATE(0, 1);
	bool const pass_event = true;

	auto const completion_port = create_completion_port(1).value();

	auto const event = create_event(
		/* auto_reset: */ true,
		/* is_initially_signaled: */ false).value();

	auto const [r, w] = nothrow::create_pipe(non_blocking).value();
	HANDLE const r_h = unwrap_handle(r.native().platform_handle);

	set_completion_information(r_h, completion_port.get(), nullptr).value();

	unsigned char r_buffer = 0;
	IO_STATUS_BLOCK io_status_block =
	{
		.Status = -1,
	};

	NTSTATUS const status = win32::NtReadFile(
		r_h,
		pass_event ? event.get() : NULL,
		/* ApcRoutine: */ nullptr,
		/* ApcContext: */ nullptr,
		&io_status_block,
		&r_buffer,
		sizeof(r_buffer),
		/* ByteOffset: */ nullptr,
		/* Key: */ 0);
	REQUIRE(status == STATUS_PENDING);

	auto future = test::spawn([&]() {
		return win32::NtWaitForSingleObject(
			event.get(),
			/* Alertable: */ false,
			kernel_timeout(std::chrono::seconds(1)));
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	unsigned char w_buffer = 42;
	REQUIRE(w.write_some(as_write_buffer(&w_buffer, sizeof(w_buffer))).value() == 1);

	if (!pass_event)
	{
		signal_event(event.get()).value();
	}

	REQUIRE(future.get() == STATUS_SUCCESS);

	REQUIRE(io_status_block.Status == 0);
	REQUIRE(io_status_block.Information == 1);
	REQUIRE(r_buffer == 42);

	test::completion_storage<2> completions;
	size_t const completion_count = completions.remove(completion_port);
	REQUIRE(completion_count == (pass_event ? 0 : 1));
}

} // namespace
