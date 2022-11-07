#pragma once

#include <allio/detail/assert.hpp>
#include <allio/dynamic_storage.hpp>
#include <allio/handle.hpp>
#include <allio/multiplexer.hpp>

#include <unifex/sender_concepts.hpp>
#include <unifex/receiver_concepts.hpp>

#include <type_traits>

namespace allio {

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
			, m_args(static_cast<decltype(sender.m_args)&&>(sender.m_args))
			, m_receiver(static_cast<Receiver&&>(receiver))
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

				auto r = [&]() -> result<async_operation_ptr>
				{
					allio_TRY(storage, m_storage.get(relation.operation_storage_requirements));
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
			// Signaling the receiver invalidates *this.
			// Destroy the operation state before signaling.
			m_operation_descriptor->destroy(*m_operation);

			if (std::error_code const result = operation.get_result())
			{
				if (result.default_error_condition() == make_error_code(std::errc::operation_canceled))
				{
					unifex::set_done(static_cast<Receiver&&>(m_receiver));
				}
				else
				{
					set_error(result);
				}
			}
			else
			{
				if constexpr (std::is_void_v<result_type>)
				{
					unifex::set_value(static_cast<Receiver&&>(m_receiver));
				}
				else
				{
					unifex::set_value(static_cast<Receiver&&>(m_receiver), static_cast<result_type&&>(this->result));
				}
			}
		}

		void set_error(std::error_code const result) noexcept
		{
			if constexpr (requires { unifex::set_error(static_cast<Receiver&&>(m_receiver), result); })
			{
				unifex::set_error(static_cast<Receiver&&>(m_receiver), result);
			}
			else
			{
				unifex::set_error(static_cast<Receiver&&>(m_receiver), std::make_exception_ptr(std::system_error(result, "basic_sender")));
			}
		}
	};

	size_t m_operation_index;
	io::parameters_with_handle<Operation> m_args;

public:
	static constexpr bool sends_done = true;

	template<template<typename...> typename Variant, template<typename...> typename Tuple>
	using value_types = Variant<typename basic_sender_helper<typename io::parameters<Operation>::result_type>::template value_tuple<Tuple>::type>;

	template<template<typename...> typename Variant>
	using error_types = Variant<std::error_code>;


	template<typename Handle, typename... Args>
	basic_sender(Handle& handle, Args&&... args)
		: m_operation_index(type_list_index<typename Handle::async_operations, Operation>)
		, m_args(handle, static_cast<Args&&>(args)...)
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
		return { static_cast<basic_sender&&>(*this), static_cast<Receiver&&>(receiver) };
	}
};


namespace detail {

template<typename T>
struct result_value_type
{
	static_assert(sizeof(T) == 0);
};

template<typename T>
struct result_value_type<result<T>>
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
		: m_receiver(static_cast<Receiver&&>(receiver))
	{
	}

	void set_value(std::same_as<result<T>> auto&& result) &&
	{
		if (result)
		{
			if constexpr (std::is_void_v<T>)
			{
				unifex::set_value(static_cast<Receiver&&>(m_receiver));
			}
			else
			{
				unifex::set_value(static_cast<Receiver&&>(m_receiver), *static_cast<decltype(result)&&>(result));
			}
		}
		else
		{
			unifex::set_error(static_cast<Receiver&&>(m_receiver), static_cast<decltype(result)&&>(result).error());
		}
	}

	void set_error(auto&& error) && noexcept
	{
		//TODO: enable the assert...
		//static_assert(sizeof(error) == 0,
		//	"");
	}

	void set_done() && noexcept
	{
		unifex::set_done(static_cast<Receiver&&>(m_receiver));
	}
};

template<typename Sender>
class result_into_error_sender
{
	Sender m_sender;

public:
	static constexpr bool sends_done = unifex::sender_traits<Sender>::sends_done;

	template<template<typename...> typename Variant, template<typename...> typename Tuple>
	using value_types = unifex::sender_value_types_t<Sender, Variant, result_into_error_helper<Tuple>::template value_tuple>;

	template<template<typename...> typename Variant>
	using error_types = Variant<std::error_code>;

	result_into_error_sender(auto&& sender)
		: m_sender(static_cast<decltype(sender)&&>(sender))
	{
	}

	template<typename Receiver>
	auto connect(Receiver&& receiver) &&
	{
		using receiver_type = result_into_error_receiver<std::remove_cvref_t<Receiver>,
			result_value_type_t<unifex::sender_single_value_return_type_t<Sender>>>;
		return unifex::connect(static_cast<Sender&&>(m_sender), receiver_type(static_cast<Receiver&&>(receiver)));
	}
};

struct result_into_error_fn
{
	template<unifex::_single_typed_sender Sender>
	result_into_error_sender<std::remove_cvref_t<Sender>> operator()(Sender&& sender) const
	{
		return { static_cast<Sender&&>(sender) };
	}
};


