#pragma once

#include <allio/error.hpp>
#include <allio/detail/memory.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/inplace_vector.hpp>
#include <allio/nothrow/map.hpp>

#include <vsm/numeric.hpp>
#include <vsm/result.hpp>

#include <bit>
#include <ranges>
#include <span>

#include <cstdint>

namespace allio {

class fd_tree
{
	using mask_t = uintptr_t;

	static constexpr size_t k = sizeof(mask_t) * CHAR_BIT;
	static constexpr size_t log_k = std::countr_zero(k);

	static constexpr size_t index_bit_count = (sizeof(int) * CHAR_BIT) - 1;
	static constexpr size_t max_index_count = static_cast<size_t>(1) << index_bit_count;
	static constexpr size_t max_tree_height = index_bit_count / log_k;

	struct leaf_t
	{
		mask_t mask;

		[[nodiscard]] size_t find_free() const
		{
			return static_cast<size_t>(std::countr_one(mask));
		}

		[[nodiscard]] bool is_set(size_t const index)
		{
			return mask & static_cast<mask_t>(1) << index;
		}

		[[nodiscard]] bool set(size_t const index)
		{
			mask |= static_cast<mask_t>(1) << index;
			return mask != static_cast<mask_t>(-1);
		}

		void clear(size_t const index)
		{
			vsm_assert(index < k);
			mask &= ~(static_cast<mask_t>(1) << index);
		}
	};

	struct node_t : leaf_t
	{
		leaf_t* children;
	};

	static constexpr size_t half_block_size = k * sizeof(leaf_t);
	static constexpr size_t full_block_size = k * sizeof(node_t);
	static_assert(full_block_size == half_block_size * 2);

	using map_handle_t = nothrow::map_handle;

	class block_array_t
	{
		using vector_type = inplace_vector<
			map_handle_t,
			(half_block_size - sizeof(size_t)) / sizeof(map_handle_t)>;

		static_assert(sizeof(vector_type) <= half_block_size);

		vector_type m_handles;

	public:
		block_array_t() = default;
		~block_array_t() = delete;

		[[nodiscard]] bool is_full() const
		{
			return m_handles.size() == m_handles.capacity();
		}

		[[nodiscard]] map_handle_t& front()
		{
			return m_handles.front();
		}

		[[nodiscard]] map_handle_t& back()
		{
			return m_handles.back();
		}

		map_handle_t& emplace_back(auto&&... args)
		{
			return m_handles.unchecked_emplace_back(vsm_forward(args)...);
		}

		void release()
		{
			// Ensure that the first handle is destroyed after all others:
			map_handle_t const front(vsm_move(m_handles.front()));

			m_handles.clear();
		}
	};

	using block_pair = std::pair<void*, void*>;

	node_t m_root = {};
	size_t m_height = 0;
	size_t m_count = 0;

	void* m_free_half_block = nullptr;
	size_t m_last_block_commit_offset = 0;
	size_t m_last_block_consume_offset = 0;

	struct block_array_deleter
	{
		void operator()(block_array_t* const block_array) const
		{
			block_array->release();
		}
	};
	std::unique_ptr<block_array_t, block_array_deleter> m_block_array;

public:
	[[nodiscard]] vsm::result<int> allocate()
	{
		if (m_count == max_index_count)
		{
			return vsm::unexpected(allio_error(error::maximum_capacity_exceeded));
		}

		if (m_root.mask == static_cast<size_t>(-1))
		{
			vsm_try_void(allocate_root_node());
		}

		struct node_stack_type
		{
			node_t* node;
			size_t index;
		};

		node_stack_type stack_storage[max_tree_height];
		std::span const stack(stack_storage, m_height);

		leaf_t* tree = &m_root;
		size_t total_index = 0;
		size_t local_index = 0;

		for (auto const [height, stack_node] : std::views::enumerate(stack))
		{
			auto& [node, stack_index] = stack_node;

			vsm_assert(local_index < k);
			node = &static_cast<node_t*>(tree)[local_index];

			local_index = node->find_free();
			total_index = (total_index << log_k) + local_index;
			stack_index = local_index;

			if (node->children == nullptr)
			{
				bool const is_leaf = static_cast<size_t>(height) == m_height - 1;
				vsm_try_assign(node->children, allocate_node(is_leaf));
			}

			tree = node->children;
		}

		vsm_assert(local_index < k);
		leaf_t& leaf = tree[local_index];

		local_index = leaf.find_free();
		total_index = (total_index << log_k) + local_index;

		if (!leaf.set(local_index))
		{
			for (auto const& [node, stack_index] : std::views::reverse(stack))
			{
				if (node->set(stack_index))
				{
					break;
				}
			}
		}

		++m_count;

		return vsm::truncating(total_index);
	}

