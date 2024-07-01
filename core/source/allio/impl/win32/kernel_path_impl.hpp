#pragma once

#include <allio/impl/win32/kernel_path.hpp>

#include <allio/error.hpp>
#include <allio/impl/transcode.hpp>
#include <allio/path_view.hpp>

#include <vsm/lift.hpp>

#include <memory>
#include <optional>

#include <cwctype>

namespace allio::detail::kernel_path_impl {

using namespace win32;
using namespace path_impl;

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
	using unique_lock_type = typename Context::unique_lock_type;
	using storage_type = basic_kernel_path_storage<unique_lock_type>;
	using parameters_type = basic_kernel_path_parameters<handle_type>;
	using path_type = basic_kernel_path<handle_type>;

	static constexpr size_t max_path_size = 0x7fff;

	Context& m_context;
	typename Context::unique_lock_type m_lock;

	storage_type& m_storage;
	handle_type m_handle;

	bool m_win32_api_form = false;
	bool m_reject_local_device_current_directory = false;
	bool m_reject_dos_device_name = true;

	wchar_t* m_buffer_beg = nullptr;
	wchar_t* m_buffer_pos = nullptr;
	wchar_t* m_buffer_end = nullptr;

public:
	basic_kernel_path_converter(basic_kernel_path_converter const&) = delete;
	basic_kernel_path_converter& operator=(basic_kernel_path_converter const&) = delete;

	static vsm::result<path_type> make_path(Context& context, storage_type& storage, parameters_type const& args)
	{
		// Win32 API form does not allow a directory handle.
		vsm_assert(!(args.win32_api_form && args.handle));

		if (args.path.empty())
		{
			return vsm::unexpected(error::invalid_path);
		}

		basic_kernel_path_converter c(context, storage, args);
		vsm_try_void(args.path.string().visit(vsm_lift_borrow(c._make_path_select)));

		storage.m_lock = static_cast<unique_lock_type&&>(c.m_lock);
		return path_type{ c.m_handle, { c.m_buffer_pos, c.m_buffer_end } };
	}

private:
	explicit basic_kernel_path_converter(Context& context, storage_type& storage, parameters_type const& args)
		: m_context(context)
		, m_storage(storage)
		, m_handle(args.handle)
		, m_win32_api_form(args.win32_api_form)
	{
	}


