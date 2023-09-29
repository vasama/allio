#pragma once

#include <vsm/assert.h>
#include <vsm/atomic.hpp>
#include <vsm/intrusive/mpsc_queue.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_ptr.hpp>
#include <vsm/utility.hpp>

namespace allio::detail {

template<typename Event>
class basic_event_queue
{
	struct operation_base;

	struct operation_vtable
	{
		void(*set_value)(operation_base&);
		void(*set_stopped)(operation_base&);
	};

	struct operation_base : vsm::intrusive::mpsc_queue_link
	{
		using ptr_type = vsm::tag_ptr<operation_vtable const, bool>;

		vsm::atomic<ptr_type> vtable_and_cancel;

		explicit operation_base(operation_vtable const& vtable)
			: vtable_and_cancel(ptr_type(&vtable, false))
		{
		}
	};

	template<typename R>
	class operation : operation_base
	{
		basic_event_queue& m_queue;
		vsm_no_unique_address R m_receiver;
	
	public:
		explicit operation(basic_event_queue& queue, R&& receiver)
			: operation_base(s_vtable)
			, m_queue(queue)
			, m_receiver(vsm_forward(receiver))
		{
		}

		friend void tag_invoke(ex::start_t, operation& self) noexcept
		{
			if (self.m_queue.m_mpsc_queue.push(&self))
			{
				self.m_queue.m_event();
			}
		}

	private:
		static void set_value(operation_base& self)
		{
			ex::set_value(static_cast<operation&&>(self).m_receiver);
		}

		static void set_stopped(operation_base& self)
		{
			ex::set_stopped(static_cast<operation&&>(self).m_receiver);
		}

		static constexpr operation_vtable s_vtable =
		{
			.set_value = set_value,
			.set_stopped = set_stopped,
		};
	};

	class scheduler
	{
		class sender
		{
			class env
			{
				basic_event_queue* m_queue;
				
			public:
				explicit env(basic_event_queue& queue) noexcept
					: m_queue(&queue)
				{
				}

				template<typename CPO>
				friend scheduler tag_invoke(ex::get_completion_scheduler_t<CPO>, env const& self) noexcept
				{
					return scheduler(*self.m_queue);
				}
			};

			basic_event_queue* m_queue;

		public:
			using completion_signatures = ex::completion_signatures<
				ex::set_value_t(),
				ex::set_stopped_t()
			>;

			explicit sender(basic_event_queue& queue) noexcept
				: m_queue(&queue)
			{
			}

			template<typename R>
			friend operation<std::decay_t<R>> tag_invoke(ex::connect_t, sender const& sender, R&& receiver)
			{
				return operation<std::decay_t<R>>(*sender.m_queue, vsm_forward(receiver));
			}
			
			friend env tag_invoke(ex::get_env_t, sender const& self) noexcept
			{
				return env(*self.m_queue);
			}
		};
		static_assert(ex::sender<sender>);

		basic_event_queue* m_queue;

	public:
		explicit scheduler(basic_event_queue& queue) noexcept
			: m_queue(&queue)
		{
		}

		friend sender tag_invoke(ex::schedule_t, scheduler const& self) noexcept
		{
			return sender(*self.m_queue);
		}

		friend bool operator==(scheduler const&, scheduler const&) = default;
	};
	static_assert(ex::scheduler<scheduler>);

	Event m_event;
	vsm::intrusive::mpsc_queue<operation_base> m_mpsc_queue;
	vsm::intrusive::forward_list<operation_base> m_forward_list;

public:
	basic_event_queue() = default;

	template<std::convertible_to<Event> E = Event>
	explicit basic_event_queue(E&& event)
		: m_event(vsm_forward(event))
	{
	}

	basic_event_queue(basic_event_queue&&) = default;
	basic_event_queue& operator=(basic_event_queue&&) = default;

	~basic_event_queue()
	{
#if 0
		m_forward_list.splice_back(m_mpsc_queue.pop_all());

		for (operation_base& operation : m_forward_list)
		{
			operation.vtable.set_stopped(operation);
		}
#endif
	}


	scheduler get_scheduler()
	{
		return scheduler(*this);
	}


	[[nodiscard]] bool try_execute_one()
	{
		if (m_forward_list.empty())
		{
			auto list = m_mpsc_queue.pop_all();

			if (list.empty())
			{
				return false;
			}

			m_forward_list.splice_back(list);
		}

		vsm_assert(!m_forward_list.empty());
		execute(*m_forward_list.pop_front());

		return true;
	}

	[[nodiscard]] auto execute_one()
	{
		
	}

	[[nodiscard]] auto execute_all()
	{
		
	}

private:
	void execute(operation_base& operation)
	{
		auto const ptr = operation.vtable.load(std::memory_order_acquire);
		(ptr.tag() ? ptr->set_stopped : ptr->set_value)(operation);
	}
};

} // namespace allio::detail
