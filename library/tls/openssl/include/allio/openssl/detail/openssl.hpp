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

struct openssl_ssl_ctx;

void openssl_acquire_ssl_ctx(openssl_ssl_ctx* ssl_ctx);
void openssl_release_ssl_ctx(openssl_ssl_ctx* ssl_ctx);

struct openssl_ssl_ctx_deleter
{
	vsm_static_operator void operator()(openssl_ssl_ctx* const ssl_ctx) vsm_static_operator_const
	{
		openssl_release_ssl_ctx(ssl_ctx);
	}
};
using openssl_ssl_ctx_ptr = std::unique_ptr<openssl_ssl_ctx, openssl_ssl_ctx_deleter>;

vsm::result<openssl_ssl_ctx_ptr> openssl_create_client_ssl_ctx(security_context_parameters const& args);
vsm::result<openssl_ssl_ctx_ptr> openssl_create_server_ssl_ctx(security_context_parameters const& args);

class openssl_security_context
{
	openssl_ssl_ctx_ptr m_ssl_ctx;

protected:
	explicit openssl_security_context(openssl_ssl_ctx_ptr ssl_ctx)
		: m_ssl_ctx(vsm_move(ssl_ctx))
	{
	}

	friend openssl_ssl_ctx* openssl_get_ssl_ctx(openssl_security_context const& security_context);
};

inline openssl_ssl_ctx* openssl_get_ssl_ctx(openssl_security_context const& security_context)
{
	return security_context.m_ssl_ctx.get();
}


struct openssl_ssl;

void openssl_release_ssl(openssl_ssl* ssl);

struct openssl_ssl_deleter
{
	vsm_static_operator void operator()(openssl_ssl* const ssl) vsm_static_operator_const
	{
		openssl_release_ssl(ssl);
	}
};
using openssl_ssl_ptr = std::unique_ptr<openssl_ssl, openssl_ssl_deleter>;


struct tunnel_io_needed
{
	explicit tunnel_io_needed() = default;
};

template<typename T>
using tunnel_result = vsm::result<T, tunnel_io_needed>;


enum class tunnel_io_status : bool
{
	io_needed,
	io_failure,
};

template<typename T>
using tunnel_io_result = vsm::result<T, tunnel_io_status>;

class tunnel_io
{
protected:
	bool m_is_initialized : 1 = false;
	bool m_is_connected : 1 = false;

	bool m_is_tunnel_writable : 1 = false;

	bool m_is_transport_readable : 1 = false;
	bool m_is_transport_writable : 1 = false;

	bool m_read_needed : 1 = false;
	bool m_write_needed : 1 = false;

	bool m_read_buffer_allocated : 1 = false;
	bool m_write_buffer_allocated : 1 = false;

	bool m_read_eof : 1 = false;
	bool m_read_last : 1 = false;

	size_t m_read_beg_offset = 0;
	size_t m_read_end_offset = 0;

	size_t m_write_beg_offset = 0;
	size_t m_write_end_offset = 0;

	size_t m_read_buffer_size = 0;
	size_t m_write_buffer_size = 0;

	std::byte* m_read_buffer_data = nullptr;
	std::byte* m_write_buffer_data = nullptr;

	std::error_code m_read_error;
	std::error_code m_write_error;

public:
	[[nodiscard]] std::error_code consume_last_error()
	{
		std::error_code& e = m_read_last ? m_read_error : m_write_error;
		vsm_assert(e);

		return std::exchange(e, std::error_code());
	}

	[[nodiscard]] bool end_of_stream() const
	{
		return m_read_eof;
	}

	[[nodiscard]] tunnel_io_result<read_result> read_some(read_buffer buffer);
	[[nodiscard]] tunnel_io_result<size_t> write_some(write_buffer buffer);

protected:
	tunnel_io() = default;
	tunnel_io(tunnel_io const&) = delete;
	tunnel_io& operator=(tunnel_io const&) = delete;
	~tunnel_io();

	[[nodiscard]] vsm::result<void> allocate_buffers(
		size_t min_read_buffer_size,
		size_t min_write_buffer_size);

	void read_completed(vsm::result<read_result> const& result);
	void write_completed(vsm::result<size_t> const& result);
};


enum class tunnel_mode : bool
{
	client,
	server,
};

template<typename Tunnel>
concept tls_tunnel_provider = requires (Tunnel& tunnel, tunnel_io& io)
{
	typename Tunnel::config_type;

	{
		tunnel.initialize(
			io,
			tunnel_mode(),
			std::declval<typename Tunnel::config_type const&>())
	} -> std::same_as<vsm::result<void>>;

	{
		tunnel.connect(io, tunnel_mode())
	} -> std::same_as<vsm::result<tunnel_result<void>>>;

	{
		tunnel.disconnect(io, /* full_disconnect: */ false)
	} -> std::same_as<vsm::result<tunnel_result<bool>>>;

	{
		tunnel.read_some(io, read_buffer())
	} -> std::same_as<vsm::result<tunnel_result<read_result>>>;

	{
		tunnel.write_some(io, write_buffer())
	} -> std::same_as<vsm::result<tunnel_result<size_t>>>;
};

template<tls_tunnel_provider Tunnel>
class tunnel_state : tunnel_io
{
	Tunnel m_tunnel;

public:
	[[nodiscard]] vsm::result<void> allocate_buffers(
		size_t const min_read_buffer_size,
		size_t const min_write_buffer_size)
	{
		return tunnel_io::allocate_buffers(min_read_buffer_size, min_write_buffer_size);
	}

