#include <allio/path.hpp>

#include <allio/test/reverse_iterator.hpp>

#include <vsm/lift.hpp>

#include <catch2/catch_all.hpp>

#include <filesystem>
#include <ranges>

using namespace allio;

namespace {

template<std::input_iterator Iterator>
class checked_iterator
{
	Iterator m_it;

public:
	using iterator_concept = typename std::reverse_iterator<Iterator>::iterator_concept;
	using iterator_category = typename std::reverse_iterator<Iterator>::iterator_category;
	using value_type = typename std::reverse_iterator<Iterator>::value_type;
	using difference_type = typename std::reverse_iterator<Iterator>::difference_type;
	using pointer = typename std::reverse_iterator<Iterator>::pointer;
	using reference = typename std::reverse_iterator<Iterator>::reference;

	checked_iterator() = default;

	checked_iterator(Iterator const it)
		: m_it(it)
	{
	}

	reference operator*() const
	{
		return m_it.operator*();
	}

	pointer operator->() const
	{
		if constexpr (std::is_pointer_v<Iterator>)
		{
			return m_it;
		}
		else
		{
			return m_it.operator->();
		}
	}

	checked_iterator& operator++() &
	{
		if constexpr (std::bidirectional_iterator<Iterator>)
		{
			Iterator const prev = m_it;
			Iterator next = ++m_it;
			--next;
			CHECK(next == prev);
		}
		else
		{
			++m_it;
		}
		return *this;
	}

	checked_iterator operator++(int) &
	{
		auto it = *this;
		++*this;
		return it;
	}

	checked_iterator& operator--() &
		requires std::bidirectional_iterator<Iterator>
	{
		Iterator const prev = m_it;
		Iterator next = --m_it;
		++next;
		CHECK(next == prev);
		return *this;
	}

	checked_iterator operator--(int) &
		requires std::bidirectional_iterator<Iterator>
	{
		auto it = *this;
		--*this;
		return it;
	}

	bool operator==(checked_iterator const& other) const = default;
};

template<std::input_iterator Iterator>
checked_iterator(Iterator) -> checked_iterator<Iterator>;

} // namespace


static void generate_separator(std::string& buffer, size_t const max_separators = 2)
{
	size_t const count = GENERATE_COPY(range<size_t>(0, max_separators));

	for (size_t i = 0; i <= count; ++i)
	{
		char separator = '/';

#if vsm_os_win32
		if (GENERATE(0, 1))
		{
			separator = '\\';
		}
#endif

		buffer.push_back(separator);
	}
}

static void generate_root(std::string& buffer)
{
#if vsm_os_win32
	if (GENERATE(0, 1))
	{
		buffer += GENERATE(
			as<std::string_view>{}
			, "C:"
			, "\\\\."
			, "\\\\?"
			, "\\\\server"
		);

		if (GENERATE(0, 1))
		{
			return;
		}
	}
#endif

	generate_separator(buffer);
}

static void generate_relative(
	std::string& buffer,
	size_t const max_components = 2,
	size_t const max_separators = 2)
{
	size_t const count = GENERATE_COPY(range<size_t>(0, max_components));

	for (size_t i = 0; i <= count; ++i)
	{
		if (i > 0)
		{
			generate_separator(buffer, max_separators);
		}

		buffer += GENERATE(
			as<std::string_view>{}
			, "."
			, ".."
			, "file"
			, "file."
			, "file.cpp"
			, "file.generated.cpp"
			, ".file"
			, ".file.cpp"
		);
	}
}

static std::filesystem::path trim_trailing_separators(std::filesystem::path path)
{
	using native_type = std::filesystem::path::string_type;
	using native_char = native_type::value_type;

	constexpr native_char sep_1 = '/';
	constexpr native_char sep_2 = std::filesystem::path::preferred_separator;

	constexpr auto is_separator = [](native_char const character)
	{
		return character == sep_1 || character == sep_2;
	};

	native_type native = vsm_move(path).native();

	if (native.size() >= 1 && is_separator(native.back()))
	{
		while (native.size() >= 2 && is_separator(native.end()[-2]))
		{
			native.pop_back();
		}
	}

	return std::filesystem::path(vsm_move(native));
}

static std::filesystem::path trim_root_path_separators(std::filesystem::path path)
{
	auto root = trim_trailing_separators(path.root_path());
	auto relative = path.relative_path();
	return std::filesystem::path(vsm_move(root).native() + vsm_move(relative).native());
}

