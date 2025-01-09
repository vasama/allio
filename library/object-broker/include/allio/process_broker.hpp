#pragma once

namespace allio {

template<typename Handle>
class ipc_connection
{
public:
	template<std::derived_from<platform_handle> PlatformHandle>
	void send_handle(PlatformHandle const& handle) const
	{

	}

	template<std::derived_from<platform_handle> PlatformHandle>
	vsm::result<PlatformHandle> receive_handle() const
	{

	}
};

} // namespace allio
