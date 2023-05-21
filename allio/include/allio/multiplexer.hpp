#pragma once

#include <allio/detail/alloca.hpp>
#include <allio/error.hpp>
#include <allio/storage.hpp>
#include <allio/type_id.hpp>
#include <allio/type_list.hpp>
#include <allio/parameters.hpp>
#include <allio/untyped_parameter.hpp>

#include <vsm/flags.hpp>
#include <vsm/result.hpp>

#include <atomic>
#include <memory>

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
#if 0
	// The asynchronous operation has been handed to a multiplexer.
	// The multiplexer now borrows the operation state and it may not be destroyed.
	scheduled                   = 1 << 0,

	// The asynchronous operation has been submitted and is now in progress.
	submitted                   = 1 << 1,

	// The asynchronous operation has been completed.
	// Note that cancellation is a kind of completion.
	completed                   = 1 << 2,

	// The asynchronous operation has been concluded.
	// The multiplexer no longer borrows the operation state and it may be destroyed.
	// Note that if a listener is used, the operation state is still borrowed until
	// async_operation_listener::concluded() is invoked on this operation state.
	concluded                   = 1 << 3,

	// The asynchronous operation has been cancelled.
	cancelled                   = 1 << 4,
#endif

	// The asynchronous operation has been submitted and is now in progress.
	submitted                   = 1,

	// The asynchronous operation has been completed.
	// Note that cancellation is a kind of completion.
	completed                   = 2,

	// The asynchronous operation has been concluded.
	// The multiplexer no longer borrows the operation state and it may be destroyed.
	// Note that if a listener is used, the operation state is still borrowed until
	// async_operation_listener::concluded() is invoked on this operation state.
	concluded                   = 3,

};
vsm_flag_enum(async_operation_status);

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

#if 0
	bool is_scheduled() const
	{
		return (get_status() & async_operation_status::scheduled) != async_operation_status{};
	}
#endif

	bool is_submitted() const
	{
		return get_status() >= async_operation_status::submitted;
		//return (get_status() & async_operation_status::submitted) != async_operation_status{};
	}

	bool is_completed() const
	{
		return get_status() >= async_operation_status::completed;
		//return (get_status() & async_operation_status::completed) != async_operation_status{};
	}

#if 0
	bool is_cancelled() const
	{
		return (get_status() & async_operation_status::cancelled) != async_operation_status{};
	}
#endif

	bool is_concluded() const
	{
		return get_status() >= async_operation_status::concluded;
		//return (get_status() & async_operation_status::concluded) != async_operation_status{};
	}

	std::error_code get_result() const
	{
		vsm_assert(is_completed());
		return m_result;
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

	void submitted()
	{
		vsm_assert(!is_submitted());
		set_status(async_operation_status::submitted);
		if (async_operation_listener* const listener = get_listener())
		{
			listener->submitted(*this);
		}
	}

	void completed(std::error_code const result)
	{
		vsm_assert(!is_completed());
		set_result(result);
		set_status(async_operation_status::completed);
		if (async_operation_listener* const listener = get_listener())
		{
			listener->completed(*this);
		}
	}

	void concluded()
	{
		vsm_assert(!is_concluded());
		set_status(async_operation_status::concluded);
		if (async_operation_listener* const listener = get_listener())
		{
			listener->concluded(*this);
		}
	}

	friend class multiplexer;
};

class async_operation_deleter
{
	void(*m_destroy)(async_operation& operation);

public:
	async_operation_deleter(void(*destroy)(async_operation& operation))
		: m_destroy(destroy)
	{
	}

	void operator()(async_operation* const operation) const
	{
		m_destroy(*operation);
	}
};

using async_operation_ptr = std::unique_ptr<async_operation, async_operation_deleter>;

struct async_operation_descriptor
{
	vsm::result<async_operation_ptr>(*construct)(multiplexer& multiplexer, storage_ptr storage, async_operation_parameters const& arguments, async_operation_listener* listener);
	vsm::result<void>(*start)(multiplexer& multiplexer, async_operation& operation);
	vsm::result<async_operation_ptr>(*construct_and_start)(multiplexer& multiplexer, storage_ptr storage, async_operation_parameters const& arguments, async_operation_listener* listener);
	vsm::result<void>(*cancel)(multiplexer& multiplexer, async_operation& operation);
	vsm::result<void>(*block)(multiplexer& multiplexer, async_operation_parameters const& arguments);
	void(*destroy)(async_operation& operation);
};

