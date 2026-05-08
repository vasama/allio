#include <allio/impl/openssl/openssl.hpp>

#include <allio/error.hpp>
#include <allio/impl/new.hpp>
#include <allio/nothrow/file.hpp>

#include <vsm/assert.h>
#include <vsm/defer.hpp>
#include <vsm/numeric.hpp>
#include <vsm/standard.hpp>
#include <vsm/standard/string.hpp>

#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>

#include <memory>
#include <optional>

//TODO: Use error encoding

using namespace allio;
using namespace allio::detail;

namespace {

static std::string get_error_string(unsigned long const e)
{
	// 120 from OpenSSL documentation.
	// - 1 for the null terminator.
	static constexpr size_t buffer_size = 120 - 1;

	std::string buffer;
	vsm::resize_and_overwrite(
		buffer,
		buffer_size,
		[&](char* const p, size_t const n) -> size_t
		{
			ERR_error_string_n(
				e,
				p,
				// The function overwrites the null terminator.
				n + 1);

			return strnlen(p, n);
		});
	return buffer;
}


enum class openssl_error : unsigned {};

struct openssl_error_category : std::error_category
{
	const char* name() const noexcept override
	{
		return "OpenSSL-CRYPTO";
	}

	std::string message(int const e) const override
	{
		static bool const init = []() -> bool
		{
			return OPENSSL_init_crypto(
				OPENSSL_INIT_LOAD_CRYPTO_STRINGS,
				/* settings: */ nullptr) == 1;
		}();

		if (init)
		{
			return get_error_string(static_cast<unsigned long>(e));
		}
		else
		{
			return std::format("OpenSSL:{:08x}", static_cast<unsigned int>(e));
		}
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
		static bool const init = []() -> bool
		{
			return OPENSSL_init_ssl(
				OPENSSL_INIT_LOAD_SSL_STRINGS |
				OPENSSL_INIT_LOAD_CRYPTO_STRINGS,
				/* settings: */ nullptr) == 1;
		}();

		if (init)
		{
			return get_error_string(static_cast<unsigned long>(e));
		}
		else
		{
			return std::format("OpenSSL:{:08x}", static_cast<unsigned int>(e));
		}
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
	vsm_static_operator void operator()(T* const pointer) vsm_static_operator_const
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
		return vsm::unexpected(get_last_openssl_error());
	}

	return vsm::result<openssl_make_ptr_t<Make, Free, Args...>>(vsm::result_value, ptr);
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
	object_deleter()(static_cast<typename T::data_type*>(BIO_get_data(bio)));
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
static vsm::result<bio_ptr> create_bio(unique_ptr<typename T::data_type> data)
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
		nothrow::file_handle file;
		fs_size file_offset;
		std::error_code file_error;

