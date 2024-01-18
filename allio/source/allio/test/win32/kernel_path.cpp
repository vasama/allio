#include <allio/impl/win32/kernel_path_impl.hpp>

#include <catch2/catch_all.hpp>

#include <mutex>
#include <string>

using namespace allio;
using namespace allio::win32;

namespace {

using test_handle = int;

class test_mutex
{
	bool m_is_locked = false;

public:
	test_mutex() = default;

	test_mutex(test_mutex const&) = delete;
	test_mutex& operator=(test_mutex const&) = delete;

	void lock()
	{
		REQUIRE(!m_is_locked);
		m_is_locked = true;
	}

	void unlock()
	{
		REQUIRE(m_is_locked);
		m_is_locked = false;
	}
};
using unique_test_lock = std::unique_lock<test_mutex>;

struct test_context
{
	using handle_type = test_handle;
	using unique_lock_type = unique_test_lock;

	test_mutex mutex;
	handle_type current_handle = {};
	std::wstring current_path;

	std::unordered_map<wchar_t, std::wstring> current_path_on_drive;


	unique_lock_type lock()
	{
		return unique_lock_type(mutex);
	}

	detail::kernel_path_impl::current_directory<test_context> get_current_directory(unique_lock_type const& lock)
	{
		REQUIRE(lock.mutex() == &mutex);
		REQUIRE(lock.owns_lock());

		wchar_t const* const beg = current_path.data();
		return { current_handle, { beg, beg + current_path.size() } };
	}

	std::optional<detail::kernel_path_impl::path_section<wchar_t>> get_current_directory_on_drive(unique_lock_type const& lock, wchar_t const drive)
	{
		REQUIRE(lock.mutex() == &mutex);
		REQUIRE(lock.owns_lock());

		if (auto const it = current_path_on_drive.find(std::towupper(drive)); it != current_path_on_drive.end())
		{
			auto const& string = it->second;
			wchar_t const* const data = string.data();
			return detail::kernel_path_impl::path_section<wchar_t>{ data, data + string.size() };
		}

		return std::nullopt;
	}
};

using test_path_parameters = detail::kernel_path_impl::basic_kernel_path_parameters<test_handle>;
using test_path_storage = detail::kernel_path_impl::basic_kernel_path_storage<unique_test_lock>;
using test_path = detail::kernel_path_impl::basic_kernel_path<test_handle>;
using test_converter = detail::kernel_path_impl::basic_kernel_path_converter<test_context>;

} // namespace

template<typename Char, size_t Size>
static Char const(&test_string(char const(&utf8)[Size], wchar_t const(&wide)[Size]))[Size]
{
	if constexpr (std::is_same_v<Char, char>)
	{
		return utf8;
	}
	else
	{
		return wide;
	}
}

#define S(...) (::test_string<char_type>(__VA_ARGS__, L"" __VA_ARGS__))


static vsm::result<test_path> make_test_path(test_context& context, test_path_storage& storage, test_path_parameters const& args)
{
	return test_converter::make_path(context, storage, args);
}

