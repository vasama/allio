#pragma once

#include <allio/detail/execution.hpp>
#include <allio/dynamic_storage.hpp>
#include <allio/handle.hpp>
#include <allio/multiplexer.hpp>

#include <vsm/assert.h>
#include <vsm/concepts.hpp>
#include <vsm/lazy.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <optional>
#include <type_traits>
#include <variant>

namespace allio {

//TODO: Clean up this header. Move the error sender algorithms to vsm.

template<typename T>
struct basic_sender_helper
{
	template<template<typename...> typename Tuple>
	//using value_tuple = Tuple<T>;
	struct value_tuple { using type = Tuple<T>; };
};

template<>
struct basic_sender_helper<void>
{
	template<template<typename...> typename Tuple>
	//using value_tuple = Tuple<>;
	struct value_tuple { using type = Tuple<>; };
};

template<typename Operation>
class basic_sender
{
	using handle_type = typename io::parameters<Operation>::handle_type;
	using result_type = typename io::parameters<Operation>::result_type;

	template<typename Receiver>
	class operation
		: async_operation_listener
		, io::result_storage<result_type>
	{
		union
		{
			size_t m_operation_index;
			async_operation_descriptor const* m_operation_descriptor;
		};
		io::parameters_with_result<Operation> m_args;
		Receiver m_receiver;
		small_dynamic_storage<96> m_storage;
		async_operation* m_operation = nullptr;

	public:
		operation(basic_sender&& sender, Receiver&& receiver)
			: m_operation_index(sender.m_operation_index)
			, m_args(vsm_move(sender.m_args))
			, m_receiver(vsm_forward(receiver))
		{
		}

		void start() & noexcept
		{
			handle const& handle = *m_args.handle;
			if (multiplexer* const multiplexer = handle.get_multiplexer())
			{
				m_args.bind_storage(*this);

				multiplexer_handle_relation const& relation = handle.get_multiplexer_relation();
				async_operation_descriptor const& descriptor = relation.operations[m_operation_index];

				auto r = [&]() -> vsm::result<async_operation_ptr>
				{
					vsm_try(storage, m_storage.get(relation.operation_storage_requirements));
					return multiplexer->construct_and_start(descriptor, storage, m_args, this);
				}();

				if (r)
				{
					m_operation_descriptor = &descriptor;
					// The async operation lifetime is managed by the S&R operation state.
					m_operation = r->release();
				}
				else
				{
					set_error(r.error());
				}
			}
			else
			{
				set_error(error::handle_is_not_multiplexable);
			}
		}

	private:
		void concluded(async_operation& operation) override
		{
			std::error_code const e = operation.get_result();

			// Signaling the receiver invalidates *this.
			// Destroy the operation state before signaling.
			m_operation_descriptor->destroy(*m_operation);

			if (e)
			{
				if (e.default_error_condition() == make_error_code(std::errc::operation_canceled))
				{
					detail::execution::allio_detail_set_stopped(vsm_move(m_receiver));
				}
				else
				{
					set_error(e);
				}
			}
			else
			{
				if constexpr (std::is_void_v<result_type>)
				{
					detail::execution::set_value(vsm_move(m_receiver));
				}
				else
				{
					detail::execution::set_value(vsm_move(m_receiver), vsm_move(this->result));
				}
			}
		}

		void set_error(std::error_code const result) noexcept
		{
			if constexpr (requires { detail::execution::set_error(vsm_move(m_receiver), result); })
			{
				detail::execution::set_error(vsm_move(m_receiver), result);
			}
			else
			{
				detail::execution::set_error(vsm_move(m_receiver), std::make_exception_ptr(std::system_error(result, "allio::basic_sender")));
			}
		}
	};

	size_t m_operation_index;
	io::parameters_with_handle<Operation> m_args;

public:
#if allio_config_unifex
	static constexpr bool sends_done = true;

	template<template<typename...> typename Variant, template<typename...> typename Tuple>
	using value_types = Variant<typename basic_sender_helper<typename io::parameters<Operation>::result_type>::template value_tuple<Tuple>::type>;