struct multiplexer_handle_relation
{
	vsm::result<void>(*register_handle)(multiplexer& multiplexer, handle const& handle);
	vsm::result<void>(*deregister_handle)(multiplexer& multiplexer, handle const& handle);
	storage_requirements operation_storage_requirements;
	async_operation_descriptor const* operations;
};

class multiplexer_handle_relation_provider
{
public:
	virtual vsm::result<multiplexer_handle_relation const*> find_multiplexer_handle_relation(
		type_id<multiplexer> multiplexer_type, type_id<handle> handle_type) const = 0;

protected:
	multiplexer_handle_relation_provider() = default;
	multiplexer_handle_relation_provider(multiplexer_handle_relation_provider const&) = default;
	multiplexer_handle_relation_provider& operator=(multiplexer_handle_relation_provider const&) = default;
	~multiplexer_handle_relation_provider() = default;
};

class async_operation_handler
{
public:
	virtual void submitted(multiplexer& multiplexer, untyped_parameter context) = 0;
	virtual void completed(multiplexer& multiplexer, untyped_parameter context, std::error_code result, untyped_parameter data) = 0;
	virtual void concluded(multiplexer& multiplexer, untyped_parameter context) = 0;
};

class async_operation_handler_with_context
{
	async_operation_handler* m_handler;
	untyped_parameter m_context;

public:
	async_operation_handler_with_context(async_operation_handler& handler)
		: m_handler(&handler)
		, m_context{}
	{
	}

	explicit async_operation_handler_with_context(async_operation_handler& handler, untyped_parameter const context)
		: m_handler(&handler)
		, m_context(context)
	{
	}

	void submitted(multiplexer& multiplexer) const
	{
		m_handler->submitted(multiplexer, m_context);
	}

	void completed(multiplexer& multiplexer, std::error_code const result, untyped_parameter const data) const
	{
		m_handler->completed(multiplexer, m_context, result, data);
	}

	void concluded(multiplexer& multiplexer) const
	{
		m_handler->concluded(multiplexer, m_context);
	}
};

class multiplexer : public multiplexer_handle_relation_provider
{
public:
	enum class pump_mode
	{
		none                            = 0,

		submit                          = 1 << 0,
		complete                        = 1 << 1,
		flush                           = 1 << 2,
	};
	vsm_flag_enum_friend(pump_mode);

	#define allio_multiplexer_pump_parameters(type, data, ...) \
		type(allio::multiplexer, pump_parameters) \
		data(::allio::multiplexer::pump_mode, mode, static_cast<::allio::multiplexer::pump_mode>(0b111)) \

	allio_interface_parameters(allio_multiplexer_pump_parameters);

	struct statistics
	{
		size_t submitted;
		size_t completed;
		size_t concluded;

		friend bool operator==(statistics const&, statistics const&) = default;
	};


	multiplexer(multiplexer const&) = delete;
	multiplexer& operator=(multiplexer const&) = delete;
	virtual ~multiplexer() = default;

	virtual type_id<multiplexer> get_type_id() const = 0;

	vsm::result<multiplexer_handle_relation const*> find_multiplexer_handle_relation(
		type_id<multiplexer> multiplexer_type, type_id<handle> handle_type) const final override;

	virtual vsm::result<multiplexer_handle_relation const*> find_handle_relation(type_id<handle> handle_type) const = 0;

	virtual vsm::result<async_operation_ptr> construct(async_operation_descriptor const& descriptor, storage_ptr storage, async_operation_parameters const& arguments, async_operation_listener* listener = nullptr);
	virtual vsm::result<void> start(async_operation_descriptor const& descriptor, async_operation& operation);
	virtual vsm::result<async_operation_ptr> construct_and_start(async_operation_descriptor const& descriptor, storage_ptr storage, async_operation_parameters const& arguments, async_operation_listener* listener = nullptr);
	virtual vsm::result<void> cancel(async_operation_descriptor const& descriptor, async_operation& operation);
	virtual vsm::result<void> block(async_operation_descriptor const& descriptor, async_operation_parameters const& arguments);


	template<parameters<pump_parameters> Parameters = pump_parameters::interface>
	vsm::result<statistics> pump(Parameters const& args = {})
	{
		return pump(static_cast<pump_parameters const&>(args));
	}

