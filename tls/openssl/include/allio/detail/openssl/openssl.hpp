#pragma once

#include <vsm/result.hpp>

namespace allio::detail {

struct openssl_context;

vsm::result<openssl_context*> openssl_create_context();
void openssl_delete_context(openssl_context* context);

enum class openssl_data_request : bool
{
	read,
	write,
};

void openssl_read_completed(openssl_context* context);
void openssl_write_completed(openssl_context* context);

template<typename T>
using openssl_io_result = vsm::result<T, openssl_data_request>;

vsm::result<openssl_io_result<void>> openssl_connect(openssl_context* context);
vsm::result<openssl_io_result<void>> openssl_disconnect(openssl_context* context);
vsm::result<openssl_io_result<void>> openssl_accept(openssl_context* context);

vsm::result<openssl_io_result<size_t>> openssl_read(openssl_context* context, void* data, size_t size);
vsm::result<openssl_io_result<size_t>> openssl_write(openssl_context* context, void const* data, size_t size);

} // namespace allio::detail
