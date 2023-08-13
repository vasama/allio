#pragma once

#include <allio/multiplexer.hpp>
#include <allio/parameters.hpp>
#include <allio/type_list.hpp>

#include <vsm/assert.h>
#include <vsm/flags.hpp>
#include <vsm/linear.hpp>

#include <cstdint>

namespace allio {

class handle;

template<typename Handle>
class final_handle;

namespace io {

struct flush;
struct close;

} // namespace io

template<>
struct io::parameters<io::flush>
	: basic_parameters
{
	using handle_type = handle;
	using result_type = void;
};

template<>
struct io::parameters<io::close>
	: basic_parameters
{
	using handle_type = handle;
	using result_type = void;
};


namespace detail {

template<typename Enum>
concept handle_flags_enum = requires { Enum::allio_detail_handle_flags_flag_count; };

} // namespace detail

class handle_flags
{
	uint32_t m_flags;

public:
	static handle_flags const none;


	handle_flags() = default;

	constexpr handle_flags(detail::handle_flags_enum auto const index)
		: m_flags(convert_index(index))
	{
	}

	constexpr handle_flags& operator=(handle_flags const&) & = default;


	[[nodiscard]] constexpr bool operator[](detail::handle_flags_enum auto const index) const
	{
		return m_flags & convert_index(index);
	}


	[[nodiscard]] constexpr handle_flags operator~() const
	{
		return handle_flags(~m_flags);
	}

	constexpr handle_flags& operator&=(handle_flags const flags) &
	{
		m_flags &= flags.m_flags;
		return *this;
	}

	constexpr handle_flags& operator|=(handle_flags const flags) &
	{
		m_flags |= flags.m_flags;
		return *this;
	}

	constexpr handle_flags& operator^=(handle_flags const flags) &
	{
		m_flags ^= flags.m_flags;
		return *this;
	}

	[[nodiscard]] friend constexpr handle_flags operator&(handle_flags const& lhs, handle_flags const& rhs)
	{
		return handle_flags(lhs.m_flags & rhs.m_flags);
	}

	[[nodiscard]] friend constexpr handle_flags operator|(handle_flags const& lhs, handle_flags const& rhs)
	{
		return handle_flags(lhs.m_flags | rhs.m_flags);
	}

	[[nodiscard]] friend constexpr handle_flags operator^(handle_flags const& lhs, handle_flags const& rhs)
	{
		return handle_flags(lhs.m_flags ^ rhs.m_flags);
	}

	[[nodiscard]] bool operator==(handle_flags const&) const = default;

private:
	explicit constexpr handle_flags(uint32_t const flags)
		: m_flags(flags)
	{
	}

	[[nodiscard]] static constexpr uint32_t convert_index(detail::handle_flags_enum auto const index)
	{
		return (static_cast<uint32_t>(1) << decltype(index)::allio_detail_handle_flags_base_count) << static_cast<uint32_t>(index);
	}
};

inline constexpr handle_flags handle_flags::none = handle_flags(0);


#define allio_detail_handle_flags(base, ...) \
	struct flags : base::flags \
	{ \
		enum : ::uint32_t \
		{ \
			__VA_ARGS__ \
			allio_detail_handle_flags_enum_count, \
			allio_detail_handle_flags_base_count = base::flags::allio_detail_handle_flags_flag_count, \
			allio_detail_handle_flags_flag_count = allio_detail_handle_flags_base_count + allio_detail_handle_flags_enum_count, \
		}; \
		static_assert(allio_detail_handle_flags_flag_count <= base::flags::allio_detail_handle_flags_flag_limit); \
	} \

#define allio_handle_flags(...) \
	allio_detail_handle_flags(base_type, __VA_ARGS__)

#define allio_handle_implementation_flags(...) \
	allio_detail_handle_flags(base_type::implementation, __VA_ARGS__)


using synchronous_operation = vsm::result<void>(*)(async_operation_parameters const& args);

template<typename Handle, typename Operation>
struct synchronous_operation_implementation;
#if 0
{
	static vsm::result<void> execute(io::parameters_with_result<Operation> const& args)
	{
		return Handle::sync_impl(args);
	}
};
#endif


template<typename M, typename H>
struct multiplexer_handle_implementation;

template<typename M, typename H, typename O>
struct multiplexer_handle_operation_implementation;

namespace detail {

// https://godbolt.org/z/Y7Gqncz6T
class handle_base
{
	using async_operations = type_list<>;

protected:
	struct flags : handle_flags
	{
		flags() = delete;
		static constexpr uint32_t allio_detail_handle_flags_flag_count = 0;
		static constexpr uint32_t allio_detail_handle_flags_flag_limit = 16;
	};

