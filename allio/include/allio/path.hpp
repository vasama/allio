#pragma once

#include <allio/detail/path.hpp>
#include <allio/path_view.hpp>

#include <string>

namespace allio {

template<typename Char, typename String>
class basic_path_adaptor : String
{
	using path_view_type = basic_path_view<Char>;

	using string_type = String;
	using string_view_type = std::basic_string_view<Char>;

public:
	static constexpr Char preferred_separator = path_view_type::preferred_separator;

	static constexpr bool is_separator(Char const character)
	{
		return path_view_type::is_separator(character);
	}


	class iterator : path_view_type::iterator
	{
		using base_type = typename path_view_type::iterator;

	public:
		using iterator_category = typename base_type::iterator_category;
		using value_type = typename base_type::value_type;
		using difference_type = typename base_type::difference_type;
		using pointer = typename base_type::pointer;
		using reference = typename base_type::reference;

		iterator() = default;

		using base_type::operator*;
		using base_type::operator->;

		using base_type::operator++;
		using base_type::operator--;

		bool operator==(iterator const&) const = default;
		bool operator!=(iterator const&) const = default;

	private:
		explicit iterator(base_type const iterator)
			: base_type(iterator)
		{
		}

		friend class basic_path_adaptor;
	};
	using const_iterator = iterator;

	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;


	basic_path_adaptor() = default;

	constexpr basic_path_adaptor(path_view_type const path)
		: string_type(path.string())
	{
	}

	explicit constexpr basic_path_adaptor(string_view_type const string)
		: string_type(string)
	{
	}

	explicit constexpr basic_path_adaptor(string_type&& string)
		: string_type(static_cast<string_type&&>(string))
	{
	}

	explicit constexpr basic_path_adaptor(string_type const& string)
		: string_type(string)
	{
	}

	constexpr basic_path_adaptor(Char const* const c_str)
		: string_type(c_str)
	{
	}

	explicit constexpr basic_path_adaptor(Char const* const data, size_t const size)
		: string_type(data, size)
	{
	}

	template<std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
	explicit constexpr basic_path_adaptor(Iterator const iterator, Sentinel const sentinel)
		//requires std::is_same_v<std::iterator_value_t<Iterator>, Char>
		: string_type(iterator, sentinel)
	{
	}

	template<std::ranges::input_range Range>
	explicit constexpr basic_path_adaptor(Range&& range)
		//requires std::is_same_v<std::ranges::range_value_t<Range>, Char>
		: string_type(static_cast<Range&&>(range))
	{
	}

	basic_path_adaptor(decltype(nullptr)) = delete;


	using string_type::empty;

	constexpr string_type& string()
	{
		return *this;
	}

	constexpr string_type const& string() const
	{
		return *this;
	}


	constexpr bool is_absolute() const
	{
		return view().is_absolute();
	}

	constexpr bool is_relative() const
	{
		return view().is_relative();
	}


	constexpr path_view_type root_name() const
	{
		return view().root_name();
	}

	constexpr path_view_type root_directory() const
	{
		return view().root_directory();
	}

	constexpr path_view_type root_path() const
	{
		return view().root_path();
	}

	constexpr path_view_type relative_path() const
	{
		return view().relative_path();
	}

	constexpr path_view_type parent_path() const
	{
		return view().parent_path();
	}

	constexpr path_view_type filename() const
	{
		return view().filename();
	}

	constexpr path_view_type stem() const
	{
		return view().stem();
	}

	constexpr path_view_type extension() const
	{
		return view().extension();
	}


	constexpr bool has_trailing_separators() const
	{
		return view().has_trailing_separators();
	}

	constexpr path_view_type without_trailing_separators() const
	{
		return view().has_trailing_separators();
	}

	constexpr void remove_trailing_separators()
	{
		string_type::resize(without_trailing_separators().string().size());
	}


	constexpr path_view_type copy_lexically_normal(Char* const buffer) const
	{
		return view().copy_lexically_normal(buffer);
	}

	constexpr path_view_type copy_lexically_relative(path_view_type const base, Char* buffer) const
	{
		return view().copy_lexically_relative(base, buffer);
	}

	constexpr path_view_type copy_lexically_proximate(path_view_type const base, Char* buffer) const
	{
		return view().copy_lexically_proximate(base, buffer);
	}


	constexpr basic_path_adaptor lexically_normal() const
	{
		basic_path_adaptor result;
		result.resize(string_type::size()); //TODO: use resize_for_overwrite or equivalent.
		result.resize(copy_lexically_normal(result.data()).string().size());
		return result;
	}

	constexpr basic_path_adaptor lexically_relative(path_view_type const base) const
	{
		basic_path_adaptor result;
		result.resize(string_type::size()); //TODO: use resize_for_overwrite or equivalent.
		result.resize(copy_lexically_relative(base, result.data()).string().size());
		return result;
	}

