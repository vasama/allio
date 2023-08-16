#pragma once

#include <allio/async_io.hpp>
#include <allio/detail/api.hpp>
#include <allio/detail/handle_flags.hpp>
#include <allio/detail/parameters.hpp>
#include <allio/detail/type_list.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/linear.hpp>
#include <vsm/result.hpp>
#include <vsm/utility.hpp>

#include <cstdint>

namespace allio {
namespace detail {

struct handle_base
{
	using async_operations = type_list<>;

	struct flags : handle_flags
	{
		flags() = delete;
		static constexpr uint32_t allio_detail_handle_flags_flag_count = 0;
		static constexpr uint32_t allio_detail_handle_flags_flag_limit = 16;
	};

	struct implementation
	{
		struct flags : handle_flags
		{
			flags() = delete;
			static constexpr uint32_t allio_detail_handle_flags_flag_count = 16;
			static constexpr uint32_t allio_detail_handle_flags_flag_limit = 32;
		};
	};
};

} // namespace detail

#define allio_detail_default_lifetime(handle) \
	handle() = default; \
	handle(handle&&) = default; \
	handle& operator=(handle&&) = default; \
	~handle() = default \

allio_detail_export
class handle : public detail::handle_base
{
protected:
	using base_type = detail::handle_base;

private:
	vsm::linear<handle_flags> m_flags;

public:
	allio_handle_flags
	(
		not_null,
	);


	struct native_handle_type
	{
		handle_flags flags;
	};

	[[nodiscard]] native_handle_type get_native_handle() const
	{
		vsm_assert(*this); //PRECONDITION
		return
		{
			m_flags.value,
		};
	}


	[[nodiscard]] handle_flags get_flags() const
	{
		vsm_assert(*this); //PRECONDITION
		return m_flags.value;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_flags.value[flags::not_null];
	}


	struct close_t;
	using close_parameters = no_parameters;


	#define allio_handle_create_parameters(type, data, ...) \
		type(allio::handle, create_parameters) \
		data(bool, multiplexable, false) \

protected:
	struct unchecked_t {};


	allio_detail_default_lifetime(handle);

	handle(unchecked_t, native_handle_type const& native)
		: m_flags(native.flags)
	{
	}


	[[nodiscard]] static bool check_native_handle(native_handle_type const& native)
	{
		return native.flags[flags::not_null];
	}

	void set_native_handle(native_handle_type const& native)
	{
		m_flags.value = native.flags;
	}


	template<typename H>
	struct sync_interface
	{
		template<parameters<close_parameters> P = close_parameters::interface>
		vsm::result<void> close(P const& args = {});
	};