TEST_CASE("path_view standard compliance - decomposition", "[path_view]")
{
	std::string buffer;

	if (vsm_os_win32 && GENERATE(0, 1))
	{
		// MSSTL has non-conforming handling of NT paths. For that reason, only paths of the
		// following forms are tested:
		// * /??/
		// * /??/*

		generate_separator(buffer, 1);
		buffer += "??";
		generate_separator(buffer, 1);

		if (GENERATE(0, 1))
		{
			generate_relative(buffer);
		}
	}
	else
	{
		if (GENERATE(0, 1))
		{
			generate_root(buffer);
		}

		if (GENERATE(0, 1))
		{
			generate_relative(buffer);

			if (GENERATE(0, 1))
			{
				generate_separator(buffer);
			}
		}
	}

	CAPTURE(buffer);


	auto const a_path = path_view(buffer);
	auto const s_path = std::filesystem::path(buffer);


	// Path properties
	{
#define allio_property(property) \
	CHECK(a_path.property() == s_path.property())

		allio_property(is_absolute);
		allio_property(is_relative);
#undef allio_property

#define allio_property(transform, property) \
	CHECK(a_path.property().string() == transform(s_path.property()).string())

		allio_property(trim_trailing_separators, root_path);
		allio_property(trim_trailing_separators, root_directory);
		allio_property(trim_trailing_separators, parent_path);
#undef allio_property

#define allio_property(property) \
	CHECK(a_path.property().string() == s_path.property().string())

		allio_property(root_name);
		allio_property(relative_path);

		allio_property(filename);
		allio_property(stem);
		allio_property(extension);
#undef allio_property
	}

	// Iteration
	{
		// ALLIO always trims root directory separators, but it is not guaranteed that the standard
		// library do so. For that reason, the standard root path separators are trimmed manually.
		auto const s_iterable_path = trim_root_path_separators(s_path);

		// Forward iteration:
		{
			auto const a_beg = checked_iterator(a_path.begin());
			auto const a_end = checked_iterator(a_path.end());

			auto const s_beg = s_iterable_path.begin();
			auto const s_end = s_iterable_path.end();

			auto a_pos = a_beg;
			auto s_pos = s_beg;

			for (; a_pos != a_end && s_pos != s_end; ++a_pos, ++s_pos)
			{
				REQUIRE(a_pos->string() == s_pos->string());
			}

			REQUIRE((a_pos == a_end) == (s_pos == s_end));
		}

		// Reverse iteration:
		{
			auto const a_beg = checked_iterator(a_path.rbegin());
			auto const a_end = checked_iterator(a_path.rend());

			auto const s_beg = reverse_iterator(s_iterable_path.end());
			auto const s_end = reverse_iterator(s_iterable_path.begin());

			auto a_pos = a_beg;
			auto s_pos = s_beg;

			for (; a_pos != a_end && s_pos != s_end; ++a_pos, ++s_pos)
			{
				REQUIRE(a_pos->string() == s_pos->string());
			}

			REQUIRE((a_pos == a_end) == (s_pos == s_end));
		}
	}

	// Trailing separators
	{
		bool const has_trailing_separators =
			a_path.has_relative_path() &&
			path_view::is_separator(a_path.string().back());

		CHECK(a_path.has_trailing_separators() == has_trailing_separators);

		if (has_trailing_separators)
		{
			CHECK(a_path.without_trailing_separators() == a_path.parent_path());
		}
		else
		{
			CHECK(a_path.without_trailing_separators() == a_path);
		}
	}

	// Lexical normalization
	{
		auto const a_normal = lexically_normal(a_path);
		auto const s_normal = s_path.lexically_normal();
		CHECK(a_normal.string() == s_normal.string());

		bool const a_is_lexically_normal = a_normal.string() == a_path.string();
		CHECK(a_is_lexically_normal == a_path.is_lexically_normal());

		bool const s_is_lexically_normal = s_normal.native() == s_path.native();
		CHECK(a_is_lexically_normal == s_is_lexically_normal);

		// Relative comparison
		auto const a_cmp = a_path <=> a_normal;
		auto const s_cmp = s_path <=> s_normal;
		CHECK((a_cmp <=> 0) == (s_cmp <=> 0));

		CHECK(lexically_equivalent(a_path, a_normal));
	}
}

TEST_CASE("path_view standard compliance - combining", "[path_view]")
{
	std::string l_buffer;
	std::string r_buffer;

	if (GENERATE(0, 1)) generate_root(l_buffer);
	if (GENERATE(0, 1)) generate_root(r_buffer);

	if (GENERATE(0, 1)) l_buffer += "foo";
	if (GENERATE(0, 1)) r_buffer += "bar";

	auto const a_l_path = path_view(std::string_view(l_buffer));
	auto const a_r_path = path_view(std::string_view(r_buffer));

	std::filesystem::path const s_l_path(l_buffer);
	std::filesystem::path const s_r_path(r_buffer);

	CAPTURE(l_buffer);
	CAPTURE(r_buffer);

	// Path combining
	{
		auto const combine_result = combine_path(a_l_path, a_r_path);

		size_t const combine_size = combine_result.size();
		REQUIRE(combine_size <= l_buffer.size() + r_buffer.size() + 1);

		std::string combine_buffer;
		combine_buffer.resize(combine_size);

		auto const a_path = combine_result.copy(combine_buffer);
		auto const s_path = s_l_path / s_r_path;

		CHECK(a_path.string() == s_path.string());
	}
}

TEST_CASE("path_view standard compliance - relations", "[path_view]")
{
	// TODO: test comparisons, lexically_relative, etc...
}