		explicit data_type(nothrow::file_handle&& file)
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
	vsm_try(file, nothrow::open_file(path, file_mode::read));
	vsm_try(data, make_unique<file_bio::data_type>(vsm_move(file)));
	return create_bio<file_bio>(vsm_move(data));
}

static vsm::result<bio_ptr> make_file_bio(fs_path const& path)
{
	vsm_try(bio, make_raw_file_bio(path));
	//TODO: Make sure the buffer BIO clears its buffers on free.
	return push_buffer_bio(vsm_move(bio));
}


enum class secret_format
{
	der,
	pem,
};

static vsm::result<std::optional<secret_format>> get_mime_type_format(
	std::string_view const mime_type)
{
	if (mime_type.empty())
	{
		return std::nullopt;
	}

	if (mime_type == "application/x-pem-file")
	{
		return secret_format::pem;
	}

	return vsm::unexpected(error::unsupported_input_format);
}

static vsm::result<secret_format> deduce_secret_format(BIO* const bio)
{
	return secret_format::pem;
}

template<typename Callable>
static auto read_secret(tls_secret const& secret, Callable&& callable)
	-> std::invoke_result_t<Callable&&, BIO*>
{
	switch (secret.kind())
	{
	case tls_secret_kind::none:
		return nullptr;

	case tls_secret_kind::data:
		{
			vsm_try(bio, make_memory_bio(secret.data()));
			return vsm_forward(callable)(bio.get());
		}

	case tls_secret_kind::path:
		{
			vsm_try(bio, make_file_bio(secret.path()));
			return vsm_forward(callable)(bio.get());
		}
	}

	vsm_unreachable();
};

template<typename Callable>
static auto read_secret(tls_secret const& secret, Callable&& callable)
	-> std::invoke_result_t<Callable&&, BIO*, secret_format>
{
	if (secret.kind() == tls_secret_kind::none)
	{
		return nullptr;
	}

	vsm_try(mime_type_format, get_mime_type_format(secret.mime_type()));

	return read_secret(
		secret,
		[&](BIO* const bio) -> std::invoke_result_t<Callable&&, BIO*, secret_format>
		{
			vsm_try(format, mime_type_format
				? vsm::result<secret_format>(*mime_type_format)
				: deduce_secret_format(bio));

			return vsm_forward(callable)(bio, format);
		});
}


using x509_ptr = openssl_ptr<X509, X509_free>;

static vsm::result<x509_ptr> read_x509(tls_secret const& secret)
{
	return read_secret(
		secret,
		[&](BIO* const bio, secret_format const format) -> vsm::result<x509_ptr>
		{
			switch (format)
			{
			case secret_format::der:
				break;

			case secret_format::pem:
				return openssl_make<PEM_read_bio_X509, X509_free>(
					bio,
					/* out: */ nullptr,
					/* password callback: */ nullptr,
					/* password callback userdata:*/ nullptr);
			}

			return vsm::unexpected(error::unsupported_input_format);
		});
}


using pkey_ptr = openssl_ptr<EVP_PKEY, EVP_PKEY_free>;

static vsm::result<pkey_ptr> read_pkey(tls_secret const& secret)
{
	return read_secret(
		secret,
		[&](BIO* const bio, secret_format const format) -> vsm::result<pkey_ptr>
		{
			switch (format)
			{
			case secret_format::der:
				break;

			case secret_format::pem:
				return openssl_make<PEM_read_bio_PrivateKey, EVP_PKEY_free>(
					bio,
					/* out: */ nullptr,
					/* password callback: */ nullptr,
					/* password callback userdata:*/ nullptr);
			}

			return vsm::unexpected(error::unsupported_input_format);
		});
}


using ssl_ctx_ptr = openssl_ptr<SSL_CTX, SSL_CTX_free>;
using ssl_ptr = openssl_ptr<SSL, SSL_free>;

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
	tls_version min_version = args.min_version;
	if (min_version == static_cast<tls_version>(0))
	{
		min_version = tls_version::tls_1_2;
	}

	switch (min_version)
	{
	case tls_version::ssl_1:
	case tls_version::ssl_2:
		return vsm::unexpected(error::unsupported_operation);
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

	vsm_msvc_warning(push)
	vsm_msvc_warning(disable: 4063)

	vsm_gnu_diagnostic(push)
	vsm_gnu_diagnostic(ignored "-Wswitch")

	case static_cast<tls_version>(0):
		vsm_unreachable();

	vsm_msvc_warning(pop)
	vsm_gnu_diagnostic(pop)
	}
	return vsm::unexpected(error::invalid_argument);
}

