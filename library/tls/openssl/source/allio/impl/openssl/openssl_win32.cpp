#include <allio/impl/openssl/openssl.hpp>

#include <allio/impl/win32/error.hpp>

#include <vsm/unique_resource.hpp>

#include <wincrypt.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

#if 0
namespace {

struct cert_store_deleter
{
	void operator()(HCERTSTORE const h_cert_store) const
	{
		CertCloseStore(h_cert_store, /* dwFlags: */ 0);
	}
};

using unique_cert_store = vsm::unique_resource<
	HCERTSTORE,
	cert_store_deleter,
	static_cast<HCERTSTORE>(NULL)>;

static unique_cert_store open_root_cert_store()
{
	return unique_cert_store(CertOpenSystemStoreW(/* hProv: */ 0, L"ROOT"));
}

class cert_store_iterator
{
	struct cert_context_deleter
	{
		void operator()(PCCERT_CONTEXT const cert_context) const
		{
			CertFreeCertificateContext(cert_context);
		}
	};

	using unique_cert_context = std::unique_ptr<
		std::remove_pointer_t<PCCERT_CONTEXT>,
		cert_context_deleter>;

	HCERTSTORE m_h_cert_store;
	unique_cert_context m_cert_context;

public:
	explicit cert_store_iterator(HCERTSTORE h_cert_store)
		: m_h_cert_store(h_cert_store)
	{
	}

	[[nodiscard]] PCCERT_CONTEXT next() &
	{
		PCCERT_CONTEXT const next_cert_context = CertEnumCertificatesInStore(
			m_h_cert_store,
			m_cert_context.release());

		m_cert_context.reset(next_cert_context);
		return next_cert_context;
	}
};

using x509_ptr = openssl_ptr<X509, X509_free>;

static vsm::result<x509_ptr> cert_to_x509(PCCERT_CONTEXT const cert_context)
{
	auto const data = reinterpret_cast<unsigned char const*>(cert_context->pbCertEncoded);
	X509* const x509 = d2i_X509(/* reuse: */ nullptr, data, cert_context->cbCertEncoded);

	if (x509 == nullptr)
	{
		return vsm::unexpected(get_last_openssl_error());
	}

	return vsm::result<x509_ptr>(vsm::result_value, x509);
}

} // namespace

vsm::result<void> detail::openssl_load_system_certificates(SSL_CTX* const ssl_ctx)
{
	if (unique_cert_store const root_cert_store = open_root_cert_store())
	{
		X509_STORE* const ssl_ctx_store = SSL_CTX_get_cert_store(ssl_ctx);
		vsm_assert(ssl_ctx_store != nullptr);

		cert_store_iterator iterator(root_cert_store.get());
		while (PCCERT_CONTEXT const cert_context = iterator.next())
		{
			vsm_try(x509, cert_to_x509(cert_context));

			if (!X509_STORE_add_cert(ssl_ctx_store, x509.get()))
			{
				return vsm::unexpected(get_last_openssl_error());
			}
		}
	}

	return {};
}
#endif

// TODO: Finish the above implementation.
vsm::result<void> detail::openssl_load_system_certificates(SSL_CTX* const ssl_ctx)
{
	return {};
}
