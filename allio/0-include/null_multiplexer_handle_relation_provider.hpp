#pragma once

namespace allio {

class null_multiplexer_handle_relation_provider : public multiplexer_handle_relation_provider
{
public:
	multiplexer_handle_relation const* find_multiplexer_handle_relation(
		type_id<multiplexer> multiplexer_type, type_id<handle> handle_type) const override;

	static multiplexer_handle_relation_provider const& get()
	{
		return s_instance;
	}

private:
	static null_multiplexer_handle_relation_provider const s_instance;
};

} // namespace allio