	template<template<typename...> typename Variant>
	using error_types = Variant<std::error_code>;
#else
	using completion_signatures = stdexec::completion_signatures<
		stdexec::set_value_t(result_type),
		stdexec::set_error_t(std::error_code),
		stdexec::set_stopped_t()
	>;
#endif


	template<typename Handle, typename... Args>
	basic_sender(Handle& handle, Args&&... args)
		: m_operation_index(type_list_index<typename Handle::async_operations, Operation>)
		, m_args(handle, vsm_forward(args)...)
	{
	}

	io::parameters<Operation>& arguments()
	{
		return m_args;
	}

	io::parameters<Operation> const& arguments() const
	{
		return m_args;
	}

	template<typename Receiver>
	operation<std::remove_cvref_t<Receiver>> connect(Receiver&& receiver) noexcept
	{
		return { vsm_move(*this), vsm_forward(receiver) };
	}
};


namespace detail {

template<typename T>
struct result_value_type
{
	static_assert(sizeof(T) == 0);
};

template<typename T>
struct result_value_type<vsm::result<T>>
{
	using type = T;
};

template<typename T>
using result_value_type_t = typename result_value_type<T>::type;


template<template<typename...> typename Tuple>
struct result_into_error_helper
{
	template<typename... Values>
	using value_tuple = Tuple<result_value_type_t<Values>...>;
};

template<typename Receiver, typename T>
class result_into_error_receiver
{
	Receiver m_receiver;

public:
	result_into_error_receiver(Receiver&& receiver)
		: m_receiver(vsm_move(receiver))
	{
	}

	void set_value(std::same_as<vsm::result<T>> auto&& result) &&
	{
		if (result)
		{
			if constexpr (std::is_void_v<T>)
			{
				detail::execution::set_value(vsm_move(m_receiver));
			}
			else
			{
				detail::execution::set_value(vsm_move(m_receiver), *vsm_move(result));
			}
		}
		else
		{
			detail::execution::set_error(vsm_move(m_receiver), vsm_move(result).error());
		}
	}

	void set_error(auto&& error) && noexcept
	{
		static_assert(sizeof(error) == 0);
	}

	void allio_detail_set_stopped() && noexcept
	{
		detail::execution::allio_detail_set_stopped(vsm_move(m_receiver));
	}
};

template<typename Sender>
class result_into_error_sender
{
	Sender m_sender;

public:
#if allio_config_unifex
	static constexpr bool sends_done = unifex::sender_traits<Sender>::sends_done;

	template<template<typename...> typename Variant, template<typename...> typename Tuple>
	using value_types = unifex::sender_value_types_t<Sender, Variant, result_into_error_helper<Tuple>::template value_tuple>;

	template<template<typename...> typename Variant>
	using error_types = Variant<std::error_code>;
#else
#endif

	result_into_error_sender(auto&& sender)
		: m_sender(vsm_move(sender))
	{
	}

	template<typename Receiver>
	auto connect(Receiver&& receiver) &&
	{
	#if allio_config_unifex
		using receiver_type = result_into_error_receiver<
			std::remove_cvref_t<Receiver>,
			result_value_type_t<unifex::sender_single_value_return_type_t<Sender>>>;
	#else
		using receiver_type = result_into_error_receiver<
			std::remove_cvref_t<Receiver>,
			result_value_type_t<stdexec::__single_sender_value_t<Sender>>>;
	#endif

		return detail::execution::connect(vsm_move(m_sender), receiver_type(vsm_forward(receiver)));
	}
};

struct result_into_error_fn
{
	//template<stdexec::__single_typed_sender Sender>
	template<unifex::_single_typed_sender Sender>
	result_into_error_sender<std::remove_cvref_t<Sender>> operator()(Sender&& sender) const
	{
		return { vsm_forward(sender) };
	}
};


template<template<typename...> typename Tuple>
struct error_into_result_helper
{
	template<typename... Values>
	using value_tuple = Tuple<vsm::result<Values>...>;
};

template<typename Receiver, typename T>
class error_into_result_receiver
{
	Receiver m_receiver;

public:
	error_into_result_receiver(Receiver&& receiver)
		: m_receiver(vsm_move(receiver))
	{
	}

