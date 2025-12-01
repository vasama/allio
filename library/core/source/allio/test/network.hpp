#pragma once

#include <allio/network.hpp>
#include <allio/path.hpp>
#include <allio/test/shared_object.hpp>

#include <vsm/lazy.hpp>

#include <catch2/generators/catch_generators.hpp>

#include <filesystem>
#include <format>

namespace allio::test {

// Use shared_ptr with the aliasing constructor to hide the storage used for the
// local endpoint path.
using endpoint_type = shared_object<any_endpoint_view>;

class endpoint_factory
{
public:
	virtual network_address_kind address_kind() const = 0;
	virtual endpoint_type create_endpoint() = 0;

protected:
	endpoint_factory() = default;
	endpoint_factory(endpoint_factory const&) = default;
	endpoint_factory& operator=(endpoint_factory const&) = default;
	~endpoint_factory() = default;

	template<typename Endpoint>
	struct endpoint_storage
	{
		Endpoint endpoint;
		any_endpoint_view view;

		explicit endpoint_storage(auto&&... args)
			: endpoint(vsm_forward(args)...)
			, view(endpoint)
		{
		}
	};

	template<typename Endpoint>
	endpoint_type make_endpoint_view(auto&&... args)
	{
		auto ptr = std::make_shared<endpoint_storage<Endpoint>>(vsm_forward(args)...);
		return endpoint_type(std::shared_ptr<any_endpoint_view>(vsm_move(ptr), &ptr->view));
	}
};

struct local_endpoint_factory final : endpoint_factory
{
	size_t next_id = 0;

	network_address_kind address_kind() const override
	{
		return network_address_kind::local;
	}

	endpoint_type create_endpoint() override
	{
		auto const name = std::format("allio_{}.sock", next_id++);
		auto file_path = std::filesystem::temp_directory_path() / name;
		std::filesystem::remove(file_path);
		return make_endpoint_view<path>(vsm_move(file_path).string());
	}
};

struct ipv4_endpoint_factory final : endpoint_factory
{
	uint16_t next_port = 50000;

	network_address_kind address_kind() const override
	{
		return network_address_kind::ipv4;
	}

	endpoint_type create_endpoint() override
	{
		vsm_assert(next_port != 0);
		return make_endpoint_view<ipv4_endpoint>(ipv4_address::localhost, next_port++);
	}
};

struct ipv6_endpoint_factory final : endpoint_factory
{
	uint16_t next_port = 50000;

	network_address_kind address_kind() const override
	{
		return network_address_kind::ipv6;
	}

	endpoint_type create_endpoint() override
	{
		vsm_assert(next_port != 0);
		return make_endpoint_view<ipv6_endpoint>(ipv6_address::localhost, next_port++);
	}
};

using endpoint_factory_ptr = std::shared_ptr<endpoint_factory>;

template<typename EndpointFactory>
endpoint_factory_ptr make_endpoint_factory()
{
	auto ptr = std::make_shared<EndpointFactory>();
	endpoint_factory* const factory = ptr.get();
	return endpoint_factory_ptr(vsm_move(ptr), factory);
};

inline endpoint_factory_ptr generate_endpoint_factory()
{
	return GENERATE(
		as<endpoint_factory_ptr(*)()>()
		, make_endpoint_factory<local_endpoint_factory>
		, make_endpoint_factory<ipv4_endpoint_factory>
		, make_endpoint_factory<ipv6_endpoint_factory>
	)();
}

inline endpoint_type generate_endpoint()
{
	return generate_endpoint_factory()->create_endpoint();
}

} // namespace allio::test