static vsm::result<openssl_ssl_ctx_ptr> create_ssl_ctx(
	security_context_parameters const& args,
	bool const client)
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

		tls_verification verification = args.verification;
		if (verification == static_cast<tls_verification>(0))
		{
			verification = client
				? tls_verification::required
				: tls_verification::optional;
		}

		switch (verification)
		{
		default:
			vsm_unreachable();

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
			/* callback: */ nullptr); //TODO: Verify callback

		if (verify != SSL_VERIFY_NONE)
		{
			if (client)
			{
				if (vsm::any_flags(args.options, tls_options::use_system_certificates))
				{
					if (!SSL_CTX_set_default_verify_paths(ssl_ctx.get()))
					{
						return vsm::unexpected(get_last_openssl_error());
					}

					vsm_try_void(openssl_load_system_certificates(ssl_ctx.get()));
				}

#if 0 //TODO: Just for testing
				if (!SSL_CTX_load_verify_file(
					ssl_ctx.get(),
					"D:\\Code\\allio\\build\\msvc-asan\\library\\openssl\\allio-test-secrets\\server-certificate.pem"))
				{
					return vsm::unexpected(get_last_openssl_error());
				}
#endif
			}

			if (args.peer_certificate)
			{
				X509_STORE* const ssl_ctx_store = SSL_CTX_get_cert_store(ssl_ctx.get());
				vsm_assert(ssl_ctx_store != nullptr);

				vsm_try(x509, read_x509(args.peer_certificate));

				if (!X509_STORE_add_cert(ssl_ctx_store, x509.get()))
				{
					return vsm::unexpected(get_last_openssl_error());
				}
			}
		}
	}

	if (args.certificate)
	{
		vsm_try(x509, read_x509(args.certificate));
		SSL_CTX_use_certificate(ssl_ctx.get(), x509.get());
	}

	if (args.private_key)
	{
		vsm_try(pkey, read_pkey(args.private_key));
		SSL_CTX_use_PrivateKey(ssl_ctx.get(), pkey.get());
	}

	return vsm::result<openssl_ssl_ctx_ptr>(
		vsm::result_value,
		reinterpret_cast<openssl_ssl_ctx*>(ssl_ctx.release()));
}

vsm::result<openssl_ssl_ctx_ptr> detail::openssl_create_client_ssl_ctx(
	security_context_parameters const& args)
{
	return create_ssl_ctx(args, /* client: */ true);
}

vsm::result<openssl_ssl_ctx_ptr> detail::openssl_create_server_ssl_ctx(
	security_context_parameters const& args)
{
	return create_ssl_ctx(args, /* client: */ false);
}

void detail::openssl_release_ssl(openssl_ssl* const ssl)
{
	SSL_free(reinterpret_cast<SSL*>(ssl));
}


namespace {

static allio_allocation allocate_buffer(size_t const min_size)
{
	return allio_acquire_storage(
		min_size,
		static_cast<size_t>(-1),
		/* min_alignment: */ 1,
		allio_allocation_strategy_buffering);
}

static void deallocate_buffer(std::byte* const storage, size_t const size)
{
	allio_release_storage(
		storage,
		size,
		/* min_alignment: */ 1,
		allio_allocation_strategy_buffering);
}

struct buffer_deleter
{
	size_t m_size;

public:
	buffer_deleter() = default;

	explicit buffer_deleter(size_t const size)
		: m_size(size)
	{
	}

	[[nodiscard]] size_t size() const
	{
		return m_size;
	}

	void operator()(std::byte* const storage) const
	{
		deallocate_buffer(storage, m_size);
	}
};

using unique_buffer_ptr = std::unique_ptr<std::byte, buffer_deleter>;

static vsm::result<unique_buffer_ptr> allocate_unique_buffer(size_t const min_size)
{
	auto const allocation = allocate_buffer(min_size);

	if (allocation.storage == nullptr)
	{
		return vsm::unexpected(error::not_enough_memory);
	}

	return vsm::result<unique_buffer_ptr>(
		vsm::result_value,
		static_cast<std::byte*>(allocation.storage),
		buffer_deleter(allocation.size));
}

} // namespace

tunnel_io::~tunnel_io()
{
	if (m_read_buffer_allocated)
	{
		deallocate_buffer(m_read_buffer_data, m_read_buffer_size);
	}

	if (m_write_buffer_allocated)
	{
		deallocate_buffer(m_write_buffer_data, m_write_buffer_size);
	}
}

vsm::result<void> tunnel_io::allocate_buffers(
	size_t const min_read_buffer_size,
	size_t const min_write_buffer_size)
{
	unique_buffer_ptr read_buffer;
	unique_buffer_ptr write_buffer;

	if (min_read_buffer_size != 0)
	{
		vsm_try_assign(read_buffer, allocate_unique_buffer(min_read_buffer_size));
	}

	if (min_write_buffer_size != 0)
	{
		vsm_try_assign(write_buffer, allocate_unique_buffer(min_write_buffer_size));
	}

	if (read_buffer)
	{
		m_read_buffer_size = read_buffer.get_deleter().size();
		m_read_buffer_data = read_buffer.release();
		m_read_buffer_allocated = true;
	}

	if (write_buffer)
	{
		m_write_buffer_size = write_buffer.get_deleter().size();
		m_write_buffer_data = write_buffer.release();
		m_write_buffer_allocated = true;
	}

	return {};
}

