#pragma once

#include <allio/impl/handles/fs_object_open.hpp>

#include <allio/impl/win32/kernel.hpp>

namespace allio::detail {

struct platform_open_options
{
	ACCESS_MASK desired_access;
	ULONG attributes;
	ULONG share_access;
	ULONG create_disposition;
	ULONG create_options;
	ULONG object_attributes;

	static vsm::result<platform_open_options> make(generic_open_options const& args);
};

} // namespace allio::detail