	constexpr basic_path_adaptor lexically_proximate(path_view_type const base) const
	{
		basic_path_adaptor result;
		result.resize(string_type::size()); //TODO: use resize_for_overwrite or equivalent.
		result.resize(copy_lexically_proximate(base, result.data()).string().size());
		return result;
	}


	friend constexpr basic_path_adaptor operator/(basic_path_adaptor const& lhs, basic_path_adaptor const& rhs)
	{
		return combine(lhs.view(), rhs.view());
	}

	friend constexpr basic_path_adaptor operator/(basic_path_adaptor const& lhs, path_view_type const rhs)
	{
		return combine(lhs.view(), rhs);
	}

	friend constexpr basic_path_adaptor operator/(path_view_type const lhs, basic_path_adaptor const& rhs)
	{
		return combine(lhs, rhs.view());
	}

	constexpr basic_path_adaptor& operator/=(basic_path_adaptor const& rhs) &;

	constexpr basic_path_adaptor& operator/=(path_view_type const rhs) &;


	constexpr iterator begin() const
	{
		return iterator(view().begin());
	}

	constexpr iterator end() const
	{
		return iterator(view().end());
	}

	constexpr reverse_iterator rbegin() const
	{
		return reverse_iterator(view().end());
	}

	constexpr reverse_iterator rend() const
	{
		return reverse_iterator(view().begin());
	}


	constexpr int compare(path_view_type const other) const
	{
		return view().compare(other);
	}

	constexpr int compare(basic_path_adaptor const& other) const
	{
		return view().compare(other.view());
	}

	friend constexpr bool operator==(basic_path_adaptor const& lhs, basic_path_adaptor const& rhs)
	{
		return lhs.view() == rhs.view();
	}

	friend constexpr bool operator==(basic_path_adaptor const& lhs, path_view_type const rhs)
	{
		return lhs.view() == rhs;
	}

	friend constexpr bool operator==(path_view_type const lhs, basic_path_adaptor const& rhs)
	{
		return lhs == rhs.view();
	}

	friend constexpr bool operator!=(basic_path_adaptor const& lhs, basic_path_adaptor const& rhs)
	{
		return lhs.view() != rhs.view();
	}

	friend constexpr bool operator!=(basic_path_adaptor const& lhs, path_view_type const rhs)
	{
		return lhs.view() != rhs;
	}

	friend constexpr bool operator!=(path_view_type const lhs, basic_path_adaptor const& rhs)
	{
		return lhs != rhs.view();
	}

	friend constexpr auto operator<=>(basic_path_adaptor const& lhs, basic_path_adaptor const& rhs)
	{
		return lhs.view() <=> rhs.view();
	}

	friend constexpr auto operator<=>(basic_path_adaptor const& lhs, path_view_type const rhs)
	{
		return lhs.view() <=> rhs;
	}

	friend constexpr auto operator<=>(path_view_type const lhs, basic_path_adaptor const& rhs)
	{
		return lhs <=> rhs.view();
	}


	friend constexpr bool lexically_equivalent(basic_path_adaptor const& lhs, basic_path_adaptor const& rhs)
	{
		return lexically_equivalent(lhs.view(), rhs.view());
	}

	friend constexpr bool lexically_equivalent(basic_path_adaptor const& lhs, path_view_type const rhs)
	{
		return lexically_equivalent(lhs.view(), rhs);
	}

	friend constexpr bool lexically_equivalent(path_view_type const lhs, basic_path_adaptor const& rhs)
	{
		return lexically_equivalent(lhs, rhs.view());
	}

private:
	constexpr path_view_type view() const
	{
		return path_view_type(string_type::data(), string_type::size());
	}

	static constexpr basic_path_adaptor combine(path_view_type const lhs, path_view_type const rhs)
	{
		auto const combine_result = combine_path(lhs, rhs);

		basic_path_adaptor path;
		path.resize(combine_result.size());
		combine_result.copy(path.data());
		return path;
	}

	template<vsm::any_cvref_of<basic_path_adaptor> Self>
	friend string_view_type tag_invoke(get_path_string_t, Self&& self)
	{
		return static_cast<vsm::copy_cvref_t<Self&&, string_type>>(self);
	}
};

template<typename Char, typename Allocator = std::allocator<Char>>
using basic_path = basic_path_adaptor<Char, std::basic_string<Char, std::char_traits<Char>, Allocator>>;


using path = basic_path<char>;

} // namespace allio

#include <allio/linux/detail/undef.i>
#include vsm_pp_include(allio/vsm_os/detail/path.hpp)
#include <allio/linux/detail/undef.i>
