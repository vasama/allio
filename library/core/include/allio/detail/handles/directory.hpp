#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/dynamic_buffer.hpp>
#include <allio/detail/facade.hpp>
#include <allio/detail/handles/fs_object.hpp>
#include <allio/path_char.hpp>

#include <vsm/arrow.hpp>

namespace allio::detail {

enum class directory_stream_position : size_t
{
	end_of_stream                       = static_cast<size_t>(-1),
	end_of_directory                    = static_cast<size_t>(-2),
};

enum class directory_stream_pointer : uintptr_t
{
	end_of_stream                       = static_cast<uintptr_t>(-1),
	end_of_directory                    = static_cast<uintptr_t>(-2),
};

[[nodiscard]] inline directory_stream_pointer operator+(
	std::byte const* const stream,
	directory_stream_position const position)
{
	return position >= directory_stream_position::end_of_directory
		? static_cast<directory_stream_pointer>(
			static_cast<std::make_signed_t<size_t>>(position))
		: static_cast<directory_stream_pointer>(
			reinterpret_cast<uintptr_t>(stream + static_cast<size_t>(position)));
}

[[nodiscard]] inline directory_stream_position operator-(
	directory_stream_pointer const pointer,
	std::byte const* const stream)
{
	return pointer >= directory_stream_pointer::end_of_directory
		? static_cast<directory_stream_position>(
			static_cast<std::make_signed_t<uintptr_t>>(pointer))
		: static_cast<directory_stream_position>(
			reinterpret_cast<std::byte const*>(pointer) - stream);
}

[[nodiscard]] directory_stream_pointer next_directory_entry(directory_stream_pointer pointer);

[[nodiscard]] fs_entry_type get_directory_entry_type(directory_stream_pointer pointer);

[[nodiscard]] std::basic_string_view<native_path_char_t> get_directory_entry_name(
	directory_stream_pointer pointer);

[[nodiscard]] vsm::result<size_t> copy_directory_entry_name(
	directory_stream_pointer pointer,
	any_string_buffer buffer);

template<typename String>
[[nodiscard]] vsm::result<String> copy_directory_entry_name(directory_stream_pointer const pointer)
{
	vsm::result<String> r(vsm::result_value);
	if (auto const r2 = detail::copy_directory_entry_name(pointer, *r); !r2)
	{
		r = vsm::unexpected(r2.error());
	}
	return r;
}

template<typename Traits>
class basic_directory_entry_view;

template<>
class basic_directory_entry_view<void>
{
	directory_stream_pointer m_pointer;

public:
	explicit basic_directory_entry_view(directory_stream_pointer const pointer)
		: m_pointer(pointer)
	{
	}

	[[nodiscard]] fs_entry_type type() const
	{
		return get_directory_entry_type(m_pointer);
	}

	//TODO: Rename to native_name?
	[[nodiscard]] std::basic_string_view<native_path_char_t> name() const
	{
		return get_directory_entry_name(m_pointer);
	}

private:
	template<typename>
	friend class basic_directory_entry_view;

	template<typename, typename>
	friend struct rebind_traits;
};

template<typename Traits>
class basic_directory_entry_view : public basic_directory_entry_view<void>
{
public:
	using basic_directory_entry_view<void>::basic_directory_entry_view;

	[[nodiscard]] auto get_name(any_string_buffer const buffer) const
	{
		auto r = detail::copy_directory_entry_name(m_pointer, buffer);

		if constexpr (Traits::has_transform_result)
		{
			return Traits::transform_result(vsm_move(r));
		}
		else
		{
			return r;
		}
	}

	template<typename String = std::basic_string<native_path_char_t>>
	[[nodiscard]] auto get_name() const
	{
		auto r = detail::copy_directory_entry_name<String>(m_pointer);

		if constexpr (Traits::has_transform_result)
		{
			return Traits::transform_result(vsm_move(r));
		}
		else
		{
			return r;
		}
	}
};

template<typename Traits>
class basic_directory_stream_sentinel {};

template<typename Traits>
class basic_directory_stream_iterator
{
	directory_stream_pointer m_pointer;

public:
	explicit basic_directory_stream_iterator(directory_stream_pointer const pointer)
		: m_pointer(pointer)
	{
	}

