#include <allio/map_handle.hpp>

#include <vsm/defer.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

page_size allio::get_supported_page_sizes()
{
	static page_size const value = []() -> page_size
	{
		//TODO: Read 
		return page_size::_4KB;
	}();
	return value;
}


static int get_page_protection(page_access const access)
{
	int p = 0;
	if (vsm::any_flags(access, page_access::read))
	{
		p |= PROT_READ;
	}
	if (vsm::any_flags(access, page_access::write))
	{
		p |= PROT_WRITE;
	}
	if (vsm::any_flags(access, page_access::execute))
	{
		p |= PROT_EXEC;
	}
	return p;
}

static vsm::result<int> get_page_size_flags(page_size const size)
{
	switch (size)
	{
	case page_size::default_size:
	case page_size::smallest:
		return 0;

	case page_size::largest:
		//TODO: Select based on /sys/kernel/mm/hugepages?
		return MAP_HUGETLB;

#if vsm_arch_x86
	case page_size::x86_4KB:
		return 0;

	case page_size::x86_2MB:
		return MAP_HUGETLB;

	case page_size::x86_2MB:
		return MAP_HUGETLB | MAP_HUGE_2MB;

	case page_size::x86_2MB:
		return MAP_HUGETLB | MAP_HUGE_1GB;
#endif
	}

	return vsm::unexpected(unsupported_operation);
}

vsm::result<void> detail::map_handle_base::sync_impl(io::parameters_with_result<io::map> const& args)
{
	map_handle& h = *args.handle;

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_assert(args.section != nullptr);
	section_handle const& sh = *args.section;

	if (!sh)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	int flags = MAP_SHARED_VALIDATE | page_size_flags;
	int fd = -1;

	if (sh.get_flags()[section_handle::impl_flags::anonymous])
	{
		flags |= MAP_ANONYMOUS;
	}
	else
	{
		fd = unwrap_handle(sh.get_platform_handle());
	}

	vsm_try(page_size_flags, get_page_size_flags(args.page_size));
	flags |= page_size_flags;

	if (args.address != static_cast<uintptr_t>(0))
	{
		flags |= MAP_FIXED_NOREPLACE;
	}

	void* address = mmap(
		reinterpret_cast<void*>(args.address),
		size,
		get_page_protection(args.access),
		flags,
		fd,
		args.offset);

	if (address == MAP_FAILED)
	{
		return vsm::unexpected(get_last_error());
	}

	vsm_defer
	{
		if (address != nullptr)
		{
			munmap(address, args.size);
		}
	};

	if (args.address != nullptr && address != args.address)
	{
		return vsm::unexpected(error::address_not_available);
	}

	h.m_base.value = std::exchange(address, nullptr);
	h.m_size.value = args.size;

	return {};
}
