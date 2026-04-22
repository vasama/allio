#pragma once

#include <allio/byte_io_buffers.hpp>
#include <allio/detail/handles/socket_base.hpp>
#include <allio/detail/handles/listen_socket_base.hpp>
#include <allio/detail/io.hpp>
#include <allio/detail/network_security.hpp>
#include <allio/detail/new.hpp>

#include <vsm/assert.h>
#include <vsm/atomic.hpp>
#include <vsm/intrusive/mpsc_queue.hpp>
#include <vsm/standard.hpp>
#include <vsm/result.hpp>

#include <memory>
#include <span>

namespace allio::detail {

struct a_s2n_config;

void a_s2n_acquire_ssl_ctx(a_s2n_config* config);
void a_s2n_release_config(a_s2n_config* config);

struct a_s2n_config_deleter
{
	vsm_static_operator void operator()(a_s2n_config* const config) vsm_static_operator_const
	{
		a_s2n_release_config(config);
	}
};
using a_s2n_config_ptr = std::unique_ptr<a_s2n_config, a_s2n_config_deleter>;

vsm::result<a_s2n_config_ptr> a_s2n_create_client_ssl_ctx(security_context_parameters const& args);
vsm::result<a_s2n_config_ptr> a_s2n_create_server_ssl_ctx(security_context_parameters const& args);

class a_s2n_security_context
{
	a_s2n_config_ptr m_config;

protected:
	explicit a_s2n_security_context(a_s2n_config_ptr config)
		: m_config(vsm_move(config))
	{
	}

	friend a_s2n_config* a_s2n_get_ssl_ctx(a_s2n_security_context const& security_context);
};

inline a_s2n_config* a_s2n_get_ssl_ctx(a_s2n_security_context const& security_context)
{
	return security_context.m_config.get();
}


struct a_s2n_connection;

void a_s2n_release_connection(a_s2n_connection* connection);

struct a_s2n_connection_deleter
{
	vsm_static_operator void operator()(a_s2n_connection* const connection) vsm_static_operator_const
	{
		a_s2n_release_connection(connection);
	}
};
using a_s2n_connection_ptr = std::unique_ptr<a_s2n_connection, a_s2n_connection_deleter>;


struct a_s2n_pending_t
{
	explicit a_s2n_pending_t() = default;
};

template<typename T>
using a_s2n_result = vsm::result<T, a_s2n_pending_t>;

struct a_s2n_operation_base : vsm::intrusive::mpsc_queue_link
{
	enum class state_flags : uint8_t
	{
		active                  = 1 << 0,
		cancel                  = 1 << 1,
	};

	vsm::atomic<state_flags> m_state_flags;
};


class a_s2n_stream
{
	struct bio_type;

	a_s2n_connection_ptr m_connection;

	uint32_t m_r_io_desired : 1 = 0;
	uint32_t m_w_io_desired : 1 = 0;

	uint32_t m_r_unique_buffer : 1 = 0;
	uint32_t m_w_unique_buffer : 1 = 0;

	uint32_t m_r_end_of_stream : 1 = 0;

	size_t m_r_beg_offset = 0;
	size_t m_r_end_offset = 0;

	size_t m_w_beg_offset = 0;
	size_t m_w_end_offset = 0;

	union
	{
		struct
		{
			std::byte* m_r_buffer_data;
			size_t m_r_buffer_size;
		};
	};

	union
	{
		struct
		{
			std::byte* m_w_buffer_data;
			size_t m_w_buffer_size;
		};
	};

	std::error_code m_last_error;

public:
	a_s2n_stream() = default;
	a_s2n_stream(a_s2n_stream const&) = delete;
	a_s2n_stream& operator=(a_s2n_stream const&) = delete;
	~a_s2n_stream();

	vsm::result<void> initialize(a_s2n_config* config);

	vsm::result<void> allocate_buffers();

	void set_r_buffer(std::span<std::byte> const buffer)
	{
		vsm_assert(m_r_beg_offset == m_r_end_offset);

		m_r_buffer_data = buffer.data();
		m_r_buffer_size = buffer.size();

		m_r_beg_offset = 0;
		m_r_end_offset = 0;
	}

	void set_w_buffer(std::span<std::byte> const buffer)
	{
		vsm_assert(m_w_beg_offset == m_w_end_offset);

		m_w_buffer_data = buffer.data();
		m_w_buffer_size = buffer.size();

		m_w_beg_offset = 0;
		m_w_end_offset = 0;
	}

	bool want_read() const
	{
		return m_r_io_desired;
	}

	bool want_write() const
	{
		return m_w_io_desired;
	}

	read_buffer get_read_buffer() const
	{
		vsm_assert(m_r_beg_offset == m_r_end_offset);
		return read_buffer(m_r_buffer_data, m_r_buffer_size);
	}

	write_buffer get_write_buffer() const
	{
		return write_buffer(m_w_buffer_data, m_w_buffer_size)
			.subspan(m_w_beg_offset, m_w_end_offset - m_w_beg_offset);
	}

	void read_completed(vsm::result<size_t> result);
	void write_completed(vsm::result<size_t> result);

	vsm::result<a_s2n_result<void>> accept();
	vsm::result<a_s2n_result<void>> connect();
	vsm::result<a_s2n_result<void>> disconnect();

	vsm::result<a_s2n_result<size_t>> read_some(read_buffer user_buffer);
	vsm::result<a_s2n_result<size_t>> write_some(write_buffer user_buffer);

private:
	void set_last_error(std::error_code error);
};

vsm::result<a_s2n_stream*> a_s2n_new_stream(a_s2n_config* config);
void a_s2n_delete_stream(a_s2n_stream* socket);

} // namespace allio::detail