	struct implementation
	{
		struct flags : handle_flags
		{
			flags() = delete;
			static constexpr uint32_t allio_detail_handle_flags_flag_count = 16;
			static constexpr uint32_t allio_detail_handle_flags_flag_limit = 32;
		};
	};

protected:
	void sync_impl();
};


template<typename H, typename O>
struct synchronous_operation_translation;

template<typename H, typename O>
struct synchronous_operation_translation<final_handle<H>, O>
{
	static vsm::result<void> execute(async_operation_parameters const& args)
	{
		return synchronous<final_handle<H>>(static_cast<io::parameters_with_result<O> const&>(args));
	}
};

template<typename H, typename Os = typename H::async_operations>
struct synchronous_operations;

template<typename H, typename... Os>
struct synchronous_operations<final_handle<H>, type_list<Os...>>
{
	static synchronous_operation const operations[];
};

template<typename H, typename... Os>
constinit synchronous_operation const synchronous_operations<final_handle<H>, type_list<Os...>>::operations[] =
{
	synchronous_operation_translation<final_handle<H>, Os>::execute...
};

} // namespace detail

template<>
struct type_id_traits<handle>
{
	using object_type = synchronous_operation[];
};

class handle : public detail::handle_base
{
	synchronous_operation const(* m_synchronous_operations)[];
	vsm::linear<handle_flags> m_flags;
	vsm::linear<multiplexer*> m_multiplexer;
	vsm::linear<multiplexer_handle_relation const*> m_multiplexer_relation;
	//void* m_multiplexer_data;

public:
	using base_type = detail::handle_base;

	struct native_handle_type
	{
		handle_flags flags;
	};

	allio_handle_flags
	(
		not_null,
	);

	using async_operations = type_list<
		io::flush,
		io::close
	>;


	[[nodiscard]] type_id<handle> get_type_id() const
	{
		return type_id<handle>(m_synchronous_operations);
	}

	[[nodiscard]] native_handle_type get_native_handle() const
	{
		return
		{
			m_flags.value,
		};
	}

	[[nodiscard]] handle_flags get_flags() const
	{
		return m_flags.value;
	}

	[[nodiscard]] multiplexer* get_multiplexer() const
	{
		return m_multiplexer.value;
	}

	vsm::result<void> set_multiplexer(multiplexer* const multiplexer)
	{
		return set_multiplexer(multiplexer, multiplexer);
	}

	vsm::result<void> set_multiplexer(multiplexer* const multiplexer, multiplexer_handle_relation_provider const& relation_provider)
	{
		return set_multiplexer(multiplexer, &relation_provider);
	}

	[[nodiscard]] synchronous_operation const* get_synchronous_operations() const
	{
		return *m_synchronous_operations;
	}

	[[nodiscard]] multiplexer_handle_relation const& get_multiplexer_relation() const
	{
		vsm_assert(m_multiplexer.value != nullptr);
		return *m_multiplexer_relation.value;
	}

	[[nodiscard]] storage_requirements get_async_operation_storage_requirements() const
	{
		vsm_assert(m_multiplexer.value != nullptr);
		return m_multiplexer_relation.value->operation_storage_requirements;
	}

#if 0
	[[nodiscard]] void* get_multiplexer_data() const
	{
		vsm_assert(m_multiplexer.value != nullptr);
		return m_multiplexer_data;
	}
#endif

	[[nodiscard]] explicit operator bool() const
	{
		return m_flags.value != flags::none;
	}

protected:
	explicit constexpr handle(type_id<handle> const type)
		: m_synchronous_operations(type.get_object())
	{
	}

	handle(handle&&) = default;
	handle& operator=(handle&&) = default;
	~handle() = default;

	void set_flags(handle_flags const flags)
	{
		m_flags = flags;
	}

#if 0
	vsm::result<void> flush_sync(basic_parameters const& args)
	{
		return {};
	}
#endif

	vsm::result<void> set_multiplexer(multiplexer* multiplexer, multiplexer_handle_relation_provider const* relation_provider);

	vsm::result<void> register_handle()
	{
		vsm_assert(m_multiplexer.value != nullptr);
		vsm_try_void(m_multiplexer_relation.value->register_handle(*m_multiplexer.value, *this));
		return {};
	}

	vsm::result<void> deregister_handle()
	{
		vsm_assert(m_multiplexer.value != nullptr);
		vsm_try_void(m_multiplexer_relation.value->deregister_handle(*m_multiplexer.value, *this));
		return {};
	}

	struct unchecked_tag {};

	static bool check_native_handle(native_handle_type const& handle);
	void set_native_handle(native_handle_type const& handle);
	native_handle_type release_native_handle();

protected:
	static vsm::result<void> sync_impl(io::parameters_with_result<io::flush> const& args)
	{
		return {};
	}

	static vsm::result<void> sync_impl(io::parameters_with_result<io::close> const& args)
	{
		args.handle->m_flags.reset();

		return {};
	}

private:
	friend class multiplexer;