TEMPLATE_TEST_CASE("win32::make_kernel_path", "[win32][kernel_path]", char, wchar_t)
{
	using char_type = TestType;

	test_context context;
	test_path_storage storage;

	std::basic_string_view<char_type> path;

	int handle_expectation = 0;
	std::optional<std::wstring_view> path_expectation;

	#define TEST_ARGS(p, he, pe) \
		SECTION(p) \
		{ \
			path = S(p); \
			handle_expectation = he; \
			path_expectation = pe; \
		}

	SECTION("Absolute paths")
	{
		context.current_path = L"F:\\not\\used";

		TEST_ARGS("X:\\ABC\\DEF",                             0, L"\\??\\X:\\ABC\\DEF");
		TEST_ARGS("X:\\",                                     0, L"\\??\\X:\\");
		TEST_ARGS("X:\\ABC\\",                                0, L"\\??\\X:\\ABC\\");
		TEST_ARGS("X:\\ABC\\DEF. .",                          0, std::nullopt); // Rejected due to trailing dots/spaces.
		TEST_ARGS("X:/ABC/DEF",                               0, L"\\??\\X:\\ABC\\DEF");
		TEST_ARGS("X:\\ABC\\..\\XYZ",                         0, L"\\??\\X:\\XYZ");
		TEST_ARGS("X:\\ABC\\..\\..\\..",                      0, L"\\??\\X:\\");
		TEST_ARGS("\\\\server\\share\\ABC\\DEF",              0, L"\\??\\UNC\\server\\share\\ABC\\DEF");
		TEST_ARGS("\\\\server",                               0, std::nullopt); // Rejected due to missing UNC share name.
		TEST_ARGS("\\\\server\\share",                        0, L"\\??\\UNC\\server\\share");
		TEST_ARGS("\\\\server\\share\\ABC. .",                0, std::nullopt); // Rejected due to trailing dots/spaces.
		TEST_ARGS("//server/share/ABC/DEF",                   0, L"\\??\\UNC\\server\\share\\ABC\\DEF");
		TEST_ARGS("\\\\server\\share\\ABC\\..\\XYZ",          0, L"\\??\\UNC\\server\\share\\XYZ");
		TEST_ARGS("\\\\server\\share\\ABC\\..\\..\\..",       0, L"\\??\\UNC\\server\\share");
		TEST_ARGS("\\\\.\\COM20",                             0, L"\\??\\COM20");
		TEST_ARGS("\\\\.\\pipe\\mypipe",                      0, L"\\??\\pipe\\mypipe");
		TEST_ARGS("\\\\.\\X:\\ABC\\DEF. .",                   0, std::nullopt); // Rejected due to trailing dots/spaces.
		TEST_ARGS("\\\\.\\X:/ABC/DEF",                        0, L"\\??\\X:\\ABC\\DEF");
		TEST_ARGS("\\\\.\\X:\\ABC\\..\\XYZ",                  0, L"\\??\\X:\\XYZ");
		TEST_ARGS("\\\\.\\X:\\ABC\\..\\..\\C:\\",             0, L"\\??\\C:\\");
		TEST_ARGS("\\\\.\\pipe\\mypipe\\..\\notmine",         0, L"\\??\\pipe\\notmine");
		TEST_ARGS("COM1",                                     0, std::nullopt); // Rejected due to device name.
		TEST_ARGS("X:\\COM1",                                 0, std::nullopt); // Rejected due to device name.
		TEST_ARGS("X:COM1",                                   0, std::nullopt); // Rejected due to device name.
		TEST_ARGS("valid\\COM1",                              0, std::nullopt); // Rejected due to device name.
		TEST_ARGS("X:\\notvalid\\COM1",                       0, std::nullopt); // Rejected due to device name.
		TEST_ARGS("X:\\COM1.blah",                            0, std::nullopt); // Rejected due to device name.
		TEST_ARGS("X:\\COM1:blah",                            0, std::nullopt); // Rejected due to device name.
		TEST_ARGS("X:\\COM1  .blah",                          0, std::nullopt); // Rejected due to device name.
		TEST_ARGS("\\\\.\\X:\\COM1",                          0, std::nullopt); // Rejected due to device name.
		TEST_ARGS("\\\\abc\\xyz\\COM1",                       0, std::nullopt); // Rejected due to device name.
		TEST_ARGS("\\\\?\\X:\\ABC\\DEF",                      0, L"\\??\\X:\\ABC\\DEF");
		TEST_ARGS("\\\\?\\X:\\",                              0, L"\\??\\X:\\");
		TEST_ARGS("\\\\?\\X:",                                0, L"\\??\\X:");
		TEST_ARGS("\\\\?\\X:\\COM1",                          0, std::nullopt); // Rejected due to device name.
		TEST_ARGS("\\\\?\\X:\\ABC\\DEF. .",                   0, std::nullopt); // Rejected due to trailing dots/spaces.
		TEST_ARGS("\\\\?\\X:/ABC/DEF",                        0, std::nullopt); // Rejected due to non-canonical separators.
		TEST_ARGS("\\\\?\\X:\\ABC\\..\\XYZ",                  0, std::nullopt); // Rejected due to backtracking.
		TEST_ARGS("\\\\?\\X:\\ABC\\..\\..\\..",               0, std::nullopt); // Rejected due to backtracking.
		TEST_ARGS("\\??\\X:\\ABC\\DEF",                       0, L"\\??\\X:\\ABC\\DEF");
		TEST_ARGS("\\??\\X:\\",                               0, L"\\??\\X:\\");
		TEST_ARGS("\\??\\X:",                                 0, L"\\??\\X:");
		TEST_ARGS("\\??\\X:\\COM1",                           0, L"\\??\\X:\\COM1");
		TEST_ARGS("\\??\\X:\\ABC\\DEF. .",                    0, L"\\??\\X:\\ABC\\DEF. .");
		TEST_ARGS("\\??\\X:/ABC/DEF",                         0, L"\\??\\X:/ABC/DEF");
		TEST_ARGS("\\??\\X:\\ABC\\..\\XYZ",                   0, L"\\??\\X:\\ABC\\..\\XYZ");
		TEST_ARGS("\\??\\X:\\ABC\\..\\..\\..",                0, L"\\??\\X:\\ABC\\..\\..\\..");
	}

	SECTION("Drive relative paths")
	{
		context.current_path = L"X:\\ABC";
		context.current_path_on_drive[L'Y'] = L"Y:\\DEF";

		TEST_ARGS("X:DEF\\GHI",                               0, L"\\??\\X:\\ABC\\DEF\\GHI");
		TEST_ARGS("X:",                                       0, L"\\??\\X:\\ABC");
		TEST_ARGS("X:DEF. .",                                 0, std::nullopt); // Rejected due to trailing dots/spaces.
		TEST_ARGS("Y:",                                       0, L"\\??\\Y:\\DEF");
		TEST_ARGS("Z:",                                       0, L"\\??\\Z:\\");
		TEST_ARGS("X:ABC\\..\\XYZ",                           0, L"\\??\\X:\\ABC\\XYZ");
		TEST_ARGS("X:ABC\\..\\..\\..",                        0, L"\\??\\X:\\");
	}

	SECTION("Rooted paths")
	{
		context.current_path = L"X:\\ABC";

		TEST_ARGS("\\ABC\\DEF",                               0, L"\\??\\X:\\ABC\\DEF");
		TEST_ARGS("\\",                                       0, L"\\??\\X:\\");
		TEST_ARGS("\\ABC\\DEF. .",                            0, std::nullopt); // Rejected due to trailing dots/spaces.
		TEST_ARGS("/ABC/DEF",                                 0, L"\\??\\X:\\ABC\\DEF");
		TEST_ARGS("\\ABC\\..\\XYZ",                           0, L"\\??\\X:\\XYZ");
		TEST_ARGS("\\ABC\\..\\..\\..",                        0, L"\\??\\X:\\");
	}

	SECTION("Relative paths with drive root")
	{
		context.current_path = L"X:\\XYZ";

		TEST_ARGS("ABC\\DEF",                                 0, L"\\??\\X:\\XYZ\\ABC\\DEF");
		TEST_ARGS(".",                                        0, L"\\??\\X:\\XYZ");
		TEST_ARGS("ABC\\DEF. .",                              0, std::nullopt); // Rejected due to trailing dots/spaces.
		TEST_ARGS("ABC\\COM1\\DEF",                           0, std::nullopt); // Rejected due to device name.
		TEST_ARGS("ABC/DEF",                                  0, L"\\??\\X:\\XYZ\\ABC\\DEF");
		TEST_ARGS("..\\ABC",                                  0, L"\\??\\X:\\ABC");
		TEST_ARGS("ABC\\..\\..\\..",                          0, L"\\??\\X:\\");
	}

	SECTION("Relative paths with UNC root")
	{
		context.current_path = L"\\\\server\\share";

		TEST_ARGS("ABC\\DEF",                                 0, L"\\??\\UNC\\server\\share\\ABC\\DEF");
		TEST_ARGS(".",                                        0, L"\\??\\UNC\\server\\share");
		TEST_ARGS("ABC\\DEF. .",                              0, std::nullopt); // Rejected due to trailing dots/spaces.
		TEST_ARGS("ABC\\COM1\\DEF",                           0, std::nullopt); // Rejected due to device name.
		TEST_ARGS("ABC/DEF",                                  0, L"\\??\\UNC\\server\\share\\ABC\\DEF");
		TEST_ARGS("..\\ABC",                                  0, L"\\??\\UNC\\server\\share\\ABC");
		TEST_ARGS("ABC\\..\\..\\..",                          0, L"\\??\\UNC\\server\\share\\");
	}


	CAPTURE(path);

	auto const r = make_test_path(context, storage,
	{
		.path = basic_path_view<char_type>(path),
	});

	REQUIRE(static_cast<bool>(r) == static_cast<bool>(path_expectation));

	if (r)
	{
		CHECK(r->path == *path_expectation);
	}
}


