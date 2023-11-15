#include <allio/detail/secure_socket.hpp>

#include <vsm/lift.hpp>

#include <openssl/bio.h>
#include <openssl/ssl.h>

#include <memory>

using namespace allio;
using namespace allio::detail;

namespace {

using bio_method_ptr = std::unique_ptr<BIO_METHOD, vsm_lift_t(BIO_meth_free)>;
using bio_ptr = std::unique_ptr<BIO, vsm_lift_t(BIO_free)>;


struct openssl_stream_socket : secure_socket_object
{
	using socket_handle_type = socket_stream_handle;

	int bio_write(BIO* const bio, char const* const buffer, size_t const size, size_t* const transferred)
	{
	}

	int bio_read(BIO* const bio, char* const buffer, size_t const size, size_t* const transferred)
	{
	}

	long bio_ctrl(BIO* const bio, int const cmd, long const larg, void* const parg)
	{
	}
};

struct openssl_listen_socket : secure_socket_object
{
	
};

struct openssl_interface : secure_socket_interface
{
	openssl_interface()
		: secure_socket_interface("OpenSSL")
	{
	}


	secure_socket_object* create_stream_socket() override
	{
		
	}

	secure_socket_object* create_listen_socket() override
	{
		
	}
};

} // namespace
