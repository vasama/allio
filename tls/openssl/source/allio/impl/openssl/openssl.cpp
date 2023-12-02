#include <allio/openssl/detail/openssl.hpp>
#include <allio/error.hpp>
#include <allio/file.hpp>
#include <allio/impl/new.hpp>
#include <allio/impl/secrets.hpp>

#include <vsm/assert.h>
#include <vsm/defer.hpp>
#include <vsm/numeric.hpp>
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

template<auto Make, typename... Args>
using openssl_make_object_t = std::remove_pointer_t<std::invoke_result_t<decltype(Make), Args...>>;

template<auto Make, auto Free, typename... Args>
using openssl_make_ptr_t = openssl_ptr<openssl_make_object_t<Make, Args...>, Free>;

template<auto Make, auto Free, typename... Args>
static vsm::result<openssl_make_ptr_t<Make, Free, Args...>> openssl_make(Args const... args)
{
	auto* const ptr = Make(args...);

	if (ptr == nullptr)
	{
		ERR_print_errors_fp(stderr);
		return vsm::unexpected(get_last_openssl_error());
	}

	return vsm_lazy(openssl_make_ptr_t<Make, Free, Args...>(ptr));
}


using bio_method_ptr = openssl_ptr<BIO_METHOD, BIO_meth_free>;
using bio_ptr = openssl_ptr<BIO, BIO_free_all>;

template<typename Signature>
struct _bio_function
{
	static_assert(sizeof(Signature));
};

template<typename Data, typename R, typename... P>
struct _bio_function<R(&)(Data&, P...)>
{
	template<R F(Data&, P...)>
	static R function(BIO* const bio, P const... args)
	{
		return F(*static_cast<Data*>(BIO_get_data(bio)), args...);
	}
};

template<typename Data, typename R, typename... P>
struct _bio_function<R(&)(Data&, BIO*, P...)>
{
	template<R F(Data&, BIO*, P...)>
	static R function(BIO* const bio, P const... args)
	{
		return F(*static_cast<Data*>(BIO_get_data(bio)), bio, args...);
	}
};

template<auto& F>
inline constexpr auto& bio_function = _bio_function<decltype(F)>::template function<F>;

template<typename T>
static int bio_method_destroy(BIO* const bio)
{
	delete static_cast<typename T::data_type*>(BIO_get_data(bio));
	return 1;
}

template<typename T, bool Owner>
static vsm::result<bio_method_ptr> create_bio_method()
{
	static int const index = BIO_get_new_index();

	if (index == -1)
	{
		return vsm::unexpected(get_last_openssl_error());
	}

	vsm_try(method, openssl_make<BIO_meth_new, BIO_meth_free>(
		index | BIO_TYPE_SOURCE_SINK,
		T::name));

	if constexpr (requires { T::read_ex; })
	{
		vsm_verify(BIO_meth_set_read_ex(method.get(), bio_function<T::read_ex>));
	}
	if constexpr (requires { T::write_ex; })
	{
		vsm_verify(BIO_meth_set_write_ex(method.get(), bio_function<T::write_ex>));
	}
	if constexpr (requires { T::ctrl; })
	{
		vsm_verify(BIO_meth_set_ctrl(method.get(), bio_function<T::ctrl>));
	}
	if constexpr (requires { T::gets; })
	{
		vsm_verify(BIO_meth_set_gets(method.get(), bio_function<T::gets>));
	}
	if constexpr (Owner)
	{
		vsm_verify(BIO_meth_set_destroy(method.get(), bio_method_destroy<T>));
	}

	return method;
}

template<typename T>
static vsm::result<bio_ptr> create_bio(typename T::data_type& data)
{
	static auto const method_r = create_bio_method<T, false>();

	vsm_try((auto const&, method), method_r);
	vsm_try(bio, openssl_make<BIO_new, BIO_free_all>(method.get()));
	BIO_set_data(bio.get(), &data);

	return bio;
}

template<typename T>
static vsm::result<bio_ptr> create_bio(std::unique_ptr<typename T::data_type> data)
{
	static auto const method_r = create_bio_method<T, true>();

	vsm_try((auto const&, method), method_r);
	vsm_try(bio, openssl_make<BIO_new, BIO_free_all>(method.get()));
	BIO_set_data(bio.get(), data.release());

	return bio;
}


static vsm::result<bio_ptr> make_memory_bio(std::span<std::byte const> const buffer)
{
	if (buffer.size() > std::numeric_limits<int>::max())
	{
		return vsm::unexpected(error::invalid_argument);
	}

	return openssl_make<BIO_new_mem_buf, BIO_free_all>(
		buffer.data(),
		static_cast<int>(buffer.size()));
}

