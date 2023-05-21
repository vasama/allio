#pragma once

#include <allio/detail/execution.hpp>

#include <vsm/atomic.hpp>
#include <vsm/utility.hpp>

#include <type_traits>

namespace allio::detail {

template<typename Scheduler>
class scheduler_ref
{
	Scheduler* m_scheduler;

public:
	scheduler_ref(Scheduler& scheduler)
		: m_scheduler(&scheduler)
	{
	}

	decltype(auto) schedule() const
		noexcept(noexcept(m_scheduler->schedule()))
	{
		return m_scheduler->schedule();
	}

	friend bool operator==(scheduler_ref const&, scheduler_ref const&) = default;
};

template<typename Scheduler>
scheduler_ref(Scheduler&&) -> scheduler_ref<std::remove_reference_t<Scheduler>>;


class manual_scheduler
{
	struct operation_base
	{
		union
		{
			manual_scheduler* m_scheduler;
			operation_base* m_next_operation;
		};
		void(*m_send)(operation_base& operation);
	};

	//TODO: Use vsm::intrusive::mpsc_queue.
	vsm::atomic<operation_base*> m_operation_list;

	class sender
	{
		manual_scheduler* m_scheduler;

		template<typename Receiver>
		class operation : operation_base
		{
			Receiver m_receiver;
		
		public:
			explicit operation(manual_scheduler& scheduler, auto&& receiver)
				: operation_base
				{
					.m_scheduler = &scheduler,
					.m_send = operation::send,
				}
				, m_receiver(vsm_forward(receiver))
			{
			}

			void start() & noexcept
			{
				auto& scheduler = *m_scheduler;

				operation_base* expected = scheduler.m_operation_list.load(std::memory_order_acquire);

				do
				{
					m_next_operation = expected;
				}
				while (!scheduler.m_operation_list.compare_exchange_weak(
					expected, this,
					std::memory_order_release, std::memory_order_acquire));
			}
			
		private:
			static void send(operation_base& self)
			{
				detail::execution::set_value(static_cast<operation&&>(self).m_receiver);
			}
		};

	public:
		static constexpr bool sends_done = false;

		template<template<typename...> typename Variant, template<typename...> typename Tuple>
		using value_types = Variant<Tuple<>>;

		template<template<typename...> typename Variant>
		using error_types = Variant<>;

		explicit sender(manual_scheduler& scheduler)
			: m_scheduler(&scheduler)
		{
		}
	
		template<typename Receiver>
		operation<std::remove_cvref_t<Receiver>> connect(Receiver&& receiver) const
		{
			return operation<std::remove_cvref_t<Receiver>>(*m_scheduler, vsm_forward(receiver));
		}
	};

public:
	sender schedule()
	{
		return sender(*this);
	}

	bool pump()
	{
		if (m_operation_list.load(std::memory_order_acquire) == nullptr)
		{
			return false;
		}

		operation_base* operation_list = reverse_operation_list(
			m_operation_list.exchange(nullptr, std::memory_order_acq_rel));

		while (operation_list != nullptr)
		{
			operation_base* const operation =
				std::exchange(operation_list, operation_list->m_next_operation);

			operation->m_send(*operation);
		}

		return true;
	}

private:
	operation_base* reverse_operation_list(operation_base* head)
	{
		operation_base* prev = nullptr;

		while (head != nullptr)
		{
			head = std::exchange(head->m_next_operation, std::exchange(prev, head));
		}

		return prev;
	}
};

} // namespace allio::detail
