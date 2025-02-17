#pragma once

#include <allio/any_path.hpp>
#include <allio/detail/parameters.hpp>

#include <vsm/assert.h>
#include <vsm/int128.hpp>
#include <vsm/result.hpp>
#include <vsm/standard/bit.hpp>
#include <vsm/utility.hpp>

#include <bit>
#include <charconv>
#include <compare>
#include <concepts>
#include <optional>
#include <string_view>

#include <cstdint>

namespace allio {

template<std::integral T>
[[nodiscard]] constexpr T network_byte_order(T const value)
{
	if constexpr (std::endian::native == std::endian::little)
	{
		return vsm::byteswap(value);
	}
	else if constexpr (std::endian::native == std::endian::big)
	{
		return value;
	}
	else
	{
		static_assert(sizeof(T) == 0);
	}
}


enum class network_address_kind : uint8_t
{
	null,
	local, //TODO: Rename to file or path?
	ipv4,
	ipv6,
};


struct null_endpoint_t {};
inline constexpr null_endpoint_t null_endpoint = {};

//TODO: Rename to local_endpoint and use in any_endpoint_view, or get rid of.
class local_address
{
	path_view m_path;

public:
	local_address() = default;

	explicit constexpr local_address(path_view const path)
		: m_path(path)
	{
	}

	[[nodiscard]] constexpr path_view path() const
	{
		return m_path;
	}
};


using network_port_t = uint16_t;

// Note that ipv4_address always represents addresses using host byte order. This means that for
// example the loopback address 127.0.0.1 is unambiguously represented as 0x7f'00'00'01. Some use
// cases outside of this library may require explicit conversion to network byte order.
class ipv4_address
{
public:
	using uint_type = uint32_t;

private:
	uint_type m_addr;

public:
	ipv4_address() = default;

	explicit constexpr ipv4_address(uint_type const integer)
		: m_addr(integer)
	{
	}

	[[nodiscard]] constexpr uint_type integer() const
	{
		return m_addr;
	}


	static ipv4_address const unspecified;
	static ipv4_address const localhost;

	[[nodiscard]] static vsm::result<ipv4_address> parse(std::string_view string);


