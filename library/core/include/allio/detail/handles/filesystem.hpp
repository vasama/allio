#pragma once

#include <allio/any_byte_buffer.hpp>
#include <allio/detail/filesystem.hpp>
#include <allio/detail/handle.hpp>
#include <allio/detail/optional_flags.hpp>

// TODO: Move this elsewhere.
#include <allio/detail/facade.hpp>

namespace allio::detail {

struct query_path_info_t
{
	using operation_concept = void;

	struct params_type
	{
		fs_path path;
		optional_flags<fs_info_mask> mask;
		fs_info_buffer_ref buffer;

		void set_argument(fs_info_mask const value)
		{
			mask = mask.value_or_zero() | value;
		}
	};

	using result_type = fs_info_view;

	template<object Object>
	static constexpr auto& blocking_io_2 = Object::query_path_info;

	template<object Object>
	static vsm::result<fs_info_view> blocking_io(
		native_handle<Object> const& h,
		io_parameters_t<Object, query_path_info_t> const& a)
		requires requires { Object::query_path_info(h, a); }
	{
		return Object::query_path_info(h, a);
	}
};

struct filesystem_t : object_t
{
	using base_type = object_t;

	static vsm::result<fs_info_view> query_path_info(
		native_handle<filesystem_t> const& h,
		io_parameters_t<filesystem_t, query_path_info_t> const& a);
};


// TODO: Move this elsewhere. Add a stateless observe in traits.
template<object Object>
class basic_stateless_handle
{
	static_assert(sizeof(native_handle<Object>) == sizeof(native_handle<object_t>));

public:
	using handle_concept = void;
	using object_type = Object;
	using multiplexer_handle_type = void;
	using native_type = native_handle<Object>;

private:
	static constexpr native_type make_native_handle()
	{
		native_type handle;
		handle.flags = object_t::flags::not_null;
		return handle;
	}

	static constexpr native_type s_native = make_native_handle();

public:
	template<object OtherObject>
	using rebind_object = basic_stateless_handle<OtherObject>;

	template<optional_multiplexer_handle_for<Object> OtherMultiplexerHandle>
		requires std::is_void_v<OtherMultiplexerHandle>
	using rebind_multiplexer = basic_stateless_handle<Object>;

	[[nodiscard]] native_type const& native() const
	{
		return s_native;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return true;
	}
};

template<object Object>
struct handle_traits<basic_stateless_handle<Object>>
{
	template<observer Operation>
	static vsm::result<io_result_t<basic_detached_handle<Object>, Operation>> blocking_io(
		handle_const_t<Operation, basic_stateless_handle<Object>> const& h,
		io_parameters_t<Object, Operation> const& a)
	{
		return detail::blocking_io<Operation>(h.native(), a);
	}
};

template<typename Traits>
inline constexpr basic_facade<basic_stateless_handle<filesystem_t>, Traits> filesystem_handle;


template<typename Traits>
[[nodiscard]] auto _query_path_info(
	fs_path const& path,
	fs_info_buffer_ref const buffer,
	auto&&... args)
{
	auto a = io_parameters_t<filesystem_t, query_path_info_t>();
	a.path = path;
	a.buffer = buffer;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template observe<query_path_info_t>(filesystem_handle<Traits>, a);
}

} // namespace allio::detail
