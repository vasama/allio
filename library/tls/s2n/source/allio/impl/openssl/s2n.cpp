#include <allio/impl/s2n/s2n.hpp>

#include <allio/error.hpp>
#include <allio/impl/new.hpp>
#include <allio/nothrow/file.hpp>

#include <vsm/assert.h>
#include <vsm/defer.hpp>
#include <vsm/numeric.hpp>
#include <vsm/standard.hpp>
#include <vsm/standard/string.hpp>

#include <memory>
#include <optional>

//TODO: Use error encoding

using namespace allio;
using namespace allio::detail;

namespace {

enum class a_s2n_error {};

struct a_s2n_error_category : std::error_category
{
	const char* name() const noexcept override
	{
		return "s2n";
	}

	std::string message(int const e) const override
	{
		return s2n_strerror(e, /* lang: */ nullptr);
	}
};
static a_s2n_error_category const a_s2n_error_category_instance;

static std::error_code make_error_code(a_s2n_error const e)
{
	return std::error_code(static_cast<int>(e), a_s2n_error_category_instance);
}

static a_s2n_error get_last_s2n_error()
{
	return static_cast<a_s2n_error>(s2n_errno);
}

} // namespace

template<>
struct std::is_error_code_enum<a_s2n_error>
{
	static constexpr bool value = true;
};


namespace {

template<typename T, auto Function>
struct a_s2n_deleter
{
	vsm_static_operator void operator()(T* const pointer) vsm_static_operator_const
	{
		Function(pointer);
	}
};

template<typename T, auto Function>
using a_s2n_ptr = std::unique_ptr<T, a_s2n_deleter<T, Function>>;

template<auto Make, typename... Args>
using a_s2n_make_object_t = std::remove_pointer_t<std::invoke_result_t<decltype(Make), Args...>>;

template<auto Make, auto Free, typename... Args>
using a_s2n_make_ptr_t = a_s2n_ptr<a_s2n_make_object_t<Make, Args...>, Free>;

template<auto Make, auto Free, typename... Args>
static vsm::result<a_s2n_make_ptr_t<Make, Free, Args...>> a_s2n_make(Args const... args)
{
	auto* const ptr = Make(args...);

	if (ptr == nullptr)
	{
		return vsm::unexpected(get_last_s2n_error());
	}

	return vsm::result<a_s2n_make_ptr_t<Make, Free, Args...>>(vsm::result_value, ptr);
}


using bio_method_ptr = a_s2n_ptr<BIO_METHOD, BIO_meth_free>;
using bio_ptr = a_s2n_ptr<BIO, BIO_free_all>;

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
		return vsm::unexpected(get_last_s2n_error());
	}

	vsm_try(method, a_s2n_make<BIO_meth_new, BIO_meth_free>(
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
	vsm_try(bio, a_s2n_make<BIO_new, BIO_free_all>(method.get()));
	BIO_set_data(bio.get(), &data);

	return bio;
}

template<typename T>
static vsm::result<bio_ptr> create_bio(unique_ptr<typename T::data_type> data)
{
	static auto const method_r = create_bio_method<T, true>();

	vsm_try((auto const&, method), method_r);
	vsm_try(bio, a_s2n_make<BIO_new, BIO_free_all>(method.get()));
	BIO_set_data(bio.get(), data.release());

	return bio;
}


static vsm::result<bio_ptr> make_memory_bio(std::span<std::byte const> const buffer)
{
	if (buffer.size() > std::numeric_limits<int>::max())
	{
		return vsm::unexpected(error::invalid_argument);
	}

	return a_s2n_make<BIO_new_mem_buf, BIO_free_all>(
		buffer.data(),
		static_cast<int>(buffer.size()));
}

static vsm::result<bio_ptr> push_buffer_bio(bio_ptr bio)
{
	vsm_try(buffer_bio, a_s2n_make<BIO_new, BIO_free_all>(BIO_f_buffer()));
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


using x509_ptr = a_s2n_ptr<X509, X509_free>;

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
				return a_s2n_make<PEM_read_bio_X509, X509_free>(
					bio,
					/* out: */ nullptr,
					/* password callback: */ nullptr,
					/* password callback userdata:*/ nullptr);
			}

			return vsm::unexpected(error::unsupported_input_format);
		});
}


