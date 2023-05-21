#pragma once

#include <allio/async_result.hpp>
#include <allio/multiplexer.hpp>

#include <vsm/tag_ptr.hpp>

namespace allio {
namespace detail {

class deferring_multiplexer : public multiplexer
{
public:
	class async_operation_storage : public async_operation
	{
		static constexpr async_operation_status max_status = async_operation_status::concluded;

		// The tag holds the greatest operation status of which the attached listener has been informed.
		// The pointer points to this operation object when not part of a multiplexer's deferral list.
		vsm::incomplete_tag_ptr<async_operation_storage, async_operation_status, max_status> m_next = this;

	protected:
		explicit async_operation_storage(async_operation_listener* const listener)
			: async_operation(listener)
		{
		}

		async_operation_storage(async_operation_storage const&) = default;
		async_operation_storage& operator=(async_operation_storage const&) = default;
		~async_operation_storage() = default;

	private:
		friend class deferring_multiplexer;
	};

private:
	async_operation_storage* m_head = nullptr;

	// Non-null m_head implies non-null m_tail.
	// When m_head is null, m_tail is unspecified
	async_operation_storage* m_tail;

public:
	template<typename Callable>
	vsm::result<void> start(async_operation_storage& operation, Callable&& callable)
	{
		return post_result(operation, static_cast<Callable&&>(callable)());
	}

	void post(async_operation_storage& operation, async_operation_status status);

protected:
	deferring_multiplexer() = default;
	deferring_multiplexer(deferring_multiplexer const&) = default;
	deferring_multiplexer& operator=(deferring_multiplexer const&) = default;
	~deferring_multiplexer() = default;

	[[nodiscard]] statistics flush();

private:
	vsm::result<void> post_result(async_operation_storage& operation, vsm::result<void> const result)
	{
		set_result(operation, vsm::as_error_code(result));
		post(operation, async_operation_status::concluded);
		return {};
	}

	vsm::result<void> post_result(async_operation_storage& operation, async_result<void> const result)
	{
		if (!result)
		{
			async_error_code const& error = result.error();
			if (error.is_synchronous())
			{
				return std::unexpected(error.error_code());
			}
			set_result(operation, error.error_code());
			post(operation, async_operation_status::concluded);
		}
		return {};
	}
};

} // namespace detail

using detail::deferring_multiplexer;

} // namespace allio
