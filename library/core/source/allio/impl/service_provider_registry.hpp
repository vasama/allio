#pragma once

#include <allio/detail/new.hpp>
#include <allio/impl/error_encoding.hpp>

#include <vsm/result.hpp>
#include <vsm/pointer_tag_pair.hpp>

#include <utility>

namespace allio::detail {

class service_provider_registry_impl
{
	using slot_pair = vsm::pointer_tag_pair<void, bool>;

	struct slot_chunk
	{
		static constexpr size_t size = 7;

		slot_pair data[size + 1];

		constexpr slot_chunk()
			: slot_chunk(std::make_integer_sequence<size>())
		{
		}

		template<size_t... Is>
		explicit constexpr slot_chunk(std::integer_sequence<Is>)
			: data{ slot_pair(nullptr, Is == size)... }
		{
		}
	};

	slot_chunk m_root_chunk;

protected:
	class iterator
	{
		slot_pair* m_slot;

	public:
		explicit iterator(slot_pair* const slot) noexcept
			: m_slot(slot)
		{
		}

		iterator& operator++() & noexcept
		{
			m_slot = next(m_slot);
			return *this;
		}

		[[nodiscard]] iterator operator++(int) & noexcept
		{
			auto r = *this;
			m_slot = next(m_slot);
			return r;
		}

		[[nodiscard]] friend bool operator==(iterator const&, iterator const&) = default;

	protected:
		[[nodiscard]] void const* get() const noexcept
		{
			return load(m_slot).pointer();
		}
	};

	[[nodiscard]] vsm::result<void> insert(void const* const ptr) noexcept
	{
		unique_ptr<slot_chunk> new_chunk;
		slot_pair* slot = m_root_chunk.data;

		while (true)
		{
			slot_pair pair = load(slot);

			auto const compare_exchange = [&](slot_pair const new_pair)
			{
				return vsm::atomic_ref<slot_pair>(*slot).compare_exchange_weak(
					pair,
					new_pair,
					std::memory_order_release,
					std::memory_order_acquire);
			};

			while (pair.pointer() == nullptr)
			{
				if (pair.tag())
				{
					if (new_chunk == nullptr)
					{
						vsm_try_assign(new_chunk, make_unique<slot_chunk>());
					}

					if (compare_exchange(slot_pair(new_chunk.get()->data, true)))
					{
						slot = new_chunk.release()->data;
						pair = slot_pair(nullptr, false);
					}
				}
				else
				{
					if (compare_exchange(slot_pair(ptr, false)))
					{
						return;
					}
				}
			}

			slot = next(slot, pair);
		}
	}

	[[nodiscard]] iterator begin() const noexcept
	{
		return iterator(m_root_chunk.data);
	}

	[[nodiscard]] iterator end() const noexcept
	{
		return iterator(nullptr);
	}

private:
	[[nodiscard]] static slot_pair load(slot_pair* const slot) noexcept
	{
		return vsm::atomic_ref<slot_pair>(*slot).load(std::memory_order_acquire);
	}

	[[nodiscard]] static slot_pair* next(slot_pair* const slot, slot_pair const pair) noexcept
	{
		if (pair.tag())
		{
			return static_cast<slot_pair*>(pair.pointer());
		}
		else
		{
			return slot + 1;
		}
	}

	[[nodiscard]] static slot_pair* next(slot_pair* const slot) noexcept
	{
		return next(slot, load(slot));
	}
};

template<typename ServiceProvider>
class service_provider_registry : service_provider_registry_impl
{
public:
	class iterator : public service_provider_registry_impl::iterator
	{
	public:
		using service_provider_registry_impl::iterator::iterator;

		[[nodiscard]] ServiceProvider const& operator*() const noexcept
		{
			return *static_cast<ServiceProvider const*>(get());
		}

		[[nodiscard]] ServiceProvider const* operator->() const noexcept
		{
			return static_cast<ServiceProvider const*>(get());
		}
	};

	[[nodiscard]] vsm::result<void> insert(ServiceProvider const& service_provider) noexcept
	{
		service_provider_registry_impl::insert(&service_provider);
	}

	[[nodiscard]] iterator begin() const noexcept
	{
		return iterator(service_provider_registry_impl::begin());
	}

	[[nodiscard]] iterator end() const noexcept
	{
		return iterator(service_provider_registry_impl::end());
	}
};

} // namespace allio::detail
