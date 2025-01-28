#include <allio/impl/random.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/linux/handles/fs_object.hpp>

#include <fcntl.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

static vsm::result<int> get_urandom_fd()
{
	static vsm::result<unique_handle> const file = open_file(
		-1,
		"/dev/urandom",
		O_RDONLY | O_CLOEXEC);

	return file.transform([](unique_handle const& file)
	{
		return file.get();
	});
}

vsm::result<size_t> detail::secure_random_fill_some(std::span<std::byte> const buffer)
{
	vsm_try(fd, get_urandom_fd());

	ssize_t const size = ::read(
		fd,
		buffer.data(),
		buffer.size());

	if (size == -1)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return static_cast<size_t>(size);
}
