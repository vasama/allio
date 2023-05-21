#pragma once

#include <allio/input_path_view.hpp>
#include <allio/win32/detail/win32_fwd.hpp>

#include <allio/impl/win32/encoding.hpp>
#include <allio/impl/win32/kernel.hpp>

#include <vsm/linear.hpp>

#include <memory>
#include <optional>

#include <cwctype>

namespace allio {
namespace detail::kernel_path_impl {

using namespace win32;
using namespace path_impl;

template<typename Context>
struct basic_kernel_path_parameters
{
	typename Context::handle_type handle = {};
	input_path_view path;

	// Use full path instead of handle and include a null terminator.
	bool win32_api_form = false;
};

template<typename Context>
struct basic_kernel_path
{
	typename Context::handle_type handle;
	std::wstring_view path;
};

template<typename Context>
class basic_kernel_path_converter;

template<typename Context>
class basic_kernel_path_storage
{
	static constexpr size_t small_path_size = 272;
	static constexpr size_t max_root_size = 8;

	std::unique_ptr<wchar_t[]> m_dynamic;
	wchar_t m_static[small_path_size];
	wchar_t m_root[max_root_size];

	typename Context::unique_lock_type m_lock;

public:
	basic_kernel_path_storage() = default;
	basic_kernel_path_storage(basic_kernel_path_storage const&) = delete;
	basic_kernel_path_storage& operator=(basic_kernel_path_storage const&) = delete;

private:
	friend class basic_kernel_path_converter<Context>;
};

template<typename Char>
struct path_section
{
	Char const* beg;
	Char const* end;
};

template<typename Context>
struct current_directory
{
	typename Context::handle_type handle;
	path_section<wchar_t> path;
};

template<typename Context>
class basic_kernel_path_converter
{
	using handle_type = typename Context::handle_type;

	static constexpr size_t max_path_size = 0x7fff;

	Context& m_context;
	typename Context::unique_lock_type m_lock;

	basic_kernel_path_storage<Context>& m_storage;
	handle_type m_handle;

	bool m_win32_api_form = false;
	bool m_reject_local_device_current_directory = false;

	wchar_t* m_buffer_beg = nullptr;
	wchar_t* m_buffer_pos = nullptr;
	wchar_t* m_buffer_end = nullptr;

public:
	basic_kernel_path_converter(basic_kernel_path_converter const&) = delete;
	basic_kernel_path_converter& operator=(basic_kernel_path_converter const&) = delete;

	static vsm::result<basic_kernel_path<Context>> make_path(
		basic_kernel_path_storage<Context>& storage,
		basic_kernel_path_parameters<Context> const& args, Context& context)
	{
		// Win32 API form does not allow a directory handle.
		vsm_assert(!(args.win32_api_form && args.handle));

		if (args.path.is_too_long())
		{
			return std::unexpected(error::filename_too_long);
		}

		if (args.path.empty())
		{
			return std::unexpected(error::invalid_path);
		}

		basic_kernel_path_converter c(storage, args, context);
		vsm_try_void(c.make_path(args.path));

		storage.m_lock = static_cast<typename Context::unique_lock_type&&>(c.m_lock);
		return basic_kernel_path<Context>{ c.m_handle, { c.m_buffer_pos, c.m_buffer_end } };
	}

private:
	explicit basic_kernel_path_converter(
		basic_kernel_path_storage<Context>& storage,
		basic_kernel_path_parameters<Context> const& args, Context& context)
		: m_context(context)
		, m_storage(storage)
		, m_handle(args.handle)
		, m_win32_api_form(args.win32_api_form)
	{
	}


	vsm::result<wchar_t*> resize_buffer(size_t const new_data_size)
	{
		if (m_storage.m_dynamic != nullptr)
		{
			return std::unexpected(error::filename_too_long);
		}

		wchar_t const* const old_beg = m_buffer_pos;
		wchar_t const* const old_end = m_buffer_end;
		size_t const old_data_size = old_end - old_beg;

		wchar_t* new_buffer_beg;
		wchar_t* new_buffer_end;

		if (old_data_size + new_data_size <= m_storage.small_path_size)
		{
			new_buffer_beg = m_storage.m_static;
			new_buffer_end = new_buffer_beg + m_storage.small_path_size;
		}
		else
		{
			new_buffer_beg = new (std::nothrow) wchar_t[max_path_size];

			if (new_buffer_beg == nullptr)
			{
				return std::unexpected(error::not_enough_memory);
			}

			m_storage.m_dynamic.reset(new_buffer_beg);

			new_buffer_end = new_buffer_beg + max_path_size;
		}

		wchar_t* const old_data_beg = new_buffer_end - old_data_size;
		memcpy(old_data_beg, old_beg, old_data_size * sizeof(wchar_t));

		wchar_t* const new_data_beg = old_data_beg - new_data_size;

		m_buffer_beg = new_buffer_beg;
		m_buffer_pos = new_data_beg;
		m_buffer_end = new_buffer_end;

		return new_data_beg;
	}

