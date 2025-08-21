#pragma once

#include <allio/detail/byte_io.hpp>
#include <allio/detail/byte_io_buffers.hpp>
#include <allio/detail/handle.hpp>

namespace allio::detail {

struct standard_stream_t : object_t
{
	using base_type = object_t;

	allio_handle_flags
	(
		stream_bit_0,
		stream_bit_1,
		);

	using stream_read_t = byte_io::stream_read_t;
	using stream_write_t = byte_io::stream_write_t;

	using operations = type_list_append
		<
		base_type::operations
		, stream_read_t
		, stream_write_t
		>;

	static byte_io_limits get_byte_io_limits(native_handle<standard_stream_t> const& h);

	static vsm::result<size_t> stream_read(
		native_handle<standard_stream_t> const& h,
		io_parameters_t<standard_stream_t, stream_read_t> const& args);

	static vsm::result<size_t> stream_write(
		native_handle<standard_stream_t> const& h,
		io_parameters_t<standard_stream_t, stream_write_t> const& args);

	static vsm::result<void> close(
		native_handle<standard_stream_t>& h,
		io_parameters_t<standard_stream_t, close_t> const& args)
	{
		h = {};
		return {};
	}


	template<typename Handle, typename Traits>
	struct facade
		: base_type::facade<Handle, Traits>
		, byte_io::stream_facade<Handle, Traits>
	{
	};
};

[[nodiscard]] inline constexpr native_handle<standard_stream_t> make_standard_handle(int const n)
{
	handle_flags flags = handle_flags(object_t::flags::not_null);
	
	switch (n)
	{
	case 1:
		flags |= standard_stream_t::flags::stream_bit_0;
		break;

	case 2:
		flags |= standard_stream_t::flags::stream_bit_1;
		break;
	}

	return native_handle<standard_stream_t>
	{
		native_handle<object_t>
		{
			flags
		}
	};
}

#if 0
template<object Object>
class unmanaged_detached_handle
{
public:
	using handle_concept = void;
	using object_type = Object;
	using multiplexer_handle_type = void;
	using native_type = native_handle<Object>;

private:
	native_type m_native = {};

public:
	template<std::same_as<Object> OtherObject>
	using rebind_object = unmanaged_detached_handle;

	template<std::same_as<void> OtherMultiplexer>
	using rebind_multiplexer = unmanaged_detached_handle;


	unmanaged_detached_handle() = default;

	explicit constexpr unmanaged_detached_handle(
		adopt_handle_t,
		std::convertible_to<native_type> auto&& native)
		: m_native(vsm_forward(native))
	{
	}

	[[nodiscard]] constexpr native_type const& native() const
	{
		return m_native;
	}

	[[nodiscard]] explicit constexpr operator bool() const
	{
		using object_native_handle_type [[maybe_unused]] = native_handle<object_t>;
		return m_native.object_native_handle_type::flags[object_t::flags::not_null];
	}
};
#endif

} // namespace allio::detail