	template<typename>
	friend class final_handle;
};

template<std::derived_from<handle> Handle>
struct type_id_traits<Handle> : type_id_traits<handle> {};


template<typename Operation, typename Handle>
synchronous_operation get_synchronous_operation(Handle const& handle)
{
	return handle.get_synchronous_operations()[
		type_list_index<typename Handle::async_operations, Operation>];
}

template<typename Operation, typename Handle>
async_operation_descriptor const& get_async_operation_descriptor(Handle const& handle)
{
	return handle.get_multiplexer_relation().operations[
		type_list_index<typename Handle::async_operations, Operation>];
}


template<typename Handle, typename Operation>
vsm::result<void> synchronous(io::parameters_with_result<Operation> const& args)
{
	return Handle::sync_impl(args);
}

template<typename Operation, typename Handle, typename... Args>
vsm::result<typename io::parameters<Operation>::result_type> block(Handle& handle, auto const& optional_args, Args&&... required_args)
{
	using result_type = typename io::parameters<Operation>::result_type;

	vsm::result<result_type> r(vsm::result_value);
	io::parameters_with_result<Operation> const args_object(
		io::result_pointer<result_type>(r), handle, optional_args, static_cast<Args&&>(required_args)...);

	if (multiplexer* const multiplexer = handle.get_multiplexer())
	{
		vsm_try_void(multiplexer->block(get_async_operation_descriptor<Operation>(handle), args_object));
	}
	else
	{
		vsm_try_void(get_synchronous_operation<Operation>(handle)(args_object));
	}

	return r;
}


template<typename Handle>
class final_handle final : public Handle
{
	static_assert(std::is_trivially_copyable_v<typename Handle::native_handle_type>);

public:
	constexpr final_handle()
		: Handle(type_of<final_handle>())
	{
	}

	final_handle(final_handle&& source) noexcept = default;

	final_handle& operator=(final_handle&& source) & noexcept
	{
		if (*this)
		{
			detail::unrecoverable(close());
		}

		Handle::operator=(static_cast<Handle&&>(source));

		return *this;
	}

	~final_handle()
	{
		if (*this)
		{
			detail::unrecoverable(close());
		}
	}


	vsm::result<void> set_native_handle(typename Handle::native_handle_type const& handle)
	{
		if (*this)
		{
			return vsm::unexpected(error::handle_is_not_null);
		}

		if (!Handle::check_native_handle(handle))
		{
			return vsm::unexpected(error::invalid_argument);
		}

		Handle::set_native_handle(handle);

		return {};
	}

	vsm::result<typename Handle::native_handle_type> release_native_handle()
	{
		if (!*this)
		{
			return vsm::unexpected(error::handle_is_null);
		}

		return Handle::release_native_handle();
	}

	typename Handle::native_handle_type release_native_handle(handle::unchecked_tag)
	{
		return Handle::release_native_handle();
	}


	vsm::result<void> flush(basic_parameters::interface const& args = {})
	{
		return block<io::flush>(*this, args);
	}

	vsm::result<void> close(basic_parameters::interface const& args = {})
	{
		return block<io::close>(*this, args);
	}


	vsm::result<void> duplicate(final_handle const& handle)
	{
		if (*this)
		{
			return vsm::unexpected(error::handle_is_not_null);
		}

		vsm_try(new_handle, Handle::duplicate(Handle::get_native_handle()));
		Handle::set_native_handle(new_handle);

		return {};
	}

	vsm::result<final_handle> duplicate() const
	{
		vsm::result<final_handle> r(vsm::result_value);

		vsm_try(new_handle, Handle::duplicate(Handle::get_native_handle()));
		r->Handle::set_native_handle(new_handle);

		return r;
	}

protected:
	using Handle::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::flush> const& args)
	{
		if (!*args.handle)
		{
			return vsm::unexpected(error::handle_is_null);
		}
	
		return Handle::sync_impl(args);
		//return static_cast<final_handle*>(args.handle)->Handle::flush_sync(args);
	}

	static vsm::result<void> sync_impl(io::parameters_with_result<io::close> const& args)
	{
		if (!*args.handle)
		{
			return vsm::unexpected(error::handle_is_null);
		}
	
		return Handle::sync_impl(args);
		//return static_cast<final_handle*>(args.handle)->Handle::close_sync(args);
	}

	template<typename H, typename O>
	friend vsm::result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

template<std::derived_from<handle> Handle>
struct type_id_traits<final_handle<Handle>> : type_id_traits<Handle>
{
	static constexpr typename type_id_traits<Handle>::object_type const* get_object()
	{
		return &detail::synchronous_operations<final_handle<Handle>>::operations;
	}
};


template<typename Handle, typename Multiplexer>
class basic_async_handle
{
};


#define allio_handle_implementation(H) \
	template struct ::allio::detail::synchronous_operations<H>

} // namespace allio
