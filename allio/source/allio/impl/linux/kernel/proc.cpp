#include <allio/linux/impl/kernel/proc.hpp>

#include <vsm/lazy.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

vsm::result<proc_scanner> proc_scanner::open(char const* const path)
{
	int const fd = open(path, O_RDONLY | O_CLOEXEC, 0);

	if (fd == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	
	return vsm_lazy(proc_scanner(fd));
}

vsm::result<void> proc_scanner::_scan_field(std::string_view const field)
{

}

vsm::result<std::string_view> proc_scanner::_scan_value()
{

}