	vsm::result<std::pair<wchar_t*, wchar_t*>> resize_buffer(size_t const new_data_size)
	{
		if (m_storage.m_dynamic != nullptr)
		{
			return vsm::unexpected(error::filename_too_long);
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
				return vsm::unexpected(error::not_enough_memory);
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

		return std::pair<wchar_t*, wchar_t*>{ new_data_beg, old_data_beg };
	}

	vsm::result<std::pair<wchar_t*, wchar_t*>> push_buffer(size_t const new_data_size)
	{
		if (new_data_size <= static_cast<size_t>(m_buffer_pos - m_buffer_beg))
		{
			wchar_t* const buffer_pos = m_buffer_pos;
			wchar_t* const new_buffer_pos = buffer_pos - new_data_size;
			m_buffer_pos = new_buffer_pos;
			return std::pair<wchar_t*, wchar_t*>{ new_buffer_pos, buffer_pos };
		}

		return resize_buffer(new_data_size);
	}


	template<typename Char>
	vsm::result<void> push_literal(Char const* const beg, Char const* const end)
	{
		if (beg == end)
		{
			return {};
		}

		wchar_t* const transcode1_beg = m_buffer_beg;

		auto const r1 = transcode<wchar_t>(
			std::basic_string_view(beg, end),
			std::span(transcode1_beg, m_buffer_pos));

		if (r1.ec != transcode_error{})
		{
			if (r1.ec != transcode_error::no_buffer_space)
			{
				return vsm::unexpected(error::invalid_encoding);
			}

			if (m_storage.m_dynamic != nullptr)
			{
				return vsm::unexpected(error::filename_too_long);
			}


			auto const r2 = transcode_size<wchar_t>(
				std::basic_string_view(beg, end).substr(r1.decoded),
				max_path_size - r1.encoded - (m_buffer_end - m_buffer_pos));

			if (r2.ec != transcode_error{})
			{
				return r2.ec == transcode_error::no_buffer_space
					? vsm::unexpected(error::filename_too_long)
					: vsm::unexpected(error::invalid_encoding);
			}


			vsm_try_bind((out_beg_2, out_end_2), push_buffer(r2.encoded));

			transcode_unchecked<wchar_t>(
				std::basic_string_view(beg, end).substr(r1.decoded),
				std::span(out_beg_2, out_end_2));
		}

		vsm_try_bind((out_beg_1, out_end_1), push_buffer(r1.encoded));

		if (transcode1_beg + r1.encoded != out_beg_1)
		{
			memmove(out_beg_1, transcode1_beg, r1.encoded * sizeof(wchar_t));
		}

		return {};
	}

	vsm::result<void> push_literal(wchar_t const* const beg, wchar_t const* const end)
	{
		if (beg == end)
		{
			return {};
		}

		size_t const size = end - beg;

		wchar_t* const buffer_beg = m_buffer_beg;
		wchar_t* const buffer_pos = m_buffer_pos;

		if (size <= static_cast<size_t>(buffer_pos - buffer_beg))
		{
			// If this piece fits in the existing buffer, just copy it.
			wchar_t* const new_buffer_pos = buffer_pos - size;
			memcpy(new_buffer_pos, beg, size * sizeof(wchar_t));
			m_buffer_pos = new_buffer_pos;
		}
		else if (buffer_beg == m_buffer_end)
		{
			// If this is the first piece, borrow it instead of copying.
			// This avoids allocating for large paths if they can be borrowed.
			m_buffer_beg = const_cast<wchar_t*>(beg);
			m_buffer_pos = const_cast<wchar_t*>(beg);
			m_buffer_end = const_cast<wchar_t*>(end);
		}
		else
		{
			// Otherwise, resize the buffer and copy the string.
			vsm_try_bind((new_buffer_beg, new_buffer_end), resize_buffer(size));
			memcpy(new_buffer_beg, beg, size * sizeof(wchar_t));
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

				Char const lower = upper != '$' ? upper + 0x20 : upper;
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
					// Match "LPT[0-9]" or "COM[0-9]".
					if (std::iswdigit(*beg))
					{
						++beg;
						return true;
					}

					return false;
				}

				if (consume("CON"))
				{
					// Match "CONIN$", "CONOUT$" or "CON".
					return consume("IN$") || consume("OUT$") || true;
				}
			}

			return false;
		};

		if (consume_device())
		{
			while (beg != end && *beg == ' ')
			{
				++beg;
			}

			return beg == end || *beg == '.' || *beg == ':';
		}