	vsm::result<void> push_literal(char const* const beg, char const* const end)
	{
		if (size_t const utf8_size = end - beg)
		{
			std::string_view const utf8_string = std::string_view(beg, utf8_size);

			vsm_try(wide_size, utf8_to_wide(utf8_string));

			wchar_t* const buffer_beg = m_buffer_beg;
			wchar_t* const buffer_end = m_buffer_pos;

			wchar_t* new_buffer_end;

			if (wide_size <= buffer_end - buffer_beg)
			{
				m_buffer_pos = new_buffer_end = buffer_end - wide_size;
			}
			else
			{
				vsm_try_assign(new_buffer_end, resize_buffer(wide_size));
			}

			vsm_verify(utf8_to_wide(utf8_string, std::span(new_buffer_end, wide_size)));
		}
		return {};
	}

	vsm::result<void> push_literal(wchar_t const* const beg, wchar_t const* const end)
	{
		if (size_t const size = end - beg)
		{
			wchar_t* const buffer_beg = m_buffer_beg;
			wchar_t* const buffer_end = m_buffer_pos;

			if (size <= buffer_end - buffer_beg)
			{
				wchar_t* const new_buffer_end = buffer_end - size;
				memcpy(new_buffer_end, beg, size * sizeof(wchar_t));
				m_buffer_pos = new_buffer_end;
			}
			else
			{
				if (buffer_beg == m_buffer_end)
				{
					m_buffer_beg = const_cast<wchar_t*>(beg);
					m_buffer_pos = const_cast<wchar_t*>(beg);
					m_buffer_end = const_cast<wchar_t*>(end);
				}
				else
				{
					vsm_try(new_buffer_end, resize_buffer(size));
					memcpy(new_buffer_end, beg, size * sizeof(wchar_t));
				}
			}
		}
		return {};
	}

	template<typename Char>
	vsm::result<void> push_literal(std::basic_string_view<Char> const string)
	{
		Char const* const beg = string.data();
		return push_literal(beg, beg + string.size());
	}


	template<typename Char>
	bool is_untrimmed_name(Char const* const beg, Char const* const end)
	{
		vsm_assert(beg != end);
		return end[-1] == ' ' || end[-1] == '.';
	}

	template<typename Char>
	bool is_device_name(Char const* beg, Char const* const end)
	{
		auto const consume = [&](std::string_view const string) -> bool
		{
			Char const* pos = beg;
			for (char const upper : string)
			{
				if (pos == end)
				{
					return false;
				}

				char const lower = upper != '$' ? upper + 0x20 : upper;
				if (Char const input = *pos++; input != upper && input != lower)
				{
					return false;
				}
			}
			beg = pos;
			return true;
		};

		auto const consume_device = [&]() -> bool
		{
			if (size_t const size = end - beg; size >= 3)
			{
				if (consume("AUX") || consume("PRN") || consume("NUL"))
				{
					return true;
				}

				if (consume("LPT") || consume("COM"))
				{
					bool const is_digit = std::iswdigit(*beg);
					beg += is_digit;
					return is_digit;
				}

				if (consume("CON"))
				{
					return consume("IN$") || consume("OUT$") || true;
				}
			}

			return false;
		};

		return consume_device() && (beg == end || *beg == ' ' || *beg == '.' || *beg == ':');
	}

