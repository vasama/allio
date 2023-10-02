#pragma once

#include <allio/detail/execution.hpp>
#include <allio/detail/io.hpp>

#include <vsm/standard.hpp>
#include <vsm/type_traits.hpp>

namespace allio::detail {

template<typename H, typename O>
class io_sender
{
	using handle_base_type = std::remove_cv_t<typename O::handle_type>;
	static_assert(std::is_base_of_v<handle_base_type, H>);

	using handle_type = vsm::select_t<std::is_const_v<typename O::handle_type>, H const, H>;
	using multiplexer_type = typename H::multiplexer_type;

	using operation_type = operation_t<multiplexer_type, handle_base_type, O>;

	using params_type = io_parameters_t<O>;
	using result_type = typename O::result_type;

	template<typename Receiver>
	class operation : io_callback
	{
		vsm_no_unique_address handle_type* m_handle;
		vsm_no_unique_address operation_type m_operation;
		vsm_no_unique_address io_result_storage<result_type> m_result;
		vsm_no_unique_address Receiver m_receiver;

	public:
		explicit operation(auto&& sender, auto&& receiver)
			noexcept(noexcept(Receiver(vsm_forward(receiver))))
			: m_handle(sender.m_handle)
			, m_operation(*this, vsm_forward(sender).m_args)
			, m_receiver(vsm_forward(receiver))
		{
		}

	private:
		friend void tag_invoke(ex::start_t, operation& self) noexcept
		{
			self.handle_result(submit_io(*self.m_handle, self.m_operation));
		}

		void notify(operation_base& operation, io_status const status) noexcept override
		{
			vsm_assert(&operation == &m_operation);
			handle_result(notify_io(*m_handle, m_operation, status));
		}

		void handle_result(vsm::result<io_result> const& r)
		{
			if (r)
			{
				if (io_result const& v = *r)
				{
					if (std::error_code const& e = *v)
					{
						if (e == std::error_code(error::async_operation_cancelled))
						{
							ex::set_stopped(vsm_move(m_receiver));
						}
						else
						{
							ex::set_error(vsm_move(m_receiver), r.error());
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
			}
			else
			{
				ex::set_error(vsm_move(m_receiver), r.error());
			}
		}
	};

	handle_type* m_handle;
	vsm_no_unique_address params_type m_args;

public:
	using completion_signatures = ex::completion_signatures<
		ex::set_value_t(result_type),
		ex::set_error_t(std::error_code),
		ex::set_stopped_t()
	>;

	template<std::convertible_to<params_type> Args>
	explicit io_sender(handle_type& handle, Args&& args)
		: m_handle(&handle)
		, m_args(vsm_forward(args))
	{
	}

	template<typename R>
	friend operation<std::remove_cvref_t<R>> tag_invoke(
		ex::connect_t,
		vsm::any_cvref_of<io_sender> auto&& sender,
		R&& receiver)
	{
		return operation<std::remove_cvref_t<R>>(vsm_forward(sender), vsm_forward(receiver));
	}
};

} // namespace allio::detail
