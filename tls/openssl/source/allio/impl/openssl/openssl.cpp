#include <allio/openssl/detail/openssl.hpp>
#include <allio/error.hpp>
#include "../../../../../../allio/source/allio/impl/new.hpp"

#include <vsm/assert.h>
#include <vsm/standard.hpp>

#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>

#include <memory>
#include <optional>

using namespace allio;
using namespace allio::detail;

namespace {

enum class openssl_error : unsigned {};

struct openssl_error_category : std::error_category
{
	const char* name() const noexcept override
	{
		return "OpenSSL-Crypto";
	}

	std::string message(int const e) const override
	{
		//TODO
		return "openssl error";
	}
};
static openssl_error_category const openssl_error_category_instance;

static std::error_code make_error_code(openssl_error const e)
{
	return std::error_code(static_cast<int>(e), openssl_error_category_instance);
}

static openssl_error get_last_openssl_error()
{
	return static_cast<openssl_error>(ERR_peek_error());
}


enum class ssl_error : int {};

struct ssl_error_category : std::error_category
{
	const char* name() const noexcept override
	{
		return "OpenSSL-SSL";
	}

	std::string message(int const e) const override
	{
		//TODO
		return "ssl error";
	}
};
static ssl_error_category const ssl_error_category_instance;

static std::error_code make_error_code(ssl_error const e)
{
	return std::error_code(static_cast<int>(e), ssl_error_category_instance);
}

static ssl_error get_ssl_error(SSL* const ssl, int const r)
{
	return static_cast<ssl_error>(SSL_get_error(ssl, r));
}

} // namespace

template<>
struct std::is_error_code_enum<openssl_error>
{
	static constexpr bool value = true;
};

template<>
struct std::is_error_code_enum<ssl_error>
{
	static constexpr bool value = true;
};