	virtual vsm::result<statistics> pump(pump_parameters const& args) = 0;


	template<std::derived_from<multiplexer> Multiplexer = multiplexer>
	static vsm::result<void> block_async(Multiplexer& multiplexer, async_operation& operation)
	{
		while (!operation.is_concluded())
		{
			vsm_try_discard(multiplexer.pump({ .mode = pump_mode::submit | pump_mode::complete }));
		}

		if (std::error_code const error = operation.get_result())
		{
			return std::unexpected(error);
		}

		return {};
	}

	template<std::derived_from<multiplexer> Multiplexer = multiplexer>
	static vsm::result<void> block_async(multiplexer& multiplexer, async_operation_descriptor const& descriptor, storage_ptr const storage, async_operation_parameters const& arguments)
	{
		vsm_try(operation, multiplexer.construct_and_start(descriptor, storage, arguments, nullptr));
		return block_async(multiplexer, *operation);
	}

	template<std::derived_from<multiplexer> Multiplexer = multiplexer>
	static vsm::result<void> block_async(Multiplexer& multiplexer, multiplexer_handle_relation const& relation, async_operation_descriptor const& descriptor, async_operation_parameters const& arguments)
	{
		return detail::with_alloca(relation.operation_storage_requirements, [&](storage_ptr const storage)
		{
			return block_async(multiplexer, descriptor, storage, arguments);
		});
	}

protected:
	multiplexer() = default;


	static void set_result(async_operation& operation, std::error_code const result)
	{
		operation.set_result(result);
	}

	static void set_status(async_operation& operation, async_operation_status const status)
	{
		operation.set_status(status);
	}

	static void add_statistics(statistics& inout, statistics const& in)
	{
		inout.submitted += in.submitted;
		inout.completed += in.completed;
		inout.concluded += in.concluded;
	}

	static vsm::result<statistics> gather_statistics(auto&& callable)
	{
		vsm::result<statistics> r(vsm::result_value);
		if (vsm::result<void> const e = static_cast<decltype(callable)&&>(callable)(*r); !e)
		{
			statistics const& s = *r;
			if (s.submitted == 0 && s.completed == 0 && s.concluded == 0)
			{
				return std::unexpected(e.error());
			}
		}
		return r;
	}
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

	vsm::result<Result> consume()
	{
		return static_cast<Result&&>(result);
	}
};

template<>
struct result_storage<void>
{
	vsm::result<void> consume()
	{
		return {};
	}
};

template<typename Result>
struct result_pointer
{
	Result* result;

	result_pointer()
		: result(nullptr)
	{
	}

	result_pointer(vsm::result<Result>& storage)
	{
		result = &*storage;
	}

	result_pointer(result_storage<Result>& storage)
	{
		result = &storage.result;
	}

	vsm::result<void> produce(vsm::result<Result>&& r) const
	{
		vsm_try_assign(*result, static_cast<decltype(r)&&>(r));
		return {};
	}

	void bind_storage(result_storage<Result>& storage)
	{
		vsm_assert(result == nullptr);
		result = &storage.result;
	}
};

template<>
struct result_pointer<void>
{
	result_pointer() = default;

	result_pointer(vsm::result<void>& storage)
	{
	}

	result_pointer(result_storage<void>& storage)
	{
	}

	vsm::result<void> produce(vsm::result<void> const r) const
	{
		return r;
	}

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
	explicit parameters_with_result(parameters_with_handle<Operation> const& args)
		: parameters_with_handle<Operation>(args)
	{
	}

	explicit parameters_with_result(result_pointer<typename parameters<Operation>::result_type> const storage, parameters_with_handle<Operation> const& args)
		: parameters_with_handle<Operation>(args)
		, result_pointer<typename parameters<Operation>::result_type>(storage)
	{
	}

	template<typename... Args>
	explicit parameters_with_result(Args&&... args)
		: parameters_with_handle<Operation>(static_cast<Args&&>(args)...)
	{
	}

	template<typename... Args>
	explicit parameters_with_result(result_pointer<typename parameters<Operation>::result_type> const storage, Args&&... args)
		: parameters_with_handle<Operation>(static_cast<Args&&>(args)...)
		, result_pointer<typename parameters<Operation>::result_type>(storage)
	{
	}
};

} // namespace io
} // namespace allio