	[[nodiscard]] basic_directory_entry_view<Traits> operator*() const
	{
		return basic_directory_entry_view<Traits>(m_pointer);
	}

	[[nodiscard]] vsm::arrow<basic_directory_entry_view<Traits>> operator->() const
	{
		return basic_directory_entry_view<Traits>(m_pointer);
	}

	basic_directory_stream_iterator& operator++() &
	{
		m_pointer = next_directory_entry(m_pointer);
		return *this;
	}

	[[nodiscard]] basic_directory_stream_iterator operator++(int) &
	{
		auto it = *this;
		m_pointer = next_directory_entry(m_pointer);
		return it;
	}

	[[nodiscard]] bool operator==(basic_directory_stream_sentinel<Traits>) const
	{
		return m_pointer == directory_stream_pointer::end_of_stream;
	}

	[[nodiscard]] bool operator!=(basic_directory_stream_sentinel<Traits>) const
	{
		return m_pointer != directory_stream_pointer::end_of_stream;
	}
};

template<typename Traits>
class basic_directory_stream_view
{
	directory_stream_pointer m_pointer;

public:
	basic_directory_stream_view() = default;

	explicit basic_directory_stream_view(directory_stream_pointer const pointer)
		: m_pointer(pointer)
	{
	}

	[[nodiscard]] directory_stream_pointer get_stream_pointer() const
	{
		return m_pointer;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_pointer != directory_stream_pointer::end_of_directory;
	}

	[[nodiscard]] basic_directory_stream_iterator<Traits> begin() const
	{
		vsm_assert(m_pointer != directory_stream_pointer::end_of_directory);
		return basic_directory_stream_iterator<Traits>(m_pointer);
	}

	[[nodiscard]] basic_directory_stream_sentinel<Traits> end() const
	{
		vsm_assert(m_pointer != directory_stream_pointer::end_of_directory);
		return {};
	}

private:
	template<typename, typename>
	friend struct rebind_traits;
};


template<typename From, typename To>
struct rebind_traits<basic_directory_entry_view<From>, basic_directory_entry_view<To>>
{
	static vsm::result<basic_directory_entry_view<To>> rebind(
		basic_directory_entry_view<From> const view)
	{
		return vsm::result<basic_directory_entry_view<To>>(vsm::result_value, view.m_pointer);
	}
};

template<typename From, typename To>
struct rebind_traits<basic_directory_stream_view<From>, basic_directory_stream_view<To>>
{
	static vsm::result<basic_directory_stream_view<To>> rebind(
		basic_directory_stream_view<From> const view)
	{
		return vsm::result<basic_directory_stream_view<To>>(vsm::result_value, view.m_pointer);
	}
};


template<typename Handle>
auto select_directory_stream_view(int) -> basic_directory_stream_view<typename Handle::traits_type>;

template<typename Handle>
auto select_directory_stream_view(...) -> basic_directory_stream_view<void>;


namespace directory_io {

struct read_t
{
	using operation_concept = void;

	struct params_type
	{
		read_buffer buffer;
	};

	//TODO: the basic handle types don't have traits_type
	template<handle Handle>
	using result_type_template = decltype(detail::select_directory_stream_view<Handle>(0));

	template<object Object>
	static vsm::result<basic_directory_stream_view<void>> blocking_io(
		native_handle<Object> const& h,
		io_parameters_t<Object, read_t> const& a)
		requires requires { Object::read(h, a); }
	{
		return Object::read(h, a);
	}
};

struct restart_t
{
	using operation_concept = void;
	using params_type = no_parameters_t;
	using result_type = void;

