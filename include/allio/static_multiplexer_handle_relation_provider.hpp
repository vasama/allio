	#pragma once

#include <allio/multiplexer.hpp>
#include <allio/type_list.hpp>

#include <span>

namespace allio {

template<typename M, typename H>
struct multiplexer_handle_implementation;

template<typename M, typename H, typename O>
struct multiplexer_handle_operation_implementation;

namespace detail {

template<typename M, typename H>
struct multiplexer_handle_translation
{
	using implementation = multiplexer_handle_implementation<M, H>;

	static result<void*> register_handle(multiplexer& m, handle const& h)
	{
		return implementation::register_handle(static_cast<M&>(m), static_cast<H const&>(h));
	}

	static result<void> deregister_handle(multiplexer& m, handle const& h)
	{
		return implementation::deregister_handle(static_cast<M&>(m), static_cast<H const&>(h));
	}
};

template<typename M, typename H, typename O,
	bool = requires { typename multiplexer_handle_operation_implementation<M, H, O>::async_operation_storage; }>
struct multiplexer_handle_operation_translation;

template<typename M, typename H, typename O>
struct multiplexer_handle_operation_translation<M, H, O, 1>
{
	using implementation = multiplexer_handle_operation_implementation<M, H, O>;

	using parameters_type = io::parameters_with_result<O>;
	using storage_type = typename implementation::async_operation_storage;

	static constexpr storage_requirements operation_storage_requirements = storage_requirements_of<storage_type>;
	static_assert(operation_storage_requirements.size > 0);

	static constexpr bool is_synchronous()
	{
		if constexpr (requires { implementation::synchronous; })
		{
			return implementation::synchronous;
		}
		else
		{
			return false;
		}
	}

	static result<async_operation*> construct(multiplexer& m, storage_ptr const storage, async_operation_parameters const& a, async_operation_listener* const listener)
	{
		if constexpr (requires { implementation::construct; })
		{
			return implementation::construct(static_cast<M&>(m), storage, static_cast<parameters_type const&>(a), listener);
		}
		else
		{
			return new (storage) storage_type(static_cast<parameters_type const&>(a), listener);
		}
	}

	static result<void> start(multiplexer& m, async_operation& s)
	{
		return implementation::start(static_cast<M&>(m), static_cast<storage_type&>(s));
	}

	static result<async_operation*> construct_and_start(multiplexer& m, storage_ptr const storage, async_operation_parameters const& a, async_operation_listener* const listener)
	{
		if constexpr (requires { implementation::construct_and_start; })
		{
			return implementation::construct_and_start(static_cast<M&>(m), storage, static_cast<parameters_type const&>(a), listener);
		}
		else
		{
			allio_TRY(s, construct(m, storage, a, listener));
			allio_TRYV(start(m, *s));
			return s;
		}
	}

	static result<void> cancel(multiplexer& m, async_operation& s)
	{
		if constexpr (requires { implementation::cancel; })
		{
			return implementation::cancel(static_cast<M&>(m), static_cast<storage_type&>(s));
		}
		else
		{
			return allio_ERROR(make_error_code(std::errc::not_supported));
		}
	}

	static void destroy(async_operation& s)
	{
		static_cast<storage_type&>(s).~storage_type();
	}
};

template<typename M, typename H, typename O>
struct multiplexer_handle_operation_translation<M, H, O, 0>
{
	static constexpr storage_requirements operation_storage_requirements = {};

	static constexpr bool is_synchronous()
	{
		return true;
	}

	static result<async_operation*> construct(multiplexer&, storage_ptr, async_operation_parameters const&, async_operation_listener*)
	{
		return allio_ERROR(make_error_code(std::errc::not_supported));
	}

	static result<void> start(multiplexer&, async_operation&)
	{
		return allio_ERROR(make_error_code(std::errc::not_supported));
	}

	static result<async_operation*> construct_and_start(multiplexer&, storage_ptr, async_operation_parameters const&, async_operation_listener*)
	{
		return allio_ERROR(make_error_code(std::errc::not_supported));
	}

	static result<void> cancel(multiplexer&, async_operation&)
	{
		return allio_ERROR(make_error_code(std::errc::not_supported));
	}

	static void destroy(async_operation&)
	{
	}
};

template<typename M, typename H, typename Os = typename H::async_operations>
struct static_multiplexer_handle_relation;

template<typename M, typename H, typename... Os>
struct static_multiplexer_handle_relation<M, H, type_list<Os...>>
{
	static async_operation_descriptor const operations[];
	static multiplexer_handle_relation const instance;
};

template<typename M, typename H, typename... Os>
constinit async_operation_descriptor const static_multiplexer_handle_relation<M, H, type_list<Os...>>::operations[] =
{
	{
		.synchronous = multiplexer_handle_operation_translation<M, H, Os>::is_synchronous(),
		.construct = multiplexer_handle_operation_translation<M, H, Os>::construct,
		.start = multiplexer_handle_operation_translation<M, H, Os>::start,
		.construct_and_start = multiplexer_handle_operation_translation<M, H, Os>::construct_and_start,
		.cancel = multiplexer_handle_operation_translation<M, H, Os>::cancel,
		.destroy = multiplexer_handle_operation_translation<M, H, Os>::destroy,
	}...
};

template<typename M, typename H, typename... Os>
constinit multiplexer_handle_relation const static_multiplexer_handle_relation<M, H, type_list<Os...>>::instance =
{
	.register_handle = multiplexer_handle_translation<M, H>::register_handle,
	.deregister_handle = multiplexer_handle_translation<M, H>::deregister_handle,
	.operation_storage_requirements = (multiplexer_handle_operation_translation<M, H, Os>::operation_storage_requirements | ...),
	.operations = operations,
};

struct handle_relation_info
{
	type_id<> type;
	multiplexer_handle_relation const* relation;
};

result<multiplexer_handle_relation const*> find_handle_relation(
	std::span<const handle_relation_info> handle_types, type_id<handle> handle_type);

template<typename M, typename... Hs>
struct static_multiplexer_handle_relation_provider;

template<typename M, typename... Hs>
struct static_multiplexer_handle_relation_provider<M, type_list<Hs...>>
{
	static constexpr handle_relation_info s_handles[sizeof...(Hs)] =
	{
		{ type_of<Hs>(), &static_multiplexer_handle_relation<M, Hs>::instance }...
	};

public:
	static result<multiplexer_handle_relation const*> find_handle_relation(type_id<handle> const handle_type)
	{
		return detail::find_handle_relation(s_handles, handle_type);
	}
};

} // namespace detail

using detail::static_multiplexer_handle_relation_provider;

#define allio_MULTIPLEXER_HANDLE_RELATION(M, H) \
	template struct ::allio::detail::static_multiplexer_handle_relation<M, H>

} // namespace allio
