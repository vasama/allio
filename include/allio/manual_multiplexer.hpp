#pragma once

#include <allio/detail/api.hpp>
#include <allio/multiplexer.hpp>

#include <atomic>

namespace allio {

class manual_event
{
public:
	virtual result<void> wait(deadline deadline) = 0;
	virtual void wake() = 0;
};

class manual_multiplexer final : public multiplexer
{
public:
	class async_operation_storage : public async_operation
	{
		async_operation_storage* defer_next;

		friend class manual_multiplexer;
	};

private:
	multiplexer_handle_relation_provider const* m_relation_provider;
	manual_event* m_wait_event;

	std::atomic<async_operation_storage*> m_defer_head;

public:
	struct init_options
	{
		multiplexer_handle_relation_provider const* relation_provider = nullptr;
		manual_event* wait_event = nullptr;
	};

	class init_result
	{
		init_options options;

		init_result(init_options const& options)
			: options(options)
		{
		}

		friend class manual_multiplexer;
	};

	static result<init_result> init(init_options const& options);


	manual_multiplexer(init_result&& resources);
	manual_multiplexer(manual_multiplexer const&) = delete;
	manual_multiplexer& operator=(manual_multiplexer const&) = delete;
	~manual_multiplexer();


	type_id<multiplexer> get_type_id() const override;

	result<multiplexer_handle_relation const*> find_handle_relation(type_id<handle> handle_type) const override;


	using multiplexer::pump;
	result<statistics> pump(pump_parameters const& args) override;


	void complete(async_operation_storage& storage, std::error_code result);

private:
	static async_operation_storage* reverse_defer_list(async_operation_storage* head);
};
allio_API extern allio_TYPE_ID(manual_multiplexer);

} // namespace allio