	void set_value(std::same_as<T> auto&& value) &&
	{
		detail::execution::set_value(vsm_move(m_receiver), vsm::result<T>(vsm_forward(value)));
	}

	template<typename E>
	void set_error(E&& error) && noexcept
	{
		detail::execution::set_error(vsm_move(m_receiver), vsm::result<T>(vsm::unexpected(vsm_forward(error))));
	}

	void allio_detail_set_stopped() && noexcept
	{
		detail::execution::allio_detail_set_stopped(vsm_move(m_receiver));
	}
};

template<typename Sender>
class error_into_result_sender
{
	Sender m_sender;

public:
#if allio_config_unifex
	static constexpr bool sends_done = unifex::sender_traits<Sender>::sends_done;

	template<template<typename...> typename Variant, template<typename...> typename Tuple>
	using value_types = unifex::sender_value_types_t<Sender, Variant, error_into_result_helper<Tuple>::template value_tuple>;

	template<template<typename...> typename Variant>
	using error_types = Variant<>;
#else
#endif


	error_into_result_sender(auto&& sender)
		: m_sender(vsm_forward(sender))
	{
	}

	template<typename Receiver>
	auto connect(Receiver&& receiver) &&
	{
	#if allio_config_unifex
		using receiver_type = error_into_result_receiver<
			std::remove_cvref_t<Receiver>,
			unifex::sender_single_value_return_type_t<Sender>>;
	#else
		using receiver_type = error_into_result_receiver<
			std::remove_cvref_t<Receiver>,
			stdexec::__single_sender_value_t<Sender>>;
	#endif

		return detail::execution::connect(vsm_move(m_sender), receiver_type(vsm_forward(receiver)));
	}
};

struct error_into_result_fn
{
	//template<stdexec::__single_typed_sender Sender>
	template<unifex::_single_typed_sender Sender>
	error_into_result_sender<std::remove_cvref_t<Sender>> operator()(Sender&& sender) const
	{
		return { vsm_forward(sender) };
	}
};


template<typename Receiver>
class error_into_except_receiver
{
	Receiver m_receiver;

public:
	error_into_except_receiver(Receiver&& receiver)
		: m_receiver(vsm_move(receiver))
	{
	}

	void set_value(auto&&... values) &&
	{
		detail::execution::set_value(vsm_move(m_receiver), vsm_forward(values)...);
	}

	void set_error(std::exception_ptr error) && noexcept
	{
		detail::execution::set_error(vsm_move(m_receiver), vsm_move(error));
	}

	void set_error(std::error_code const error) && noexcept
	{
		detail::execution::set_error(vsm_move(m_receiver), std::make_exception_ptr(std::system_error(error, "allio::error_into_except")));
	}

	void allio_detail_set_stopped() && noexcept
	{
		detail::execution::allio_detail_set_stopped(vsm_move(m_receiver));
	}
};

template<typename Sender>
class error_into_except_sender
{
	Sender m_sender;

public:
#if allio_config_unifex
	static constexpr bool sends_done = unifex::sender_traits<Sender>::sends_done;

	template<template<typename...> typename Variant, template<typename...> typename Tuple>
	using value_types = unifex::sender_value_types_t<Sender, Variant, Tuple>;

	template<template<typename...> typename Variant>
	//TODO: Empty if no error_code sent by source sender.
	using error_types = Variant<std::exception_ptr>;
#else
#endif


	error_into_except_sender(Sender&& sender)
		: m_sender(vsm_move(sender))
	{
	}

