#pragma once

#include <allio/handles/platform_object.hpp>

#include <memory>

namespace allio::test {

class system_config_interface
{
public:
	system_config_interface(system_config_interface const&) = default;
	system_config_interface& operator=(system_config_interface const&) = default;

	virtual ~system_config_interface() = default;

protected:
	system_config_interface() = default;
};

[[nodiscard]] std::unique_ptr<system_config_interface> configure_default_multiplexer();


class object_config_interface : public system_config_interface
{
public:
	virtual void set_max_io_size(size_t max_size) = 0;

	virtual void enable_immediate_completion(bool enable) = 0;
};

[[nodiscard]] std::unique_ptr<object_config_interface> configure_object(
	native_platform_handle handle);

template<handle_for<platform_object_t> Handle>
[[nodiscard]] std::unique_ptr<object_config_interface> configure_object(
	Handle const& handle)
{
	return configure_object(handle.native().platform_handle);
}

} // namespace allio::test