static vsm::result<bio_ptr> push_buffer_bio(bio_ptr bio)
{
	vsm_try(buffer_bio, openssl_make<BIO_new, BIO_free_all>(BIO_f_buffer()));
	vsm_verify(BIO_push(buffer_bio.get(), bio.release()) == buffer_bio.get());
	return buffer_bio;
}


struct file_bio
{
	static constexpr char name[] = "ALLIO_FILE_BIO";

	struct data_type
	{
		blocking::file_handle file;
		fs_size file_offset;
		std::error_code file_error;

		explicit data_type(blocking::file_handle&& file)
			: file(vsm_move(file))
			, file_offset(0)
			, file_error{}
		{
		}
	};

	static int read_ex(data_type& data, char* const buffer, size_t const size, size_t* const transferred)
	{
		auto const r = data.file.read_some(data.file_offset, as_read_buffer(buffer, size));

		if (!r)
		{
			data.file_error = r.error();
			return 0;
		}

		*transferred = *r;
		return 1;
	}

	static int write_ex(data_type& data, char const* const buffer, size_t const size, size_t* const transferred)
	{
		auto const r = data.file.write_some(data.file_offset, as_write_buffer(buffer, size));

		if (!r)
		{
			data.file_error = r.error();
			return 0;
		}

		*transferred = *r;
		return 1;
	}

	static long ctrl(data_type& data, int const cmd, long const larg, void* const parg)
	{
		switch (cmd)
		{
		case BIO_CTRL_PUSH:
		case BIO_CTRL_POP:
			return 0;

		case BIO_CTRL_EOF:
			return 0; //TODO

		case BIO_CTRL_FLUSH:
			return 1;

		case BIO_C_FILE_TELL:
			if (data.file_offset > std::numeric_limits<long>::max())
			{
				return -1;
			}
			return static_cast<long>(data.file_offset);

		case BIO_C_FILE_SEEK:
			if (larg < 0)
			{
				return -1;
			}
			data.file_offset = static_cast<size_t>(larg);
			return 1;
		}
		return -1;
	}
};

static vsm::result<bio_ptr> make_raw_file_bio(fs_path const& path)
{
	vsm_try(file, blocking::open_file(path, file_mode::read));
	vsm_try(data, make_unique<file_bio::data_type>(vsm_move(file)));
	return create_bio<file_bio>(vsm_move(data));
}

static vsm::result<bio_ptr> make_file_bio(fs_path const& path)
{
	vsm_try(bio, make_raw_file_bio(path));
	//TODO: Make sure the buffer BIO clears its buffers on free.
	return push_buffer_bio(vsm_move(bio));
}


template<typename Callable>
auto read_secret(tls_secret const& secret, Callable&& callable)
	-> std::invoke_result_t<Callable&&, BIO*>
{
	switch (secret.kind())
	{
	case tls_secret_kind::none:
		break;

	case tls_secret_kind::data:
		{
			vsm_try(bio, make_memory_bio(secret.data()));
			return vsm_forward(callable)(bio.get());
		}
		break;

	case tls_secret_kind::path:
		{
			vsm_try(bio, make_file_bio(secret.path()));
			return vsm_forward(callable)(bio.get());
		}
		break;
	}

	return nullptr;
}

using x509_ptr = openssl_ptr<X509, X509_free>;

static vsm::result<x509_ptr> read_x509(tls_secret const& secret)
{
	return read_secret(secret, [&](BIO* const bio)
	{
		//TODO: User specified format
		return openssl_make<PEM_read_bio_X509, X509_free>(bio, nullptr, nullptr, nullptr);
	});
}

using pkey_ptr = openssl_ptr<EVP_PKEY, EVP_PKEY_free>;

static vsm::result<pkey_ptr> read_pkey(tls_secret const& secret)
{
	return read_secret(secret, [&](BIO* const bio)
	{
		//TODO: User specified format
		return openssl_make<PEM_read_bio_PrivateKey, EVP_PKEY_free>(bio, nullptr, nullptr, nullptr);
	});
}


using ssl_ctx_ptr = openssl_ptr<SSL_CTX, SSL_CTX_free>;
using ssl_ptr = openssl_ptr<SSL, SSL_free>;

template<auto Function, int SuccessThreshold>
static vsm::result<openssl_result<int>> ssl_try(SSL* const ssl, auto const... args)
{
	int const r = Function(ssl, args...);

	if (r >= SuccessThreshold)
	{
		return r;
	}

	int const e = SSL_get_error(ssl, r);

	switch (e)
	{
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_WRITE:
		return vsm::success(vsm::unexpected(std::monostate()));
	}

	ERR_print_errors_fp(stderr); //TODO
	return vsm::unexpected(static_cast<ssl_error>(e));
}

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

