#pragma once

#include <allio/deadline.hpp>
#include <allio/detail/flags.hpp>
#include <allio/result.hpp>
#include <allio/storage.hpp>
#include <allio/type_id.hpp>
#include <allio/type_list.hpp>

#include <atomic>

#include <cstddef>

namespace allio {

class handle;
class async_operation;
class multiplexer;


class async_operation_parameters
{
protected:
	async_operation_parameters() = default;
	async_operation_parameters(async_operation_parameters const&) = default;
	async_operation_parameters& operator=(async_operation_parameters const&) = default;
	~async_operation_parameters() = default;
};

enum class async_operation_status : uint32_t
{
	// The asynchronous operation has been handed to a multiplexer.
	// The multiplexer now borrows the operation state and it may not be destroyed.
	scheduled                   = 1 << 0,

	// The asynchronous operation has been submitted and is now being performed.
	submitted                   = 1 << 1,

	// The asynchronous operation has been completed.
	// Note that cancellation is a kind of completion.
	completed                   = 1 << 2,

	// The asynchronous operation has been cancelled.
	cancelled                   = 1 << 3,

	// The asynchronous operation has been concluded.
	// The multiplexer no longer borrows the operation state and it may be destroyed.
	// Note that if a listener is used, the operation state is still borrowed until
	// async_operation_listener::concluded() is invoked on this operation state.
	concluded                   = 1 << 4,
};
allio_detail_FLAG_ENUM(async_operation_status);

class async_operation_listener
{
public:
	virtual void submitted(async_operation& operation) {}
	virtual void completed(async_operation& operation) {}
	virtual void concluded(async_operation& operation) {}
};

class async_operation
{
	async_operation_listener* m_listener;
	std::atomic<async_operation_status> m_status;
	std::error_code m_result;

public:
	async_operation_listener* get_listener() const
	{
		return m_listener;
	}

	async_operation_status get_status() const
	{
		return m_status.load(std::memory_order_acquire);
	}

	bool is_scheduled() const
	{
		return (get_status() & async_operation_status::scheduled) != async_operation_status{};
	}

	bool is_submitted() const
	{
		return (get_status() & async_operation_status::submitted) != async_operation_status{};
	}

	bool is_completed() const
	{
		return (get_status() & async_operation_status::completed) != async_operation_status{};
	}

	bool is_cancelled() const
	{
		return (get_status() & async_operation_status::cancelled) != async_operation_status{};
	}

	bool is_concluded() const
	{
		return (get_status() & async_operation_status::concluded) != async_operation_status{};
	}

	std::error_code get_result() const
	{
		return m_result;
	}

	template<std::derived_from<async_operation_parameters> Parameters>
	result<typename std::remove_cvref_t<Parameters>::result_type> get_result(Parameters&& parameters)
	{
		if (m_result)
		{
			return allio_ERROR(m_result);
		}
		else
		{
			if constexpr (std::is_void_v<typename std::remove_cvref_t<Parameters>::result_type>)
			{
				return {};
			}
			else
			{
				return static_cast<Parameters&&>(parameters).get_result();
			}
		}
	}

protected:
	explicit async_operation(async_operation_listener* const listener)
		: m_listener(listener)
	{
	}

	async_operation(async_operation const&) = delete;
	async_operation& operator=(async_operation const&) = delete;
	~async_operation() = default;

	void set_result(std::error_code const result)
	{
		m_result = result;
	}

	void set_status(async_operation_status const status)
	{
		m_status.store(m_status.load(std::memory_order_relaxed) | status, std::memory_order_release);
	}

	friend class multiplexer;
};

struct async_operation_descriptor
{
	bool synchronous;
	result<async_operation*>(*construct)(multiplexer& multiplexer, storage_ptr storage, async_operation_parameters const& arguments, async_operation_listener* listener);
	result<void>(*start)(multiplexer& multiplexer, async_operation& operation);
	result<async_operation*>(*construct_and_start)(multiplexer& multiplexer, storage_ptr storage, async_operation_parameters const& arguments, async_operation_listener* listener);
	result<void>(*cancel)(multiplexer& multiplexer, async_operation& operation);
	void(*destroy)(async_operation& operation);
};

struct multiplexer_handle_relation
{
	result<void*>(*register_handle)(multiplexer& multiplexer, handle const& handle);
	result<void>(*deregister_handle)(multiplexer& multiplexer, handle const& handle);
	storage_requirements operation_storage_requirements;
	async_operation_descriptor const* operations;
};

class multiplexer_handle_relation_provider
{
public:
	virtual result<multiplexer_handle_relation const*> find_multiplexer_handle_relation(
		type_id<multiplexer> multiplexer_type, type_id<handle> handle_type) const = 0;

protected:
	multiplexer_handle_relation_provider() = default;
	multiplexer_handle_relation_provider(multiplexer_handle_relation_provider const&) = default;
	multiplexer_handle_relation_provider& operator=(multiplexer_handle_relation_provider const&) = default;
	~multiplexer_handle_relation_provider() = default;
};

class multiplexer : public multiplexer_handle_relation_provider
{
public:
	virtual ~multiplexer() = default;

