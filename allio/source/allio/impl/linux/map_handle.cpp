#include <allio/map_handle.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/linux/detail/unique_fd.hpp>
#include <allio/linux/platform.hpp>

#include <vsm/defer.hpp>

#include <sys/mman.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

static int get_page_protection(protection const protection)
{
	int p = 0;
	if (vsm::any_flags(protection, protection::read))
	{
		p |= PROT_READ;
	}
	if (vsm::any_flags(protection, protection::write))
	{
		p |= PROT_WRITE;
	}
	if (vsm::any_flags(protection, protection::execute))
	{
		p |= PROT_EXEC;
	}
	return p;
}

static int get_page_level_flags(std::optional<page_level> const level)
{
	if (!level)
	{
		return 0;
	}

	auto const supported_levels = get_supported_page_levels();

	if (*level == supported_levels.front())
	{
		return 0;
	}

	return MAP_HUGETLB | std::to_underlying(*level) << MAP_HUGE_SHIFT;
}


vsm::result<void> map_handle_base::set_protection(void* const base, size_t const size, protection const protection)
{
	if (!*this)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	if (!check_address_range(base, size))
	{
		return vsm::unexpected(error::invalid_address);
	}

	if (mprotect(base, size, get_page_protection(protection)) == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return {};
}


vsm::result<void> map_handle_base::sync_impl(io::parameters_with_result<io::map> const& args)
{
	map_handle& h = *args.handle;

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}


	int flags = MAP_SHARED_VALIDATE;
	int fd = -1;


	if (args.section != nullptr)
	{
		section_handle const& sh = *args.section;

		if (!sh)
		{
			return vsm::unexpected(error::invalid_argument);
		}

		fd = unwrap_handle(sh.get_platform_handle());
	}
	else
	{
		if (args.offset != 0)
		{
			return vsm::unexpected(error::invalid_argument);
		}

		flags |= MAP_ANONYMOUS;
	}

	void* const requested_address = reinterpret_cast<void*>(args.address);

	if (requested_address != nullptr)
	{
		flags |= MAP_FIXED_NOREPLACE;
	}

	flags |= get_page_level_flags(args.page_level);


	void* base = mmap(
		requested_address,
		args.size,
		get_page_protection(args.protection),
		flags,
		fd,
		args.offset);

	if (base == MAP_FAILED)
	{
		return vsm::unexpected(get_last_error());
	}

	vsm_defer
	{
		if (base != nullptr)
		{
			vsm_verify(munmap(base, args.size) != -1);
		}
	};

	if (requested_address != nullptr && base != requested_address)
	{
		return vsm::unexpected(error::virtual_address_not_available);
	}


	h.m_base.value = std::exchange(base, nullptr);
	h.m_size.value = args.size;

	return {};
}

vsm::result<void> map_handle_base::sync_impl(io::parameters_with_result<io::close> const& args)
{
	map_handle& h = static_cast<map_handle&>(*args.handle);

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	if (munmap(h.m_base.value, h.m_size.value) == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	h.m_base.value = nullptr;
	h.m_size.value = 0;

	return {};
}
