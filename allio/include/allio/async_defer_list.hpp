#pragma once

#include <allio/async_result.hpp>
#include <allio/detail/async_io.hpp>
#include <allio/multiplexer.hpp>

#include <vsm/tag_ptr.hpp>
#include <vsm/utility.hpp>

namespace allio::detail {

class async_defer_list
{
public:
	class operation_storage  : public async_operation
	{
		static constexpr async_operation_status max_status = async_operation_status::concluded;

		// The tag holds the greatest operation status of which the attached listener has been informed.
		// The pointer points to this operation object when not part of a multiplexer's deferral list.
		vsm::incomplete_tag_ptr<operation_storage, async_operation_status, max_status> m_next = this;

	protected:
		using async_operation::async_operation;

		operation_storage(operation_storage const&) = default;
		operation_storage& operator=(operation_storage const&) = default;

		~operation_storage() = default;

	private:
		friend class async_defer_list;
	};

private:
	operation_storage* m_head = nullptr;

	// Non-null m_head implies non-null m_tail.
	// When m_head is null, m_tail is unspecified.
	operation_storage* m_tail;

public:
	template<std::derived_from<operation_storage> S>
	vsm::result<void> submit(S& s, auto&& submit)
	{
		return post_result(s, vsm_forward(submit)());
	}

	void post(operation_storage& s, async_operation_status const status);

	[[nodiscard]] bool flush(poll_statistics& statistics);

private:
	template<std::derived_from<operation_storage> S>
	vsm::result<void> post_result(S& s, vsm::result<void> const& r)
	{
		if (!r)
		{
			s.set_result(r.error());
			post(s, async_operation_status::concluded);
		}

		return {};
	}

	template<std::derived_from<operation_storage> S>
	vsm::result<void> post_result(S& s, async_result<void> const& r)
	{
		if (!r)
		{
			async_error_code const& e = r.error();

			if (e.is_synchronous())
			{
				return vsm::unexpected(e.error_code());
			}

			s.set_result(e.error_code());
			post(s, async_operation_status::concluded);
		}

		return {};
	}
};

} // namespace allio::detail
