#pragma once

#include <cstdint>

namespace allio::detail {

template<typename Enum>
concept handle_flags_enum = requires { Enum::allio_detail_handle_flags_flag_count; };

class handle_flags
{
public:
	using uint_type = uint32_t;

private:
	uint_type m_flags;

public:
	static handle_flags const none;


	handle_flags() = default;

	constexpr handle_flags(detail::handle_flags_enum auto const index)
		: m_flags(convert_index(index))
	{
	}

	constexpr handle_flags& operator=(handle_flags const&) & = default;


	[[nodiscard]] constexpr bool operator[](detail::handle_flags_enum auto const index) const
	{
		return m_flags & convert_index(index);
	}


	[[nodiscard]] constexpr handle_flags operator~() const
	{
		return handle_flags(~m_flags);
	}

	constexpr handle_flags& operator&=(handle_flags const flags) &
	{
		m_flags &= flags.m_flags;
		return *this;
	}

	constexpr handle_flags& operator|=(handle_flags const flags) &
	{
		m_flags |= flags.m_flags;
		return *this;
	}

	constexpr handle_flags& operator^=(handle_flags const flags) &
	{
		m_flags ^= flags.m_flags;
		return *this;
	}

	[[nodiscard]] friend constexpr handle_flags operator&(handle_flags const& lhs, handle_flags const& rhs)
	{
		return handle_flags(lhs.m_flags & rhs.m_flags);
	}

	[[nodiscard]] friend constexpr handle_flags operator|(handle_flags const& lhs, handle_flags const& rhs)
	{
		return handle_flags(lhs.m_flags | rhs.m_flags);
	}

	[[nodiscard]] friend constexpr handle_flags operator^(handle_flags const& lhs, handle_flags const& rhs)
	{
		return handle_flags(lhs.m_flags ^ rhs.m_flags);
	}

	[[nodiscard]] bool operator==(handle_flags const&) const = default;

private:
	explicit constexpr handle_flags(uint_type const flags)
		: m_flags(flags)
	{
	}

	[[nodiscard]] static constexpr uint_type convert_index(detail::handle_flags_enum auto const index)
	{
		return (static_cast<uint_type>(1) << decltype(index)::allio_detail_handle_flags_base_count) << static_cast<uint_type>(index);
	}
};

inline constexpr handle_flags handle_flags::none = handle_flags(0);


#define allio_detail_handle_flags(base, ...) \
	struct flags : base::flags \
	{ \
		enum : ::allio::detail::handle_flags::uint_type \
		{ \
			__VA_ARGS__ \
			allio_detail_handle_flags_enum_count, \
			allio_detail_handle_flags_base_count = base::flags::allio_detail_handle_flags_flag_count, \
			allio_detail_handle_flags_flag_count = allio_detail_handle_flags_base_count + allio_detail_handle_flags_enum_count, \
		}; \
		static_assert(allio_detail_handle_flags_flag_count <= base::flags::allio_detail_handle_flags_flag_limit); \
	} \

#define allio_handle_flags(...) \
	allio_detail_handle_flags(base_type, __VA_ARGS__)

#define allio_handle_implementation_flags(...) \
	allio_detail_handle_flags(base_type::impl_type, __VA_ARGS__)

} // namespace allio::detail
