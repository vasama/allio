#pragma once

#include <vsm/result.hpp>
#include <vsm/utility.hpp>

namespace allio::detail {

template<typename Operation>
struct io_operation_traits;

template<typename Operation>
using io_handle_type = typename io_operation_traits<Operation>::handle_type;

template<typename Operation>
using io_result_type = typename io_operation_traits<Operation>::result_type;


template<typename Operation>
struct io_parameters
	: io_operation_traits<Operation>
	, io_operation_traits<Operation>::params_type
{
};

template<typename Handle>
struct io_handle_parameter
{
	Handle* handle;
};

template<typename Operation>
struct io_parameters_with_handle
	: io_handle_parameter<io_handle_type<Operation>>
	, io_parameters<Operation>
{
	explicit io_parameters_with_handle(io_handle_type<Operation>& handle, auto&&... args)
		: io_handle_parameter<io_handle_type<Operation>>{ &handle }
		, io_parameters<Operation>{ vsm_forward(args)... }
	{
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
struct io_result_parameter
{
	Result* result;

	io_result_parameter()
		: result(nullptr)
	{
	}

	io_result_parameter(vsm::result<Result>& storage)
	{
		result = &*storage;
	}

	io_result_parameter(io_result_storage<Result>& storage)
	{
		result = &storage.result;
	}

	void bind_storage(io_result_storage<Result>& storage)
	{
		vsm_assert(result == nullptr);
		result = &storage.result;
	}

	vsm::result<void> produce(auto&& r) const
	{
		vsm_assert(result != nullptr);
		vsm_try_assign(*result, vsm_forward(r));
		return {};
	}
};

template<>
struct io_result_parameter<void>
{
	io_result_parameter() = default;

	io_result_parameter(vsm::result<void>& storage)
	{
	}

	io_result_parameter(io_result_storage<void>& storage)
	{
	}

	void bind_storage(io_result_storage<void>& storage)
	{
	}

	vsm::result<void> produce(vsm::result<void> const& r) const
	{
		return r;
	}
};

template<typename Operation>
struct io_parameters_with_result
	: io_parameters_with_handle<Operation>
	, io_result_parameter<io_result_type<Operation>>
{
	explicit io_parameters_with_result(io_parameters_with_handle<Operation> const& args)
		: io_parameters_with_handle<Operation>(args)
	{
	}

	explicit io_parameters_with_result(io_result_parameter<io_result_type<Operation>> const result, io_parameters_with_handle<Operation> const& args)
		: io_parameters_with_handle<Operation>(args)
		, io_result_parameter<io_result_type<Operation>>(result)
	{
	}

	explicit io_parameters_with_result(auto&&... args)
		: io_parameters_with_handle<Operation>(vsm_forward(args)...)
	{
	}

	explicit io_parameters_with_result(io_result_parameter<io_result_type<Operation>> const result, auto&&... args)
		: io_parameters_with_handle<Operation>(vsm_forward(args)...)
		, io_result_parameter<io_result_type<Operation>>(result)
	{
	}
};

} // namespace allio::detail