	void set_read_buffer(std::span<std::byte> const buffer)
	{
		m_read_buffer_data = buffer.data();
		m_read_buffer_size = buffer.size();
	}

	void set_write_buffer(std::span<std::byte> const buffer)
	{
		m_write_buffer_data = buffer.data();
		m_write_buffer_size = buffer.size();
	}

	[[nodiscard]] read_buffer get_read_buffer()
	{
		vsm_assert(m_read_beg_offset == m_read_end_offset);
		return read_buffer(m_read_buffer_data, m_read_buffer_size);
	}

	[[nodiscard]] write_buffer get_write_buffer()
	{
		return write_buffer(m_write_buffer_data, m_write_buffer_size)
			.subspan(m_write_beg_offset, m_write_end_offset - m_write_beg_offset);
	}

	[[nodiscard]] bool read_needed() const
	{
		return m_read_needed;
	}

	[[nodiscard]] bool write_needed() const
	{
		return m_write_needed;
	}


	[[nodiscard]] bool is_transport_readable() const
	{
		return m_is_transport_readable;
	}

	[[nodiscard]] bool is_transport_writable() const
	{
		return m_is_transport_writable;
	}

	void read_completed(vsm::result<read_result> const& result)
	{
		tunnel_io::read_completed(result);
	}

	void write_completed(vsm::result<size_t> const& result)
	{
		tunnel_io::write_completed(result);
	}

	void disconnect_completed(bool const read, bool const write)
	{
		m_is_transport_readable = !read;
		m_is_transport_writable = !write;
	}


	[[nodiscard]] bool is_tunnel_connected() const
	{
		return m_is_connected;
	}

	[[nodiscard]] bool is_tunnel_readable() const
	{
		vsm_assert(m_is_connected); //PRECONDITION
		return m_is_connected;
	}

	[[nodiscard]] bool is_tunnel_writable() const
	{
		vsm_assert(m_is_connected); //PRECONDITION
		return m_is_tunnel_writable;
	}

	[[nodiscard]] vsm::result<void> initialize(
		tunnel_mode const mode,
		typename Tunnel::config_type const& config)
	{
		vsm_assert(!m_is_initialized); //PRECONDITION

		auto const r = m_tunnel.initialize(
			static_cast<tunnel_io&>(*this),
			mode,
			config);

		if (r)
		{
			m_is_initialized = true;
		}

		return r;
	}

	[[nodiscard]] vsm::result<tunnel_result<void>> connect(tunnel_mode const mode)
	{
		vsm_assert(m_is_initialized); //PRECONDITION
		vsm_assert(!m_is_connected); //PRECONDITION

		auto const r = m_tunnel.connect(static_cast<tunnel_io&>(*this), mode);
		if (r && *r)
		{
			m_is_connected = true;
			m_is_tunnel_writable = true;
		}
		return r;
	}

	[[nodiscard]] vsm::result<tunnel_result<void>> disconnect(
		bool const disconnect_read,
		bool const disconnect_write)
	{
		vsm_assert(m_is_connected); //PRECONDITION

		if (!disconnect_write)
		{
			if (disconnect_read)
			{
				return vsm::unexpected(error::unsupported_operation);
			}
			else
			{
				return vsm::unexpected(error::invalid_argument);
			}
		}

		if (!m_is_tunnel_writable && (m_is_transport_writable || !disconnect_read))
		{
			return {};
		}

		auto const r_1 = m_tunnel.disconnect(
			static_cast<tunnel_io&>(*this),
			/* full_disconnect: */ disconnect_read);

		if (!r_1)
		{
			return vsm::unexpected(r_1.error());
		}

		auto const& r_2 = *r_1;

		if (!r_2)
		{
			return vsm::success(vsm::unexpected(r_2.error()));
		}

		m_is_tunnel_writable = false;

		if (*r_2)
		{
			m_is_connected = false;
		}

		return {};
	}

	[[nodiscard]] vsm::result<tunnel_result<read_result>> read_some(read_buffer const buffer)
	{
		vsm_assert(m_is_connected); //PRECONDITION
		return m_tunnel.read_some(static_cast<tunnel_io&>(*this), buffer);
	}

	[[nodiscard]] vsm::result<tunnel_result<size_t>> write_some(write_buffer const buffer)
	{
		vsm_assert(m_is_connected); //PRECONDITION
		return m_tunnel.write_some(static_cast<tunnel_io&>(*this), buffer);
	}
};


class openssl_tunnel
{
	openssl_ssl_ptr m_ssl;

public:
	using config_type = openssl_ssl_ctx*;

	vsm::result<void> initialize(tunnel_io& io, tunnel_mode mode, openssl_ssl_ctx* ssl_ctx);

	vsm::result<tunnel_result<void>> connect(tunnel_io& io, tunnel_mode mode);
	vsm::result<tunnel_result<bool>> disconnect(tunnel_io& io, bool full_disconnect);

	vsm::result<tunnel_result<read_result>> read_some(tunnel_io& io, read_buffer buffer);
	vsm::result<tunnel_result<size_t>> write_some(tunnel_io& io, write_buffer buffer);
};

using openssl_tunnel_state = tunnel_state<openssl_tunnel>;

} // namespace allio::detail