	template<object Object>
	static vsm::result<void> blocking_io(
		native_handle<Object> const& h,
		io_parameters_t<Object, restart_t> const& a)
		requires requires { Object::restart(h, a); }
	{
		return Object::restart(h, a);
	}
};

} // namespace directory_io

struct directory_t : fs_object_t
{
	using base_type = fs_object_t;

	using read_t = directory_io::read_t;
	using restart_t = directory_io::restart_t;

	using operations = type_list_append
	<
		base_type::operations
		, read_t
		, restart_t
	>;

	static vsm::result<void> open(
		native_handle<directory_t>& h,
		io_parameters_t<directory_t, open_t> const& a);

	static vsm::result<basic_directory_stream_view<void>> read(
		native_handle<directory_t> const& h,
		io_parameters_t<directory_t, read_t> const& a);

	static vsm::result<void> restart(
		native_handle<directory_t>& h,
		io_parameters_t<directory_t, restart_t> const& a);


	using is_serializable = directory_t;

	static vsm::result<void> serialize(
		native_handle<directory_t>& h,
		serialization_context& serializer);


	template<typename Handle, typename Traits>
	struct facade : base_type::facade<Handle, Traits>
	{
		[[nodiscard]] auto read(read_buffer const buffer) const
		{
			auto a = io_parameters_t<typename Handle::object_type, read_t>{};
			a.buffer = buffer;
			return Traits::template observe<read_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto restart() const
		{
			return Traits::template observe<restart_t>(
				static_cast<Handle const&>(*this),
				no_parameters_t());
		}

		[[nodiscard]] auto iterate() const;
		[[nodiscard]] auto recurse() const;

		[[nodiscard]] auto open_relative(any_path_view const relative_path) const;
		[[nodiscard]] auto operator/(any_path_view const relative_path) const;
	};
};


struct directory_iterator_t : object_t
{
	using base_type = object_t;

	struct next_t
	{
		using operation_concept = void;
		//TODO: Template this once directory_entry et. al. are themselves templated.
		using result_type = bool;
		using params_type = no_parameters_t;

		template<std::same_as<directory_iterator_t> Object>
		static vsm::result<bool> blocking_io(
			native_handle<Object> const& h,
			io_parameters_t<Object, next_t> const& a)
			requires requires { Object::next(h, a); }
		{
			return Object::next(h, a);
		}
	};

	static vsm::result<bool> next(
		native_handle<directory_iterator_t> const& h,
		io_parameters_t<directory_iterator_t, next_t> const& a);
};

template<>
struct native_handle<directory_iterator_t> : native_handle<directory_iterator_t::base_type>
{
	native_handle<directory_t> const* directory_handle;
	mutable directory_stream_position stream_position;
	//TODO: Figure out a better way to handle the memory allocation.
	mutable dynamic_buffer<std::byte, 256> storage;
};

template<typename Multiplexer>
struct async_connector<Multiplexer, directory_iterator_t>
{
	async_connector<Multiplexer, directory_t> const* directory_connector;
};

template<typename Multiplexer>
struct async_operation<Multiplexer, directory_iterator_t, directory_iterator_t::next_t>
{
	using M = Multiplexer;
	using H = native_handle<directory_iterator_t> const;
	using C = async_connector_t<M, directory_iterator_t> const;
	using S = async_operation_t<M, directory_iterator_t, directory_iterator_t::next_t>;
	using R = bool;
	using A = no_parameters_t;

	async_operation_t<Multiplexer, directory_t, directory_io::read_t> _directory_read;
	io_handler<M>* _handler;

