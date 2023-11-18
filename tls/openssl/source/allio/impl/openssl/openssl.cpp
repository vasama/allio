#include <allio/openssl/detail/openssl.hpp>
#include <allio/error.hpp>

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

enum class openssl_error : unsigned {};

static std::error_code make_error_code(openssl_error const e)
{
	return std::error_code(static_cast<int>(e), openssl_error_category_instance);
}

template<typename T>
using openssl_result = vsm::result<T, openssl_error>;

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

static vsm::result<ssl_ctx_ptr> create_ssl_ctx(openssl_mode const mode)
{
	SSL_library_init();

	SSL_METHOD const* const method =
		mode == openssl_mode::client
		? TLS_client_method()
		: TLS_server_method();
	vsm_assert(method != nullptr);

	vsm_try(ctx, openssl_make<SSL_CTX_new, SSL_CTX_free>(method));

	if (!SSL_CTX_set_min_proto_version(ctx.get(), TLS1_2_VERSION))
	{
		return vsm::unexpected(get_last_openssl_error());
	}

	return ctx;
}

static vsm::result<ssl_ptr> create_ssl_client()
{
	static vsm::result<ssl_ctx_ptr> const ctx_r = create_ssl_ctx(openssl_mode::client);

	vsm_try((auto const&, ctx), ctx_r);
	vsm_try(ssl, openssl_make<SSL_new, SSL_free>(ctx.get()));

	return ssl;
}

static vsm::result<ssl_ptr> create_ssl_server()
{
	static vsm::result<ssl_ctx_ptr> const ctx_r = create_ssl_ctx(openssl_mode::server);

	vsm_try((auto const&, ctx), ctx_r);
	vsm_try(ssl, openssl_make<SSL_new, SSL_free>(ctx.get()));

	return ssl;
}

static vsm::result<ssl_ptr> create_ssl(openssl_mode const mode)
{
	return mode == openssl_mode::client
		? create_ssl_client()
		: create_ssl_server();
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
			return openssl_io_result<int>(vsm::unexpected(openssl_data_request::read));

		case SSL_ERROR_WANT_WRITE:
			return openssl_io_result<int>(vsm::unexpected(openssl_data_request::write));

		default:
			return vsm::unexpected(static_cast<ssl_error>(e));
		}
	}

	return r;
}



struct context_impl : openssl_context
{
	static constexpr char bio_name[] = "ALLIO Socket R/W BIO";

	bio_ptr bio;
	ssl_ptr ssl;

	uint32_t r_buffer_beg;
	uint32_t r_buffer_end;

	uint32_t w_buffer_beg;
	uint32_t w_buffer_end;

	uint32_t buffer_size;
	std::byte buffer[];

	explicit context_impl(size_t const buffer_size)
		: r_buffer_beg(0)
		, r_buffer_end(0)
		, w_buffer_beg(0)
		, w_buffer_end(0)
		, buffer_size(buffer_size)
	{
	}

	int read_ex(char* const buffer, size_t size, size_t* const transferred)
	{
		if (r_buffer_end - r_buffer_beg == buffer_size)
		{
			return -1;
		}

		size = std::min<size_t>(size, r_buffer_end - r_buffer_beg);
		memcpy(buffer, this->buffer + r_buffer_beg, size);

		r_buffer_beg += size;
		*transferred = size;

		return 0;
	}

	int write_ex(char const* const buffer, size_t size, size_t* const transferred)
	{
		if (w_buffer_end - w_buffer_beg == buffer_size)
		{
			return -1;
		}

		size = std::min<size_t>(size, w_buffer_end - w_buffer_beg);
		memcpy(this->buffer + w_buffer_beg, buffer, size);

		w_buffer_end += size;
		*transferred = size;

		return 0;
	}

	long ctrl(int const cmd, long const larg, void* const parg)
	{
		return -1;
	}
};

static constexpr size_t context_buffer_size = 256;
static constexpr size_t context_buffer_offset = offsetof(context_impl, buffer);

} // namespace

vsm::result<openssl_context*> openssl_context::create(openssl_mode const mode)
{
	void* const storage = operator new(context_buffer_offset + context_buffer_size);
	if (storage == nullptr)
	{
		return vsm::unexpected(allio::error::not_enough_memory);
	}

	auto self = std::unique_ptr<context_impl>(new context_impl(context_buffer_size));

	vsm_try(bio, create_bio<context_impl>(*self));
	vsm_try_assign(self->ssl, create_ssl(mode));
	SSL_set_bio(self->ssl.get(), bio.get(), bio.get());

	return self.release();
}

void openssl_context::delete_context()
{
	delete static_cast<context_impl*>(this);
}

vsm::result<openssl_io_result<void>> openssl_context::connect()
{
	auto& self = static_cast<context_impl&>(*this);

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

vsm::result<openssl_io_result<void>> openssl_context::disconnect()
{
	auto& self = static_cast<context_impl&>(*this);

	vsm_try(r, ssl_try<SSL_shutdown>(self.ssl.get()));

	if (!r)
	{
		return openssl_io_result<void>(vsm::unexpected(r.error()));
	}

	//TODO: What to do on 0 result from SSL_shutdown?

	return {};
}

vsm::result<openssl_io_result<void>> openssl_context::accept()
{
	auto& self = static_cast<context_impl&>(*this);

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

vsm::result<openssl_io_result<size_t>> openssl_context::read(void* const data, size_t const size)
{
	auto& self = static_cast<context_impl&>(*this);

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

vsm::result<openssl_io_result<size_t>> openssl_context::write(void const* const data, size_t const size)
{
	auto& self = static_cast<context_impl&>(*this);

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

std::span<std::byte> openssl_context::get_read_buffer()
{
	auto& self = static_cast<context_impl&>(*this);
	return std::span<std::byte>(self.buffer, self.buffer_size);
}

void openssl_context::read_completed(size_t const transferred)
{
	auto& self = static_cast<context_impl&>(*this);
	vsm_assert(transferred <= self.buffer_size);
	self.r_buffer_beg = 0;
	self.r_buffer_end = transferred;
}

std::span<std::byte const> openssl_context::get_write_buffer()
{
	auto& self = static_cast<context_impl&>(*this);
	return std::span<std::byte const>(self.buffer + self.buffer_size, self.buffer_size);
}

void openssl_context::write_completed(size_t const transferred)
{
	auto& self = static_cast<context_impl&>(*this);
	vsm_assert(transferred <= self.buffer_size);
	self.w_buffer_beg = 0;
	self.w_buffer_end = transferred;
}