	void deallocate(int const index) noexcept
	{
		vsm_assert(index >= 0); //PRECONDITION

		leaf_t* tree = &m_root;

		size_t total_index = static_cast<size_t>(index);
		size_t local_index = 0;

		for (size_t i = m_height; i != 0; --i)
		{
			vsm_assert(local_index < k);
			node_t* const node = &static_cast<node_t*>(tree)[local_index];

			local_index = total_index >> (i * log_k);
			total_index = total_index - (local_index << (i * log_k));

			node->clear(local_index);

			tree = node->children;
		}

		vsm_assert(local_index < k);
		vsm_assert(tree[local_index].is_set(total_index));
		tree[local_index].clear(total_index);

		--m_count;
	}

private:
	[[nodiscard]] vsm::result<void> allocate_root_node()
	{
		if (m_height == max_tree_height)
		{
			return vsm::unexpected(allio_error(error::maximum_capacity_exceeded));
		}

		if (m_block_array == nullptr)
		{
			vsm_try(handle, nothrow::map_memory(detail::get_default_allocation_granularity()));

			vsm_try(block_array_storage, allocate_half_block(handle));
			auto const block_array = new (block_array_storage) block_array_t;
			block_array->emplace_back(vsm_move(handle));

			m_block_array.reset(block_array);
		}

		return m_height == 0
			? allocate_and_replace_root_node<leaf_t>()
			: allocate_and_replace_root_node<node_t>();
	}

	template<typename T>
	[[nodiscard]] vsm::result<void> allocate_and_replace_root_node()
	{
		vsm_try(leaf_or_node, allocate_node(std::is_same_v<leaf_t, T>));
		static_cast<T&>(*leaf_or_node) = static_cast<T const&>(m_root);

		m_root.mask = 1;
		m_root.children = leaf_or_node;

		++m_height;
		return {};
	}

	[[nodiscard]] vsm::result<leaf_t*> allocate_node(bool const is_leaf)
	{
		if (is_leaf)
		{
			vsm_try(storage, allocate_half_block());
			return vsm::start_lifetime_as<leaf_t>(storage);
		}
		else
		{
			vsm_try(storage, allocate_full_block());
			return vsm::start_lifetime_as<node_t>(storage);
		}
	}

	[[nodiscard]] vsm::result<void*> allocate_full_block()
	{
		vsm_try_bind((half_1, half_2), allocate_block_pair());
		return half_1;
	}

	[[nodiscard]] vsm::result<void*> allocate_half_block(auto&... args)
	{
		if (m_free_half_block != nullptr)
		{
			return std::exchange(m_free_half_block, nullptr);
		}

		vsm_try_bind((half_1, half_2), allocate_block_pair(args...));

		vsm_assert(m_free_half_block == nullptr);
		m_free_half_block = half_2;

		return half_1;
	}

	[[nodiscard]] vsm::result<block_pair> allocate_block_pair()
	{
		map_handle_t const* last_block_handle = &m_block_array->back();
		if (m_last_block_consume_offset == last_block_handle->size())
		{
			if (m_block_array->is_full())
			{
				return vsm::unexpected(allio_error(error::maximum_capacity_exceeded));
			}

			vsm_try(handle, nothrow::map_memory(
				2 * last_block_handle->size(),
				initial_commit(false)));

			m_last_block_commit_offset = 0;
			m_last_block_consume_offset = 0;

			last_block_handle = &m_block_array->emplace_back(vsm_move(handle));
		}

		return allocate_block_pair(*last_block_handle);
	}

	[[nodiscard]] vsm::result<block_pair> allocate_block_pair(map_handle_t const& handle)
	{
		if (m_last_block_consume_offset == m_last_block_commit_offset)
		{
			vsm_assert(m_last_block_commit_offset != handle.size());

			size_t const page_size = detail::get_default_page_size();
			vsm_try_void(handle.commit(
				static_cast<unsigned char*>(handle.base()) + m_last_block_commit_offset,
				page_size));
			m_last_block_commit_offset += page_size;
		}

		unsigned char* const base = static_cast<unsigned char*>(handle.base());
		unsigned char* const half_1 = base + m_last_block_consume_offset;
		unsigned char* const half_2 = half_1 + half_block_size;

		m_last_block_consume_offset += full_block_size;
		return block_pair{ half_1, half_2 };
	}
};

} // namespace allio