static test_path make_test_path2(
	test_context& context,
	test_path_storage& storage,
	test_path_parameters const& args)
{
	return test_converter::make_path(
		context,
		storage,
		args).value();
}

TEMPLATE_TEST_CASE("win32::make_kernel_path 2", "[win32][kernel_path]", char, wchar_t)
{
	using char_type = TestType;
	using string_type = std::basic_string<char_type>;
	using string_view_type = std::basic_string_view<char_type>;

	test_context context;

	test_path_storage storage;
	auto const make_path = [&](string_view_type const path) -> test_path
	{
		test_path_parameters const args =
		{
			.path = basic_path_view<char_type>(path),
		};

		return test_converter::make_path(context, storage, args).value();
	};

	SECTION("Valid paths")
	{
		context.current_path = L"A:\\B";
		context.current_path_on_drive[L'C'] = L"C:\\D";

		#define CHECK_CONVERSION(from, to) \
			SECTION("Convert '" from "'") \
			{ \
				CHECK(make_path(S(from)).path == std::wstring_view(L"" to)); \
			}

		// Relative
		CHECK_CONVERSION(".",                                   "\\??\\A:\\B");
		CHECK_CONVERSION("./",                                  "\\??\\A:\\B\\");
		CHECK_CONVERSION("Y/Z",                                 "\\??\\A:\\B\\Y\\Z");
		CHECK_CONVERSION("Y//Z",                                "\\??\\A:\\B\\Y\\Z");
		CHECK_CONVERSION("Y/./Z",                               "\\??\\A:\\B\\Y\\Z");
		CHECK_CONVERSION("../Y/Z",                              "\\??\\A:\\Y\\Z");
		CHECK_CONVERSION("Y/../Z",                              "\\??\\A:\\B\\Z");

		// Absolute
		CHECK_CONVERSION("/",                                   "\\??\\A:\\");
		CHECK_CONVERSION("/.",                                  "\\??\\A:\\");
		CHECK_CONVERSION("/..",                                 "\\??\\A:\\");
		CHECK_CONVERSION("/Y/Z",                                "\\??\\A:\\Y\\Z");
		CHECK_CONVERSION("/Y//Z",                               "\\??\\A:\\Y\\Z");
		CHECK_CONVERSION("/Y/./Z",                              "\\??\\A:\\Y\\Z");
		CHECK_CONVERSION("/../Y/Z",                             "\\??\\A:\\Y\\Z");
		CHECK_CONVERSION("/Y/../Z",                             "\\??\\A:\\Z");

		// Drive relative
		CHECK_CONVERSION("C:",                                  "\\??\\C:\\D");
		CHECK_CONVERSION("C:.",                                 "\\??\\C:\\D");
		CHECK_CONVERSION("C:./",                                "\\??\\C:\\D\\");
		CHECK_CONVERSION("C:Y/Z",                               "\\??\\C:\\D\\Y\\Z");
		CHECK_CONVERSION("C:Y//Z",                              "\\??\\C:\\D\\Y\\Z");
		CHECK_CONVERSION("C:Y/./Z",                             "\\??\\C:\\D\\Y\\Z");
		CHECK_CONVERSION("C:../Y/Z",                            "\\??\\C:\\Y\\Z");
		CHECK_CONVERSION("C:Y/../Z",                            "\\??\\C:\\D\\Z");

		// Drive absolute
		CHECK_CONVERSION("A:/",                                 "\\??\\A:\\");
		CHECK_CONVERSION("A:/Y/Z",                              "\\??\\A:\\Y\\Z");
		CHECK_CONVERSION("A:/Y//Z",                             "\\??\\A:\\Y\\Z");
		CHECK_CONVERSION("A:/Y/./Z",                            "\\??\\A:\\Y\\Z");
		CHECK_CONVERSION("A:/../Y/Z",                           "\\??\\A:\\Y\\Z");
		CHECK_CONVERSION("A:/Y/../Z",                           "\\??\\A:\\Z");

		// UNC
		CHECK_CONVERSION("//server/share",                      "\\??\\UNC\\server\\share");
		CHECK_CONVERSION("//server/share/",                     "\\??\\UNC\\server\\share\\");
		CHECK_CONVERSION("//server/share/Y/Z",                  "\\??\\UNC\\server\\share\\Y\\Z");
		CHECK_CONVERSION("//server/share/Y/./Z",                "\\??\\UNC\\server\\share\\Y\\Z");
		CHECK_CONVERSION("//server/share/../Y/Z",               "\\??\\UNC\\server\\share\\Y\\Z");
		CHECK_CONVERSION("//server/share/Y/../Z",               "\\??\\UNC\\server\\share\\Z");

		// Local device
		CHECK_CONVERSION("\\\\.\\",                             "\\??\\");
		CHECK_CONVERSION("\\\\.\\Y/Z",                          "\\??\\Y\\Z");
		CHECK_CONVERSION("\\\\.\\Y//Z",                         "\\??\\Y\\Z");
		CHECK_CONVERSION("\\\\.\\Y/./Z",                        "\\??\\Y\\Z");
		CHECK_CONVERSION("\\\\.\\../Y/Z",                       "\\??\\Y\\Z");
		CHECK_CONVERSION("\\\\.\\Y/../Z",                       "\\??\\Z");
		CHECK_CONVERSION("\\\\.\\X:/../Z",                      "\\??\\Z");

		// Root local device
		CHECK_CONVERSION("\\\\?\\",                             "\\??\\");
		CHECK_CONVERSION("\\\\?\\Y\\Z",                         "\\??\\Y\\Z");

		// NT
		CHECK_CONVERSION("\\??\\X:/./Y/../Z",                   "\\??\\X:/./Y/../Z");

		#undef CHECK_CONVERSION
	}

	SECTION("Miscellaneous invalid paths")
	{
		// UNC paths without share names are rejected:
		REQUIRE_THROWS(make_path(S("\\\\server")));

		// Non-canonical root local device paths are rejected:
		REQUIRE_THROWS(make_path(S("\\\\?\\A/B")));
		REQUIRE_THROWS(make_path(S("\\\\?\\A\\.\\B")));
		REQUIRE_THROWS(make_path(S("\\\\?\\A\\..\\B")));

		// NT paths without relative components are rejected:
		REQUIRE_THROWS(make_path(S("\\??\\")));
	}

	SECTION("Paths containing special device names")
	{
		string_type path;

		path += GENERATE(
			as<string_view_type>()
			, S("")
			, S("\\\\.\\")
			, S("\\\\?\\")
		);

		path += GENERATE(
			as<string_view_type>()
			, S("AUX")
			, S("PRN")
			, S("NUL")
			, S("COM*")
			, S("LPT*")
			, S("CON")
			, S("CONIN$")
			, S("CONOUT$")
		);

		if (path.ends_with(char_type('*')))
		{
			path.back() = char_type('0') + GENERATE(range(0, 10));
		}

		path += GENERATE(
			as<string_view_type>()
			, S("")
			, S(".txt")
			, S(":txt")
			, S(" .txt")
			, S(" :txt")
		);

		CAPTURE(path);

		// All win32 paths containing device names are rejected.
		REQUIRE_THROWS(make_path(path));
	}

	SECTION("Paths containing trailing dots or spaces")
	{
		string_type path;

		path += GENERATE(
			as<string_view_type>()
			, S("")
			, S("X:\\")
			, S("\\\\.\\")
			, S("\\\\?\\")
		);

		path += S("y");
		path += GENERATE(
			as<string_view_type>()
			, S(".")
			, S(". ")
			, S(". .")
			, S(". . ")
			, S(" ")
			, S(" .")
			, S(" . ")
			, S(" . .")
			, S(" . . ")
		);

		if (GENERATE(0, 1))
		{
			path += S("\\z");
		}

		CAPTURE(path);

		// All win32 paths containing trailing dots and/or
		// spaces on any path component are rejected.
		REQUIRE_THROWS(make_path(path));
	}

	SECTION("Local device current directory")
	{
		context.current_path = L"\\\\.\\C:\\";

		REQUIRE_NOTHROW(make_path(S(".")));
		REQUIRE_NOTHROW(make_path(S("Y/..")));
		REQUIRE_NOTHROW(make_path(S("D:\\")));
		REQUIRE_NOTHROW(make_path(S("\\\\server\\share")));
		REQUIRE_NOTHROW(make_path(S("\\\\.\\D:\\")));
		REQUIRE_NOTHROW(make_path(S("\\\\?\\D:\\")));
		REQUIRE_NOTHROW(make_path(S("\\??\\D:\\")));

		REQUIRE_THROWS(make_path(S("\\")));
		REQUIRE_THROWS(make_path(S("C:")));
		REQUIRE_THROWS(make_path(S("..")));
		REQUIRE_THROWS(make_path(S("Y/../..")));
	}

	SECTION("Invalid drive current directory")
	{
		context.current_path = L"C:\\";
		context.current_path_on_drive[L'D'] = L"E:\\";

		REQUIRE_THROWS(make_path(S("D:")));
		REQUIRE_THROWS(make_path(S("D:Y")));

		REQUIRE_NOTHROW(make_path(S("D:\\")));
	}
}

