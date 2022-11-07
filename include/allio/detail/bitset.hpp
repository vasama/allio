#pragma once

#include <allio/detail/box.hpp>

#include <bitset>
#include <optional>

namespace allio::detail::bitset {

//TODO: Use an optional with sentinel.

template<size_t Size>
static std::optional<size_t> flip_any(std::bitset<Size>& bitset, bool const value)
{
	static constexpr unsigned long mask = ~(static_cast<unsigned long>(-1) << Size);
	if (unsigned long const bits = (bitset.to_ulong() ^ -static_cast<int>(!value)) & mask)
	{
		size_t const index = std::countr_zero(bits);
		bitset.flip(index);
		return index;
	}
	return std::nullopt;
}

class index_sentinel {};

template<typename Word>
class index_iterator
{
	Word m_word;
	size_t m_index;

public:
	index_iterator() = default;

	explicit constexpr index_iterator(Word const word) noexcept
		: m_word(word)
		, m_index(std::countr_one(word))
	{
	}

	[[nodiscard]] constexpr size_t operator*() const noexcept
	{
		return m_index;
	}

	[[nodiscard]] constexpr detail::box<size_t> operator->() const noexcept
	{
		return box<size_t>(m_index);
	}

	constexpr index_iterator& operator++() & noexcept
	{
		increment();
		return *this;
	}

	[[nodiscard]] constexpr index_iterator operator++(int) & noexcept
	{
		auto result = *this;
		increment();
		return result;
	}

	[[nodiscard]] constexpr bool operator==(index_sentinel) const noexcept
	{
		return m_index >= sizeof(Word) * CHAR_BIT;
	}

private:
	constexpr void increment() noexcept
	{
		m_word ^= static_cast<Word>(1) << m_index;
		m_index = std::countr_one(m_word);
	}
};

template<typename Word>
class index_range
{
	Word m_word;

public:
	explicit constexpr index_range(Word const word) noexcept
		: m_word(word)
	{
	}

	[[nodiscard]] index_iterator<Word> begin() const noexcept
	{
		return index_iterator<Word>(m_word);
	}

	[[nodiscard]] index_sentinel end() const noexcept
	{
		return {};
	}
};

template<size_t Size>
static index_range<unsigned long> indices(std::bitset<Size> const& set, bool const value)
{
	return index_range<unsigned long>(set.to_ulong() ^ static_cast<unsigned long>(-static_cast<int>(value)));
}

} // namespace allio::detail::bitset