	template<typename H>
	struct async_interface
	{
		//template<parameters<close_parameters> P = close_parameters::interface>
		//basic_sender<close_t> close(P const& args = {});
	};
};


allio_detail_export
template<typename H>
class basic_handle;

allio_detail_export
template<typename H, typename M>
class basic_async_handle;

allio_detail_export
template<typename H>
class basic_handle final
	: public H
	, public H::template sync_interface<basic_handle>
{
	template<typename M>
	using async_handle_type = basic_async_handle<H, M>;

public:
	using base_type = H;


	basic_handle() = default;

	basic_handle(basic_handle&& other) noexcept = default;

	basic_handle& operator=(basic_handle&& other) & noexcept
	{
		if (*this)
		{
			unrecoverable(this->close());
		}
		H::operator=(vsm_move(other));
		return *this;
	}

	~basic_handle()
	{
		if (*this)
		{
			unrecoverable(this->close());
		}
	}


	typename H::native_handle_type get_native_handle() const
	{
		vsm_assert(*this); //PRECONDITION
		return H::get_native_handle();
	}

	vsm::result<typename H::native_handle_type> release_native_handle()
	{
		if (!*this)
		{
			return vsm::unexpected(error::handle_is_null);
		}

		return H::release_native_handle();
	}

	vsm::result<void> set_native_handle(typename H::native_handle_type const& native) &
	{
		if (*this)
		{
			return vsm::unexpected(error::handle_is_not_null);
		}

		if (!H::check_native_handle(native))
		{
			return vsm::unexpected(error::invalid_argument);
		}

		H::set_native_handle(native);

		return {};
	}

	static vsm::result<basic_handle> from_native_handle(typename H::native_handle_type const& native)
	{
		if (!H::check_native_handle(native))
		{
			return vsm::unexpected(error::invalid_argument);
		}

		vsm::result<basic_handle> r(vsm::result_value);
		r->H::set_native_handle(native);
		return r;
	}


#if 0 //TODO: Enable this with proper deduction for the multiplexer type.
	template<typename M>
	vsm::result<async_handle_type<M>> attach_multiplexer(auto&& multiplexer)
	{
		if (!*this)
		{
			return vsm::unexpected(error::handle_is_null);
		}

		vsm::result<async_handle_type<M>> r(vsm::result_value);
		vsm_try_void(r->attach_multiplexer_impl(vsm_move(*this), multiplexer));
		return r;
	}

	template<typename M>
	vsm::result<void> detach_multiplexer(async_handle_type<M>&& async_handle) &
	{
		if (*this)
		{
			return vsm::unexpected(error::handle_is_not_null);
		}

		if (!async_handle)
		{
			return vsm::unexpected(error::invalid_argument);
		}

		return async_handle.detach_multiplexer_impl(*this);
	}
#endif

private:
	template<typename, typename>
	friend class basic_async_handle;
};

allio_detail_export
template<typename H, typename M>
class basic_async_handle final
	: public H
	, public H::template sync_interface<basic_async_handle>
	, public H::template async_interface<basic_async_handle>
{
	using handle_type = basic_handle<H>;
	using multiplexer_handle_type = async_handle_storage<M, H>;

	static_assert(std::is_default_constructible_v<multiplexer_context_type>);
	static_assert(std::is_nothrow_move_assignable_v<multiplexer_context_type>);

	multiplexer_context_type m_context;

public:
	using base_type = H;


	basic_async_handle() = default;

	basic_async_handle(basic_async_handle&& other) noexcept = default;

	basic_async_handle& operator=(basic_async_handle&& other) & noexcept
	{
		if (*this)
		{
			unrecoverable(close());
		}

		H::operator=(vsm_move(other));
		m_context = vsm_move(other.m_context);

		return *this;
	}

	~basic_async_handle()
	{
		if (*this)
		{
			unrecoverable(close());
		}
	}


	#if 0 //TODO: Probably just get rid of this? Let the user do two-step instead.
	vsm::result<void> set_native_handle(typename H::native_handle_type const& native, M const& multiplexer) &
	{
		if (*this)
		{
			return vsm::unexpected(error::handle_is_not_null);
		}

		if (!H::check_native_handle(native))
		{
			return vsm::unexpected(error::invalid_argument);
		}

		handle_type handle(unchecked_t(), native);
		auto const r = attach_multiplexer_impl(vsm_move(handle), multiplexer);

		if (!r)
		{
			// On failure the user's native handle should not be consumed.
			handle.release_native_handle(unchecked_t());
		}

		return {};
	}
	
	static vsm::result<basic_async_handle> from_native_handle(typename H::native_handle_type const& native, M const& multiplexer)
	{
		if (!H::check_native_handle(native))
		{
			return vsm::unexpected(error::invalid_argument);
		}

		vsm::result<basic_async_handle> r(vsm::result_value);
		handle_type handle(unchecked_t(), native);
		auto const r2 = r->attach_multiplexer(handle, multiplexer);
		if (!r2)
		{
			// On failure the user's native handle should not be consumed.
			handle.release_native_handle(unchecked_t());
		}
		return r;
	}
	#endif


	vsm::result<void> attach_multiplexer(
		handle_type&& handle,
		std::convertible_to<multiplexer_handle_type> auto&& multiplexer) &
	{
		if (*this)
		{
			return vsm::unexpected(error::handle_is_not_null);
		}

		if (!handle)
		{
			return vsm::unexpected(error::invalid_argument);
		}

		return attach_multiplexer_impl(vsm_move(handle), vsm_forward(multiplexer));
	}

	vsm::result<handle_type> detach_multiplexer()
	{
		if (!*this)
		{
			return vsm::unexpected(error::handle_is_null);
		}

		vsm::result<handle_type> r(vsm::result_value);
		vsm_try_void(detach_multiplexer_impl(*r));
		return r;
	}


private:
	vsm::result<void> attach_multiplexer_impl(
		handle_type&& handle,
		std::convertible_to<multiplexer_handle_type> auto&& multiplexer) &
	{
		vsm_try_void(M::attach_handle(
			vsm_as_const(handle),
			m_context,
			vsm_forward(multiplexer)));

		this->H::operator=(static_cast<H&&>(handle));
		return {};
	}

	vsm::result<void> detach_multiplexer_impl(handle_type& handle)
	{
		vsm_try_void(M::detach_handle(
			vsm_as_const(handle),
			m_context));

		handle.H::operator=(static_cast<H&&>(*this));
		return {};
	}

	template<typename>
	friend class basic_handle;
};


allio_detail_export
template<typename Operation>
struct io_operation_traits;

namespace detail {

template<typename Operation>
using io_handle_type = typename io_operation_traits<Operation>::handle_type;

template<typename Operation>
using io_result_type = typename io_operation_traits<Operation>::result_type;


template<typename Operation>
struct io_parameters
	: io_operation_traits<Operation>
	: io_operation_traits<Operation>::params_type
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

