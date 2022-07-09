#pragma once

#include <allio/multiplexer.hpp>

#include <concepts>

namespace allio::detail {

template<size_t ProviderCount>
class multiple_multiplexer_handle_relation_provider final : public multiplexer_handle_relation_provider
{
	multiplexer_handle_relation_provider const* m_providers[ProviderCount];

public:
	template<std::derived_from<multiplexer_handle_relation_provider>... Providers>
	multiple_multiplexer_handle_relation_provider(Providers const&... providers)
		requires (sizeof...(Providers) == ProviderCount)
		: m_providers{ &providers... }
	{
	}

	multiplexer_handle_relation const* find_multiplexer_handle_relation(
		type_id<multiplexer> const multiplexer_type, type_id<handle> const handle_type) const override
	{
		for (auto const provider : m_providers)
		{
			if (auto const relation = provider->find_multiplexer_handle_relation(multiplexer_type, handle_type))
			{
				return relation;
			}
		}

		return nullptr;
	}
};

template<std::derived_from<multiplexer_handle_relation_provider>... Providers>
multiple_multiplexer_handle_relation_provider(Providers const&...) -> multiple_multiplexer_handle_relation_provider<sizeof...(Providers)>;

} // namespace allio::detail
