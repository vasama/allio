#include <allio/byte_io_buffers.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;

TEST_CASE("std::span layout", "[byte_io]")
{
	//TODO: Get rid of this:
	using namespace allio::detail;

	int storage[1];

	auto const span = std::span<int>(storage, 1);

	static_assert(sizeof(decltype(span)) == sizeof(io_buffer));
	static_assert(alignof(decltype(span)) == alignof(io_buffer));

	static constexpr bool size_data = vsm::any_flags(
		detail::span_layout,
		io_buffer_layout::size_data);

	io_buffer buffer;

	(size_data ? buffer.m1 : buffer.m0).data = storage;
	(size_data ? buffer.m0 : buffer.m1).size = 1;

	REQUIRE(std::memcmp(&span, &buffer, sizeof(io_buffer)) == 0);
}