	static io_result<R> submit(
		M& m,
		H& h,
		C& c,
		S& s,
		A const&,
		io_handler<M>& handler)
	{
		vsm_assert(h.stream_position != directory_stream_position::end_of_directory); //PRECONDITION

		s._handler = &handler;

		if (h.stream_position != directory_stream_position::end_of_stream)
		{
			auto const cur_pointer = h.storage.data() + h.stream_position;
			auto const new_pointer = next_directory_entry(cur_pointer);
			h.stream_position = new_pointer - h.storage.data();
		}

		if (h.stream_position == directory_stream_position::end_of_stream)
		{
			if (h.storage.size() == 0)
			{
				//TODO: Handle this in a better way
				vsm_verify(h.storage.reserve(256));
			}

			vsm_try_void(_submit_directory_read(
				m,
				h,
				c,
				s,
				handler));
		}

		return h.stream_position != directory_stream_position::end_of_directory;
	}

	static io_result<R> notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const&,
		io_handler<M>& handler,
		typename M::io_status_type status)
	{
		auto const r = decltype(_directory_read)::notify(
			m,
			*h.directory_handle,
			*c.directory_connector,
			s._directory_read,
			_make_directory_read_args(h),
			vsm_move(status));

		vsm_try(done, _notify_directory_read(
			m,
			h,
			c,
			s,
			handler,
			r));

		if (!done)
		{
			vsm_try_void(_submit_directory_read(
				m,
				h,
				c,
				s,
				handler));
		}

		return h.stream_position != directory_stream_position::end_of_directory;
	}

	static void cancel(M& m, H const& h, C const& c, S& s)
	{
		decltype(_directory_read)::cancel(
			m,
			*h.directory_handle,
			*c.directory_connector,
			s._directory_read);
	}

	static directory_io::read_t::params_type _make_directory_read_args(H& h)
	{
		directory_io::read_t::params_type args = {};
		args.buffer = std::span(h.storage.data(), h.storage.size());
		return args;
	}

	static io_result<void> _submit_directory_read(
		M& m,
		H& h,
		C& c,
		S& s,
		io_handler<M>& handler)
	{
		while (true)
		{
			auto const r = decltype(_directory_read)::submit(
				m,
				*h.directory_handle,
				*c.directory_connector,
				s._directory_read,
				_make_directory_read_args(h),
				handler);

			vsm_try(done, _notify_directory_read(
				m,
				h,
				c,
				s,
				handler,
				r));

			if (done)
			{
				return {};
			}
		}
	}

	static io_result<bool> _notify_directory_read(
		M& m,
		H& h,
		C& c,
		S& s,
		io_handler<M>& handler,
		io_result<basic_directory_stream_view<void>> const& r)
	{
		if (r)
		{
			auto const pointer = (*r).get_stream_pointer();
			h.stream_position = pointer - h.storage.data();

			if (pointer != directory_stream_pointer::end_of_stream)
			{
				return true;
			}
		}
		else
		{
			if (r.error() != std::errc::no_buffer_space)
			{
				return vsm::propagate_error(r);
			}

			//TODO: Figure out if there's a better growth strategy.
			vsm_try_discard(h.storage.reserve(h.storage.size() * 3 / 2));
		}

		return false;
	}

#if 0
	static directory_entry_view _next_directory_entry(H& h)
	{
		vsm_assert(h.stream_position < directory_stream_position::end_of_directory);
		auto const storage = h.storage.data();
		auto const pointer = storage + h.stream_position;
		h.stream_position = next_directory_entry(pointer) - storage;
		return directory_entry_view(pointer);
	}
#endif
};

template<typename MultiplexerHandle>
class directory_iterator_handle
{
	using multiplexer_type = typename MultiplexerHandle::multiplexer_type;

	native_handle<directory_iterator_t> m_native;
	async_connector_t<multiplexer_type, directory_iterator_t> m_connector;
	MultiplexerHandle const* m_multiplexer_handle;

public:
	using handle_concept = void;
	using object_type = directory_iterator_t;
	using multiplexer_handle_type = MultiplexerHandle;

	template<std::same_as<directory_iterator_t>>
	using rebind_object = directory_iterator_handle;

	template<std::same_as<MultiplexerHandle>>
	using rebind_multiplexer = directory_iterator_handle;

