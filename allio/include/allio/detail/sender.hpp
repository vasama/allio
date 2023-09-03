#pragma once

#include <allio/detail/execution.hpp>

namespace allio::detail {

template<typename H, typename O>
class basic_sender
{
	using multiplexer_handle_type = typename H::multiplexer_handle_type;
	using multiplexer_type = multiplexer_t<M>;
	using multiplexer_borrow_type = borrow_t<M>;

	using result_type = io_result_type<O>;

	template<typename Receiver>
	class operation : async_operation_listener
	{
		vsm_no_unique_address multiplexer_borrow_type m_multiplexer;
		vsm_no_unique_address io_result_storage<result_type> m_result;
		vsm_no_unique_address async_operation_storage<M, H, O> m_storage;
		vsm_no_unique_address Receiver m_receiver;

	public:
		explicit operation(auto&& sender, auto&& receiver)
			: m_storage(vsm_forward(sender).m_args)
			, m_receiver(vsm_forward(receiver))
		{
		}

		void start() & noexcept
		{
			auto const r = async_operation_facade<M, H, O>::submit(*m_multiplexer, m_storage);

			if (!r)
			{
				ex::set_error(vsm_move(m_receiver), r.error());
			}
		}

	private:
		void concluded(async_operation& operation) override
		{
			vsm_assert(&operation == &m_storage);

			if (std::error_code const error = m_storage.get_result())
			{
				if (error == std::error_code(error::async_operation_cancelled))
				{
					ex::set_stopped(vsm_move(m_receiver));
				}
				else
				{
					ex::set_error(vsm_move(m_receiver), error);
				}
			}
			else
			{
				if constexpr (std::is_void_v<result_type>)
				{
					ex::set_value(vsm_move(m_receiver));
				}
				else
				{
					ex::set_value(vsm_move(m_receiver), vsm_move(m_result.result));
				}
			}
		}
	};

	io_parameters_with_handle<O> m_args;

public:
	using completion_signatures = ex::completion_signatures<
		ex::set_value_t(result_type),
		ex::set_error_t(std::error_code),
		ex::set_stopped_t()
	>;

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
