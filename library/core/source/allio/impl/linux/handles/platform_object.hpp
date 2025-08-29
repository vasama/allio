#pragma once

#include <allio/impl/handles/platform_object.hpp>
#include <allio/linux/handles/platform_object.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

bool verify_anon_inode_type(int fd, std::string_view type);
bool verify_file_stat_mode(int fd, mode_t mode);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