namespace {

template<typename T, auto Function>
struct openssl_deleter
{
	void vsm_static_operator_invoke(T* const pointer)
	{
		Function(pointer);
	}
};

template<typename T, auto Function>
using openssl_ptr = std::unique_ptr<T, openssl_deleter<T, Function>>;

template<auto Make, auto Free, typename... Args>
static auto openssl_make(Args const... args)
	-> vsm::result<openssl_ptr<std::remove_pointer_t<std::invoke_result_t<decltype(Make), Args...>>, Free>>
{
	auto* const ptr = Make(args...);

	if (ptr == nullptr)
	{
		return vsm::unexpected(get_last_openssl_error());
	}

	return openssl_ptr<std::remove_pointer_t<std::invoke_result_t<decltype(Make), Args...>>, Free>(ptr);
}


using bio_method_ptr = openssl_ptr<BIO_METHOD, BIO_meth_free>;
using bio_ptr = openssl_ptr<BIO, BIO_free>;

template<typename T>
struct _bio_function;

template<typename Bio, typename R, typename... P>
struct _bio_function<R(Bio::*)(P...)>
{
	template<R(Bio::* F)(P...)>
	static R function(BIO* const bio, P const... args)
	{
		return (static_cast<Bio*>(BIO_get_data(bio))->*F)(args...);
	}
};

template<typename Bio, typename R, typename... P>
struct _bio_function<R(Bio::*)(BIO*, P...)>
{
	template<R(Bio::* F)(BIO*, P...)>
	static R function(BIO* const bio, P const... args)
	{
		return (static_cast<Bio*>(BIO_get_data(bio))->*F)(bio, args...);
	}
};

template<auto F>
inline constexpr auto bio_function = &_bio_function<decltype(F)>::template function<F>;

template<typename Bio>
static vsm::result<bio_method_ptr> create_bio_method()
{
	int const index = BIO_get_new_index();
	if (index == -1)
	{
		return vsm::unexpected(get_last_openssl_error());
	}

	vsm_try(method, openssl_make<BIO_meth_new, BIO_meth_free>(
		index | BIO_TYPE_SOURCE_SINK,
		Bio::bio_name));

#if 0
	vsm_verify(BIO_meth_set_destroy(method.get(), [](BIO* const bio) -> int
	{
		delete static_cast<Bio*>(BIO_get_data(bio));
		return 0;
	}));
#endif

	vsm_verify(BIO_meth_set_read_ex(method.get(), bio_function<&Bio::read_ex>));
	vsm_verify(BIO_meth_set_write_ex(method.get(), bio_function<&Bio::write_ex>));
	vsm_verify(BIO_meth_set_ctrl(method.get(), bio_function<&Bio::ctrl>));

	return method;
}

template<typename Bio>
static vsm::result<bio_ptr> create_bio(Bio& bio_object, auto&&... args)
{
	static auto const method_r = create_bio_method<Bio>();

	vsm_try((auto const&, method), method_r);
	vsm_try(bio, openssl_make<BIO_new, BIO_free>(method.get()));
	BIO_set_data(bio.get(), &bio_object);

	return bio;
}


using ssl_ctx_ptr = openssl_ptr<SSL_CTX, SSL_CTX_free>;
using ssl_ptr = openssl_ptr<SSL, SSL_free>;

static vsm::result<ssl_ctx_ptr> create_ssl_ctx(SSL_METHOD const* const ssl_method)
{
	vsm_try(ctx, openssl_make<SSL_CTX_new, SSL_CTX_free>(ssl_method));

	if (!SSL_CTX_set_min_proto_version(ctx.get(), TLS1_2_VERSION))
	{
		return vsm::unexpected(get_last_openssl_error());
	}

	if (!SSL_CTX_set_cipher_list(ctx.get(), OSSL_default_cipher_list()))
	{
		return vsm::unexpected(get_last_openssl_error());
	}

	if (!SSL_CTX_set_ciphersuites(ctx.get(), OSSL_default_ciphersuites()))
	{
		return vsm::unexpected(get_last_openssl_error());
	}

	return ctx;
}

template<auto Function>
static vsm::result<openssl_io_result<int>> ssl_try(SSL* const ssl, auto const... args)
{
	int const r = Function(ssl, args...);

	if (r < 0)
	{
		switch (int const e = SSL_get_error(ssl, r))
		{
		case SSL_ERROR_WANT_READ:
			return openssl_io_result<int>(vsm::unexpected(openssl_request_kind::read));

		case SSL_ERROR_WANT_WRITE:
			return openssl_io_result<int>(vsm::unexpected(openssl_request_kind::write));

		default:
			ERR_load_SSL_strings();
			ERR_print_errors_fp(stderr);
			return vsm::unexpected(static_cast<ssl_error>(e));
		}
	}

	return r;
}



struct ssl_implementation : openssl_ssl
{
	static constexpr char bio_name[] = "ALLIO Socket R/W BIO";

	bio_ptr bio;
	ssl_ptr ssl;

	ssl_implementation()
	{
		m_request_status = openssl_request_status::none;
	}

	int read_ex(BIO* const bio, char* const buffer, size_t size, size_t* const transferred)
	{
		switch (m_request_status)
		{
		case openssl_request_status::none:
			m_request_status = openssl_request_status::pending;
			m_request_kind = openssl_request_kind::read;
			m_request_read = reinterpret_cast<std::byte*>(buffer);
			m_request_size = size;
			break;

		case openssl_request_status::pending:
			break;

		case openssl_request_status::completed:
			if (m_request_kind == openssl_request_kind::read)
			{
				vsm_assert(m_request_read == reinterpret_cast<std::byte*>(buffer));
				vsm_assert(m_request_size <= size);
				m_request_status = openssl_request_status::none;
				*transferred = m_request_size;
				return 1;
			}
			break;
		}

		BIO_set_retry_read(bio);
		return 0;
	}

