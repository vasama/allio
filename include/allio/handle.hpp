#pragma once

#include <allio/detail/assert.hpp>
#include <allio/detail/flags.hpp>
#include <allio/detail/linear.hpp>
#include <allio/multiplexer.hpp>
#include <allio/type_list.hpp>

#include <cstdint>

namespace allio {

enum class flags : uint32_t
{
	none                                = 0,
	multiplexable                       = 1 << 0,
};
allio_detail_FLAG_ENUM(flags);

namespace io {

struct close;

} // namespace io

class handle
{
	detail::linear<flags> m_flags;

	detail::linear<multiplexer*> m_multiplexer;
	detail::linear<multiplexer_handle_relation const*> m_multiplexer_relation;
	detail::linear<void*> m_multiplexer_data;

public:
	struct native_handle_type
	{
		flags handle_flags;
	};

	using async_operations = type_list<io::close>;

	[[nodiscard]] native_handle_type get_native_handle() const
	{
		return
		{
			m_flags.value,
		};
	}

	[[nodiscard]] flags get_flags() const
	{
		return m_flags.value;
	}

	[[nodiscard]] multiplexer* get_multiplexer() const
	{
		return m_multiplexer.value;
	}

	[[nodiscard]] multiplexer_handle_relation const& get_multiplexer_relation() const
	{
		return *m_multiplexer_relation.value;
	}

	[[nodiscard]] storage_requirements get_async_operation_storage_requirements() const
	{
		allio_ASSERT(m_multiplexer_relation.value != nullptr);
		return m_multiplexer_relation.value->operation_storage_requirements;
	}

	[[nodiscard]] void* get_multiplexer_data() const
	{
		return m_multiplexer_data.value;
	}

protected:
	explicit handle(native_handle_type const handle)
		: m_flags(handle.handle_flags)
	{
	}

	handle() = default;
	handle(handle&&) = default;
	handle& operator=(handle&&) = default;
	~handle() = default;

	void set_flags(flags const flags)
	{
		m_flags.value = flags;
	}

	result<void> do_flush()
	{
		return {};
	}

	result<void> set_multiplexer(multiplexer* multiplexer, multiplexer_handle_relation_provider const* relation_provider, type_id<handle> handle_type, bool is_valid);

	result<void> register_handle()
	{
		if ((m_flags.value & flags::multiplexable) == flags::none)
		{
			return allio_ERROR(error::handle_is_not_multiplexable);
		}
		allio_ASSERT(m_multiplexer.value != nullptr);
		allio_TRYA(m_multiplexer_data.value, m_multiplexer_relation.value->register_handle(*m_multiplexer.value, *this));
		return {};
	}

	result<void> deregister_handle()
	{
		allio_ASSERT(m_multiplexer.value != nullptr);
		allio_TRYV(m_multiplexer_relation.value->deregister_handle(*m_multiplexer.value, *this));
		m_multiplexer_data.value = nullptr;
		return {};
	}

	result<void> set_native_handle(native_handle_type handle);
	result<native_handle_type> release_native_handle();
};

template<typename Handle>
class final_handle final : public Handle
{
	static_assert(std::is_same_v<type_list_at<typename Handle::async_operations, 0>, io::close>,
		"io::close must be located at index 0 in the async operation table.");

public:
	using Handle::Handle;

	final_handle(final_handle&& source) noexcept = default;

	final_handle& operator=(final_handle&& source) noexcept
	{
		if (*this)
		{
			close_unchecked();
		}
		Handle::operator=(static_cast<Handle&&>(source));
		return *this;
	}

	~final_handle()
	{
		if (*this)
		{
			close_unchecked();
		}
	}


	result<void> flush()
	{
		if (*this)
		{
			return Handle::do_flush();
		}
		return allio_ERROR(make_error_code(std::errc::bad_file_descriptor));
	}

	result<void> close()
	{
		if (*this)
		{
			allio_TRYV(Handle::do_flush());
			return Handle::do_close();
		}
		return allio_ERROR(make_error_code(std::errc::bad_file_descriptor));
	}


	template<typename Operation>
	async_operation_descriptor const& get_descriptor() const
	{
		return Handle::get_multiplexer_relation().operations[
			type_list_index<typename Handle::async_operations, Operation>];
	}


	result<void> set_multiplexer(multiplexer* const multiplexer)
	{
		return handle::set_multiplexer(multiplexer, multiplexer, type_of(*this), static_cast<bool>(*this));
	}

	result<void> set_multiplexer(multiplexer* const multiplexer, multiplexer_handle_relation_provider const& relation_provider)
	{
		return handle::set_multiplexer(multiplexer, &relation_provider, type_of(*this), static_cast<bool>(*this));
	}

private:
	void close_unchecked()
	{
		allio_VERIFY(Handle::do_flush());
		Handle::do_close();
	}
};


template<>
struct io::parameters<io::close>
{
	using handle_type = handle;
	using result_type = void;
};

} // namespace allio