static vsm::result<openssl_ssl_ctx_ptr> create_ssl_ctx(security_context_parameters const& args, bool const client)
{
	auto const ssl_method = client ? TLS_client_method() : TLS_server_method();
	vsm_try(ssl_ctx, openssl_make<SSL_CTX_new, SSL_CTX_free>(ssl_method));

	vsm_try(min_version, get_tls_version(args));
	if (!SSL_CTX_set_min_proto_version(ssl_ctx.get(), min_version))
	{
		return vsm::unexpected(get_last_openssl_error());
	}

	// Configure peer verification
	{
		int verify = SSL_VERIFY_NONE;

		auto const default_verification = client
			? tls_verification::required
			: tls_verification::optional;

		switch (args.tls_verification.value_or(default_verification))
		{
		case tls_verification::none:
			break;

		case tls_verification::required:
			verify = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
			break;

		case tls_verification::optional:
			if (client)
			{
				// OpenSSL does not support optional server verification.
				return vsm::unexpected(error::unsupported_operation);
			}
			verify = SSL_VERIFY_PEER;
			break;
		}

		SSL_CTX_set_verify(
			ssl_ctx.get(),
			verify,
			nullptr); //TODO: Verify callback

		if (verify != SSL_VERIFY_NONE)
		{
			if (client)
			{
#if 1 //TODO: Just for testing
				if (!SSL_CTX_load_verify_file(
					ssl_ctx.get(),
					"D:\\Code\\allio\\tls\\openssl\\keys\\server-certificate.pem"))
				{
					return vsm::unexpected(get_last_openssl_error());
				}
#else
				if (!SSL_CTX_set_default_verify_paths(ssl_ctx.get()))
				{
					return vsm::unexpected(get_last_openssl_error());
				}
#endif
			}
		}
	}

	if (args.tls_certificate)
	{
		vsm_try(x509, read_x509(args.tls_certificate));
		SSL_CTX_use_certificate(ssl_ctx.get(), x509.get());
	}

	if (args.tls_private_key)
	{
		vsm_try(pkey, read_pkey(args.tls_private_key));
		SSL_CTX_use_PrivateKey(ssl_ctx.get(), pkey.get());
	}

#if 0
	switch (args.tls_certificate.kind())
	{
	case tls_secret_kind::none:
		break;

	case tls_secret_kind::data:
		{
			//TODO: User specified format
			auto const data = args.tls_certificate.data();
			if (!SSL_CTX_use_certificate_ASN1(
				ssl_ctx.get(),
				//TODO: Checked saturate
				data.size(),
				reinterpret_cast<unsigned char const*>(data.data())))
			{
				return vsm::unexpected(get_last_openssl_error());
			}
		}
		break;

	case tls_secret_kind::path:
		{
			//TODO: User specified format
			if (!SSL_CTX_use_certificate_file(
				ssl_ctx.get(),
				args.tls_certificate.path(),
				SSL_FILETYPE_PEM))
			{
				return vsm::unexpected(get_last_openssl_error());
			}
		}
		break;
	}

	switch (args.tls_private_key.kind())
	{
	case tls_secret_kind::none:
		break;

	case tls_secret_kind::data:
		{
			//TODO: User specified format
			auto const data = args.tls_private_key.data();
			if (!SSL_CTX_use_PrivateKey_ASN1(
				EVP_PKEY_NONE,
				ssl_ctx.get(),
				reinterpret_cast<unsigned char const*>(data.data()),
				//TODO: Checked saturate
				data.size()))
			{
				return vsm::unexpected(get_last_openssl_error());
			}
		}
		break;

	case tls_secret_kind::path:
		{
			//TODO: User specified format
			if (!SSL_CTX_use_PrivateKey_file(
				ssl_ctx.get(),
				args.tls_private_key.path(),
				SSL_FILETYPE_PEM))
			{
				return vsm::unexpected(get_last_openssl_error());
			}
		}
		break;
	}
#endif

	return vsm_lazy(openssl_ssl_ctx_ptr(reinterpret_cast<openssl_ssl_ctx*>(ssl_ctx.release())));
}

vsm::result<openssl_ssl_ctx_ptr> detail::openssl_create_client_ssl_ctx(security_context_parameters const& args)
{
	return create_ssl_ctx(args, /* client: */ true);
}

vsm::result<openssl_ssl_ctx_ptr> detail::openssl_create_server_ssl_ctx(security_context_parameters const& args)
{
	return create_ssl_ctx(args, /* client: */ false);
}

void detail::openssl_release_ssl(openssl_ssl* const ssl)
{
	SSL_free(reinterpret_cast<SSL*>(ssl));
}


