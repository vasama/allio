#include <allio/path_view.hpp>

#include "reverse_iterator.hpp"

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
			CHECK(--next == prev);
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
		CHECK(++next == prev);
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

template<std::input_iterator Iterator>
class checked_range
{
	Iterator m_beg;
	Iterator m_end;

public:
	checked_range(std::ranges::input_range auto&& range)
		: m_beg(std::ranges::begin(range))
		, m_end(std::ranges::end(range))
	{
	}

	checked_iterator<Iterator> begin() const
	{
		return checked_iterator<Iterator>(m_beg);
	}

	checked_iterator<Iterator> end() const
	{
		return checked_iterator<Iterator>(m_end);
	}

	checked_iterator<reverse_iterator<Iterator>> rbegin() const
	{
		return checked_iterator<reverse_iterator<Iterator>>(m_end);
	}

	checked_iterator<reverse_iterator<Iterator>> rend() const
	{
		return checked_iterator<reverse_iterator<Iterator>>(m_beg);
	}
};

template<std::ranges::input_range Range>
checked_range(Range&&) -> checked_range<std::ranges::iterator_t<Range>>;

} // namespace


static void generate_separator(std::string& buffer, size_t const max_separators = 2)
{
	for (size_t i = 0, c = GENERATE_COPY(range(static_cast<size_t>(0), max_separators)); i <= c; ++i)
	{
		char separator = '/';

#if allio_detail_WIN32
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
#if allio_detail_WIN32
	if (GENERATE(0, 1))
	{
		buffer += GENERATE(
			as<std::string_view>{},
			"C:",
			"\\??",
			"\\\\.",
			"\\\\?",
			"\\\\server"
		);

		if (GENERATE(0, 1))
		{
			return;
		}
	}
#endif

	generate_separator(buffer);
}

static void generate_relative(std::string& buffer, size_t const max_components = 2, size_t const max_separators = 2)
{
	for (size_t i = 0, c = GENERATE_COPY(range(static_cast<size_t>(0), max_components)); i <= c; ++i)
	{
		if (i > 0)
		{
			generate_separator(buffer, max_separators);
		}

		buffer += GENERATE(
			as<std::string_view>{},
			".",
			"..",
			"name",
			".hidden",
			"dot.",
			"file.ext",
			"some.win32.cpp"
		);
	}
}


TEST_CASE("Standard compliance 1", "[path_view]")
{
	std::string buffer;

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

	CAPTURE(buffer);


	path_view const a_path = path_view(std::string_view(buffer));
	std::filesystem::path const s_path(buffer);
	std::string const& s_path_string = s_path.string();


#ifdef __GLIBCXX__
	//TODO: Constrain this workaround only to earlier versions once libstdc++ is fixed.
	// libstdc++ incorrectly handles paths starting with multiple slashes.

	bool const libstdcxx_long_root = buffer.starts_with("//");
	bool const libstdcxx_only_root = buffer.find_first_not_of("/") == std::string::npos;

#	define allio_LIBSTDCXX_WORKAROUND_PROLOGUE(...) if (!(__VA_ARGS__)) {
#	define allio_LIBSTDCXX_WORKAROUND_EPILOGUE }
#else
#	define allio_LIBSTDCXX_WORKAROUND_PROLOGUE(...)
#	define allio_LIBSTDCXX_WORKAROUND_EPILOGUE
#endif


	/* Path properties. */ {
#define allio_PROPERTY(property) \
	CHECK(a_path.property() == s_path.property())

		allio_PROPERTY(is_absolute);
		allio_PROPERTY(is_relative);
#undef allio_PROPERTY

#define allio_PROPERTY(property) \
	CHECK(a_path.property().string() == s_path.property().string())

		allio_PROPERTY(root_path);
		allio_PROPERTY(root_directory);
		allio_PROPERTY(root_name);
		allio_PROPERTY(relative_path);
		allio_LIBSTDCXX_WORKAROUND_PROLOGUE(libstdcxx_long_root && !libstdcxx_only_root)
		allio_PROPERTY(parent_path);
		allio_LIBSTDCXX_WORKAROUND_EPILOGUE
		allio_PROPERTY(filename);
		allio_PROPERTY(stem);
		allio_PROPERTY(extension);
#undef allio_PROPERTY
	}

	allio_LIBSTDCXX_WORKAROUND_PROLOGUE(libstdcxx_long_root && libstdcxx_only_root)
	/* Iteration. */ {
		auto const predicate = [](path_view const lhs, std::filesystem::path const& rhs)
		{
			return lhs.string() == rhs.string();
		};

		checked_range const a_range = checked_range(a_path);

		CHECK(std::ranges::equal(a_range, s_path, predicate));
		CHECK(std::ranges::equal(reverse_range(a_range), reverse_range(s_path), predicate));
	}
	allio_LIBSTDCXX_WORKAROUND_EPILOGUE

	/* Trailing separators. */ {
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

#if 0
	/* Lexical normalization. */ {
		std::string normal_buffer;

		normal_buffer.resize(a_path.string().size());
		normal_buffer.resize(a_path.render_lexically_normal(normal_buffer.data()).string().size());

		path_view const a_normal = path_view(normal_buffer);
		std::filesystem::path const s_normal = s_path.lexically_normal();
		std::string const& s_normal_string = s_normal.string();

		bool const a_is_lexically_normal = a_normal == a_path;
		CHECK(a_path.is_lexically_normal() == a_is_lexically_normal);

		bool const s_is_lexically_normal = s_normal_string == s_path_string;
		CHECK(a_is_lexically_normal == s_is_lexically_normal);

		/* Relative comparison. */ {
			int const a_cmp = a_path.compare(a_normal);
			int const s_cmp = s_path.compare(s_normal);
			CHECK((a_cmp <=> 0) == (s_cmp <=> 0));
		}

		CHECK((lexically_equivalent(a_path, a_normal)
#if allio_detail_WIN32
			// On Windows some likely invalid paths beginning with "\??\\" are
			// coincidentally normalized to valid NT paths beginning with "\??\".
			|| (a_normal.root_name().string() == "\\??" && !a_path.has_root_name())
#endif
		));
	}
#endif
}

TEST_CASE("Standard compliance 2", "[path_view]")
{
	std::string l_buffer;
	std::string r_buffer;

	if (GENERATE(0, 1)) generate_root(l_buffer);
	if (GENERATE(0, 1)) generate_root(r_buffer);

	if (GENERATE(0, 1)) l_buffer += "foo";
	if (GENERATE(0, 1)) r_buffer += "bar";

	path_view const a_l_path = path_view(std::string_view(l_buffer));
	path_view const a_r_path = path_view(std::string_view(r_buffer));

	std::filesystem::path const s_l_path(l_buffer);
	std::filesystem::path const s_r_path(r_buffer);

	CAPTURE(l_buffer);
	CAPTURE(r_buffer);

	/* Combine. */ {
		path_combine_result const combine_result = combine_path(a_l_path, a_r_path);

		size_t const combine_size = combine_result.size();
		REQUIRE(combine_size <= l_buffer.size() + r_buffer.size() + 1);

		std::string combine_buffer;
		combine_buffer.resize(combine_size);

		path_view const a_path = combine_result.copy(combine_buffer.data());
		std::filesystem::path const s_path = s_l_path / s_r_path;

		CHECK(a_path.string() == s_path.string());
	}
}