	template<attached_handle_for<directory_t> DirectoryHandle>
	explicit directory_iterator_handle(DirectoryHandle const& directory_handle)
		: m_native
		{
			object_t::flags::not_null,
			&directory_handle.native(),
			directory_stream_position::end_of_stream,
		}
		, m_connector{ &directory_handle.connector() }
		, m_multiplexer_handle(&directory_handle.multiplexer())
	{
	}

	native_handle<directory_iterator_t> const& native() const
	{
		return m_native;
	}

private:
	friend vsm::result<bool> tag_invoke(
		blocking_io_t<directory_iterator_t::next_t>,
		directory_iterator_handle const& h,
		no_parameters_t const& a)
	{
		return blocking_io<directory_iterator_t::next_t>(h.m_native, a);
	}
};

template<>
class directory_iterator_handle<void>
{
	native_handle<directory_iterator_t> m_native;

public:
	using handle_concept = void;
	using object_type = directory_iterator_t;
	using multiplexer_handle_type = void;

	template<std::same_as<directory_iterator_t>>
	using rebind_object = directory_iterator_handle;

	template<std::same_as<void>>
	using rebind_multiplexer = directory_iterator_handle;

	template<detached_handle_for<directory_t> DirectoryHandle>
	explicit directory_iterator_handle(DirectoryHandle const& directory_handle)
		: m_native
		{
			{
				object_t::flags::not_null,
			},
			&directory_handle.native(),
			directory_stream_position::end_of_stream,
		}
	{
	}

	native_handle<directory_iterator_t> const& native() const
	{
		return m_native;
	}

private:
	[[deprecated]] friend vsm::result<bool> tag_invoke(
		blocking_io_t<directory_iterator_t::next_t>,
		directory_iterator_handle const& h,
		no_parameters_t const& a)
	{
		return blocking_io<directory_iterator_t::next_t>(h.m_native, a);
	}
};

template<handle_for<directory_t> Handle, typename Traits>
class directory_iterator
{
	using multiplexer_handle_type = typename Handle::multiplexer_handle_type;
	using handle_type = directory_iterator_handle<multiplexer_handle_type>;
	using facade_type = basic_facade<handle_type, Traits>;

	facade_type m_handle;
	std::error_code m_status;

	static bool _is_valid(facade_type const& handle)
	{
		native_handle<directory_iterator_t> const& h = handle.native();
		return h.stream_position != directory_stream_position::end_of_directory;
	}

	static auto _next(facade_type const& handle)
	{
		return Traits::template observe<directory_iterator_t::next_t>(
			handle,
			no_parameters_t());
	}

	static basic_directory_entry_view<Traits> _get(facade_type const& handle)
	{
		native_handle<directory_iterator_t> const& h = handle.native();
		return basic_directory_entry_view<Traits>(h.storage.data() + h.stream_position);
	}


	class sentinel {};

	class iterator
	{
		directory_iterator const* m_directory_iterator;

	public:
		using value_type = basic_directory_entry_view<Traits>;
		using difference_type = ptrdiff_t;

		iterator() = default;

		// Intentionally takes a mutable reference, because while the _next operation is declared
		// const, the native handle actually contains mutable members mutated by the operation.
		explicit iterator(directory_iterator& directory_iterator)
			: m_directory_iterator(&directory_iterator)
		{
			++*this;
		}

		[[nodiscard]] value_type operator*() const
		{
			return _get(m_directory_iterator->m_handle);
		}

		iterator& operator++() &
		{
			[[maybe_unused]] std::same_as<bool> auto result =
				_next(m_directory_iterator->m_handle);

			return *this;
		}

		[[nodiscard]] iterator operator++(int) &
		{
			auto it = *this;
			++*this;
			return it;
		}

		[[nodiscard]] bool operator==(sentinel) const
		{
			return !_is_valid(m_directory_iterator->m_handle);
		}
	};
	static_assert(std::input_iterator<iterator>);
	static_assert(std::sentinel_for<sentinel, iterator>);

public:
	explicit directory_iterator(Handle const& handle)
		: m_handle(handle)
	{
	}