	template<typename Char>
	vsm::result<size_t> push_canonical_impl(Char const* const beg, Char const* end, size_t backtrack, auto const push_literal)
	{
		bool need_separator = m_buffer_pos != m_buffer_end && *m_buffer_pos != Char('\\');

		Char const* flush_end = end;
		auto const flush = [&](Char const* const flush_beg, Char const* const new_flush_end) -> vsm::result<void>
		{
			if (need_separator && flush_beg != flush_end && flush_end[-1] != Char('\\'))
			{
				wchar_t const* const separator = L"\\";
				vsm_try_void(push_literal(separator, separator + 1));
				need_separator = false;
			}

			return push_literal(flush_beg, std::exchange(flush_end, new_flush_end));
		};

		while (beg != end)
		{
			Char const* const separator_end = end;
			while (beg != end && is_separator(end[-1]))
			{
				--end;
			}
			Char const* const separator_beg = end;

			Char const* const segment_end = end;
			while (beg != end && !is_separator(end[-1]))
			{
				--end;
			}
			Char const* const segment_beg = end;

			if (segment_beg == segment_end)
			{
				break;
			}

			if (segment_beg[0] == '.')
			{
				size_t const segment_size = segment_end - segment_beg;

				if (segment_size == 1)
				{
					vsm_try_void(flush(separator_end, segment_beg));
					continue;
				}

				if (segment_size == 2 && segment_beg[1] == '.')
				{
					++backtrack;
					vsm_try_void(flush(separator_end, segment_beg));
					continue;
				}
			}

			if (is_untrimmed_name(segment_beg, segment_end) || is_device_name(segment_beg, segment_end))
			{
				return std::unexpected(error::invalid_path);
			}

			if (backtrack != 0)
			{
				--backtrack;
				flush_end = segment_beg;
				continue;
			}

			if (size_t const separator_size = separator_end - separator_beg)
			{
				if (separator_size > 1 || *separator_beg == Char('/'))
				{
					vsm_try_void(flush(separator_end, segment_end));

					wchar_t const* const separator = L"\\";
					vsm_try_void(push_literal(separator, separator + 1));
				}
			}
		}

		vsm_try_void(flush(beg, beg));

		return backtrack;
	}

	template<typename Char>
	vsm::result<size_t> push_canonical(Char const* const beg, Char const* const end, size_t backtrack = 0)
	{
		return push_canonical_impl(beg, end, backtrack,
			[this]<typename C>(C const* const beg, C const* const end) -> vsm::result<void>
			{
				return push_literal(beg, end);
			}
		);
	}

	template<typename Char>
	vsm::result<void> push_canonical_literal(Char const* const beg, Char const* const end)
	{
		bool first_segment = true;
		vsm_try_discard(push_canonical_impl(beg, end, 0,
			[this, &first_segment]<typename C>(C const* const beg, C const* const end) -> vsm::result<void>
			{
				if (first_segment)
				{
					first_segment = false;
					return push_literal(beg, end);
				}
				return std::unexpected(error::invalid_path);
			}
		));
		return {};
	}

	path_section<wchar_t> store_drive_root(wchar_t const drive)
	{
		static constexpr std::wstring_view root_template = L"\\??\\X:\\";
		static constexpr size_t drive_offset = root_template.find(L'X');

		wchar_t* root_buffer = m_storage.m_root;
		memcpy(root_buffer, root_template.data(), root_template.size() * sizeof(wchar_t));
		root_buffer[drive_offset] = drive;

		return { root_buffer, root_buffer + root_template.size() };
	}

	vsm::result<void> push_drive_root(wchar_t const drive)
	{
		path_section<wchar_t> const root = store_drive_root(drive);
		return push_literal(root.beg, root.end);
	}

	struct current_path
	{
		wchar_t drive;
		bool requires_trailing_separator;
		path_section<wchar_t> root;
		path_section<wchar_t> dyn_root;
		path_section<wchar_t> relative;
	};