using pkey_ptr = a_s2n_ptr<EVP_PKEY, EVP_PKEY_free>;

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
				return a_s2n_make<PEM_read_bio_PrivateKey, EVP_PKEY_free>(
					bio,
					/* out: */ nullptr,
					/* password callback: */ nullptr,
					/* password callback userdata:*/ nullptr);
			}

			return vsm::unexpected(error::unsupported_input_format);
		});
}


using config_ptr = a_s2n_ptr<s2n_config, s2n_config_free>;
using connection_ptr = a_s2n_ptr<s2n_connection, s2n_connection_free>;

} // namespace

void detail::a_s2n_acquire_config(a_s2n_config* const config)
{
	SSL_CTX_up_ref(reinterpret_cast<SSL_CTX*>(config));
}

void detail::a_s2n_release_config(a_s2n_config* const config)
{
	SSL_CTX_free(reinterpret_cast<SSL_CTX*>(config));
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

	vsm_gcc_diagnostic(pop)
	vsm_gcc_diagnostic(ignored "-Wswitch")
	vsm_msvc_warning(push)
	vsm_msvc_warning(disable: 4063)
	case static_cast<tls_version>(0):
		vsm_unreachable();

	vsm_gcc_diagnostic(pop)
	vsm_msvc_warning(pop)
	}
	return vsm::unexpected(error::invalid_argument);
}

static vsm::result<a_s2n_config_ptr> create_config(
	security_context_parameters const& args,
	bool const client)
{
	auto const ssl_method = client ? TLS_client_method() : TLS_server_method();
	vsm_try(config, a_s2n_make<SSL_CTX_new, SSL_CTX_free>(ssl_method));

	vsm_try(min_version, get_tls_version(args));
	if (!SSL_CTX_set_min_proto_version(config.get(), min_version))
	{
		return vsm::unexpected(get_last_s2n_error());
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
			[[fallthrough]];

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
			config.get(),
			verify,
			/* callback: */ nullptr); //TODO: Verify callback

		if (verify != SSL_VERIFY_NONE)
		{
			if (client)
			{
				if (vsm::any_flags(args.options, tls_options::use_system_certificates))
				{
					if (!SSL_CTX_set_default_verify_paths(config.get()))
					{
						return vsm::unexpected(get_last_s2n_error());
					}

					vsm_try_void(a_s2n_load_system_certificates(config.get()));
				}
			}

			if (args.peer_certificate)
			{
				X509_STORE* const ssl_ctx_store = SSL_CTX_get_cert_store(config.get());
				vsm_assert(ssl_ctx_store != nullptr);

				vsm_try(x509, read_x509(args.peer_certificate));

				if (!X509_STORE_add_cert(ssl_ctx_store, x509.get()))
				{
					return vsm::unexpected(get_last_s2n_error());
				}
			}
		}
	}

	if (args.certificate)
	{
		vsm_try(x509, read_x509(args.certificate));
		SSL_CTX_use_certificate(config.get(), x509.get());
	}

	if (args.private_key)
	{
		vsm_try(pkey, read_pkey(args.private_key));
		SSL_CTX_use_PrivateKey(config.get(), pkey.get());
	}

	return vsm::result<a_s2n_config_ptr>(
		vsm::result_value,
		reinterpret_cast<a_s2n_config*>(config.release()));
}

vsm::result<a_s2n_config_ptr> detail::a_s2n_create_client_config(
	security_context_parameters const& args)
{
	return create_config(args, /* client: */ true);
}

vsm::result<a_s2n_config_ptr> detail::a_s2n_create_server_config(
	security_context_parameters const& args)
{
	return create_config(args, /* client: */ false);
}

