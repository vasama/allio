#include <allio/path_discovery.hpp>

#include <win32.h>

using namespace allio;
using namespace allio::detail;

#if 0
class environment_block
{
	wchar_t const* m_environment;

	class sentinel {};

	class iterator
	{
		wchar_t const* m_variable;
		wchar_t const* m_value;
		wchar_t const* m_end;

	public:
		using value_type = std::pair<std::wstring_view, std::wstring_view>;

		explicit iterator(wchar_t const* const variable)
		{
			set(variable);
		}

		[[nodiscard]] value_type operator*() const
		{
			return value_type
			{
				std::wstring_view(m_variable, m_value - 1),
				std::wstring_view(m_value, m_end),
			};
		}
		
		[[nodiscard]] vsm::box<value_type> operator->() const
		{
			return value_type
			{
				std::wstring_view(m_variable, m_value - 1),
				std::wstring_view(m_value, m_end),
			};
		}

		iterator& operator++() &
		{
			set(m_end + 1);
			return *this;
		}

		[[nodiscard]] iterator operator++(int) &
		{
			auto const r = *this;
			set(m_end + 1);
			return r;
		}

		[[nodiscard]] friend bool operator==(iterator const& lhs, iterator const& rhs)
		{
			return lhs.m_variable == rhs.m_variable;
		}

		[[nodiscard]] friend bool operator==(iterator const& lhs, sentinel)
		{
			return *lhs.m_variable == L'\0';
		}

	private:
		void set(wchar_t const* const variable)
		{
			m_variable = variable;
			if (*variable != L'\0')
			{
				m_end = variable + std::wcslen(variable);
				auto const variable_end = std::find(m_variable, m_end, '=');
				m_value = variable_end != m_end ? variable_end + 1 : m_end;
			}
		}
	};

	static_assert(std::forward_iterator<iterator>);
	static_assert(std::sentinel_for<sentinel, iterator>);

public:
	explicit environment_block(wchar_t const* const environment)
		: m_environment(environment)
	{
	}

	[[nodiscard]] iterator begin() const
	{
		return iterator(m_environment);
	}

	[[nodiscard]] sentinel end() const
	{
		return {};
	}
};

static environment_block get_environment_block(
	vsm::no_cvref_of<unique_peb_lock> auto const&& lock)
{
	vsm_assert(lock.owns_lock());
	return environment_block(NtCurrentPeb()->ProcessParameters->Environment);
}
#endif

#if 0
static blocking::directory_handle open_temporary_directory()
{
	using namespace path_literals;

	struct candidate_type
	{
		std::wstring_view variable;
		platform_path_view relative_path;
		platform_path_view root_path;
	};

	candidate_type candidates[] =
	{
		{ L"TMP",                 L""_path },
		{ L"TEMP",                L""_path },
		{ L"LOCALAPPDATA",        L"Temp"_path },
		{ L"USERPROFILE",         L"AppData\\Local\\Temp"_path },
	};

	auto const find_candidate = [&](std::wstring_view const variable) -> candidate_type*
	{
		auto const beg = candidates;
		auto const end = candidates + std::size(candidates);
	
		auto const it = std::find_if(candidates, [&](auto const& candidate)
		{
			return variable == candidate.variable;
		});
		
		return it != end ? it : nullptr;
	};

	unique_peb_lock const peb_lock;
	for (auto const variable : get_environment_block(peb_lock))
	{
		if (auto const candidate = find_candidate(variable.first))
		{
			candidate->root_path = platform_path_view(variable.second);
		}
	}

	dynamic_buffer<wchar_t, MAX_PATH> storage;
	for (candidate_type const& candidate : candidates)
	{
		if (candidate.root_path.empty())
		{
			continue;
		}

		auto const combined = combine_path(candidate.root_path, candidate.relative_path);
		auto const combined_buffer = storage.reserve(combined.size());
		
		if (!combined_buffer)
		{
			continue;
		}

		auto const directory = blocking::open_directory(combined.copy(combined_buffer));

		if (directory)
		{
			return directory;
		}
	}

	return {};
}
#endif

namespace {

class temporary_directory
{
	wchar_t m_path_data[MAX_PATH + 1];
	uint16_t m_path_size = 0;

	blocking::directory_handle m_directory;

public:
	static platform_path_view path()
	{
		auto const& self = get();
		return platform_path_view(self.m_path_data, self.m_path_size);
	}

	static blocking::directory_handle const& directory()
	{
		auto const& self = get();
		return self.m_directory;
	}

private:
	temporary_directory()
	{
		if (size_t const size = get_temp_path(m_path_data))
		{
			vsm_assert(size <= std::numeric_limits<uint16_t>::max());
			auto const path = platform_path_view(m_path_data, size);
			
			if (auto const r = blocking::open_directory(path))
			{
				m_path_size = static_cast<uint16_t>(size);
				m_directory = vsm_move(*r);
			}
		}
	}

	static temporary_directory const& get()
	{
		static temporary_directory const info;
		return info;
	}

	static size_t get_temp_path(std::span<wchar_t> const buffer)
	{
		HANDLE const kernel32 = GetModuleHandleW(L"KERNEL32.DLL");
		vsm_assert(kernel32 != NULL);
	
		auto const new_get = GetProcAddress(kernel32, L"GetTempPath2W");
		auto const get = new_get == nullptr
			? &GetTempPathW
			: reinterpret_cast<decltype(&GetTempPathW)>(new_get);
	
		return get(buffer.data(), static_cast<DWORD>(buffer.size()));
	}
};

} // namespace

platform_path_view temporary_directory_path()
{
	return temporary_directory::path();
}

blocking::directory_handle const& paths::temporary_directory()
{
	return temporary_directory::directory();
}