	vsm::result<current_path> classify_current_path(path_section<wchar_t> const path)
	{
		wchar_t const* beg = path.beg;
		wchar_t const* const end = path.end;

		if (beg == end)
		{
			return std::unexpected(error::invalid_current_directory);
		}

		if (is_separator(*beg))
		{
			if (++beg == end || !is_separator(*beg) || ++beg == end)
			{
				return std::unexpected(error::invalid_current_directory);
			}

			if (*beg == '.' || *beg == '?')
			{
				if (++beg == end || !is_separator(*beg))
				{
					return std::unexpected(error::invalid_current_directory);
				}

				if (m_reject_local_device_current_directory)
				{
					return std::unexpected(error::invalid_current_directory);
				}

				wchar_t const* const root = L"\\??\\";
				return current_path
				{
					.root = { root, root + 4 },
					.relative = { beg + 1, end },
				};
			}

			if (is_separator(*beg))
			{
				return std::unexpected(error::invalid_current_directory);
			}

			wchar_t const* const server_beg = beg;
			wchar_t const* const server_end = beg = find_separator(beg, end);

			if (beg == end || ++beg == end || is_separator(*beg))
			{
				return std::unexpected(error::invalid_current_directory);
			}

			wchar_t const* const share_beg = beg;
			wchar_t const* const share_end = beg = find_separator(beg, end);

			wchar_t const* const relative_beg = skip_separators(beg, end);
			bool const has_trailing_separator = share_end != relative_beg;

			wchar_t const* root = L"\\??\\UNC\\";
			return current_path
			{
				.requires_trailing_separator = !has_trailing_separator,
				.root = { root, root + 8 },
				.dyn_root = { server_beg, share_end + has_trailing_separator },
				.relative = { relative_beg, end },
			};
		}

		if (has_drive_letter(beg, end))
		{
			wchar_t const drive = *beg++;

			if (++beg == end || !is_separator(*beg))
			{
				return std::unexpected(error::invalid_current_directory);
			}

			return current_path
			{
				.drive = drive,
				.root = store_drive_root(drive),
				.relative = { beg + 1, end },
			};
		}

		return std::unexpected(error::invalid_current_directory);
	}

	static vsm::result<wchar_t> detect_current_drive(path_section<wchar_t> const path)
	{
		wchar_t const* beg = path.beg;
		wchar_t const* const end = path.end;

		if (end - beg >= 4 &&
			is_separator(beg[0]) &&
			is_separator(beg[1]) &&
			(beg[2] == '.' || beg[2] == '?') &&
			is_separator(beg[3]))
		{
			beg += 4;
		}

		if (has_drive_letter(beg, end))
		{
			return *beg;
		}

		return std::unexpected(error::invalid_current_directory);
	}


	// X:/*
	//    ^
	template<typename Char>
	vsm::result<void> set_drive_absolute_path(Char const* beg, Char const* const end, wchar_t const drive)
	{
		vsm_try_discard(push_canonical(beg, end));
		vsm_try_void(push_drive_root(drive));

		m_handle = handle_type{};
		return {};
	}

	// //./* or //?/*
	//     ^        ^
	template<typename Char>
	vsm::result<void> set_local_device_path(Char const* const beg, Char const* const end, bool const canonicalize)
	{
		if (canonicalize)
		{
			vsm_try_discard(push_canonical(beg, end));
		}
		else
		{
			vsm_try_void(push_canonical_literal(beg, end));
		}

		vsm_try_void(push_literal(std::wstring_view(L"\\??\\")));

		m_handle = handle_type{};
		return {};
	}

	// //*
	//   ^
	template<typename Char>
	vsm::result<void> set_unc_path(Char const* beg, Char const* const end)
	{
		// Expect at least one non-separator character.
		if (beg == end || is_separator(*beg))
		{
			return std::unexpected(error::invalid_path);
		}

		Char const* const server_beg = beg;
		Char const* const server_end = beg = find_separator(beg, end);

		// Expect exactly one separator and at least one non-separator character.
		if (beg == end || ++beg == end || is_separator(*beg))
		{
			return std::unexpected(error::invalid_path);
		}

		Char const* const share_beg = beg;
		Char const* const share_end = beg = find_separator(beg, end);

		vsm_try_discard(push_canonical(skip_separators(beg, end), end));
		vsm_try_discard(push_canonical(server_beg, share_end));
		vsm_try_void(push_literal(std::wstring_view(L"\\??\\UNC\\")));

		m_handle = handle_type{};
		return {};
	}


	// /*
	//  ^
	template<typename Char>
	vsm::result<void> set_absolute_path(Char const* const beg, Char const* const end)
	{
		if (m_handle)
		{
			// Absolute path relative to handle is not allowed.
			return std::unexpected(error::invalid_path);
		}

		// Absolute paths with a local device current directory have unpredictable results.
		m_reject_local_device_current_directory = true;

		m_lock = m_context.lock();
		auto const current = m_context.get_current_directory(m_lock);
		vsm_try(current_drive, detect_current_drive(current.path));

		vsm_try_discard(push_canonical(beg, end));
		vsm_try_void(push_drive_root(current_drive));

		return {};
	}

