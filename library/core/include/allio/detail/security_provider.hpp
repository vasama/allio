#pragma once

#include <allio/detail/network_security.hpp>

namespace allio::detail {

class secure_socket_provider
{
#if 0
	void* m_next_ptr;
#endif

public:
#if 0
	class iterator
	{
		secure_socket_provider const* m_provider;

	public:
		using value_type = secure_socket_provider;
		using difference_type = ptrdiff_t;

		iterator() = default;

		explicit iterator(secure_socket_provider const* const provider)
			: m_provider(provider)
		{
		}

		[[nodiscard]] secure_socket_provider const& operator*() const
		{
			return *m_provider;
		}

		[[nodiscard]] secure_socket_provider const* operator->() const
		{
			return m_provider;
		}

		iterator& operator++() &
		{
			vsm_assert(m_provider != nullptr);
			m_provider = m_provider->next();
		}

		[[nodiscard]] iterator operator++(int) &
		{
			auto r = *this;
			++*this();
			return r;
		}

		[[nodiscard]] friend bool operator==(iterator const&, iterator const&) = default;
	};
#endif

	[[nodiscard]] virtual std::string_view name() const = 0;
	[[nodiscard]] virtual std::string_view version() const = 0;

	[[nodiscard]] virtual socket_handle make_stream_socket() const = 0;
	[[nodiscard]] virtual listen_socket_handle make_listen_socket() const = 0;

	[[nodiscard]] virtual socket_handle wrap_stream_socket(raw_socket_handle h) const = 0;
	[[nodiscard]] virtual listen_socket_handle wrap_listen_socket(raw_listen_socket_handle h) const = 0;

#if 0
	[[nodiscard]] static std::ranges::subrange<iterator> get_provider_list()
	{
		return { iterator(head()), iterator(nullptr) };
	}

	[[nodiscard]] static secure_socket_provider const* get_default_provider();

private:
	void register_provider_impl(secure_socket_provider& provider);
	void load_default_providers();

	void load_platform_providers();

	[[nodiscard]] static secure_socket_provider const* head();
	[[nodiscard]] secure_socket_provider const* next() const;
#endif
};

[[nodiscard]] vsm::result<void> register_socket_provider(secure_socket_provider const& provider);

[[nodiscard]] secure_socket_provider const& raw_socket_provider();
[[nodiscard]] secure_socket_provider const& tls_socket_provider();

} // namespace allio::detail
