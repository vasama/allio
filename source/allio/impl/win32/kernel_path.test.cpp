#include <allio/impl/win32/kernel_path_impl.hpp>

#include <catch2/catch_all.hpp>

#include <mutex>

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

		if (auto const it = current_path_on_drive.find(std::toupper(drive)); it != current_path_on_drive.end())
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

#define TEST_STRING(...) (::test_string<Char>(__VA_ARGS__, L"" __VA_ARGS__))


static result<test_path> make_test_path(test_context& context, test_path_storage& storage, test_path_parameters const& args)
{
	return detail::kernel_path_impl::basic_kernel_path_converter<test_context>::make_path(context, storage, args);
}

TEMPLATE_TEST_CASE("win32::make_kernel_path", "[win32][kernel_path]", char, wchar_t)
{
	using Char = TestType;

	test_context context;
	test_path_storage storage;

	std::basic_string_view<Char> path;

	int handle_expectation = 0;
	std::optional<std::wstring_view> path_expectation;

	#define TEST_ARGS(p, he, pe) \
		SECTION(p) \
		{ \
			path = TEST_STRING(p); \
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

	SECTION("Relative paths")
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
		.path = basic_path_view<Char>(path),
	});

	REQUIRE(static_cast<bool>(r) == static_cast<bool>(path_expectation));

	if (r)
	{
		CHECK(r->path == *path_expectation);
	}
}
