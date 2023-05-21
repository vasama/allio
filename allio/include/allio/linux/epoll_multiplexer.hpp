#pragma once

#include <allio/deferring_multiplexer.hpp>
#include <allio/linux/detail/unique_fd.hpp>
#include <allio/multiplexer.hpp>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

class epoll_multiplexer final : public deferring_multiplexer
{
	detail::unique_fd const m_epoll;

public:
	
	type_id<multiplexer> get_type_id() const override;

	vsm::result<multiplexer_handle_relation const*> find_handle_relation(type_id<handle> handle_type) const override;


	vsm::result<void> register_native_handle(native_platform_handle handle);
	vsm::result<void> deregister_native_handle(native_platform_handle handle);


};

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
