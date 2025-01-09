#pragma once

#include <allio/detail/facade.hpp>

namespace allio::detail {

struct nothrow_traits
{
	static constexpr bool has_transform_result = false;

	template<typename T>
	using result = vsm::result<T>;

	template<object Object, optional_multiplexer_handle_for<Object> MultiplexerHandle = void>
	using handle = basic_facade<basic_handle<Object, MultiplexerHandle>, nothrow_traits>;

	template<observer Operation, detached_handle Handle>
	[[nodiscard]] static vsm::result<io_result_t<basic_facade<Handle, nothrow_traits>, Operation>> observe(
		basic_facade<Handle, nothrow_traits> const& h,
		io_parameters_t<typename Handle::object_type, Operation> const& a)
	{
		using result_type = io_result_t<basic_facade<Handle, nothrow_traits>, Operation>;

		auto r = blocking_io<Operation>(h, a);
		using value_type = typename decltype(r)::value_type;

		if constexpr (std::is_same_v<value_type, result_type>)
		{
			return r;
		}
		else if (r)
		{
			return rebind_handle<result_type>(vsm_move(*r));
		}
		else
		{
			return vsm::unexpected(r.error());
		}
	}
};

struct nothrow_blocking_traits : nothrow_traits
{
	template<object Object, producer Operation>
	static vsm::result<handle<Object>> produce(io_parameters_t<Object, Operation> const& a)
	{
		vsm::result<handle<Object>> r(vsm::result_value);
		if (vsm::result<void> const r2 = blocking_io<Operation>(*r, a); !r2)
		{
			r = vsm::unexpected(r2.error());
		}
		return r;
	}
};

} // namespace allio::detail
