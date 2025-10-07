#pragma once

#include <allio/detail/byte_stream_io.hpp>

#include <vector>

#if 0
namespace allio::detail {

struct loop_initializing_t {};

template<typename Handle, typename Context, typename... Operations>
class nothrow_loop_context : public Context
{
	template<typename Operation>
	using result_t = vsm::result<io_result_t<Handle, Operation>>;

	std::variant<loop_initializing_t, result_t<Operations>...> m_result;

public:
	template<typename Operation>
	[[nodiscard]] result_t<Operation>* get_if() &
	{
		return std::get_if<result_t<Operation>>(&m_result);
	}

	template<typename Operation>
	[[nodiscard]] vsm::result<std::nullopt_t> submit(auto&&... args) &
	{
		m_result.emplace<result_t<Operation>>(
			vsm_lazy(nothrow::block<Operation>(vsm_forward()...)));

		return std::nullopt;
	}
};

template<typename Context, typename... Operations, typename Handle, typename Loop>
auto _nothrow_loop(Handle&& handle, Loop const loop_function)
{
	nothrow_loop_context<std::remove_cvref_t<Handle>, Context, Operations...> loop;

	while (true)
	{
		auto r = loop_function(handle, loop);

		if (!r)
		{
			return vsm::unexpected(r.error());
		}

		if (*r)
		{
			return vsm_move(**r);
		}
	}
}

} // namespace allio::detail
#endif

namespace allio::nothrow {

template<detail::handle Handle>
[[nodiscard]] vsm::result<size_t> read_to_end(
	Handle const& handle,
	detail::any_byte_buffer const buffer,
	auto&&... args)
{
	return detail::_read_to_end_x(handle, buffer, vsm_forward(args)...);
}

template<typename Container = std::vector<std::byte>, detail::handle Handle>
[[nodiscard]] vsm::result<Container> read_to_end(Handle const& handle, auto&&... args)
{
	return detail::_read_to_end_c<Container>(handle, vsm_forward(args)...);
}

} // namespace allio::nothrow
