#pragma once

#include <allio/detail/parameters2.hpp>

#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/type_traits.hpp>
#include <vsm/utility.hpp>

#include <tuple>

namespace allio::detail {

template<typename O>
struct io_parameters_t
	: O::required_params_type
	, O::optional_params_type
{
};

template<typename O>
	requires
		std::is_same_v<typename O::required_params_type, no_parameters_t> &&
		std::is_same_v<typename O::optional_params_type, no_parameters_t>
struct io_parameters_t<O> : no_parameters_t
{
};

template<typename O, typename... Args>
class io_args_t
{
	static constexpr auto pack_args(Args&&... args)
	{
		return [&args...](auto use)
		{
			return use(vsm_forward(args)...);
		};
	}

	decltype(pack_args(vsm_declval(Args)...)) m_args;

public:
	explicit io_args_t(Args&&... args)
		: m_args(pack_args(vsm_forward(args)...))
	{
	}

	io_parameters_t<O> operator()(auto&&... optional_args) &&
	{
		return m_args([&](auto&&... required_args) -> io_parameters_t<O>
		{
			io_parameters_t<O> r{ { { vsm_forward(required_args) }... } };
			(set_argument(r, vsm_forward(optional_args)), ...);
			return r;
		});
	}
};

template<typename O, typename... Args>
io_args_t<O, Args...> io_args(Args&&... args)
{
	return io_args_t<O, Args...>(vsm_forward(args)...);
}


template<typename O, typename H, typename M>
auto io_result_select()
{
	if constexpr (requires { typename O::result_type; })
	{
		return vsm::tag<typename O::result_type>();
	}
	else
	{
		return vsm::tag<typename O::template result_type_template<H, M>>();
	}
}

template<typename O, typename H, typename M = void>
using io_result_t = typename decltype(io_result_select<O, H, M>())::type;


template<typename Result>
struct io_result_storage
{
	Result result;

	vsm::result<Result> consume()
	{
		return vsm_move(result);
	}
};

template<>
struct io_result_storage<void>
{
	vsm::result<void> consume()
	{
		return {};
	}
};

template<typename Result>
struct io_result_ref
{
	Result& result;

	io_result_ref(Result& storage)
		: result(storage)
	{
	}

	io_result_ref(vsm::result<Result>& storage)
		: result(*storage)
	{
	}

	io_result_ref(io_result_storage<Result>& storage)
		: result(storage.result)
	{
	}

	io_result_ref& operator=(io_result_ref const&) = delete;

	template<std::convertible_to<Result> R = Result>
	io_result_ref operator=(R&& value) const
	{
		result = vsm_forward(value);
		return *this;
	}

	vsm::result<void> produce(auto&& r) const
	{
		vsm_try_assign(result, vsm_forward(r));
		return {};
	}
};

template<>
struct io_result_ref<void>
{
	io_result_ref() = default;

	io_result_ref(vsm::result<void>& storage)
	{
	}

	io_result_ref(io_result_storage<void>& storage)
	{
	}

	io_result_ref& operator=(io_result_ref const&) = delete;

	vsm::result<void> produce(vsm::result<void> const& r) const
	{
		return r;
	}
};

template<typename O, typename H, typename M = void>
using io_result_ref_t = io_result_ref<io_result_t<O, H, M>>;

inline constexpr io_result_ref<void> no_result = {};

} // namespace allio::detail
