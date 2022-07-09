#pragma once

#include <allio/byte_io.hpp>
#include <allio/platform_handle.hpp>
#include <allio/static_multiplexer_handle_relation_provider.hpp>
#include <allio/win32/kernel_error.hpp>
#include <allio/win32/iocp_multiplexer.hpp>
#include <allio/win32/platform.hpp>

#include "kernel.hpp"

namespace allio {

template<std::derived_from<async_operation> Storage>
class composite_async_operation_storage
	: public Storage
	, async_operation_listener
{
	async_operation_listener* m_listener;

	size_t m_not_submitted_count;
	size_t m_not_completed_count;
	size_t m_not_concluded_count;

public:
	template<std::derived_from<async_operation_parameters> Parameters>
	explicit composite_async_operation_storage(Parameters const& arguments, async_operation_listener* const listener, size_t const operation_count)
		: Storage(arguments, listener != nullptr ? this : nullptr)
		, m_listener(listener)
		, m_not_submitted_count(operation_count)
		, m_not_completed_count(operation_count)
		, m_not_concluded_count(operation_count)
	{
		allio_ASSERT(operation_count > 0);
	}

protected:
	using async_operation_listener = async_operation_listener;

	bool not_submitted(size_t count)
	{
		allio_ASSERT(count <= m_not_submitted_count);
		bool const none_submitted = count == m_not_submitted_count;

		count -= none_submitted;
		m_not_submitted_count -= count;
		m_not_completed_count -= count;
		m_not_concluded_count -= count;

		return none_submitted;
	}

private:
	void submitted(async_operation& operation) override
	{
		allio_ASSERT(&operation == this);
		allio_ASSERT(m_not_submitted_count > 0);
		if (--m_not_submitted_count == 0)
		{
			m_listener->submitted(operation);
		}
	}

	void completed(async_operation& operation) override
	{
		allio_ASSERT(&operation == this);
		allio_ASSERT(m_not_completed_count > 0);
		if (--m_not_completed_count == 0)
		{
			m_listener->completed(operation);
		}
	}

	void concluded(async_operation& operation) override
	{
		allio_ASSERT(&operation == this);
		allio_ASSERT(m_not_concluded_count > 0);
		if (--m_not_concluded_count == 0)
		{
			m_listener->concluded(operation);
		}
	}
};

struct scatter_gather_async_operation_storage
	: composite_async_operation_storage<win32::iocp_multiplexer::async_operation_storage>
	, io::scatter_gather_parameters
{
	handle const* handle;
	size_t* transferred;

	template<std::derived_from<io::scatter_gather_parameters> Parameters>
	scatter_gather_async_operation_storage(Parameters const& arguments, async_operation_listener* const listener)
		: composite_async_operation_storage(arguments, listener, arguments.buffers.size())
		, io::scatter_gather_parameters(arguments)
		, handle(arguments.handle)
		, transferred(arguments.result)
	{
	}

	result<void> submit(win32::iocp_multiplexer& m, decltype(win32::NtReadFile) const syscall)
	{
		using namespace win32;

		auto const buffers = this->buffers.buffers();

		allio::handle const& handle = *this->handle;
		bool const supports_synchronous_completion = m.supports_synchronous_completion(handle);

		if (buffers.empty())
		{
			return allio_ERROR(make_error_code(std::errc::invalid_argument));
		}

		allio_TRY(io_slots, m.acquire_io_slots(buffers.size()));

		std::error_code error = {};
		size_t not_submitted_count = buffers.size();
		uintptr_t information = 0;

		for (untyped_buffer const buffer : buffers)
		{
			*transferred = 0;
			capture_information([](scatter_gather_async_operation_storage& s, uintptr_t const information)
			{
				*s.transferred += information;
			});
			
			IO_STATUS_BLOCK& io_status_block = io_slots.bind(*this).io_status_block();

			LARGE_INTEGER offset_integer;
			offset_integer.QuadPart = offset;

			NTSTATUS const status = syscall(
				unwrap_handle(static_cast<platform_handle const&>(handle).get_platform_handle()),
				NULL,
				nullptr,
				&io_status_block,
				&io_status_block,
				const_cast<void*>(buffer.data()),
				buffer.size(),
				&offset_integer,
				nullptr);

			if (status < 0)
			{
				error = static_cast<kernel_error>(status);
				break;
			}

			if (status != STATUS_PENDING && supports_synchronous_completion)
			{
				information += io_status_block.Information;
			}
			else
			{
				--not_submitted_count;
			}
		}

		if (not_submitted(not_submitted_count) && !error)
		{
			allio_ASSERT(supports_synchronous_completion);
			m.post_synchronous_completion(*this, information);
		}

		return as_result(error);
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<win32::iocp_multiplexer, Handle, io::scatter_read_at>
{
	using async_operation_storage = scatter_gather_async_operation_storage;

	static result<void> start(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return s.submit(m, win32::NtReadFile);
	}

	static result<void> cancel(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(static_cast<platform_handle const*>(s.handle)->get_platform_handle(), s);
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<win32::iocp_multiplexer, Handle, io::gather_write_at>
{
	using async_operation_storage = scatter_gather_async_operation_storage;

	static result<void> start(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return s.submit(m, win32::NtWriteFile);
	}

	static result<void> cancel(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(static_cast<platform_handle const*>(s.handle)->get_platform_handle(), s);
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<win32::iocp_multiplexer, Handle, io::stream_scatter_read>
{
	using async_operation_storage = scatter_gather_async_operation_storage;

	static result<void> start(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return s.submit(m, win32::NtReadFile);
	}

	static result<void> cancel(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(static_cast<platform_handle const*>(s.handle)->get_platform_handle(), s);
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<win32::iocp_multiplexer, Handle, io::stream_gather_write>
{
	using async_operation_storage = scatter_gather_async_operation_storage;

	static result<void> start(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return s.submit(m, win32::NtWriteFile);
	}

	static result<void> cancel(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.cancel(static_cast<platform_handle const*>(s.handle)->get_platform_handle(), s);
	}
};

} // namespace allio
