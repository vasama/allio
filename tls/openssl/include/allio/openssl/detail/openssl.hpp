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

vsm::result<openssl_ssl_ctx_ptr> openssl_create_client_ssl_ctx(security_context_parameters const& args);
vsm::result<openssl_ssl_ctx_ptr> openssl_create_server_ssl_ctx(security_context_parameters const& args);

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


struct openssl_ssl;

void openssl_release_ssl(openssl_ssl* ssl);

struct openssl_ssl_deleter
{
	void vsm_static_operator_invoke(openssl_ssl* const ssl)
	{
		openssl_release_ssl(ssl);
	}
};
using openssl_ssl_ptr = std::unique_ptr<openssl_ssl, openssl_ssl_deleter>;


template<typename T>
using openssl_result = vsm::result<T, std::monostate>;

struct openssl_state_base
{
	struct bio_type;

	openssl_ssl_ptr m_ssl;

	std::byte* m_r_beg = nullptr;
	std::byte* m_r_pos = nullptr;
	std::byte* m_r_end = nullptr;

	std::byte* m_w_beg = nullptr;
	std::byte* m_w_pos = nullptr;
	std::byte* m_w_end = nullptr;

	bool m_want_read = false;
	bool m_want_write = false;

	vsm::result<openssl_ssl_ptr> create_ssl(openssl_ssl_ctx* ssl_ctx);

	vsm::result<openssl_result<void>> accept();
	vsm::result<openssl_result<void>> connect();
	vsm::result<openssl_result<void>> disconnect();

	vsm::result<openssl_result<size_t>> read(read_buffer user_buffer);
	vsm::result<openssl_result<size_t>> write(write_buffer user_buffer);
};

#if 0
//TODO: Move elsewhere
template<typename Allocator, size_t BufferSize>
class allocator_buffer_pool_handle
{
	static_assert(std::is_same_v<typename Allocator::value_type, std::byte>);
	vsm_no_unique_address Allocator m_allocator;

public:
	using buffer_pool_handle_concept = void;

	class buffer_handle_type
	{
		std::byte* m_buffer;

	public:
		[[nodiscard]] std::byte* data() const
		{
			return m_buffer;
		}

		[[nodiscard]] size_t size() const
		{
			return BufferSize;
		}

	private:
		explicit buffer_handle_type(std::byte* const buffer)
			: m_buffer(buffer)
		{
		}

		friend allocator_buffer_pool_handle;
	};

	[[nodiscard]] vsm::result<buffer_handle_type> acquire()
	{
		try
		{
			return buffer_handle_type(m_allocator.allocate(BufferSize));
		}
		catch (std::bad_alloc const&)
		{
			return vsm::unexpected(error::not_enough_memory);
		}
	}

	void release(buffer_handle_type const buffer)
	{
		m_allocator.deallocate(m_buffer, BufferSize);
	}
};

template<typename BufferPoolHandle>
class openssl_state : openssl_state_base
{
	using buffer_handle_type = typename BufferPoolHandle::buffer_handle_type;

	vsm_no_unique_address BufferPoolHandle m_buffer_pool_handle;
	buffer_handle_type m_r_buffer;
	buffer_handle_type m_w_buffer;

public:
	
};

template<typename BufferPool>
using openssl_state_ptr = std::unique_ptr<openssl_state<BufferPool>>;
#endif

} // namespace allio::detail
