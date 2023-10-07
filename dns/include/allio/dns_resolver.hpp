#pragma once

#include <allio/socket_handle.hpp>

namespace allio {

struct domain_name
{
	uint8_t m_size;
	char m_data[255];
};

struct dns_result
{
	uint32_t time_to_live;
	bool is_referral;
	domain_name name;
	network_endpoint address;
};

namespace detail {

class dns_query : public vsm::wb_tree_link
{
	uint16_t m_transaction_id;
	uint16_t m_record_type_mask;
	network_endpoint const* m_server_address;
	std::string_view m_domain_name;
};

class dns_resolver
{
	struct engine {};

	packet_socket_handle m_udp_socket;
	vsm::wb_tree<dns_query> m_queries;
	engine m_random_engine;

public:

private:
	vsm::result<void> start_query(dns_query* query);
	vsm::result<void> handle_udp_packet(packet_read_result const& packet);
};

} // namespace detail

using detail::dns_resolver;

} // namespace allio
