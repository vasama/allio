#pragma once

#include <allio/detail/parameters2.hpp>

#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

namespace allio::detail {

template<typename O>
struct io_parameters_t
	: O::required_params_type
	, O::optional_params_type
{
	io_parameters_t() = default;

	explicit io_parameters_t(auto&&... args)
		: O::required_params_type{ { vsm_forward(args) }... }
		, O::optional_params_type{}
	{
	}
};

template<typename O>
class io_arguments_t : io_parameters_t<O>
{
	using base = io_parameters_t<O>;

public:
	explicit io_arguments_t(auto&&... args)
		: base(vsm_forward(args)...)
	{
	}

	base&& operator()(auto&&... args) &&
	{
		(set_argument(static_cast<base&>(*this), vsm_forward(args)), ...);
		return static_cast<base&&>(*this);
	}
};


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
		: result(result)
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
		result = vsm_forwartd(value);
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

template<typename O>
using io_result_ref_t = io_result_ref<typename O::result_type>;

inline constexpr io_result_ref<void> no_result = {};

} // namespace allio::detail
