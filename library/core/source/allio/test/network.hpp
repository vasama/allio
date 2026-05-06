#pragma once

#include <allio/network.hpp>
#include <allio/path.hpp>
#include <allio/test/shared_object.hpp>

#include <vsm/lazy.hpp>

#include <catch2/generators/catch_generators.hpp>
#include <catch2/internal/catch_context.hpp>

#include <filesystem>
#include <format>
#include <random>

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

class local_endpoint_factory final : public endpoint_factory
{
	size_t next_id = 0;

public:
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

class ip_endpoint_factory : public endpoint_factory
{
	static constexpr uint16_t min_port = 50000;
	static constexpr uint16_t max_port = 59999;

	uint16_t next_port;

protected:
	ip_endpoint_factory()
		: next_port(std::uniform_int_distribution<uint16_t>(min_port, max_port)(Catch::sharedRng()))
	{
	}

	ip_endpoint_factory(ip_endpoint_factory const&) = default;
	ip_endpoint_factory& operator=(ip_endpoint_factory const&) = default;

	~ip_endpoint_factory() = default;

	uint16_t get_next_port()
	{
		uint16_t const port = next_port;
		next_port = port == max_port ? min_port : port + 1;
		return port;
	}
};

class ipv4_endpoint_factory final : public ip_endpoint_factory
{
public:
	network_address_kind address_kind() const override
	{
		return network_address_kind::ipv4;
	}

	endpoint_type create_endpoint() override
	{
		return make_endpoint_view<ipv4_endpoint>(ipv4_address::localhost, get_next_port());
	}
};

class ipv6_endpoint_factory final : public ip_endpoint_factory
{
public:
	network_address_kind address_kind() const override
	{
		return network_address_kind::ipv6;
	}

	endpoint_type create_endpoint() override
	{
		return make_endpoint_view<ipv6_endpoint>(ipv6_address::localhost, get_next_port());
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
