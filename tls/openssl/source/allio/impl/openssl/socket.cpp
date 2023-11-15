#include <allio/detail/openssl/socket.hpp>

#include <vsm/standard.hpp>

#include <openssl/bio.h>
#include <openssl/ssl.h>

using namespace allio;
using namespace allio::detail;

namespace {

enum class openssl_error : int {};

static openssl_error get_last_openssl_error();

template<typename T, void F(T*)>
struct openssl_deleter
{
	void vsm_static_operator_invoke(T* const pointer)
	{
		F(pointer);
	}
};

} // namespace

template<typename T, auto Function>
using openssl_ptr = std::unique_ptr<T, openssl_deleter<T, Function>>;

using bio_method_ptr = openssl_ptr<BIO_METHOD, BIO_meth_free>;
using bio_ptr = openssl_ptr<BIO, BIO_free>;


template<typename T>
struct _bio_function;

template<typename Bio, typename R, typename... P>
struct _bio_function<R(Bio::*)(P...)>
{
	template<R(Bio::* F)(P...)>
	static R function(BIO* const bio, P... const args)
	{
		return (static_cast<Bio*>(BIO_get_data(bio))->*F)(args...);
	}
};

template<auto F>
inline constexpr auto bio_function = &_bio_function<decltype(F)>::function<F>;

template<typename Bio>
static vsm::result<bio_method_ptr> create_bio_method()
{
	static int const index = BIO_get_new_index();

	auto method = bio_method_ptr(BIO_meth_new(index | BIO_TYPE_SOURCE_SINK, Bio::name));
	if (method == nullptr)
	{
		return vsm::unexpected(get_last_openssl_error());
	}

	vsm_verify(BIO_meth_set_destroy(method.get(), [](BIO* const bio) -> int
	{
		delete static_cast<Bio*>(BIO_get_data(bio));
		return 0;
	}));
	vsm_verify(BIO_meth_set_read_ex(method.get(), bio_function<&Bio::read_ex>));
	vsm_verify(BIO_meth_set_write_ex(method.get(), bio_function<&Bio::write_ex>));
	vsm_verify(BIO_meth_set_ctrl_ex(method.get(), bio_function<&Bio::ctrl_ex>));

	return method;
}

template<typename Bio>
static vsm::result<bio_ptr> create_bio(auto&&... args)
{
	static auto const method = create_bio_method();

	if (!method)
	{
		return vsm::unexpected(method.error());
	}

	vsm_try(data, make_unique<Bio>(vsm_forward(args)...));

	auto bio = bio_ptr(BIO_new(method.get()));
	if (bio == nullptr)
	{
		return vsm::unexpected(get_last_openssl_error());
	}
	BIO_set_data(bio.get(), data.release());

	return bio;
}


struct socket_bio
{
	int read_ex(char* const buffer, size_t const size, size_t* const transferred)
	{
		
	}

	int write_ex(char const* const buffer, size_t const size, size_t* const transferred)
	{
		
	}

	long ctrl_ex(int const cmd, long const larg, void* const parg)
	{
		
	}
};


namespace {

enum class ssl_error : int {};

} // namespace

using ssl_ptr = openssl_ptr<SSL, SSL_free>;

static ssl_error get_ssl_error(SSL* const ssl, int const r)
{
	return static_cast<ssl_error>(SSL_get_error(ssl, r));
}

struct detail::openssl_context
{
	ssl_ptr ssl;
};

static vsm::result<openssl_result<int>> do_ssl(SSL* const ssl, auto... const args)
{
	int const r = F(ssl, args...);

	if (r < 0)
	{
		switch (int const e = SSL_get_error(ssl, r))
		{
		case SSL_ERROR_WANT_READ:
			return openssl_data_request::read;
	
		case SSL_ERROR_WANT_WRITE:
			return openssl_data_request::write;
	
		default:
			return vsm::unexpected(static_cast<ssl_error>(e));
		}
	}

	return r;
}

vsm::result<openssl_result<void>> detail::openssl_connect(openssl_context& context)
{
	vsm_try(r, do_ssl<SSL_connect>(context.ssl.get()));

	if (!r)
	{
		return openssl_result<void>(vsm::unexpected(r.error()));
	}

	if (*r == 0)
	{
		return vsm::unexpected(get_ssl_error(context.ssl.get(), *r));
	}

	return {};
}

vsm::result<openssl_result<void>> detail::openssl_disconnect(openssl_connect& context)
{
	vsm_try(r, do_ssl<SSL_shutdown>(context.ssl.get()));

	if (!r)
	{
		return openssl_result<void>(vsm::unexpected(r.error()));
	}

	//TODO: What to do on 0 result from SSL_shutdown?

	return {};
}

vsm::result<openssl_result<void>> detail::openssl_accept(openssl_context& context)
{
	vsm_try(r, do_ssl<SSL_accept>(context.ssl.get()));

	if (!r)
	{
		return openssl_result<void>(vsm::unexpected(r.error()));
	}

	if (*r == 0)
	{
		return return vsm::unexpected(get_ssl_error(context.ssl.get(), *r));
	}

	return {};
}

vsm::result<openssl_result<size_t>> detail::openssl_read(openssl_context& context, void* const data, size_t const size)
{
	size_t transferred;
	vsm_try(r, do_ssl<SSL_read_ex>(context.ssl.get(), data, size, &transferred));

	if (!r)
	{
		return openssl_result<size_t>(vsm::unexpected(r.error()));
	}

	if (*r == 0)
	{
		return return vsm::unexpected(get_ssl_error(context.ssl.get(), *r));
	}

	return transferred;
}

vsm::result<openssl_result<size_t>> detail::openssl_write(openssl_context& context, void const* const data, size_t const size)
{
	size_t transferred;
	vsm_try(r, do_ssl<SSL_write_ex>(context.ssl.get(), data, size, &transferred));

	if (!r)
	{
		return openssl_result<size_t>(vsm::unexpected(r.error()));
	}

	if (*r == 0)
	{
		return return vsm::unexpected(get_ssl_error(context.ssl.get(), *r));
	}

	return transferred;
}
