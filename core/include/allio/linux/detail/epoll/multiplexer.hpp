#pragma once

#include <allio/detail/unique_handle.hpp>

#include <vsm/intrusive/list.hpp>
#include <vsm/offset_ptr.hpp>
#include <vsm/result.hpp>

namespace allio::detail {

class epoll_multiplexer
{
	struct subscription_link : vsm::intrusive::list_link {};

public:
	class operation_type
	{
	};

private:
	struct operation_list
	{
		vsm::intrusive_list<operation_type> list;
	};

public:
	class connector_type
	{
		size_t index;

		friend epoll_multiplexer;
	};

	enum class subscription_flags : uint16_t
	{
		read                    = 1 << 0,
		write                   = 1 << 1,
		exclusive               = 1 << 2,
		multishot               = 1 << 3,
	};
	vsm_flag_enum_friend(subscription_flags);

	class subscription : subscription_link
	{
		vsm::offset_ptr<operation_type, int16_t> m_operation;
		subscription_flags m_flags;

	public:
		subscription(subscription const&) = delete;
		subscription& operator=(subscription const&) = delete;

		void bind(operation_type const& operation) const
		{
			m_operation = &operation;
		}
	};

private:
	struct file_descriptor
	{
		int fd;
		uint32_t read_count;
		uint32_t write_count;
	};

	unique_handle m_epoll;

	std::deque<file_descriptor> m_file_descriptors;

public:
	[[nodiscard]] vsm::result<void> subscribe(
		connector_type const& connector,
		subscription& subscription,
		subscription_flags flags);

	void unsubscribe(
		connector_type const& connector,
		subscription& subscription);

	class subscription_guard
	{
		epoll_multiplexer& m_multiplexer;
		connector_type const& m_connector;
		subscription& m_subscription;

	public:
		subscription_guard(subscription_guard const&) = default;
		subscription_guard& operator=(subscription_guard const&) = default;

		~subscription_guard()
		{
			m_multiplexer.unsubscribe(m_connector, m_connector, m_subscription);
		}
	};


	[[nodiscard]] vsm::result<void> poll(auto&&... args)
	{
	}
};

} // namespace allio::detail
