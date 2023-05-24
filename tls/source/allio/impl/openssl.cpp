#include <allio/secure_socket_handle.hpp>

#include <openssl/bio.h>
#include <openssl/ssl.h>

using namespace allio;

namespace {

template<typename T, typename... Args>
vsm::result<std::unique_ptr<T>> make_unique(Args&&... args)
{
	T* const ptr = new (std::nothrow) T(static_cast<Args&&>(args)...);

	if (ptr == nullptr)
	{
		return vsm::unexpected(error::not_enough_memory);
	}

	return ptr;
}


enum class openssl_error : int
{
};

static openssl_error get_last_openssl_error();


template<typename T, void Function(T*)>
struct openssl_deleter
{
	void operator()(T* const pointer) const
	{
		Function(pointer);
	}
};

template<typename T, auto Function>
using openssl_ptr = std::unique_ptr<T, openssl_deleter<T, Function>>;


using bio_method_ptr = openssl_ptr<BIO_METHOD, BIO_meth_free>;
using bio_ptr = openssl_ptr<BIO, BIO_free>;


static vsm::result<bio_method_ptr> const& create_stream_socket_bio_method()
{
	BIO_METHOD* const method = BIO_meth_new(BIO_get_new_index() | BIO_TYPE_SOURCE_SINK, "allio_stream_socket");

	if (method == nullptr)
	{
		return vsm::unexpected(get_last_openssl_error());
	}

	BIO_meth_set_write_ex(method, [](BIO* const bio, char const* const buffer, size_t const size, size_t* const transferred) -> int
	{
	});

	BIO_meth_set_read_ex(method, [](BIO* const bio, char* const buffer, size_t const size, size_t* const transferred) -> int
	{
	});

	BIO_meth_set_ctrl_ex(method, [](BIO* const bio, int const cmd, long const larg, void* const parg) -> long
	{
	});

	return vsm::result<bio_method_ptr>(vsm::result_value, method);
}


struct openssl_resources
{
	bio_method_ptr stream_socket_bio_method;

	vsm::result<bio_ptr> create_stream_socket_bio(stream_socket_handle& socket)
	{
		BIO* const bio = BIO_new(resources.stream_socket_bio_method.get());

		if (bio == nullptr)
		{
			return vsm::unexpected(get_last_openssl_error());
		}

		BIO_set_data(bio, &socket);

		return vsm::result<bio_ptr>(vsm::result_value, bio);
	}

	vsm::result<bio_ptr> create_ssl_bio(bool const is_client, bool const verify_peer)
	{
		BIO* const bio = BIO_new_ssl(nullptr, is_client);

		if (bio == nullptr)
		{
			return vsm::unexpected(get_last_openssl_error());
		}

		return vsm::result<bio_ptr>(vsm::result_value, bio);
	}
};

vsm::result<openssl_resources const*> initialize_resources()
{
	static vsm::result<openssl_resources> resources = [&]() -> vsm::result<openssl_resources>
	{
		openssl_resources r;
		vsm_try_assign(r.stream_socket_method, create_stream_socket_method());
		return r;
	};

	if (!resources)
	{
		return vsm::unexpected(resources.error());
	}

	return &*resources;
}


struct openssl_stream_socket
{
	using socket_handle_type = socket_stream_handle;

	socket_handle_type* m_socket;

	bio_ptr m_socket_bio;
	bio_ptr m_secure_bio;

	explicit openssl_stream_socket(socket_handle_type& socket)
		: m_socket(&socket)
	{
	}

	int bio_write(BIO* const bio, char const* const buffer, size_t const size, size_t* const transferred)
	{
	}

	int bio_read(BIO* const bio, char* const buffer, size_t const size, size_t* const transferred)
	{
	}

	long bio_ctrl(BIO* const bio, int const cmd, long const larg, void* const parg)
	{
	}

	vsm::result<void> initialize(openssl_resources& openssl, bool const is_client)
	{
		vsm_try_assign(m_socket_bio, openssl.create_stream_socket_bio(*this));
		vsm_try_assign(m_secure_bio, openssl.create_ssl_bio(is_client, true));

		vsm_verify(BIO_push(m_secure_bio.get(), m_socket_bio.get()) == m_secure_bio);

		return {};
	}
};

struct openssl_listen_socket
{
	using socket_handle_type = socket_listen_handle;

	socket_handle_type* m_socket;

	explicit openssl_listen_socket(socket_handle_type& socket)
		: m_socket(&socket)
	{
	}
};

template<typename SecureSocket>
struct openssl_socket
{
	typename SecureSocket::socket_handle_type m_socket_handle;

	openssl_socket()
		: SecureSocket(m_socket_handle)
	{
	}
};

struct openssl_socket_source
{
	template<typename SecureSocket>
	vsm::result<void> create_internal()
	{
		vsm_try(socket, make_unique<SecureSocket>());
		vsm_try_void(socket->initialize());
	}

	vsm::result<secure_stream_socket_handle> create()
	{
	}

	vsm::result<secure_listen_socket_handle> create()
	{
	}


	template<typename SecureSocket>
	vsm::result<void> wrap_internal(typename SecureSocket::socket_handle_type& user_socket)
	{
		vsm_try(socket, make_unique<openssl_socket<SecureSocket>>(user_socket));
		vsm_try_void(socket->initialize());
	}

	vsm::result<secure_stream_socket_handle> wrap(stream_socket_handle& socket)
	{
		return wrap_internal<secure_stream_socket_handle>(socket);
	}

	vsm::result<secure_listen_socket_handle> wrap(listen_socket_handle& socket)
	{
		return wrap_internal<secure_listen_socket_handle>(socket);
	}
};

} // namespace
