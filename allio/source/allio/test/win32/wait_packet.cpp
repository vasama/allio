#include <allio/event.hpp>
#include <allio/impl/win32/completion_port.hpp>
#include <allio/impl/win32/wait_packet.hpp>
#include <allio/win32/platform.hpp>

#include <catch2/catch_all.hpp>

#include <random>

using namespace allio;
using namespace allio::win32;

template<typename T>
static T random_integer()
{
	return std::uniform_int_distribution<T>()(Catch::sharedRng());
}

static void* random_pointer()
{
	return reinterpret_cast<void*>(random_integer<uintptr_t>());
}


TEST_CASE("wait packet can be used to wait for events", "[wait_packet][win32][kernel]")
{
	auto const completion_port = create_completion_port(1).value();

	bool const initially_signaled = GENERATE(false, true);
	CAPTURE(initially_signaled);

	auto const event = blocking::create_event(
		auto_reset_event,
		signal_event(initially_signaled)).value();

	auto const wait_packet = create_wait_packet().value();

	auto const key_context = random_pointer();
	auto const apc_context = random_pointer();
	auto const information = random_integer<ULONG_PTR>();
	auto const wait_status = random_integer<NTSTATUS>();

	bool const already_signaled = associate_wait_packet(
		unwrap_wait_packet(wait_packet.get()),
		completion_port.get(),
		unwrap_handle(event.platform_handle()),
		key_context,
		apc_context,
		wait_status,
		information).value();

	REQUIRE(already_signaled == initially_signaled);

	FILE_IO_COMPLETION_INFORMATION completions[2];
	FILE_IO_COMPLETION_INFORMATION& completion = completions[0];
	auto const get_completion = [&]() -> bool
	{
		size_t const completion_count = remove_io_completions(
			completion_port.get(),
			completions,
			deadline::instant()).value();

		REQUIRE(completion_count <= 1);
		bool const got_completion = completion_count == 1;

		if (got_completion)
		{
			CHECK(completion.KeyContext == key_context);
			CHECK(completion.ApcContext == apc_context);
			CHECK(completion.IoStatusBlock.Information == information);
			CHECK(completion.IoStatusBlock.Status == wait_status);
		}

		return got_completion;
	};

	auto const cancel = [&](bool const remove_queued_completion) -> bool
	{
		return cancel_wait_packet(
			unwrap_wait_packet(wait_packet.get()),
			remove_queued_completion).value();
	};

	if (already_signaled)
	{
		SECTION("an already signaled wait still posts a completion")
		{
			REQUIRE(get_completion());
		}

		SECTION("a posted completion can be removed on cancellation")
		{
			REQUIRE(cancel(
				/* remove_queued_completion: */ true));

			REQUIRE(!get_completion());
		}

		SECTION("a posted completion must be removed on cancellation")
		{
			REQUIRE(!cancel(
				/* remove_queued_completion: */ false));

			REQUIRE(get_completion());
		}
	}
	else
	{
		REQUIRE(!get_completion());

		SECTION("an uncompleted wait can be completed")
		{
			event.signal().value();
			REQUIRE(get_completion());
		}

		SECTION("an uncompleted wait can be cancelled without removing a completion")
		{
			REQUIRE(cancel(
				/* remove_queued_completion: */ GENERATE(false, true)));

			event.signal().value();
			REQUIRE(!get_completion());
		}
	}

	event.signal().value();
	REQUIRE(!get_completion());
}

TEST_CASE("wait packets do not support skipping the completion port on synchronous completion", "[wait_packet][win32][kernel]")
{
	// The purpose of this test is to detect future support for this feature.

	auto const wait_packet = create_wait_packet().value();

	IO_STATUS_BLOCK io_status_block = make_io_status_block();

	FILE_IO_COMPLETION_NOTIFICATION_INFORMATION information =
	{
		.Flags = FILE_SKIP_COMPLETION_PORT_ON_SUCCESS,
	};

	NTSTATUS const status = NtSetInformationFile(
		unwrap_wait_packet(wait_packet.get()),
		&io_status_block,
		&information,
		sizeof(information),
		FileIoCompletionNotificationInformation);

	REQUIRE(status == STATUS_OBJECT_TYPE_MISMATCH);
}