	[[nodiscard]] auto next()
	{
		return _next(m_handle);
	}

	[[nodiscard]] basic_directory_entry_view<Traits> get() const
	{
		return _get(m_handle);
	}

	[[nodiscard]] explicit operator bool() const
	{
		return _is_valid(m_handle);
	}


	[[nodiscard]] iterator begin()
	{
		return iterator(*this);
	}

	[[nodiscard]] sentinel end()
	{
		return {};
	}
};

template<typename Handle, typename Traits>
[[nodiscard]] auto directory_t::facade<Handle, Traits>::iterate() const
{
	return directory_iterator<Handle, Traits>(static_cast<Handle const&>(*this));
}

#if 0
template<typename Handle, typename Traits>
[[nodiscard]] auto directory_t::facade<Handle, Traits>::recurse() const
{
}
#endif


template<typename Traits>
[[nodiscard]] auto open_directory(fs_path const& path, auto&&... args)
{
	auto a = io_parameters_t<directory_t, fs_io::open_t>{};
	a.path = path;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<directory_t, fs_io::open_t>(a);
}

template<typename Traits>
[[nodiscard]] auto open_temp_directory(auto&&... args)
{
	auto a = io_parameters_t<directory_t, fs_io::open_t>{};
	a.special = open_options::temporary;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<directory_t, fs_io::open_t>(a);
}

template<typename Traits>
[[nodiscard]] auto open_unique_directory(auto&&... args)
{
	auto a = io_parameters_t<directory_t, fs_io::open_t>{};
	a.special = open_options::unique_name;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<directory_t, fs_io::open_t>(a);
}


[[nodiscard]] vsm::result<size_t> _get_current_directory(any_path_buffer buffer);

template<typename Path>
[[nodiscard]] vsm::result<Path> _get_current_directory()
{
	vsm::result<Path> r(vsm::result_value);
	if (auto const r2 = _get_current_directory(*r); !r2)
	{
		r = vsm::unexpected(r2.error());
	}
	return r;
}

[[nodiscard]] vsm::result<void> _set_current_directory(fs_path const& path);

[[nodiscard]] vsm::result<basic_detached_handle<directory_t>> _open_current_directory();


template<typename Traits>
[[nodiscard]] auto get_current_directory(any_path_buffer const buffer)
{
	auto r = _get_current_directory(buffer);

	if constexpr (Traits::has_transform_result)
	{
		return Traits::transform_result(vsm_move(r));
	}
	else
	{
		return r;
	}
}

template<typename Traits, typename Path>
[[nodiscard]] auto get_current_directory()
{
	auto r = _get_current_directory<Path>();

	if constexpr (Traits::has_transform_result)
	{
		return Traits::transform_result(vsm_move(r));
	}
	else
	{
		return r;
	}
}

template<typename Traits>
[[nodiscard]] auto set_current_directory(fs_path const& path)
{
	auto r = _set_current_directory(path);

	if constexpr (Traits::has_transform_result)
	{
		return Traits::transform_result(vsm_move(r));
	}
	else
	{
		return r;
	}
}

template<typename Handle>
[[nodiscard]] vsm::result<Handle> open_current_directory_h()
{
	auto r = _open_current_directory();

	if constexpr (std::is_same_v<typename decltype(r)::value_type, Handle>)
	{
		return r;
	}
	else
	{
		if (r)
		{
			return rebind_handle<Handle>(*vsm_move(r));
		}
		else
		{
			return vsm::unexpected(r.error());
		}
	}
}

template<typename Traits>
[[nodiscard]] auto open_current_directory()
{
	using handle_type = typename Traits::template handle<directory_t>;

	auto r = open_current_directory_h<handle_type>();

	if constexpr (Traits::has_transform_result)
	{
		return Traits::transform_result(vsm_move(r));
	}
	else
	{
		return r;
	}
}

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/directory.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/directory.hpp>
#endif