void detail::a_s2n_release_connection(a_s2n_connection* const connection)
{
	s2n_connection_free(reinterpret_cast<s2n_connection*>(connection));
}


// TODO: Debugging
inline bool enable_debug;

static int a_s2n_recv_callback(
	void* const context,
	uint8_t* const buffer,
	uint32_t const requested_size)
{
}

static int a_s2n_send_callback(
	void* const context,
	uint8_t const* const buffer,
	uint32_t const requested_size)
{
}

a_s2n_stream::~a_s2n_stream()
{
	if (m_r_unique_buffer)
	{
		allio_release_storage(
			m_r_buffer_data,
			m_r_buffer_size,
			/* min_alignment: */ 1,
			allio_allocation_strategy_buffering);
	}

	if (m_w_unique_buffer)
	{
		allio_release_storage(
			m_w_buffer_data,
			m_w_buffer_size,
			/* min_alignment: */ 1,
			allio_allocation_strategy_buffering);
	}
}

vsm::result<void> a_s2n_stream::initialize(a_s2n_config* const _config)
{
	// TODO: Pick client/server appropriately.
	vsm_try(connection, a_s2n_make<s2n_connection_new, s2n_connection_free>(S2N_CLIENT));

	vsm_verify(s2n_connection_set_recv_cb(connection.get(), a_s2n_recv_callback) == S2N_SUCCESS);
	vsm_verify(s2n_connection_set_recv_ctx(connection.get(), this) == S2N_SUCCESS);

	vsm_verify(s2n_connection_set_send_cb(connection.get(), a_s2n_send_callback) == S2N_SUCCESS);
	vsm_verify(s2n_connection_set_send_ctx(connection.get(), this) == S2N_SUCCESS);

	m_ssl.reset(reinterpret_cast<a_s2n_connection*>(connection.release()));

	return {};
}

vsm::result<void> a_s2n_stream::allocate_buffers()
{
	static constexpr size_t min_rw_buffer_size = 4096;

	auto const r_allocation = allio_acquire_storage(
		min_rw_buffer_size,
		static_cast<size_t>(-1),
		/* min_alignment: */ 1,
		allio_allocation_strategy_buffering);

	if (r_allocation.storage == nullptr)
	{
		return vsm::unexpected(error::not_enough_memory);
	}

	unique_storage_ptr<void> r_allocation_owner(
		r_allocation.storage,
		storage_deleter(r_allocation.size));

	auto const w_allocation = allio_acquire_storage(
		min_rw_buffer_size,
		static_cast<size_t>(-1),
		/* min_alignment: */ 1,
		allio_allocation_strategy_buffering);

	if (r_allocation.storage == nullptr)
	{
		return vsm::unexpected(error::not_enough_memory);
	}

	auto const r_storage = static_cast<std::byte*>(r_allocation.storage);
	auto const w_storage = static_cast<std::byte*>(w_allocation.storage);

	set_r_buffer(std::span(r_storage, r_allocation.size));
	set_w_buffer(std::span(w_storage, w_allocation.size));

	m_r_unique_buffer = true;
	m_w_unique_buffer = true;

	(void)r_allocation_owner.release();

	return {};
}

vsm::result<a_s2n_stream*> detail::a_s2n_new_stream(a_s2n_config* const config)
{
	vsm_try(socket, detail::make_unique<a_s2n_stream>());
	vsm_try_void(socket->initialize(config));
	return socket.release();
}

void detail::a_s2n_delete_stream(a_s2n_stream* const stream)
{
	detail::delete_object(stream);
}

void a_s2n_stream::read_completed(vsm::result<size_t> const result)
{
	if (result)
	{
		m_r_end_offset += *result;
		m_r_io_desired = false;
	}
	else
	{
		set_last_error(result.error());
	}
}

void a_s2n_stream::write_completed(vsm::result<size_t> const result)
{
	if (result)
	{
		m_w_beg_offset += *result;

		if (m_w_beg_offset == m_w_end_offset)
		{
			m_w_beg_offset = 0;
			m_w_end_offset = 0;
			m_w_io_desired = false;
		}
	}
	else
	{
		set_last_error(result.error());
	}
}

