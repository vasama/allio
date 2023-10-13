#pragma once

#include <allio/detail/execution.hpp>
#include <allio/detail/io.hpp>

#include <vsm/standard.hpp>
#include <vsm/type_traits.hpp>

namespace allio::detail {

template<bool>
struct _set_value_signature;

template<>
struct _set_value_signature<0>
{
	template<typename R>
	using type = ex::set_value_t(R);
};

template<>
struct _set_value_signature<1>
{
	template<typename R>
	using type = ex::set_value_t();
};

template<typename R>
using set_value_signature = typename _set_value_signature<std::is_void_v<R>>::template type<R>;

template<typename H, typename O>
class io_sender
{
	using handle_base_type = std::remove_cv_t<typename O::handle_type>;
	static_assert(std::is_base_of_v<handle_base_type, H>);

	using handle_type = vsm::select_t<std::is_const_v<typename O::handle_type>, H const, H>;

	using multiplexer_handle_type = typename H::multiplexer_handle_type;
	using multiplexer_type = typename multiplexer_handle_type::multiplexer_type;

	using operation_type = operation_t<multiplexer_type, handle_base_type, O>;

	using params_type = io_parameters_t<O>;
	using result_type = io_result_t<O, multiplexer_handle_type>;

	template<typename Receiver>
	class operation : io_callback
	{
		vsm_no_unique_address handle_type* m_handle;
		vsm_no_unique_address operation_type m_operation;
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
			//self.handle_result(submit_io(*self.m_handle, self.m_operation));
			self.handle_result(tag_invoke(submit_io, *self.m_handle, self.m_operation));
		}

		void notify(operation_base& operation, io_status const status) noexcept override
		{
			vsm_assert(&operation == &m_operation);
			handle_result(tag_invoke(notify_io, *m_handle, m_operation, status));
			//handle_result(notify_io(*m_handle, m_operation, status));
		}

		void handle_result(io_result2<result_type>&& r)
		{
			if (r.has_value())
			{
				// This is not a stream sender.
				vsm_assert(!r.is_pending());

				if constexpr (std::is_void_v<result_type>)
				{
					ex::set_value(vsm_move(m_receiver));
				}
				else
				{
					ex::set_value(vsm_move(m_receiver), vsm_move(*r));
				}
			}
			else if (r.is_pending())
			{
			}
			else if (r.is_cancelled())
			{
				ex::set_stopped(vsm_move(m_receiver));
			}
			else
			{
				vsm_assert(r.has_error());
				ex::set_error(vsm_move(m_receiver), r.error());
			}
		}
	};

	handle_type* m_handle;
	vsm_no_unique_address params_type m_args;

public:
	using completion_signatures = ex::completion_signatures<
		set_value_signature<result_type>,
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

template<typename H, typename O>
class io_handle_sender
{
};

} // namespace allio::detail