struct openssl_state_base::bio_type
{
	static constexpr char name[] = "ALLIO_RW_BIO";

	using data_type = openssl_state_base;

	static int read_ex(
		data_type& data,
		BIO* const bio,
		char* const buffer,
		size_t const size,
		size_t* const transferred)
	{
		std::byte* const r_pos = data.m_r_pos;
		size_t const remaining = data.m_r_end - r_pos;

		if (remaining == 0)
		{
			data.m_want_read = true;
			BIO_set_retry_read(bio);
			return 0;
		}

		size_t const transfer_size = std::min(size, remaining);

		memcpy(buffer, r_pos, transfer_size);
		data.m_r_pos = r_pos + transfer_size;
		data.m_want_read = false;

		*transferred = transfer_size;
		return 1;
	}

	static int write_ex(
		data_type& data,
		BIO* const bio,
		char const* const buffer,
		size_t const size,
		size_t* const transferred)
	{
		std::byte* const w_pos = data.m_w_pos;
		size_t const remaining = data.m_w_end - w_pos;

		if (remaining == 0)
		{
			data.m_want_write = true;
			BIO_set_retry_write(bio);
			return 0;
		}

		size_t const transfer_size = std::min(size, remaining);

		memcpy(w_pos, buffer, transfer_size);
		data.m_w_pos = w_pos + transfer_size;
		data.m_want_write = true;

		*transferred = transfer_size;
		return 1;
	}

	static long ctrl(
		data_type& data,
		int const cmd,
		long const larg,
		void* const parg)
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

vsm::result<openssl_ssl_ptr> openssl_state_base::create_ssl(openssl_ssl_ctx* const _ssl_ctx)
{
	auto const ssl_ctx = reinterpret_cast<SSL_CTX*>(_ssl_ctx);

	vsm_try(bio, create_bio<openssl_state_base::bio_type>(*this));
	vsm_try(ssl, openssl_make<SSL_new, SSL_free>(ssl_ctx));

	auto const p_bio = bio.release();
	SSL_set_bio(ssl.get(), p_bio, p_bio);

	return vsm_lazy(openssl_ssl_ptr(reinterpret_cast<openssl_ssl*>(ssl.release())));
}

vsm::result<openssl_result<void>> openssl_state_base::accept()
{
	auto const ssl = reinterpret_cast<SSL*>(m_ssl.get());

	vsm_try(r, ssl_try<SSL_accept, 0>(ssl));

	if (!r)
	{
		return vsm::success(vsm::unexpected(std::monostate()));
	}

	if (*r == 0)
	{
		return vsm::unexpected(get_ssl_error(ssl, *r));
	}

	return {};
}

vsm::result<openssl_result<void>> openssl_state_base::connect()
{
	auto const ssl = reinterpret_cast<SSL*>(m_ssl.get());

	vsm_try(r, ssl_try<SSL_connect, 0>(ssl));

	if (!r)
	{
		return vsm::success(vsm::unexpected(std::monostate()));
	}

	if (*r == 0)
	{
		return vsm::unexpected(get_ssl_error(ssl, *r));
	}

	return {};
}

vsm::result<openssl_result<void>> openssl_state_base::disconnect()
{
	auto const ssl = reinterpret_cast<SSL*>(m_ssl.get());

	vsm_try(r, ssl_try<SSL_shutdown, 0>(ssl));

	if (!r)
	{
		return vsm::success(vsm::unexpected(std::monostate()));
	}

	//TODO: What to do on 0 result from SSL_shutdown?

	return {};
}

vsm::result<openssl_result<size_t>> openssl_state_base::read(read_buffer const buffer)
{
	auto const ssl = reinterpret_cast<SSL*>(m_ssl.get());

	vsm_try(r, ssl_try<SSL_read, 1>(
		ssl,
		buffer.data(),
		vsm::saturating(buffer.size())));

	if (!r)
	{
		return vsm::success(vsm::unexpected(std::monostate()));
	}

	if (*r == 0)
	{
		return vsm::unexpected(get_ssl_error(ssl, *r));
	}

	return static_cast<size_t>(*r);
}

vsm::result<openssl_result<size_t>> openssl_state_base::write(write_buffer const buffer)
{
	auto const ssl = reinterpret_cast<SSL*>(m_ssl.get());

	vsm_try(r, ssl_try<SSL_write, 1>(
		ssl,
		buffer.data(),
		vsm::saturating(buffer.size())));

	if (!r)
	{
		return vsm::success(vsm::unexpected(std::monostate()));
	}

	if (*r == 0)
	{
		return vsm::unexpected(get_ssl_error(ssl, *r));
	}

	return static_cast<size_t>(*r);
}