// TODO: Share the accept and connect implementation.
vsm::result<s2n_result<void>> a_s2n_stream::accept()
{
	auto const connection = reinterpret_cast<s2n_connection*>(m_connection.get());

	s2n_blocked_status blocked;
	if (s2n_negotiate(connection, &blocked) != S2N_SUCCESS)
	{
		int const e = s2n_errno;

		if (s2n_error_get_type(e) == S2N_ERR_T_BLOCKED)
		{
			return vsm::success(vsm::unexpected(a_s2n_pending_t()));
		}

		return vsm::unexpected(static_cast<a_s2n_error>(e));
	}

	return {};
}

vsm::result<s2n_result<void>> a_s2n_stream::connect()
{
	auto const connection = reinterpret_cast<s2n_connection*>(m_connection.get());

	s2n_blocked_status blocked;
	if (s2n_negotiate(connection, &blocked) != S2N_SUCCESS)
	{
		int const e = s2n_errno;

		if (s2n_error_get_type(e) == S2N_ERR_T_BLOCKED)
		{
			return vsm::success(vsm::unexpected(a_s2n_pending_t()));
		}

		return vsm::unexpected(static_cast<a_s2n_error>(e));
	}

	return {};
}

vsm::result<s2n_result<void>> a_s2n_stream::disconnect()
{
	auto const connection = reinterpret_cast<s2n_connection*>(m_connection.get());

	s2n_blocked_status blocked;
	if (s2n_shutdown(connection, &blocked) != S2N_SUCCESS)
	{
		int const e = s2n_errno;

		if (s2n_error_get_type(e) == S2N_ERR_T_BLOCKED)
		{
			return vsm::success(vsm::unexpected(a_s2n_pending_t()));
		}

		// TODO: Is it necessary to handle EOF separately?
		return vsm::unexpected(static_cast<a_s2n_error>(e));
	}

	return {};
}

vsm::result<s2n_result<size_t>> a_s2n_stream::read_some(read_buffer const buffer)
{
	auto const connection = reinterpret_cast<s2n_connection*>(m_connection.get());

	s2n_blocked_status blocked;
	ssize_t const transferred = s2n_recv(
		connection,
		buffer.data(),
		vsm::saturating(buffer.size()),
		&blocked);

	if (transferred == S2N_FAILURE)
	{
		int const e = s2n_errno;

		if (s2n_error_get_type(e) == S2N_ERR_T_BLOCKED)
		{
			return vsm::success(vsm::unexpected(a_s2n_pending_t()));
		}

		return vsm::unexpected(static_cast<a_s2n_error>(e));
	}

	if (transferred == 0)
	{
		return vsm::unexpected(error::end_of_stream);
	}

	vsm_assert(transferred > 0);
	return static_cast<size_t>(transferred);
}

// TODO: Implement support for s2n_sendv_with_offset
vsm::result<s2n_result<size_t>> a_s2n_stream::write_some(write_buffer const buffer)
{
	auto const connection = reinterpret_cast<s2n_connection*>(m_connection.get());

	s2n_blocked_status blocked;
	ssize_t const transferred = s2n_send(
		connection,
		buffer.data(),
		vsm::saturating(buffer.size()),
		&blocked);

	if (transferred == S2N_FAILURE)
	{
		int const e = s2n_errno;

		if (s2n_error_get_type(e) == S2N_ERR_T_BLOCKED)
		{
			return vsm::success(vsm::unexpected(a_s2n_pending_t()));
		}

		return vsm::unexpected(static_cast<a_s2n_error>(e));
	}

	vsm_assert(transferred > 0);
	return static_cast<size_t>(transferred);
}

void a_s2n_stream::set_last_error(std::error_code const error)
{
	if (!m_r_end_of_stream)
	{
		if (error.default_error_condition() == make_error_condition(error::end_of_stream))
		{
			m_r_end_of_stream = true;
		}
	}

	m_last_error = error;
}