		return false;
	}


	struct canonicalization_context
	{
		size_t backtrack = 0;
		bool trailing_separator = true;

		template<typename Char>
		explicit canonicalization_context(Char const* const beg, Char const* const end)
			: trailing_separator(beg != end && is_separator(end[-1]))
		{
		}
	};

	template<typename Char>
	vsm::result<void> push_canonical_impl(
		Char const* const beg,
		Char const* end,
		canonicalization_context& context,
		auto const push_literal)
	{
		// Relative paths never begin with a separator.
		vsm_assert(beg == end || !is_separator(*beg));

		// Any subsequent part of the path does not begin with a separator.
		vsm_assert(m_buffer_pos == m_buffer_end || *m_buffer_pos != Char('\\'));

		Char const* flush_end = end;
		auto const flush = [&](Char const* const flush_beg, Char const* const new_flush_end) -> vsm::result<void>
		{
			Char const* const literal_beg = flush_beg;
			Char const* literal_end = flush_end;
			flush_end = new_flush_end;

			if (literal_beg == literal_end)
			{
				return {};
			}

			if (context.trailing_separator)
			{
				if (literal_end[-1] != Char('\\'))
				{
					static constexpr wchar_t const* separator = L"\\";
					vsm_try_void(push_literal(separator, separator + 1));
				}
			}
			else
			{
				if (literal_end[-1] == Char('\\'))
				{
					vsm_verify(--literal_end != literal_beg);
				}
			}

			context.trailing_separator = true;
			return push_literal(literal_beg, literal_end);
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
					++context.backtrack;
					vsm_try_void(flush(separator_end, segment_beg));
					continue;
				}
			}

			if (is_untrimmed_name(segment_beg, segment_end))
			{
				return vsm::unexpected(error::invalid_path);
			}

			if (m_reject_dos_device_name && is_device_name(segment_beg, segment_end))
			{
				return vsm::unexpected(error::invalid_path);
			}

			if (context.backtrack != 0)
			{
				--context.backtrack;
				flush_end = segment_beg;
				continue;
			}

			if (separator_beg != separator_end)
			{
				if (separator_end - separator_beg > 1 || *separator_beg != Char('\\'))
				{
					bool const has_canonical_separator = *separator_beg == Char('\\');
					vsm_try_void(flush(separator_end, segment_end + has_canonical_separator));
				}
			}
		}

		vsm_try_void(flush(beg, beg));

		return {};
	}

	template<typename Char>
	vsm::result<void> push_canonical(
		Char const* const beg,
		Char const* const end,
		canonicalization_context& context)
	{
		return push_canonical_impl(
			beg,
			end,
			context,
			[this]<typename C>(C const* const beg, C const* const end) -> vsm::result<void>
			{
				return push_literal(beg, end);
			}
		);
	}

	template<typename Char>
	vsm::result<void> push_canonical(
		Char const* const beg,
		Char const* const end)
	{
		canonicalization_context context(beg, end);
		return push_canonical(beg, end, context);
	}

	template<typename Char>
	vsm::result<void> push_canonical_literal(Char const* const beg, Char const* const end)
	{
		size_t const old_path_size = m_buffer_end - m_buffer_pos;

		bool first_segment = true;
		canonicalization_context context(beg, end);
		vsm_try_discard(push_canonical_impl(
			beg,
			end,
			context,
			[&]<typename C>(C const* const beg, C const* const end) -> vsm::result<void>
			{
				if (first_segment)
				{
					first_segment = false;
					return push_literal(beg, end);
				}
				return vsm::unexpected(error::invalid_path);
			}
		));

		size_t const new_path_size = m_buffer_end - m_buffer_pos;

		// The size of the resulting path must match the input.
		// This catches any skipped components at the front and back.
		if (new_path_size != old_path_size + static_cast<size_t>(end - beg))
		{
			return vsm::unexpected(error::invalid_path);
		}

		return {};
	}

	path_section<wchar_t> store_drive_root(wchar_t const drive)
	{
		static constexpr std::wstring_view root_template = L"\\??\\X:\\";
		static_assert(root_template.size() <= storage_type::max_root_size);
		static constexpr size_t drive_offset = root_template.find(L'X');

		wchar_t* const root_buffer = m_storage.m_root;
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
		path_section<wchar_t> root;
		path_section<wchar_t> dynamic_root[2];
		path_section<wchar_t> relative;
	};

	vsm::result<current_path> classify_current_path(path_section<wchar_t> const path)
	{
		wchar_t const* beg = path.beg;
		wchar_t const* const end = path.end;

		if (beg == end)
		{
			return vsm::unexpected(error::invalid_current_directory);
		}

		if (is_separator(*beg))
		{
			if (++beg == end || !is_separator(*beg) || ++beg == end)
			{
				return vsm::unexpected(error::invalid_current_directory);
			}

			if (*beg == '.' || *beg == '?')
			{
				if (++beg == end || !is_separator(*beg))
				{
					return vsm::unexpected(error::invalid_current_directory);
				}

				if (m_reject_local_device_current_directory)
				{
					return vsm::unexpected(error::invalid_current_directory);
				}

				static constexpr std::wstring_view root = L"\\??\\";
				static constexpr wchar_t const* root_beg = root.data();
				static constexpr wchar_t const* root_end = root.data() + root.size();

				return current_path
				{
					.root = { root_beg, root_end },
					.relative = { beg + 1, end },
				};
			}

			if (is_separator(*beg))
			{
				return vsm::unexpected(error::invalid_current_directory);
			}

			wchar_t const* const server_beg = beg;
			wchar_t const* const server_end = beg = find_separator(beg, end);

			if (beg == end || ++beg == end || is_separator(*beg))
			{
				return vsm::unexpected(error::invalid_current_directory);
			}

			wchar_t const* const share_beg = beg;
			wchar_t const* const share_end = beg = find_separator(beg, end);

			wchar_t const* const relative_beg = skip_separators(beg, end);

			static constexpr std::wstring_view root = L"\\??\\UNC\\";
			static constexpr wchar_t const* root_beg = root.data();
			static constexpr wchar_t const* root_end = root.data() + root.size();

			return current_path
			{
				.root = { root_beg, root_end },
				.dynamic_root =
				{
					{ share_beg, share_end },
					{ server_beg, server_end },
				},
				.relative = { relative_beg, end },
			};
		}

		if (has_drive_letter(beg, end))
		{
			wchar_t const drive = *beg++;

			if (++beg == end || !is_separator(*beg))
			{
				return vsm::unexpected(error::invalid_current_directory);
			}

			return current_path
			{
				.drive = drive,
				.root = store_drive_root(drive),
				.relative = { beg + 1, end },
			};
		}

		return vsm::unexpected(error::invalid_current_directory);
	}

	vsm::result<void> push_current_path_root(current_path const& current)
	{
		for (auto const& dynamic_root : current.dynamic_root)
		{
			if (dynamic_root.beg == dynamic_root.end)
			{
				break;
			}

			vsm_try_void(push_literal(std::wstring_view(L"\\")));
			vsm_try_void(push_literal(dynamic_root.beg, dynamic_root.end));
		}

		return push_literal(current.root.beg, current.root.end);
	}


	// X:/*
	//    ^
	template<typename Char>
	vsm::result<void> set_drive_absolute_path(Char const* const beg, Char const* const end, wchar_t const drive)
	{
		vsm_try_void(push_canonical(beg, end));
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
			vsm_try_void(push_canonical(skip_separators(beg, end), end));
		}
		else
		{
			if (beg != end && is_separator(*beg))
			{
				return vsm::unexpected(error::invalid_path);
			}

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
			return vsm::unexpected(error::invalid_path);
		}

		Char const* const server_beg = beg;
		beg = find_separator(beg, end);

		// Expect exactly one separator and at least one non-separator character.
		if (beg == end || ++beg == end || is_separator(*beg))
		{
			return vsm::unexpected(error::invalid_path);
		}

		Char const* const share_end = beg = find_separator(beg, end);

		// DOS device names are permitted in UNC paths.
		m_reject_dos_device_name = false;

		canonicalization_context context(beg, end);

		beg = skip_separators(beg, end);
		vsm_try_void(push_canonical(beg, end, context));

		context.backtrack = 0;
		vsm_try_void(push_canonical(server_beg, share_end, context));
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
			// This is because "absolute" paths are not quite truly absolute,
			// and the meaning of this combination would be a path relative to
			// the root of the particular filesystem referred to by the handle.
			return vsm::unexpected(error::invalid_path);
		}

		// Absolute paths with a local device current directory have unpredictable results.
		m_reject_local_device_current_directory = true;

		m_lock = m_context.lock();
		auto const current = m_context.get_current_directory(m_lock);
		vsm_try(classification, classify_current_path(current.path));

		vsm_try_void(push_canonical(beg, end));
		return push_current_path_root(classification);
	}

	// X:*
	//   ^
	template<typename Char>
	vsm::result<void> set_drive_relative_path(Char const* const beg, Char const* const end, wchar_t const drive)
	{
		if (m_handle)
		{
			// Drive relative path relative to handle is not allowed.
			return vsm::unexpected(error::invalid_path);
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
				return vsm::unexpected(error::invalid_current_directory);
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
		canonicalization_context context(beg, end);
		vsm_try_void(push_canonical(beg, end, context));

		if (context.backtrack != 0)
		{
			// Handle relative paths cannot backtrack.
			return vsm::unexpected(error::invalid_path);
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

		canonicalization_context context(beg, end);
		vsm_try_void(push_canonical(beg, end, context));

		if (context.backtrack == 0 && current_handle && !m_win32_api_form)
		{
			m_handle = current_handle;
		}
		else
		{
			if (context.backtrack != 0)
			{
				// Backtracking with a local device current directory has unpredictable results.
				m_reject_local_device_current_directory = true;
			}

			vsm_try(current, get_current_path());
			vsm_try_void(push_canonical(current.relative.beg, current.relative.end, context));
			vsm_try_void(push_current_path_root(current));
		}

		return {};
	}


	template<typename Char>
	vsm::result<void> _make_path(std::basic_string_view<Char> const string)
	{
		static_assert(!std::is_same_v<Char, char32_t>);
		Char const* const beg = string.data();
		Char const* const end = beg + string.size();
		vsm_assert(beg != end);

		size_t const size = end - beg;
		switch (*beg)
		{
		case '\\':
			// Match "\??" and "\??\*".
			if (size >= 3 && beg[1] == '?' && beg[2] == '?' && (size == 3 || size >= 4 && beg[3] == '\\'))
			{
				if (size <= 4)
				{
					// Reject "\??", "\??\". Win32 APIs don't recognize these
					// and convert them to e.g. "\??\C:\??" and "\??\C:\??\".
					return vsm::unexpected(error::invalid_path);
				}

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

	template<typename Char>
	vsm::result<void> _make_path_select(std::basic_string_view<Char> const string)
	{
		if (m_win32_api_form)
		{
			vsm_try_void(push_literal(std::wstring_view(L"\0")));
		}
		return _make_path(string);
	}

	template<typename Char>
	vsm::result<void> _make_path_select(std::basic_string_view<Char> string, std::same_as<null_terminated_t> auto...)
	{
		if (m_win32_api_form)
		{
			vsm_assert(string.data()[string.size()] == Char(0));
			string = std::basic_string_view<Char>(string.data(), string.size() + 1);
		}
		return _make_path(string);
	}

	vsm::result<void> _make_path_select(std::basic_string_view<char8_t> const string, std::same_as<null_terminated_t> auto... tag)
	{
		return _make_path_select(
			std::basic_string_view<char>(
				reinterpret_cast<char const*>(string.data()),
				string.size()),
			tag...);
	}

	vsm::result<void> _make_path_select(std::basic_string_view<char16_t> const string, std::same_as<null_terminated_t> auto... tag)
	{
		//TODO: This is technically UB.
		return _make_path_select(
			std::basic_string_view<wchar_t>(
				reinterpret_cast<wchar_t const*>(string.data()),
				string.size()),
			tag...);
	}

	vsm::result<void> _make_path_select(std::basic_string_view<char32_t>, std::same_as<null_terminated_t> auto...)
	{
		return vsm::unexpected(error::unsupported_encoding);
	}

	vsm::result<void> _make_path_select(string_length_out_of_range_t)
	{
		return vsm::unexpected(error::filename_too_long);
	}
};

} // namespace allio::detail::kernel_path_impl