tunnel_io_result<read_result> tunnel_io::read_some(read_buffer const buffer)
{
	if (m_read_error)
	{
		m_read_last = true;
		return vsm::unexpected(tunnel_io_status::io_failure);
	}

	if (m_read_eof)
	{
		return detail::end_of_stream;
	}

	size_t const beg_offset = m_read_beg_offset;
	size_t const total_size = m_read_end_offset - beg_offset;

	size_t const transfer_size = std::min(
		total_size,
		buffer.size());

	if (transfer_size != 0)
	{
		std::memcpy(
			buffer.data(),
			m_read_buffer_data + beg_offset,
			transfer_size);

		m_read_beg_offset = beg_offset + transfer_size;
	}

	if (transfer_size != buffer.size())
	{
		m_read_beg_offset = 0;
		m_read_end_offset = 0;
		m_read_needed = true;
	}

	if (transfer_size == 0)
	{
		return vsm::unexpected(tunnel_io_status::io_needed);
	}

	return transfer_size;
}

tunnel_io_result<size_t> tunnel_io::write_some(write_buffer const buffer)
{
	if (m_write_error)
	{
		m_read_last = false;
		return vsm::unexpected(tunnel_io_status::io_failure);
	}

	size_t const end_offset = m_write_end_offset;
	size_t const space_size = m_write_buffer_size - end_offset;

	size_t const transfer_size = std::min(
		space_size,
		buffer.size());

	if (transfer_size != 0)
	{
		std::memcpy(
			m_write_buffer_data + end_offset,
			buffer.data(),
			transfer_size);

		m_write_end_offset = end_offset + transfer_size;
		m_write_needed = true;
	}

	if (transfer_size == 0)
	{
		return vsm::unexpected(tunnel_io_status::io_needed);
	}

	return transfer_size;
}

void tunnel_io::read_completed(vsm::result<read_result> const& result)
{
	if (result)
	{
		if (result->is_end_of_stream())
		{
			m_read_eof = true;
		}
		else
		{
			m_read_end_offset += result->size();
		}

		m_read_needed = false;
	}
	else
	{
		m_read_error = result.error();
	}
}

void tunnel_io::write_completed(vsm::result<size_t> const& result)
{
	if (result)
	{
		m_write_beg_offset += *result;

		if (m_write_beg_offset == m_write_end_offset)
		{
			m_write_beg_offset = 0;
			m_write_end_offset = 0;
			m_write_needed = false;
		}
	}
	else
	{
		m_write_error = result.error();
	}
}


namespace {

struct tunnel_bio
{
	static constexpr char name[] = "ALLIO_TUNNEL_BIO";

	using data_type = tunnel_io;

	static int read_ex(
		tunnel_io& io,
		BIO* const bio,
		char* const buffer,
		size_t const requested_size,
		size_t* const transferred)
	{
		auto const r = io.read_some(as_read_buffer(buffer, requested_size));

		if (!r && r.error() == tunnel_io_status::io_needed)
		{
			BIO_set_retry_read(bio);
		}
		else
		{
			BIO_clear_retry_flags(bio);
		}

		if (!r || r->is_end_of_stream())
		{
			return 0;
		}

		*transferred = r->size();
		return 1;
	}

	static int write_ex(
		tunnel_io& io,
		BIO* const bio,
		char const* const buffer,
		size_t const available_size,
		size_t* const transferred)
	{
		auto const r = io.write_some(as_write_buffer(buffer, available_size));

		if (!r && r.error() == tunnel_io_status::io_needed)
		{
			BIO_set_retry_write(bio);
		}
		else
		{
			BIO_clear_retry_flags(bio);
		}

		if (r)
		{
			*transferred = *r;
		}

		return r.has_value();
	}