template<template<typename...> typename Tuple>
struct error_into_result_helper
{
	template<typename... Values>
	using value_tuple = Tuple<result<Values>...>;
};

template<typename Receiver, typename T>
class error_into_result_receiver
{
	Receiver m_receiver;

public:
	error_into_result_receiver(Receiver&& receiver)
		: m_receiver(static_cast<Receiver&&>(receiver))
	{
	}

	void set_value(std::same_as<T> auto&& value) &&
	{
		unifex::set_value(static_cast<Receiver&&>(m_receiver), result<T>(static_cast<decltype(value)&&>(value)));
	}

	template<typename E>
	void set_error(E&& error) && noexcept
	{
		unifex::set_error(static_cast<Receiver&&>(m_receiver), result<T>(allio_ERROR(static_cast<E&&>(error))));
	}

	void set_done() && noexcept
	{
		unifex::set_done(static_cast<Receiver&&>(m_receiver));
	}
};

template<typename Sender>
class error_into_result_sender
{
	Sender m_sender;

public:
	static constexpr bool sends_done = unifex::sender_traits<Sender>::sends_done;

	template<template<typename...> typename Variant, template<typename...> typename Tuple>
	using value_types = unifex::sender_value_types_t<Sender, Variant, error_into_result_helper<Tuple>::template value_tuple>;

	template<template<typename...> typename Variant>
	using error_types = Variant<>;

	error_into_result_sender(auto&& sender)
		: m_sender(static_cast<decltype(sender)&&>(sender))
	{
	}

	template<typename Receiver>
	auto connect(Receiver&& receiver) &&
	{
		using receiver_type = error_into_result_receiver<std::remove_cvref_t<Receiver>,
			unifex::sender_single_value_return_type_t<Sender>>;
		return unifex::connect(static_cast<Sender&&>(m_sender), receiver_type(static_cast<Receiver&&>(receiver)));
	}
};

struct error_into_result_fn
{
	template<unifex::_single_typed_sender Sender>
	error_into_result_sender<std::remove_cvref_t<Sender>> operator()(Sender&& sender) const
	{
		return { static_cast<Sender&&>(sender) };
	}
};


template<typename Receiver>
class error_into_except_receiver
{
	Receiver m_receiver;

public:
	error_into_except_receiver(Receiver&& receiver)
		: m_receiver(static_cast<Receiver&&>(receiver))
	{
	}

	void set_value(auto&&... values) &&
	{
		unifex::set_value(static_cast<Receiver&&>(m_receiver), static_cast<decltype(values)&&>(values)...);
	}

	void set_error(std::exception_ptr&& error) && noexcept
	{
		unifex::set_error(static_cast<Receiver&&>(m_receiver), static_cast<std::exception_ptr&&>(error));
	}

	void set_error(std::error_code const error) && noexcept
	{
		if constexpr (requires { unifex::set_error(static_cast<Receiver&&>(m_receiver), error); })
		{
			unifex::set_error(static_cast<Receiver&&>(m_receiver), error);
		}
		else
		{
			unifex::set_error(static_cast<Receiver&&>(m_receiver), std::make_exception_ptr(std::system_error(error, "error_into_except")));
		}
	}

	void set_done() && noexcept
	{
		unifex::set_done(static_cast<Receiver&&>(m_receiver));
	}
};

template<typename Sender>
class error_into_except_sender
{
	Sender m_sender;

public:
	static constexpr bool sends_done = unifex::sender_traits<Sender>::sends_done;

	template<template<typename...> typename Variant, template<typename...> typename Tuple>
	using value_types = unifex::sender_value_types_t<Sender, Variant, Tuple>;

	template<template<typename...> typename Variant>
	using error_types = Variant<std::exception_ptr>;

	error_into_except_sender(Sender&& sender)
		: m_sender(static_cast<Sender&&>(sender))
	{
	}

	template<unifex::receiver Receiver>
	auto connect(Receiver&& receiver) &&
	{
		using receiver_type = error_into_except_receiver<std::remove_cvref_t<Receiver>>;
		return unifex::connect(static_cast<Sender&&>(m_sender), receiver_type(static_cast<Receiver&&>(receiver)));
	}
};

struct error_into_except_fn
{
	template<unifex::sender Sender>
	error_into_except_sender<std::remove_cvref_t<Sender>> operator()(Sender&& sender) const
	{
		return { static_cast<Sender&&>(sender) };
	}
};

} // namespace detail

inline constexpr detail::result_into_error_fn result_into_error = {};
inline constexpr detail::error_into_result_fn error_into_result = {};
inline constexpr detail::error_into_except_fn error_into_except = {};

} // namespace allio