	virtual type_id<multiplexer> get_type_id() const = 0;

	result<multiplexer_handle_relation const*> find_multiplexer_handle_relation(
		type_id<multiplexer> multiplexer_type, type_id<handle> handle_type) const final override;

	virtual result<multiplexer_handle_relation const*> find_handle_relation(type_id<handle> handle_type) const = 0;

	virtual result<async_operation*> construct(async_operation_descriptor const& descriptor, storage_ptr storage, async_operation_parameters const& arguments, async_operation_listener* listener = nullptr);
	virtual result<void> start(async_operation_descriptor const& descriptor, async_operation& operation);
	virtual result<async_operation*> construct_and_start(async_operation_descriptor const& descriptor, storage_ptr storage, async_operation_parameters const& arguments, async_operation_listener* listener = nullptr);
	virtual result<void> cancel(async_operation_descriptor const& descriptor, async_operation& operation);

	virtual result<void> submit(deadline deadline = {});
	virtual result<void> poll(deadline deadline = {}) = 0;
	virtual result<void> submit_and_poll(deadline deadline = {});

	result<void> block(multiplexer_handle_relation const& relation, async_operation_descriptor const& descriptor, async_operation_parameters const& arguments);

protected:
	multiplexer() = default;
	multiplexer(multiplexer const&) = delete;
	multiplexer& operator=(multiplexer const&) = delete;

	static void set_result(async_operation& operation, std::error_code const result)
	{
		operation.set_result(result);
	}

	static void set_status(async_operation& operation, async_operation_status const status)
	{
		operation.set_status(status);
	}

	result<void> do_block(async_operation_descriptor const& descriptor, storage_ptr storage, async_operation_parameters const& arguments);
};


namespace io {

template<typename Operation>
struct parameters;

template<typename Operation>
struct parameters_with_handle : parameters<Operation>
{
	typename parameters<Operation>::handle_type* handle;

	template<typename... Args>
	explicit parameters_with_handle(typename parameters<Operation>::handle_type& handle, Args&&... args)
		: parameters<Operation>{ static_cast<Args&&>(args)... }
		, handle(&handle)
	{
	}
};


template<typename Result>
struct result_storage
{
	Result result;

	allio::result<Result> consume()
	{
		return static_cast<Result&&>(result);
	}
};

template<>
struct result_storage<void>
{
	allio::result<void> consume()
	{
		return {};
	}
};

template<typename Result>
struct result_pointer
{
	Result* result;

	void bind_storage(result_storage<Result>& storage)
	{
		result = &storage.result;
	}
};

template<>
struct result_pointer<void>
{
	void bind_storage(result_storage<void>& storage)
	{
	}
};

template<typename Operation>
struct parameters_with_result
	: async_operation_parameters
	, parameters_with_handle<Operation>
	, result_pointer<typename parameters<Operation>::result_type>
{
	template<typename... Args>
	explicit parameters_with_result(Args&&... args)
		: parameters_with_handle<Operation>{ static_cast<Args&&>(args)... }
	{
	}

	template<typename... Args>
	explicit parameters_with_result(result_storage<typename parameters<Operation>::result_type>& storage, Args&&... args)
		: parameters_with_handle<Operation>{ static_cast<Args&&>(args)... }
	{
		this->bind_storage(storage);
	}
};

} // namespace io


template<typename Operation, typename Handle>
async_operation_descriptor const& get_descriptor(Handle const& handle)
{
	return handle.get_multiplexer_relation().operations[
		type_list_index<typename Handle::async_operations, Operation>];
}

template<typename Operation, typename Handle>
bool is_synchronous(Handle const& handle)
{
	return get_descriptor<Operation>(handle).synchronous;
}

#if 0
template<typename Operation, typename Handle>
result<async_operation*> construct(Handle& handle, storage_ptr const storage, io::parameters<Operation> const& arguments, async_operation_listener* const listener = nullptr)
{
	return handle.get_multiplexer()->construct(get_descriptor<Operation>(handle), storage, arguments, listener);
}

template<typename Operation, typename Handle>
result<async_operation*> construct_and_start(Handle& handle, storage_ptr const storage, io::parameters<Operation> const& arguments, async_operation_listener* const listener = nullptr)
{
	return handle.get_multiplexer()->construct_and_start(get_descriptor<Operation>(handle), storage, arguments, listener);
}
#endif

template<typename O, typename Handle, typename... Args>
result<typename io::parameters<O>::result_type> block(Handle& handle, Args&&... args)
{
	io::result_storage<typename io::parameters<O>::result_type> storage;
	allio_TRYV(handle.get_multiplexer()->block(
		handle.get_multiplexer_relation(),
		get_descriptor<O>(handle),
		io::parameters_with_result<O>(storage, handle, args...)));
	return storage.consume();
}

} // namespace allio