	void bind_storage(result_storage<Result>& storage)
	{
		vsm_assert(result == nullptr);
		result = &storage.result;
	}

	vsm::result<void> produce(vsm::instance_of<vsm::result> auto&& r) const
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

	io_result_parameter(result_storage<void>& storage)
	{
	}

	void bind_storage(result_storage<void>& storage)
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
	explicit io_parameters_with_result(parameters_with_handle<Operation> const& args)
		: io_parameters_with_handle<Operation>(args)
	{
	}

	explicit io_parameters_with_result(io_result_parameter<io_result_type<Operation>> const result, parameters_with_handle<Operation> const& args)
		: io_parameters_with_handle<Operation>(args)
		, io_result_parameter<io_result_type<Operation>>(result)
	{
	}

	explicit io_parameters_with_result(auto&&... args)
		: io_parameters_with_handle(vsm_forward(args)...)
	{
	}

	explicit io_parameters_with_result(io_result_parameter<io_result_type<Operation>> const result, auto&&... args)
		: io_parameters_with_handle(vsm_forward(args)...)
		, io_result_parameter<io_result_type<Operation>>(result)
	{
	}
};


template<typename Operation, typename H>
vsm::result<io_result_type<Operation>> block(basic_handle<H>& handle, auto&&... args)
{
	return sync_impl(handle, vsm_forward(args)...);
}

template<typename Operation, typename H>
vsm::result<io_result_type<Operation>> block(basic_handle<H> const& handle, auto&&... args)
{
	return sync_impl(handle, vsm_forward(args)...);
}

template<typename Operation, typename H, typename M>
vsm::result<io_result_type<Operation>> block(basic_async_handle<H, M>& handle, auto&&... args)
{
	return block_async<Operation>(handle, vsm_forward(args)...);
}

template<typename Operation, typename H, typename M>
vsm::result<io_result_type<Operation>> block(basic_async_handle<H, M> const& handle, auto&&... args)
{
	return block_async<Operation>(handle, vsm_forward(args)...);
}

} // namespace detail
} // namespace allio
