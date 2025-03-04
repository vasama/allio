#pragma once

#include <allio/abi.h>
#include <allio/detail/deadline.hpp>
#include <allio/detail/handles/platform_object.hpp>

//#include <vsm/intrusive_ptr.hpp>

namespace allio::detail {

struct opaque_object_t : object_t
{
	using base_type = object_t;

	struct poll_t
	{
		using operation_concept = void;
		using params_type = deadline_t;
		using result_type = void;
	};

	using operations = type_list_append
	<
		base_type::operations
		, poll_t
	>;

	static vsm::result<void> poll(
		native_handle<opaque_object_t> const& h,
		io_parameters_t<opaque_object_t, poll_t> const& a);

	static vsm::result<void> close(
		native_handle<opaque_object_t>& h,
		io_parameters_t<opaque_object_t, close_t> const& a);

	template<typename Handle, typename Traits>
	struct facade : base_type::facade<Handle, Traits>
	{
		[[nodiscard]] auto poll(auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, poll_t>{};
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<poll_t>(static_cast<Handle const&>(*this), a);
		}
	};
};

template<>
struct native_handle<opaque_object_t> : native_handle<opaque_object_t::base_type>
{
	allio_abi_object_v1* object;
};


#if 0
template<typename Implementation>
struct opaque_object_wrapper_base
{
	vsm_no_unique_address Implementation m_implementation;
};

template<typename Implementation>
class opaque_object_wrapper
	: public vsm::intrusive_refcount
	, public opaque_object_wrapper_base<Implementation>
	, public allio_abi_object
{
	using base = opaque_object_wrapper_base<Implementation>;

public:
	template<typename... Args>
		requires std::constructible_from<Implementation, Args...>
	explicit opaque_object_wrapper(Args&&... args)
		: base{ Implementation(vsm_forward(args)...) }
		, allio_abi_object
		{
			.version = allio_abi_v1,
			.handle_value = base::get_handle_value(),
			.handle_flags = base::get_handle_flags(),
			.functions = &functions,
		}
	{
	}

private:
	static void _close(allio_abi_object* const object)
	{
		auto const self = static_cast<opaque_object_wrapper*>(object);
		(void)vsm::intrusive_ptr<opaque_object_wrapper>::adopt(self);
	}

	static allio_abi_result _notify(allio_abi_object* const object, uintptr_t const information)
	{
		auto const self = static_cast<opaque_object_wrapper*>(object);

		if constexpr (requires { self->m_implementation.notify(information); })
		{
			return self->m_implementation.notify(information);
		}
		else
		{
			return allio_abi_result_success;
		}
	}

	static allio_abi_object_functions const functions;
};

template<typename Implementation>
allio_abi_object_functions const opaque_object_wrapper<Implementation>::functions =
{
	.close = _close,
	.notify = _notify,
};

template<typename Implementation, typename... Args>
	requires std::constructible_from<Implementation, Args...>
vsm::result<basic_detached_handle<opaque_object_t>> make_opaque_object(Args&&... args)
{
	//TODO: Use acquire_storage
	auto const object = new (std::nothrow) opaque_object_wrapper<Implementation>(
		vsm_forward(args)...);

	if (object == nullptr)
	{
		return vsm::unexpected(error::not_enough_memory);
	}

	return basic_detached_handle<opaque_object_t>(
		adopt_handle,
		native_handle<opaque_object_t>
		{
			native_handle<object_t>
			{
				object_t::flags::not_null,
			},
			object,
		});
}
#endif

} // namespace allio::detail
