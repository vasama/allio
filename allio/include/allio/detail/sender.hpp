#pragma once

namespace allio::detail {

template<bool>
struct basic_sender_helper;

template<>
struct basic_sender_helper<0>
{
	template<typename T, template<typename...> typename Tuple>
	using tuple = Tuple<T>;
};

template<>
struct basic_sender_helper<1>
{
	template<typename T, template<typename...> typename Tuple>
	using tuple = Tuple<>;
};

template<typename M, typename H, typename O>
class basic_sender
{
	using result_type = io_result_type<O>;

	template<typename Receiver>
	class operation
	{
		async_operation_storage<M, H, O> m_storage;
		Receiver m_receiver;

	public:
		explicit operation(auto&& sender, auto&& receiver)
			: m_storage(vsm_forward(sender).m_args)
			, m_receiver(vsm_forward(receiver))
		{
		}

		void start() & noexcept
		{

		}

	private:

	};

	io_parameters_with_handle<O> m_args;

public:
#if allio_config_unifex
	static constexpr bool sends_done = true;

	template<template<typename...> typename Variant, template<typename...> typename Tuple>
	using value_types = Variant<typename basic_sender_helper<std::is_void_v<result_type>>::template tuple<result_type, Tuple>>;

	template<template<typename...> typename Variant>
	using error_types = Variant<std::error_code>;
#else
	using completion_signatures = stdexec::completion_signatures<
		stdexec::set_value_t(result_type),
		stdexec::set_error_t(std::error_code),
		stdexec::set_stopped_t()
	>;
#endif

	explicit basic_sender(auto&&... args)
		: m_args(vsm_forward(args)...)
	{
	}

	template<typename R>
	operation<std::remove_cvref_t<R>> connect(R&& receiver) noexcept
	{
		return operation<std::remove_cvref_t<R>>(vsm_move(*this), vsm_forward(receiver));
	}
};

} // namespace allio::detail