	//template<stdexec::receiver Receiver>
	template<unifex::receiver Receiver>
	auto connect(Receiver&& receiver) &&
	{
		using receiver_type = error_into_except_receiver<std::remove_cvref_t<Receiver>>;
		return detail::execution::connect(vsm_move(m_sender), receiver_type(vsm_forward(receiver)));
	}
};

struct error_into_except_fn
{
	//template<stdexec::sender Sender>
	template<unifex::sender Sender>
	error_into_except_sender<std::remove_cvref_t<Sender>> operator()(Sender&& sender) const
	{
		return { vsm_forward(sender) };
	}
};


template<typename Handle, typename SenderFactory>
class initialize_handle_sender
{
	using sender_type = std::invoke_result_t<SenderFactory&&, Handle&>;

	multiplexer* m_multiplexer;
	SenderFactory m_sender_factory;

	template<typename Receiver>
	class operation
	{
		class receiver
		{
			Handle* m_handle;
			vsm_no_unique_address Receiver m_receiver;

		public:
			explicit receiver(Handle& handle, auto&& receiver)
				: m_handle(&handle)
				, m_receiver(vsm_forward(receiver))
			{
			}

			void set_value() &&
			{
				detail::execution::set_value(vsm_move(m_receiver), vsm_move(*m_handle));
			}

			void set_error(auto&& error) && noexcept
			{
				detail::execution::set_error(vsm_move(m_receiver), vsm_forward(error));
			}

			void allio_detail_set_stopped() && noexcept
			{
				detail::execution::allio_detail_set_stopped(vsm_move(m_receiver));
			}

		private:
			friend class operation;
		};
		using operation_type = detail::execution::connect_result_t<sender_type, receiver>;

		initialize_handle_sender m_sender;
		std::variant<Receiver, operation_type> m_variant;
		Handle m_handle;

	public:
		operation(auto&& sender, auto&& receiver)
			: m_sender(vsm_forward(sender))
			, m_variant(vsm_forward(receiver))
		{
		}

		void start() & noexcept
		{
			Receiver&& user_receiver = std::get<Receiver>(vsm_move(m_variant));

			if (auto const r = m_handle.set_multiplexer(m_sender.m_multiplexer))
			{
				receiver receiver(m_handle, vsm_move(user_receiver));

				detail::execution::start(m_variant.template emplace<operation_type>(vsm_lazy(
					detail::execution::connect(
						vsm_move(m_sender.m_sender_factory)(m_handle),
						vsm_move(receiver))
				)));
			}
			else
			{
				detail::execution::set_error(vsm_move(user_receiver), r.error());
			}
		}
	};

public:
#if allio_config_unifex
	static constexpr bool sends_done = unifex::sender_traits<sender_type>::sends_done;

	template<template<typename...> typename Variant, template<typename...> typename Tuple>
	using value_types = Variant<Tuple<Handle>>;

	//TODO: Include sender_type error types.
	template<template<typename...> typename Variant>
	using error_types = Variant<std::error_code>;
#else
	using completion_signatures = stdexec::make_completion_signatures<
		sender_type,
		stdexec::no_env,
		stdexec::completion_signatures<
			stdexec::set_value_t(Handle),
			stdexec::set_error_t(std::error_code)
		>,
		stdexec::__mconst<stdexec::completion_signatures<>>::__f
	>;
#endif


	explicit initialize_handle_sender(multiplexer& multiplexer, vsm::any_cvref_of<SenderFactory> auto&& sender_factory)
		: m_multiplexer(&multiplexer)
		, m_sender_factory(vsm_forward(sender_factory))
	{
	}

	template<detail::execution::receiver Receiver>
	operation<std::remove_cvref_t<Receiver>> connect(Receiver&& receiver) &&
	{
		return { vsm_move(*this), vsm_forward(receiver) };
	}
};

} // namespace detail

inline constexpr detail::result_into_error_fn result_into_error = {};
inline constexpr detail::error_into_result_fn error_into_result = {};
inline constexpr detail::error_into_except_fn error_into_except = {};

template<typename Handle, typename SenderFactory>
auto initialize_handle(multiplexer& multiplexer, SenderFactory&& sender_factory)
{
	return detail::initialize_handle_sender<Handle, std::remove_cvref_t<SenderFactory>>(multiplexer, vsm_forward(sender_factory));
}

} // namespace allio