#if 0
TEMPLATE_TEST_CASE("win32::make_kernel_path 2", "[win32][kernel_path]", char, wchar_t)
{
	using char_type = TestType;
	using string_type = std::basic_string<char_type>;
	using string_view_type = std::basic_string_view<char_type>;

	test_context context;
	test_path_storage storage;

	struct root_path_pair
	{
		string_view_type win32_root;
		string_view_type kernel_root;
	};

	auto const root = GENERATE(
		as<root_path_pair>()
		, root_path_pair{ S(""),                        S("\\??") }
		, root_path_pair{ S("X:"),                      S("\\??\\X:") }
		, root_path_pair{ S("Y:"),                      S("\\??\\Y:") }
		, root_path_pair{ S("\\\\server\\share"),       S("\\??\\UNC\\server\\share") }
		, root_path_pair{ S("\\\\."),                   S("\\??") }
		, root_path_pair{ S("\\\\?"),                   S("\\??") }
		, root_path_pair{ S("\\??"),                    S("\\??") }
	);

	std::vector<string_view_type> win32_segments;
	std::vector<string_view_type> kernel_segments;
	bool contains_device_name = false;

	if (root.starts_with(S("\\")) && GENERATE(0, 1))
	{
		segments.push_back(S("X:"));
	}

	for (int i = 0, c = GENERATE(0, 1, 2); i < c; ++i)
	{
		auto segment = GENERATE(
			as<string_view_type>()
			, S("")
			, S(".")
			, S("..")
			, S("ABC")
			, S("COM1")
			, S("COM10")
		);

		win32_segments.push_back(segment);

		if (segment == S("") || segment == S("."))
		{
		}
		else if (segment == S(".."))
		{
			if (!kernel_segments.empty())
			{
				kernel_segments.pop_back();
			}
		}
		else
		{
			kernel_segments.push_back(segment);
		}

		if (segment == S("COM1"))
		{
			contains_device_name = true;
		}
	}

	if (!segments.empty())
	{
		auto const suffix = GENERATE(
			as<string_view_type>()
			, S("")
			, S(".")
			, S(".ext")
		);

		segments.back() += suffix;
	}

	auto const join = [](std::span<string_view_type const> const segments) -> string_type
	{
		string_type s;
		for (string_view_type const segment : segments)
		{
			if (!s.empty())
			{
				s += S("\\");
			}
			s += segment;
		}
		return s;
	};


	string_type win32_path = string_type(root.win32_root);

	if (!win32_segments.empty())
	{
		if ()

		win32_path += join(win32_segments);
	}


	string_type kernel_path = string_type(root.kernel_root);

	if (!kernel_segments.empty())
	{
		kernel_path += S("\\");
		kernel_path += join(kernel_segments);
	}
}
#endif
