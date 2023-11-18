#pragma once

#include <vsm/standard.hpp>
#include <vsm/result.hpp>

#include <memory>
#include <span>

namespace allio::detail {

enum class openssl_mode : bool
{
	client,
	server,
};

enum class openssl_data_request : bool
{
	read,
	write,
};

template<typename T>
using openssl_io_result = vsm::result<T, openssl_data_request>;

class openssl_context
{
public:
	static vsm::result<openssl_context*> create(openssl_mode mode);
	void delete_context();

	vsm::result<openssl_io_result<void>> connect();
	vsm::result<openssl_io_result<void>> disconnect();
	vsm::result<openssl_io_result<void>> accept();

	vsm::result<openssl_io_result<size_t>> read(void* data, size_t size);
	vsm::result<openssl_io_result<size_t>> write(void const* data, size_t size);

	std::span<std::byte> get_read_buffer();
	void read_completed(size_t transferred);

	std::span<std::byte const> get_write_buffer();
	void write_completed(size_t transferred);

protected:
	openssl_context() = default;
	openssl_context(openssl_context const&) = default;
	openssl_context& operator=(openssl_context const&) = default;
	~openssl_context() = default;
};

} // namespace allio::detail
