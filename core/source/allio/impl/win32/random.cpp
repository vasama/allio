#include <allio/impl/random.hpp>

#include <allio/impl/win32/kernel.hpp>

#include <vsm/numeric.hpp>

#include <bcrypt.h>

#pragma comment(lib, "Bcrypt.lib")

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

vsm::result<size_t> detail::secure_random_fill_some(std::span<std::byte> const buffer)
{
	size_t size = 0;
	while (size != buffer.size())
	{
		vsm_assert(size < buffer.size());
		ULONG const part_size = vsm::saturating(buffer.size() - size);

		NTSTATUS const status = BCryptGenRandom(
			NULL,
			reinterpret_cast<PUCHAR>(buffer.data() + size),
			part_size,
			BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	
		if (!NT_SUCCESS(status))
		{
			if (size != 0)
			{
				break;
			}

			return vsm::unexpected(static_cast<kernel_error>(status));
		}

		size += part_size;
	}
	return size;
}