	[[nodiscard]] friend auto operator<=>(ipv4_address const&, ipv4_address const&) = default;
};

inline constexpr ipv4_address ipv4_address::unspecified = ipv4_address(0);
inline constexpr ipv4_address ipv4_address::localhost = ipv4_address(0x7f'00'00'01);

struct ipv4_endpoint
{
	ipv4_address address;
	network_port_t port;


	[[nodiscard]] static vsm::result<ipv4_endpoint> parse(std::string_view string);

	[[nodiscard]] friend auto operator<=>(ipv4_endpoint const&, ipv4_endpoint const&) = default;
};


using ipv6_zone_t = uint32_t;

class ipv6_address
{
public:
	using uint_type = vsm::uint128_t;

private:
	uint_type m_addr;

public:
	ipv6_address() = default;

	explicit constexpr ipv6_address(uint_type const integer)
		: m_addr(integer)
	{
	}

	[[nodiscard]] constexpr uint_type integer() const
	{
		return m_addr;
	}


	static ipv6_address const unspecified;
	static ipv6_address const localhost;

	[[nodiscard]] static vsm::result<ipv6_address> parse(std::string_view string);


	[[nodiscard]] friend auto operator<=>(ipv6_address const&, ipv6_address const&) = default;
};

inline constexpr ipv6_address ipv6_address::unspecified = ipv6_address(0);
inline constexpr ipv6_address ipv6_address::localhost = ipv6_address(1);

struct ipv6_endpoint
{
	ipv6_address address;
	network_port_t port;
	ipv6_zone_t zone;


	[[nodiscard]] static vsm::result<ipv6_endpoint> parse(std::string_view string);

	[[nodiscard]] friend auto operator<=>(ipv6_endpoint const&, ipv6_endpoint const&) = default;
};


class network_endpoint
{
	network_address_kind m_kind;
	union
	{
		null_endpoint_t m_null;
		local_address m_local;
		ipv4_endpoint m_ipv4;
		ipv6_endpoint m_ipv6;
	};

public:
	constexpr network_endpoint()
		: m_kind(network_address_kind::null)
		, m_null{}
	{
	}

	constexpr network_endpoint(null_endpoint_t)
		: m_kind(network_address_kind::null)
		, m_null{}
	{
	}

	constexpr network_endpoint(local_address const& address)
		: m_kind(network_address_kind::local)
		, m_local(address)
	{
	}

	constexpr network_endpoint(ipv4_endpoint const& endpoint)
		: m_kind(network_address_kind::ipv4)
		, m_ipv4(endpoint)
	{
	}

	constexpr network_endpoint(ipv6_endpoint const& endpoint)
		: m_kind(network_address_kind::ipv6)
		, m_ipv6(endpoint)
	{
	}

	[[nodiscard]] constexpr network_address_kind kind() const
	{
		return m_kind;
	}

	[[nodiscard]] constexpr bool is_null() const
	{
		return m_kind == network_address_kind::null;
	}

	[[nodiscard]] constexpr local_address const& local() const
	{
		vsm_assert(m_kind == network_address_kind::local);
		return m_local;
	}

	[[nodiscard]] constexpr ipv4_endpoint const& ipv4() const
	{
		vsm_assert(m_kind == network_address_kind::ipv4);
		return m_ipv4;
	}

	[[nodiscard]] constexpr ipv6_endpoint const& ipv6() const
	{
		vsm_assert(m_kind == network_address_kind::ipv6);
		return m_ipv6;
	}

	[[nodiscard]] decltype(auto) visit(auto&& visitor) const
	{
		switch (m_kind)
		{
		case network_address_kind::null:
			return vsm_forward(visitor)(null_endpoint_t());

		case network_address_kind::local:
			return vsm_forward(visitor)(m_local);

		case network_address_kind::ipv4:
			return vsm_forward(visitor)(m_ipv4);

		case network_address_kind::ipv6:
			return vsm_forward(visitor)(m_ipv6);
		}
	}
};


class platform_endpoint_view : std::span<std::byte const>
{
	using span_type = std::span<std::byte const>;

public:
	template<no_cvref_of<platform_endpoint_view> First, typename... Rest>
		requires std::constructible_from<span_type, First, Rest...>
	explicit platform_endpoint_view(First&& first, Rest&&... rest)
		: span_type(vsm_forward(first), vsm_forward(rest)...)
	{
	}

	using span_type::data;
	using span_type::size;
};

class any_endpoint_view
{
	struct local_t
	{
		explicit local_t() = default;
	};

	static constexpr uint32_t ctrl_bits = 32;

	static constexpr uint32_t type_bits = 4;
	static constexpr uint32_t type_mask = (static_cast<uint32_t>(1) << type_bits) - 1;

	static constexpr uint32_t size_bits = ctrl_bits - type_bits;
	static constexpr uint32_t size_mask = (static_cast<uint32_t>(1) << size_bits) - 1;

	static constexpr uint32_t error_type                        = 0;
	static constexpr uint32_t platform_type                     = 1;
	static constexpr uint32_t generic_type_offset               = 2;

	void const* m_data;
	uint32_t m_ctrl;

public:
	any_endpoint_view(null_endpoint_t)
	{
	}

	template<detail::_any_path Path>
	any_endpoint_view(Path const& path)
		: any_endpoint_view(local_t(), get_path_string(path))
	{
		static_assert(
			std::is_same_v<typename Path::value_type, char>,
			"Automatic transcoding of local addresses is not currently supported.");
	}

	any_endpoint_view(ipv4_endpoint const& endpoint)
		: any_endpoint_view(
			&endpoint,
			sizeof(endpoint),
			make_kind_ctrl(network_address_kind::ipv4))
	{
	}

	any_endpoint_view(ipv6_endpoint const& endpoint)
		: any_endpoint_view(
			&endpoint,
			sizeof(endpoint),
			make_kind_ctrl(network_address_kind::ipv6))
	{
	}

	any_endpoint_view(platform_endpoint_view const view)
		: any_endpoint_view(
			view.data(),
			view.size(),
			make_kind_ctrl(network_address_kind::local))
	{
	}

	[[nodiscard]] bool is_generic_endpoint() const
	{
		return m_ctrl >> size_bits > extra_state_count;
	}

	[[nodiscard]] network_address_kind get_generic_kind() const
	{
		vsm_assert(is_generic_endpoint()); //PRECONDITION
		return static_cast<network_address_kind>(m_ctrl >> size_bits);
	}

	template<std::same_as<path_view>>
	[[nodiscard]] path_view get_generic_endpoint() const
	{
		vsm_assert(get_generic_kind() == network_address_kind::local); //PRECONDITION
		return path_view(static_cast<char const*>(m_data), m_ctrl & size_mask);
	}

	template<std::same_as<ipv4_endpoint>>
	[[nodiscard]] ipv4_endpoint const& get_generic_endpoint() const
	{
		vsm_assert(get_generic_kind() == network_address_kind::ipv4); //PRECONDITION
		return *static_cast<ipv4_endpoint const*>(m_data);
	}

	template<std::same_as<ipv6_endpoint>>
	[[nodiscard]] ipv6_endpoint const& get_generic_endpoint() const
	{
		vsm_assert(get_generic_kind() == network_address_kind::ipv6); //PRECONDITION
		return *static_cast<ipv6_endpoint const*>(m_data);
	}

	[[nodiscard]] bool is_platform_endpoint() const
	{
		return m_ctrl >> size_bits == platform_type;
	}

	[[nodiscard]] platform_endpoint_view get_platform_endpoint() const
	{
		vsm_assert(is_platform_endpoint()); //PRECONDITION
		return platform_endpoint_view(
			static_cast<std::byte const*>(m_data),
			m_ctrl & size_mask);
	}

private:
	explicit any_endpoint_view(
		void const* const data,
		size_t const size,
		uint32_t const ctrl)
		: m_data(data)
		, m_ctrl(size > size_mask ? 0 : ctrl | static_cast<uint32_t>(size))
	{
	}

	explicit any_endpoint_view(local_t, std::string_view const path)
		: any_endpoint_view(
			path.data(),
			path.size(),
			make_kind_ctrl(network_address_kind::local))
	{
	}

	static consteval uint32_t make_kind_type(network_address_kind const kind)
	{
		return static_cast<uint32_t>(kind) + generic_type_offset;
	}

	static consteval uint32_t make_kind_ctrl(network_address_kind const kind)
	{
		return make_kind_type(kind) << size_bits;
	}

	[[nodiscard]] decltype(auto) visit(auto&& visitor) const
	{
		switch (m_ctrl >> size_bits)
		{
		case platform_type:
			return vsm_forward(visitor)(get_platform_endpoint());

		case make_kind_type(network_address_kind::null):
			return vsm_forward(visitor)(null_endpoint_t());

		case make_kind_type(network_address_kind::local):
			return vsm_forward(visitor)(get_local_endpoint());

		case make_kind_type(network_address_kind::ipv4):
			return vsm_forward(visitor)(get_generic_endpoint<ipv4_endpoint>());

		case make_kind_type(network_address_kind::ipv6):
			return vsm_forward(visitor)(get_generic_endpoint<ipv6_endpoint>());
		}

		//TODO: Rename the error tag to be more generic, move out of detail.
		return vsm_forward(visitor)(detail::string_length_out_of_range_t());
	}
};


#if 1 // NEW
namespace detail {

class network_endpoint_buffer
{
};

class network_endpoint_format_functions
{
public:
	virtual vsm::result<void> generic_to_raw(
		network_endpoint const& endpoint,
		network_endpoint_buffer const& buffer) = 0;

	virtual vsm::result<void> raw_to_generic(
		std::span<std::byte const> endpoint,
		network_endpoint& out_endpoint) = 0;

protected:
	network_endpoint_format_functions() = default;
	network_endpoint_format_functions(network_endpoint_format_functions const&) = default;
	network_endpoint_format_functions& operator=(network_endpoint_format_functions const&) = default;
	~network_endpoint_format_functions() = default;
};

class network_endpoint_format
{
	[[maybe_unused]] //TODO: Temporary workaround
	network_endpoint_format_functions const* m_functions;

public:
	explicit constexpr network_endpoint_format(
		network_endpoint_format_functions const* const functions)
		: m_functions(functions)
	{
	}

	[[nodiscard]] static constexpr network_endpoint_format generic()
	{
		return network_endpoint_format(nullptr);
	}
};

} // namespace detail
#endif // NEW

} // namespace allio
