#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/bitset.hpp>
#include <allio/implementation.hpp>
#include <allio/platform_handle.hpp>
#include <allio/step_deadline.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/nt_error.hpp>
#include <allio/win32/iocp_multiplexer.hpp>
#include <allio/win32/platform.hpp>

namespace allio {
namespace win32 {

using byte_io_syscall = decltype(NtReadFile);

class iocp_byte_io_syscall
{
	byte_io_syscall m_syscall;
	HANDLE m_handle;
	bool m_supports_synchronous_completion;

public:
	explicit iocp_byte_io_syscall(byte_io_syscall const syscall, platform_handle const& handle)
		: m_syscall(syscall)
		, m_handle(unwrap_handle(handle.get_platform_handle()))
		, m_supports_synchronous_completion(iocp_multiplexer::supports_synchronous_completion(handle))
	{
	}

	NTSTATUS operator()(IO_STATUS_BLOCK& io_status_block, untyped_buffer const buffer, file_size const offset) const
	{
		if (buffer.empty())
		{
			io_status_block.Status = STATUS_SUCCESS;
			io_status_block.Information = 0;
			return io_status_block.Status;
		}

		LARGE_INTEGER offset_integer;
		offset_integer.QuadPart = offset;

		NTSTATUS status = m_syscall(
			m_handle,
			NULL,
			nullptr,
			&io_status_block,
			&io_status_block,
			const_cast<void*>(buffer.data()),
			buffer.size(),
			&offset_integer,
			nullptr);

		if (NT_SUCCESS(status))
		{
			// Any other success code here is unexpected, but there is no documentation.
			// However if it were anything else, that information would be lost.
			// It remains to be seen if this assertion will ever fire.
			vsm_assert(status == STATUS_SUCCESS || status == STATUS_PENDING);

			if (!m_supports_synchronous_completion)
			{
				status = STATUS_PENDING;
			}
		}
		else
		{
			io_status_block.Status = status;
		}

		return status;
	}
};

class iocp_scatter_gather_async_operation_storage
	: public iocp_multiplexer::async_operation_storage
{
	struct
	{
		deadline deadline;
		detail::untyped_buffers_storage buffers;
		file_size offset;
		platform_handle const* handle;
		size_t* result;
	}
	m_args;

	byte_io_syscall m_syscall;

	static constexpr size_t slot_count = 8;
	using slot_mask = std::bitset<slot_count>;
	using index_type = uint16_t;

	iocp_multiplexer::timeout_slot m_timeout_slot;
	iocp_multiplexer::io_status_block m_slots[slot_count];
	index_type m_slot_buffer_index[slot_count];

	slot_mask m_pending;
	slot_mask m_errored;

	index_type m_buffer_index;
	index_type m_cancel_index;

public:
	iocp_scatter_gather_async_operation_storage(auto const& args, async_operation_listener* const listener)
		: async_operation_storage(listener)
		, m_args
		{
			args.deadline,
			args.buffers,
			args.offset,
			static_cast<platform_handle const*>(args.handle),
			args.result,
		}
	{
	}

	async_result<void> submit(iocp_multiplexer& m, byte_io_syscall const syscall)
	{
		static constexpr size_t max_buffer_count = static_cast<index_type>(-1);
		static constexpr size_t max_buffer_total = static_cast<size_t>(-1) >> 1;

		vsm_assert(!async_operation::is_submitted());

		auto const buffers = m_args.buffers.buffers();
		if (buffers.empty() || buffers.size() > max_buffer_count)
		{
			return vsm::unexpected(error::invalid_argument);
		}
		for (size_t space = max_buffer_total; auto const buffer : buffers)
		{
			if (space < buffer.size())
			{
				return vsm::unexpected(error::invalid_argument);
			}
			space -= buffer.size();
		}


		m_syscall = syscall;

		m_buffer_index = 0;
		m_cancel_index = buffers.size();

		if (m_args.deadline.is_trivial())
		{
			m_timeout_slot->Status = STATUS_SUCCESS;
		}
		else
		{
			m_timeout_slot.set_handler(*this);
			vsm_try_void(m.start_timeout(m_timeout_slot, m_args.deadline));
		}

		submit_suboperations(m);

		if (m_pending.any())
		{
			for (iocp_multiplexer::io_status_block& slot : m_slots)
			{
				slot.set_handler(*this);
			}

			m.post(*this, async_operation_status::submitted);
		}

		return {};
	}

	vsm::result<void> cancel(iocp_multiplexer& m)
	{
		cancel_subsequent_suboperations(m);
		return {};
	}

private:
	void handle_operation_conclusion(iocp_multiplexer& m)
	{
		auto const buffers = m_args.buffers.buffers();

		NTSTATUS status = STATUS_SUCCESS;
		size_t buffer_count = buffers.size();

		for (size_t const slot_index : detail::bitset::indices(m_errored, true))
		{
			size_t const buffer_index = m_slot_buffer_index[slot_index];

			if (buffer_index < buffer_count)
			{
				buffer_count = buffer_index;
				status = m_slots[slot_index]->Status;
			}
		}

		size_t transferred = 0;
		for (auto const buffer : buffers.subspan(0, buffer_count))
		{
			transferred += buffer.size();
		}
		if (buffer_count < buffers.size() && NT_SUCCESS(status))
		{
			transferred += m_slots[buffer_count]->Information;
		}
		*m_args.result = transferred;

		if (transferred > 0)
		{
			status = STATUS_SUCCESS;
		}

		set_result(nt_error(status));
		m.post(*this, async_operation_status::concluded);
	}

	bool handle_suboperation_completion(iocp_multiplexer& m, size_t const slot_index)
	{
		auto const buffers = m_args.buffers.buffers();

		vsm_assert(m_pending.test(slot_index));
		IO_STATUS_BLOCK const& io_status_block = *m_slots[slot_index];
		vsm_assert(io_status_block.Status != STATUS_PENDING);
		size_t const buffer_index = m_slot_buffer_index[slot_index];
		m_pending.reset(slot_index);

		if (!NT_SUCCESS(io_status_block.Status) ||
			io_status_block.Information != buffers[buffer_index].size())
		{
			m_errored.set(slot_index);

			// Cancel any pending suboperations on buffers after this one.
			cancel_subsequent_suboperations(m, buffer_index + 1);

			goto handle_potential_conclusion;
		}

		if (m_errored.any() || m_buffer_index == buffers.size())
		{
		handle_potential_conclusion:
			if (m_pending.none())
			{
				if (m_timeout_slot->Status == STATUS_PENDING)
				{
					m.cancel_timeout(m_timeout_slot);
				}

				handle_operation_conclusion(m);

				return true;
			}
		}

		return false;
	}

	void submit_suboperations(iocp_multiplexer& m)
	{
		auto const buffers = m_args.buffers.buffers();
		auto const syscall = iocp_byte_io_syscall(m_syscall, *m_args.handle);

		while (m_errored.none() && m_buffer_index < buffers.size())
		{
			auto const opt_slot_index = detail::bitset::flip_any(m_pending, false);

			if (!opt_slot_index)
			{
				break;
			}

			size_t const slot_index = *opt_slot_index;

			size_t const buffer_index = m_buffer_index++;
			auto const buffer = buffers[buffer_index];
			m_slot_buffer_index[slot_index] = buffer_index;

			file_size const offset = m_args.offset;
			m_args.offset = offset + buffer.size();

			if (syscall(*m_slots[slot_index], buffer, offset) != STATUS_PENDING)
			{
				(void)handle_suboperation_completion(m, slot_index);
			}
		}
	}

	// Attempt to cancel any pending suboperations on buffers starting with the specified index.
	void cancel_subsequent_suboperations(iocp_multiplexer& m, size_t const buffer_index = 0)
	{
		if (buffer_index < m_cancel_index)
		{
			native_platform_handle const handle = m_args.handle->get_platform_handle();
			for (size_t const slot_index : detail::bitset::indices(m_pending, true))
			{
				size_t const slot_buffer_index = m_slot_buffer_index[slot_index];
				if (buffer_index <= slot_buffer_index && slot_buffer_index < m_cancel_index)
				{
					// Verify because there is no good way to handle cancellation failure.
					vsm_verify(m.cancel_io(m_slots[slot_index], handle));
				}
			}
			m_cancel_index = buffer_index;
		}
	}

	void io_completed(iocp_multiplexer& m, iocp_multiplexer::io_slot& slot) override
	{
		if (&slot == &m_timeout_slot)
		{
			vsm_assert(m_pending.any());
			cancel_subsequent_suboperations(m);
		}
		else
		{
			size_t const slot_index = &static_cast<iocp_multiplexer::io_status_block&>(slot) - m_slots;
			if (!handle_suboperation_completion(m, slot_index))
			{
				submit_suboperations(m);
			}
		}
	}
};

} // namespace win32

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<win32::iocp_multiplexer, Handle, io::scatter_read_at>
{
	using async_operation_storage = win32::iocp_scatter_gather_async_operation_storage;

	static vsm::result<void> start(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&] { return s.submit(m, win32::NtReadFile); });
	}

	static vsm::result<void> cancel(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return s.cancel(m);
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<win32::iocp_multiplexer, Handle, io::gather_write_at>
{
	using async_operation_storage = win32::iocp_scatter_gather_async_operation_storage;

	static vsm::result<void> start(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&] { return s.submit(m, win32::NtWriteFile); });
	}

	static vsm::result<void> cancel(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return s.cancel(m);
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<win32::iocp_multiplexer, Handle, io::stream_scatter_read>
{
	using async_operation_storage = win32::iocp_scatter_gather_async_operation_storage;

	static vsm::result<void> start(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&] { return s.submit(m, win32::NtReadFile); });
	}

	static vsm::result<void> cancel(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return s.cancel(m);
	}
};

template<std::derived_from<platform_handle> Handle>
struct multiplexer_handle_operation_implementation<win32::iocp_multiplexer, Handle, io::stream_gather_write>
{
	using async_operation_storage = win32::iocp_scatter_gather_async_operation_storage;

	static vsm::result<void> start(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return m.start(s, [&] { return s.submit(m, win32::NtWriteFile); });
	}

	static vsm::result<void> cancel(win32::iocp_multiplexer& m, async_operation_storage& s)
	{
		return s.cancel(m);
	}
};

} // namespace allio