	int write_ex(BIO* const bio, char const* const buffer, size_t size, size_t* const transferred)
	{
		switch (m_request_status)
		{
		case openssl_request_status::none:
			m_request_status = openssl_request_status::pending;
			m_request_kind = openssl_request_kind::write;
			m_request_write = reinterpret_cast<std::byte const*>(buffer);
			m_request_size = size;
			break;

		case openssl_request_status::pending:
			break;

		case openssl_request_status::completed:
			if (m_request_kind == openssl_request_kind::write)
			{
				vsm_assert(m_request_read == reinterpret_cast<std::byte const*>(buffer));
				vsm_assert(m_request_size <= size);
				m_request_status = openssl_request_status::none;
				*transferred = m_request_size;
				return 1;
			}
			break;
		}

		BIO_set_retry_write(bio);
		return 0;
	}

	long ctrl(BIO* const bio, int const cmd, long const larg, void* const parg)
	{
		switch (cmd)
		{
		case BIO_CTRL_EOF:
		case BIO_CTRL_PUSH:
		case BIO_CTRL_POP:
			return 0;

		case BIO_CTRL_FLUSH:
			return 1;
		}
		return -1;
	}
};

//static constexpr size_t context_buffer_size = 256;
//static constexpr size_t context_buffer_offset = offsetof(ssl_implementation, buffer);

} // namespace

void detail::openssl_acquire_ssl_ctx(openssl_ssl_ctx* const ssl_ctx)
{
	SSL_CTX_up_ref(reinterpret_cast<SSL_CTX*>(ssl_ctx));
}

void detail::openssl_release_ssl_ctx(openssl_ssl_ctx* const ssl_ctx)
{
	SSL_CTX_free(reinterpret_cast<SSL_CTX*>(ssl_ctx));
}

static vsm::result<int> get_tls_version(security_context_parameters const& args)
{
	switch (args.tls_min_version.value_or(tls_version::tls_1_2))
	{
	case tls_version::ssl_3:
		return SSL3_VERSION;
	case tls_version::tls_1_0:
		return TLS1_VERSION;
	case tls_version::tls_1_1:
		return TLS1_1_VERSION;
	case tls_version::tls_1_2:
		return TLS1_2_VERSION;
	case tls_version::tls_1_3:
		return TLS1_3_VERSION;
	}
	return vsm::unexpected(error::invalid_argument);
}

static int get_verification(security_context_parameters const& args, bool const client)
{
	auto const default_value = client
		? tls_verification::optional
		: tls_verification::required;

	int verify = SSL_VERIFY_NONE;
	switch (args.tls_verification.value_or(default_value))
	{
	case tls_verification::required:
	case tls_verification::optional:
		verify = SSL_VERIFY_PEER;
		break;
	}
	return verify;
}

static vsm::result<openssl_ssl_ctx_ptr> create_ssl_ctx(security_context_parameters const& args, bool const client)
{
	vsm_try(ssl_ctx, openssl_make<SSL_CTX_new, SSL_CTX_free>(
		client ? TLS_client_method() : TLS_server_method()));

	vsm_try(min_version, get_tls_version(args));
	if (!SSL_CTX_set_min_proto_version(ssl_ctx.get(), min_version))
	{
		return vsm::unexpected(get_last_openssl_error());
	}

	int const verification = get_verification(args, client);
	SSL_CTX_set_verify(
		ssl_ctx.get(),
		get_verification(args, client),
		nullptr); //TODO: Verify callback

#if 0
	if (!SSL_CTX_use_certificate_file(
		ssl_ctx.get(),
		))
	{
		return vsm::unexpected(get_last_openssl_error());
	}

	if (!SSL_CTX_use_PrivateKey_file(
		ssl_ctx.get(),
		))
	{
		return vsm::unexpected(get_last_openssl_error());
	}
#endif

	return vsm_lazy(openssl_ssl_ctx_ptr(reinterpret_cast<openssl_ssl_ctx*>(ssl_ctx.release())));
}

