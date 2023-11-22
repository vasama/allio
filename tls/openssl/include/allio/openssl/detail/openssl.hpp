#pragma once

#include <allio/byte_io_buffers.hpp>
#include <allio/detail/network_security.hpp>

#include <vsm/assert.h>
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
	void vsm_static_operator_invoke(openssl_ssl_ctx* const ssl_ctx)
	{
		openssl_release_ssl_ctx(ssl_ctx);
	}
};

using openssl_ssl_ctx_ptr = std::unique_ptr<openssl_ssl_ctx, openssl_ssl_ctx_deleter>;


vsm::result<openssl_ssl_ctx_ptr> create_client_ssl_ctx(security_context_parameters const& args);
vsm::result<openssl_ssl_ctx_ptr> create_server_ssl_ctx(security_context_parameters const& args);


class openssl_security_context
{
	openssl_ssl_ctx_ptr m_ssl_ctx;

public:
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


enum class openssl_request_status : unsigned char
{
	none,
	pending,
	completed,
};

enum class openssl_request_kind : bool
{
	read,
	write,
};

template<typename T>
using openssl_io_result = vsm::result<T, openssl_request_kind>;

class openssl_ssl
{
protected:
	openssl_request_status m_request_status;
	openssl_request_kind m_request_kind;
	union
	{
		std::byte* m_request_read;
		std::byte const* m_request_write;
	};
	size_t m_request_size;

public:
	static vsm::result<openssl_ssl*> create(openssl_ssl_ctx* ssl_ctx);
	void delete_context();

	vsm::result<openssl_io_result<void>> connect();
	vsm::result<openssl_io_result<void>> disconnect();
	vsm::result<openssl_io_result<void>> accept();

	vsm::result<openssl_io_result<size_t>> read(void* data, size_t size);
	vsm::result<openssl_io_result<size_t>> write(void const* data, size_t size);

	read_buffer get_read_buffer()
	{
		vsm_assert(m_request_status == openssl_request_status::pending);
		vsm_assert(m_request_kind == openssl_request_kind::read);
		return read_buffer(m_request_read, m_request_size);
	}

	void read_completed(size_t const transferred)
	{
		vsm_assert(m_request_status == openssl_request_status::pending);
		vsm_assert(m_request_kind == openssl_request_kind::read);
		vsm_assert(transferred <= m_request_size);
		m_request_size = transferred;
		m_request_status = openssl_request_status::completed;
	}

	write_buffer get_write_buffer()
	{
		vsm_assert(m_request_status == openssl_request_status::pending);
		vsm_assert(m_request_kind == openssl_request_kind::write);
		return write_buffer(m_request_write, m_request_size);
	}

	void write_completed(size_t const transferred)
	{
		vsm_assert(m_request_status == openssl_request_status::pending);
		vsm_assert(m_request_kind == openssl_request_kind::write);
		vsm_assert(transferred <= m_request_size);
		m_request_size = transferred;
		m_request_status = openssl_request_status::completed;
	}

protected:
	openssl_ssl() = default;
	openssl_ssl(openssl_ssl const&) = default;
	openssl_ssl& operator=(openssl_ssl const&) = default;
	~openssl_ssl() = default;
};

} // namespace allio::detail