	// X:*
	//   ^
	template<typename Char>
	vsm::result<void> set_drive_relative_path(Char const* const beg, Char const* const end, wchar_t const drive)
	{
		if (m_handle)
		{
			// Drive relative path relative to handle is not allowed.
			return std::unexpected(error::invalid_path);
		}

		// Drive relative paths with a local device current directory have unpredictable results.
		m_reject_local_device_current_directory = true;

		m_lock = m_context.lock();
		auto const current = m_context.get_current_directory(m_lock);
		vsm_try(classification, classify_current_path(current.path));

		bool const is_current_drive = drive == classification.drive;
		handle_type const current_handle = is_current_drive ? current.handle : handle_type{};

		return set_relative_from_current(beg, end, current_handle, [&]() -> vsm::result<current_path>
		{
			if (is_current_drive)
			{
				return classification;
			}

			auto const current_on_drive = m_context.get_current_directory_on_drive(m_lock, drive);

			if (!current_on_drive)
			{
				return current_path
				{
					.root = store_drive_root(drive),
				};
			}

			vsm_try(classification_on_drive, classify_current_path(*current_on_drive));

			if (classification_on_drive.drive != drive)
			{
				return std::unexpected(error::invalid_current_directory);
			}

			return classification_on_drive;
		});
	}

	// *
	// ^
	template<typename Char>
	vsm::result<void> set_relative_path(Char const* const beg, Char const* const end)
	{
		return m_handle
			? set_relative_from_handle(beg, end)
			: set_relative_from_current(beg, end);
	}

	template<typename Char>
	vsm::result<void> set_relative_from_handle(Char const* const beg, Char const* const end)
	{
		vsm_try(backtrack, push_canonical(beg, end));

		if (backtrack != 0)
		{
			// Handle relative paths cannot backtrack.
			return std::unexpected(error::invalid_path);
		}

		return {};
	}

	template<typename Char>
	vsm::result<void> set_relative_from_current(Char const* const beg, Char const* const end)
	{
		m_lock = m_context.lock();
		auto const current = m_context.get_current_directory(m_lock);
		return set_relative_from_current(beg, end, current.handle, [&]() -> vsm::result<current_path>
		{
			return classify_current_path(current.path);
		});
	}

	template<typename Char>
	vsm::result<void> set_relative_from_current(
		Char const* const beg, Char const* const end,
		handle_type const current_handle, auto const get_current_path)
	{
		vsm_assert(!m_handle);

		vsm_try(backtrack, push_canonical(beg, end));

		if (backtrack == 0 && current_handle && !m_win32_api_form)
		{
			m_handle = current_handle;
		}
		else
		{
			if (backtrack != 0)
			{
				// Backtracking with a local device current directory has unpredictable results.
				m_reject_local_device_current_directory = true;
			}

			vsm_try(current, get_current_path());
			vsm_try_assign(backtrack, push_canonical(current.relative.beg, current.relative.end, backtrack));

			if (backtrack != 0 && current.requires_trailing_separator)
			{
				vsm_try_void(push_literal(std::wstring_view(L"\\")));
			}

			vsm_try_discard(push_canonical(current.dyn_root.beg, current.dyn_root.end));
			vsm_try_void(push_literal(current.root.beg, current.root.end));
		}

		return {};
	}


	template<typename Char>
	vsm::result<void> make_path(std::basic_string_view<Char> const string)
	{
		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		vsm_assert(beg != end);

		size_t const size = end - beg;
		switch (Char const first = *beg)
		{
		case '\\':
			if (size >= 4 && beg[1] == '?' && beg[2] == '?' && beg[3] == '\\')
			{
				return push_literal(beg, end);
			}

		case '/':
			if (size >= 2 && is_separator(beg[1]))
			{
				if (size >= 4 && is_separator(beg[3]))
				{
					switch (beg[2])
					{
					case '.':
						return set_local_device_path(beg + 4, end, /* canonicalize: */ true);

					case '?':
						return set_local_device_path(beg + 4, end, /* canonicalize: */ false);
					}
				}

				return set_unc_path(beg + 2, end);
			}

			return set_absolute_path(beg + 1, end);

		default:
			if (has_drive_letter(beg, end))
			{
				wchar_t const drive = *beg; //TODO: convert to upper?

				if (size >= 3 && is_separator(beg[2]))
				{
					return set_drive_absolute_path(skip_separators(beg + 3, end), end, drive);
				}

				return set_drive_relative_path(beg + 2, end, drive);
			}
		}

		return set_relative_path(beg, end);
	}

