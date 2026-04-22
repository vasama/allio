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


struct openssl_pending_t
{
	explicit openssl_pending_t() = default;
};

template<typename T>
using openssl_result = vsm::result<T, openssl_pending_t>;

struct openssl_operation_base : vsm::intrusive::mpsc_queue_link
{
	enum class state_flags : uint8_t
	{
		active                  = 1 << 0,
		cancel                  = 1 << 1,
	};

	vsm::atomic<state_flags> m_state_flags;
};


class openssl_stream
{
	struct bio_type;

	openssl_ssl_ptr m_ssl;

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
	openssl_stream() = default;
	openssl_stream(openssl_stream const&) = delete;
	openssl_stream& operator=(openssl_stream const&) = delete;
	~openssl_stream();

	vsm::result<void> initialize(openssl_ssl_ctx* ssl_ctx);

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

	vsm::result<openssl_result<void>> accept();
	vsm::result<openssl_result<void>> connect();
	vsm::result<openssl_result<void>> disconnect();

	vsm::result<openssl_result<size_t>> read_some(read_buffer user_buffer);
	vsm::result<openssl_result<size_t>> write_some(write_buffer user_buffer);

private:
	void set_last_error(std::error_code error);

	template<auto Function, int SuccessThreshold, typename SSL>
	vsm::result<openssl_result<int>> ssl_try(SSL* const ssl, auto const... args);
};

vsm::result<openssl_stream*> new_openssl_socket(openssl_ssl_ctx* ssl_ctx);
void delete_openssl_socket(openssl_stream* socket);


#if 0
struct openssl_socket_state_base
{
	openssl_ssl_ptr m_ssl;

	vsm::result<openssl_result<void>> accept();
	vsm::result<openssl_result<void>> connect();
	vsm::result<openssl_result<void>> disconnect();

	vsm::result<openssl_result<size_t>> read(read_buffer user_buffer);
	vsm::result<openssl_result<size_t>> write(write_buffer user_buffer);
};

template<typename RawSocket>
struct openssl_socket_state
{
	native_handle<RawSocket> m_raw_h;
};


struct openssl_listen_socket_state_base
{
	openssl_ssl_ptr m_ssl;
};

template<typename Multiplexer, typename RawSocketObject>
struct openssl_listen_socket_state : openssl_listen_socket_state_base
{
	async_operation_t<Multiplexer, RawSocketObject, listen_t> m_raw_state;
};



struct openssl_object_base
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

	vsm::intrusive::mpsc_queue<openssl_operation_base> m_queue;

	openssl_object_base() = default;
	openssl_object_base(openssl_object_base const&) = delete;
	openssl_object_base& operator=(openssl_object_base const&) = delete;
	virtual ~openssl_object_base() = default;

	vsm::result<void> initialize(openssl_ssl_ctx* ssl_ctx);

	vsm::result<openssl_result<void>> accept();
	vsm::result<openssl_result<void>> connect();
	vsm::result<openssl_result<void>> disconnect();

	vsm::result<openssl_result<size_t>> read(read_buffer user_buffer);
	vsm::result<openssl_result<size_t>> write(write_buffer user_buffer);

	read_buffer get_read_buffer()
	{
		return read_buffer(m_r_beg, m_r_end);
	}

	void read_completed(size_t const transferred)
	{
		m_want_read = false;
	}

	write_buffer get_write_buffer()
	{
		return write_buffer(m_w_beg, m_w_pos);
	}

	void write_completed(size_t const transferred)
	{
		m_want_write = false;
	}

	void delete_context();
};

template<typename Multiplexer, typename RawSocket>
struct openssl_socket_object : openssl_object_base
{
	template<typename Operation>
	using operation_state = async_operation_t<Multiplexer, RawSocket, Operation>;

	native_handle<RawSocket> m_h;
	async_connector_t<Multiplexer, RawSocket> m_c;

	operation_state<byte_io::stream_read_t> m_r_state;
	operation_state<byte_io::stream_write_t> m_w_state;

	using openssl_object_base::openssl_object_base;
};

vsm::result<void*> allocate_openssl_socket(size_t size);

template<typename Multiplexer, typename RawSocket>
vsm::result<openssl_socket_object<Multiplexer, RawSocket>*> create_openssl_socket(
	openssl_ssl_ctx* const ssl_ctx)
{
	using object_type = openssl_socket_object<Multiplexer, RawSocket>;
	static_assert(alignof(object_type) <= alignof(std::max_align_t));

	vsm_try(object, detail::make_unique<object_type>());
	vsm_try_void(object->initialize(ssl_ctx));

	return object.release();
}

template<typename... States>
struct openssl_listen_socket_object : openssl_object_base
{
	std::variant<States...> m_raw_states;
};

template<typename... RawOperationStates>
struct openssl_object : openssl_object_base
{
	std::variant<RawOperationStates...> m_raw_state;

	static vsm::result<openssl_object*> create(openssl_ssl_ctx* const ssl_ctx);
};
#endif

} // namespace allio::detail
