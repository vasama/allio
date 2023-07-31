#pragma once

#include <allio/error.hpp>
#include <allio/input_string_view.hpp>

#include <vsm/result.hpp>
#include <vsm/utility.hpp>

#include <memory>
#include <span>
#include <string_view>

#include <cstring>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

class api_string_builder;

class api_string_storage
{
	static constexpr size_t static_buffer_size = 1016;

	std::unique_ptr<std::byte[]> m_dynamic;
	std::byte m_static alignas(uintptr_t) [static_buffer_size];

public:
	api_string_storage() = default;

	api_string_storage(api_string_storage const&) = delete;
	api_string_storage& operator=(api_string_storage const&) = delete;

private:
	friend class api_string_builder;
};

class api_string_builder
{
	api_string_storage& m_storage;

	size_t m_data_size = 0;
	size_t m_ptr_count = 0;

	char* m_out_data = nullptr;
	char const** m_out_ptrs = nullptr;

public:
	explicit api_string_builder(api_string_storage& storage)
		: m_storage(storage)
	{
	}

	static vsm::result<void> make(api_string_storage& storage, auto&& lambda)
	{
		return api_string_builder(storage).make(vsm_forward(lambda));
	}

private:
	static constexpr char const* null_string_ptr = nullptr;

	template<typename Traits>
	class context
	{
		api_string_builder& m_self;

	public:
		explicit context(api_string_builder& self)
			: m_self(self)
		{
		}

		std::string_view operator()(input_string_view const string)
		{
			return string.is_c_string()
				? string.utf8_view()
				: push_string(string.utf8_view());
		}

		char const* const* operator()(std::span<input_string_view const> const strings)
		{
			if (strings.empty())
			{
				return &null_string_ptr;
			}

			char const* const* const ptrs = m_self.m_out_ptrs;
			for (input_string_view const string : strings)
			{
				Traits::push_string_ptr(
					m_self,
					string.is_c_string()
						? string.utf8_c_string()
						: push_string(string.utf8_view()).data());
			}
			Traits::push_string_ptr(m_self, nullptr);

			return ptrs;
		}

	private:
		std::string_view push_string(std::string_view const string)
		{
			if (string.empty())
			{
				return "";
			}

			char const* const ptr = m_self.m_out_data;
			Traits::push_string(m_self, string);
			return ptr;
		}
	};

	struct size_traits
	{
		static void push_string(api_string_builder& self, std::string_view const string)
		{
			self.m_data_size += string.size() + 1;
		}

		static void push_string_ptr(api_string_builder& self, char const* const c_string)
		{
			++self.m_ptr_count;
		}
	};

	struct data_traits
	{
		static void push_string(api_string_builder& self, std::string_view const string)
		{
			size_t const size = string.size();
			memcpy(self.m_out_data, string.data(), size);
			self.m_out_data[size] = '\0';
			self.m_out_data += size + 1;
		}

		static void push_string_ptr(api_string_builder& self, char const* const c_string)
		{
			*self.m_out_ptrs++ = c_string;
		}
	};

	vsm::result<void> make(auto&& lambda)
	{
		lambda(context<size_traits>(*this));

		vsm_try(buffer, [&]() -> vsm::result<void*>
		{
			size_t const size = m_data_size + m_ptr_count * sizeof(char const*);

			if (size <= api_string_storage::static_buffer_size)
			{
				return m_storage.m_static;
			}

			std::byte* const buffer = new (std::nothrow) std::byte[size];

			if (buffer == nullptr)
			{
				return vsm::unexpected(error::not_enough_memory);
			}

			m_storage.m_dynamic.reset(buffer);

			return buffer;
		}());

		m_out_ptrs = reinterpret_cast<char const**>(buffer);
		m_out_data = reinterpret_cast<char*>(m_out_ptrs + m_ptr_count);

		lambda(context<data_traits>(*this));

		return {};
	}
};

inline vsm::result<std::string_view> make_api_string(
	api_string_storage& storage, input_string_view const string)
{
	std::string_view result;
	vsm_try_void(api_string_builder::make(storage, [&](auto&& context)
	{
		result = vsm_forward(context)(string);
	}));
	return result;
}

inline vsm::result<char const*> make_api_c_string(
	api_string_storage& storage, input_string_view const string)
{
	vsm_try(r, make_api_string(storage, string));
	return r.data();
}

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