	vsm::result<void> make_path(input_path_view const path)
	{
		if (m_win32_api_form && !path.is_c_string())
		{
			vsm_try_void(push_literal(std::wstring_view(L"\0")));
		}

		auto const extend_over_null_terminator = [&](auto string)
		{
			bool const null_terminator_size = m_win32_api_form && path.is_c_string();
			return decltype(string)(string.data(), string.size() + null_terminator_size);
		};

		vsm_try_void(path.is_wide()
			? make_path(extend_over_null_terminator(path.wide_view()))
			: make_path(extend_over_null_terminator(path.utf8_view())));

		return {};
	}
};

template<typename Context>
vsm::result<basic_kernel_path<Context>> make_kernel_path(
	basic_kernel_path_storage<Context>& storage,
	basic_kernel_path_parameters<std::type_identity_t<Context>> const& args,
	std::type_identity_t<Context>& context = {})
{
	return basic_kernel_path_converter<Context>::make_path(storage, args, context);
}


struct peb_context
{
	using handle_type = HANDLE;

	class unique_lock_type
	{
		vsm::linear<bool, false> m_owns_lock;

	public:
		explicit unique_lock_type(bool const lock = false)
		{
			if (lock)
			{
				this->lock();
			}
		}

		unique_lock_type(unique_lock_type&&) = default;

		unique_lock_type& operator=(unique_lock_type&& other) & noexcept
		{
			if (m_owns_lock.value)
			{
				unlock();
			}
			m_owns_lock = static_cast<unique_lock_type&&>(other).m_owns_lock;
			return *this;
		}

		~unique_lock_type()
		{
			if (m_owns_lock.value)
			{
				unlock();
			}
		}


		[[nodiscard]] bool owns_lock() const
		{
			return m_owns_lock.value;
		}


		void lock()
		{
			vsm_assert(!m_owns_lock.value);
			EnterCriticalSection(NtCurrentPeb()->FastPebLock);
			m_owns_lock.value = true;
		}

		void unlock()
		{
			vsm_assert(m_owns_lock.value);
			LeaveCriticalSection(NtCurrentPeb()->FastPebLock);
			m_owns_lock.value = false;
		}
	};

	static unique_lock_type lock()
	{
		return unique_lock_type(true);
	}

	static current_directory<peb_context> get_current_directory(unique_lock_type const& lock)
	{
		vsm_assert(lock.owns_lock());
		RTL_USER_PROCESS_PARAMETERS const& process_parameters = *NtCurrentPeb()->ProcessParameters;

		wchar_t const* const beg = process_parameters.CurrentDirectoryPath.Buffer;
		wchar_t const* const end = beg + process_parameters.CurrentDirectoryPath.Length / sizeof(wchar_t);
		return { process_parameters.CurrentDirectoryHandle, { beg, end } };
	}

	static std::optional<path_section<wchar_t>> get_current_directory_on_drive(unique_lock_type const& lock, wchar_t const drive)
	{
		vsm_assert(L'A' <= drive && drive <= L'Z');

		vsm_assert(lock.owns_lock());
		RTL_USER_PROCESS_PARAMETERS const& process_parameters = *NtCurrentPeb()->ProcessParameters;
		wchar_t const* environment = static_cast<wchar_t const*>(process_parameters.Environment);

		while (*environment)
		{
			wchar_t const* const var_beg = environment;
			wchar_t const* const var_end = std::wcschr(environment, L'=');
			wchar_t const* const val_end = var_beg + std::wcslen(var_end);

			if (var_end != nullptr && var_end - var_beg == 3 &&
				var_beg[0] == L'=' && var_beg[1] == drive && var_beg[2] == L':')
			{
				return path_section{ var_end + 1, val_end };
			}

			environment = val_end + 1;
		}

		return std::nullopt;
	}
};

} // namespace detail::kernel_path_impl
namespace win32 {

using kernel_path_parameters = detail::kernel_path_impl::basic_kernel_path_parameters<detail::kernel_path_impl::peb_context const>;
using kernel_path_storage = detail::kernel_path_impl::basic_kernel_path_storage<detail::kernel_path_impl::peb_context const>;
using kernel_path = detail::kernel_path_impl::basic_kernel_path<detail::kernel_path_impl::peb_context const>;
using detail::kernel_path_impl::make_kernel_path;

} // namespace win32
} // namespace allio