	static long ctrl(
		tunnel_io& io,
		int const cmd,
		long const larg,
		void* const parg)
	{
		switch (cmd)
		{
		case BIO_CTRL_EOF:
			return io.end_of_stream();

		case BIO_CTRL_PUSH:
		case BIO_CTRL_POP:
			return 0;

		case BIO_CTRL_FLUSH:
			// TODO: Handle OpenSSL BIO flush?
			return 1;
		}
		return -1;
	}
};

} // namespace

vsm::result<void> openssl_tunnel::initialize(
	tunnel_io& io,
	tunnel_mode const mode,
	openssl_ssl_ctx* const _ssl_ctx)
{
	auto const ssl_ctx = reinterpret_cast<SSL_CTX*>(_ssl_ctx);

	vsm_try(bio, create_bio<tunnel_bio>(io));
	vsm_try(ssl, openssl_make<SSL_new, SSL_free>(ssl_ctx));

	auto const p_bio = bio.release();
	SSL_set_bio(ssl.get(), p_bio, p_bio);

	m_ssl.reset(reinterpret_cast<openssl_ssl*>(ssl.release()));

	return {};
}

vsm::result<tunnel_result<void>> openssl_tunnel::connect(tunnel_io& io, tunnel_mode const mode)
{
	auto const ssl = reinterpret_cast<SSL*>(m_ssl.get());

	int const r = mode == tunnel_mode::server
		? SSL_accept(ssl)
		: SSL_connect(ssl);

	if (r < 0)
	{
		switch (int const e = SSL_get_error(ssl, r))
		{
		case SSL_ERROR_SYSCALL:
			return vsm::unexpected(io.consume_last_error());

		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			return vsm::success(vsm::unexpected(tunnel_io_needed()));

		default:
			return vsm::unexpected(static_cast<ssl_error>(e));
		}
	}

	if (r != 1)
	{
		return vsm::unexpected(get_ssl_error(ssl, r));
	}

	return {};
}

vsm::result<tunnel_result<bool>> openssl_tunnel::disconnect(
	tunnel_io& io,
	bool const full_disconnect)
{
	auto const ssl = reinterpret_cast<SSL*>(m_ssl.get());

	int r = SSL_shutdown(ssl);

	if (r < 0)
	{
		switch (int const e = SSL_get_error(ssl, r))
		{
		case SSL_ERROR_SYSCALL:
			return vsm::unexpected(io.consume_last_error());

		case SSL_ERROR_ZERO_RETURN:
			// End of stream is not considered an error during disconnect.
			break;

		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			return vsm::success(vsm::unexpected(tunnel_io_needed()));

		default:
			return vsm::unexpected(static_cast<ssl_error>(e));
		}
	}

	return r == 1;
}

vsm::result<tunnel_result<read_result>> openssl_tunnel::read_some(
	tunnel_io& io,
	read_buffer const buffer)
{
	auto const ssl = reinterpret_cast<SSL*>(m_ssl.get());

	int const r = SSL_read(
		ssl,
		buffer.data(),
		vsm::saturating(buffer.size()));

	if (r < 1)
	{
		switch (int const e = SSL_get_error(ssl, r))
		{
		case SSL_ERROR_SYSCALL:
			return vsm::unexpected(io.consume_last_error());

		case SSL_ERROR_ZERO_RETURN:
			return end_of_stream;

		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			return vsm::success(vsm::unexpected(tunnel_io_needed()));

		default:
			return vsm::unexpected(static_cast<ssl_error>(e));
		}
	}

	return static_cast<size_t>(r);
}

vsm::result<tunnel_result<size_t>> openssl_tunnel::write_some(
	tunnel_io& io,
	write_buffer const buffer)
{
	auto const ssl = reinterpret_cast<SSL*>(m_ssl.get());

	int const r = SSL_write(
		ssl,
		buffer.data(),
		vsm::saturating(buffer.size()));

	if (r < 1)
	{
		switch (int const e = SSL_get_error(ssl, r))
		{
		case SSL_ERROR_SYSCALL:
			return vsm::unexpected(io.consume_last_error());

		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			return vsm::success(vsm::unexpected(tunnel_io_needed()));

		default:
			return vsm::unexpected(static_cast<ssl_error>(e));
		}
	}

	return static_cast<size_t>(r);
}
