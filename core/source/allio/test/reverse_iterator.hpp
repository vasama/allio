#pragma once

#include <ranges>

namespace allio {

template<typename Iterator>
concept reversible_iterator =
	std::input_iterator<Iterator> &&
	requires (Iterator it)
	{
		{ --it } -> std::same_as<Iterator&>;
		{ it-- } -> std::same_as<Iterator>;
	};

template<typename Range>
concept reversible_range =
	std::ranges::input_range<Range> &&
	reversible_iterator<std::ranges::iterator_t<Range>>;


template<reversible_iterator Iterator>
class reverse_iterator
{
	Iterator m_it;
	mutable Iterator m_prev;

public:
	using iterator_concept = typename std::reverse_iterator<Iterator>::iterator_concept;
	using iterator_category = typename std::reverse_iterator<Iterator>::iterator_category;
	using value_type = typename std::reverse_iterator<Iterator>::value_type;
	using difference_type = typename std::reverse_iterator<Iterator>::difference_type;
	using pointer = typename std::reverse_iterator<Iterator>::pointer;
	using reference = typename std::reverse_iterator<Iterator>::reference;

	reverse_iterator() = default;

	constexpr reverse_iterator(Iterator const it)
		: m_it(it), m_prev(it)
	{
	}

	constexpr reference operator*() const
	{
		return *prev();
	}

	constexpr pointer operator->() const
	{
		if constexpr (std::is_pointer_v<Iterator>)
		{
			return prev();
		}
		else
		{
			return prev().operator->();
		}
	}

	constexpr reverse_iterator& operator++() &
	{
		m_prev = --m_it;
		return *this;
	}

	constexpr reverse_iterator operator++(int) &
	{
		auto it = *this;
		++*this;
		return it;
	}

	constexpr reverse_iterator& operator--() &
	{
		m_prev = ++m_it;
		return *this;
	}

	constexpr reverse_iterator operator--(int) &
	{
		auto it = *this;
		--*this;
		return it;
	}

	friend constexpr reverse_iterator operator+(reverse_iterator const& it, difference_type const off)
		requires std::random_access_iterator<Iterator>
	{
		return reverse_iterator(it.m_it - off);
	}

	friend constexpr reverse_iterator operator+(difference_type const off, reverse_iterator const& it)
		requires std::random_access_iterator<Iterator>
	{
		return reverse_iterator(it.m_it - off);
	}

	constexpr reverse_iterator& operator+=(difference_type const off) &
		requires std::random_access_iterator<Iterator>
	{
		m_prev = m_it -= off;
		return *this;
	}

	friend constexpr reverse_iterator operator-(reverse_iterator const& it, difference_type const off)
		requires std::random_access_iterator<Iterator>
	{
		return reverse_iterator(it.m_it + off);
	}

	constexpr reverse_iterator& operator-=(difference_type const off) &
		requires std::random_access_iterator<Iterator>
	{
		m_prev = m_it += off;
		return *this;
	}

	friend constexpr difference_type operator-(reverse_iterator const& lhs, reverse_iterator const& rhs)
		requires std::random_access_iterator<Iterator>
	{
		return rhs.m_it - lhs.m_it;
	}

	constexpr reference operator[](difference_type const off) const
		requires std::random_access_iterator<Iterator>
	{
		return m_it[static_cast<difference_type>(-off - 1)];
	}

	friend constexpr auto operator==(reverse_iterator const& lhs, reverse_iterator const& rhs)
	{
		return lhs.m_it == rhs.m_it;
	}

	friend constexpr auto operator!=(reverse_iterator const& lhs, reverse_iterator const& rhs)
	{
		return lhs.m_it != rhs.m_it;
	}

	friend constexpr auto operator<=>(reverse_iterator const& lhs, reverse_iterator const& rhs)
		requires std::random_access_iterator<Iterator>
	{
		return lhs.m_it <=> rhs.m_it;
	}

private:
	constexpr Iterator& prev() const
	{
		if (m_prev == m_it)
		{
			--m_prev;
		}
		return m_prev;
	}
};

template<reversible_iterator Iterator>
reverse_iterator(Iterator) -> reverse_iterator<Iterator>;

template<reversible_iterator Iterator>
class reverse_range
{
	Iterator m_beg;
	Iterator m_end;

public:
	constexpr reverse_range(reversible_range auto&& range)
		: m_beg(std::ranges::begin(range))
		, m_end(std::ranges::end(range))
	{
	}

	constexpr reverse_iterator<Iterator> begin() const
	{
		return reverse_iterator<Iterator>(m_end);
	}

	constexpr reverse_iterator<Iterator> end() const
	{
		return reverse_iterator<Iterator>(m_beg);
	}

	constexpr Iterator rbegin() const
	{
		return m_beg;
	}

	constexpr Iterator rend() const
	{
		return m_end;
	}
};

template<reversible_range Range>
reverse_range(Range&&) -> reverse_range<std::ranges::iterator_t<Range>>;

} // namespace allio