vsm::result<openssl_ssl_ctx_ptr> detail::create_client_ssl_ctx(security_context_parameters const& args)
{
	return create_ssl_ctx(args, /* client: */ true);
}

vsm::result<openssl_ssl_ctx_ptr> detail::create_server_ssl_ctx(security_context_parameters const& args)
{
	return create_ssl_ctx(args, /* client: */ false);
}


vsm::result<openssl_ssl*> openssl_ssl::create(openssl_ssl_ctx* const _ssl_ctx)
{
	auto const ssl_ctx = reinterpret_cast<SSL_CTX*>(_ssl_ctx);

	//void* const storage = operator new(context_buffer_offset + context_buffer_size);
	//if (storage == nullptr)
	//{
	//	return vsm::unexpected(allio::error::not_enough_memory);
	//}
	//
	//auto self = std::unique_ptr<ssl_implementation>(new ssl_implementation(context_buffer_size));
	vsm_try(self, make_unique<ssl_implementation>());

	vsm_try_assign(self->bio, create_bio<ssl_implementation>(*self));
	vsm_try_assign(self->ssl, openssl_make<SSL_new, SSL_free>(ssl_ctx));
	SSL_set_bio(self->ssl.get(), self->bio.get(), self->bio.get());

	return self.release();
}

void openssl_ssl::delete_context()
{
	delete static_cast<ssl_implementation*>(this);
}

vsm::result<openssl_io_result<void>> openssl_ssl::connect()
{
	auto& self = static_cast<ssl_implementation&>(*this);

	vsm_try(r, ssl_try<SSL_connect>(self.ssl.get()));

	if (!r)
	{
		return openssl_io_result<void>(vsm::unexpected(r.error()));
	}

	if (*r == 0)
	{
		return vsm::unexpected(get_ssl_error(self.ssl.get(), *r));
	}

	return {};
}

vsm::result<openssl_io_result<void>> openssl_ssl::disconnect()
{
	auto& self = static_cast<ssl_implementation&>(*this);

	vsm_try(r, ssl_try<SSL_shutdown>(self.ssl.get()));

	if (!r)
	{
		return openssl_io_result<void>(vsm::unexpected(r.error()));
	}

	//TODO: What to do on 0 result from SSL_shutdown?

	return {};
}

vsm::result<openssl_io_result<void>> openssl_ssl::accept()
{
	auto& self = static_cast<ssl_implementation&>(*this);

	vsm_try(r, ssl_try<SSL_accept>(self.ssl.get()));

	if (!r)
	{
		return openssl_io_result<void>(vsm::unexpected(r.error()));
	}

	if (*r == 0)
	{
		return vsm::unexpected(get_ssl_error(self.ssl.get(), *r));
	}

	return {};
}

vsm::result<openssl_io_result<size_t>> openssl_ssl::read(void* const data, size_t const size)
{
	auto& self = static_cast<ssl_implementation&>(*this);

	size_t transferred;
	vsm_try(r, ssl_try<SSL_read_ex>(self.ssl.get(), data, size, &transferred));

	if (!r)
	{
		return openssl_io_result<size_t>(vsm::unexpected(r.error()));
	}

	if (*r == 0)
	{
		return vsm::unexpected(get_ssl_error(self.ssl.get(), *r));
	}

	return transferred;
}

vsm::result<openssl_io_result<size_t>> openssl_ssl::write(void const* const data, size_t const size)
{
	auto& self = static_cast<ssl_implementation&>(*this);

	size_t transferred;
	vsm_try(r, ssl_try<SSL_write_ex>(self.ssl.get(), data, size, &transferred));

	if (!r)
	{
		return openssl_io_result<size_t>(vsm::unexpected(r.error()));
	}

	if (*r == 0)
	{
		return vsm::unexpected(get_ssl_error(self.ssl.get(), *r));
	}

	return transferred;
}
