#pragma once

#include <allio/detail/assert.hpp>

#include <optional>
#include <type_traits>

namespace allio::detail {

enum { unique_resource_sentinel };

template<typename Resource, typename Deleter, auto Sentinel = unique_resource_sentinel>
class unique_resource;

template<typename Resource, auto Sentinel = unique_resource_sentinel>
class unique_resource_storage
{
	Resource m_resource;

public:
	[[nodiscard]] Resource const& get() const
	{
		vsm_assert(m_resource != Sentinel);
		return m_resource;
	}

	explicit operator bool() const
	{
		return m_resource != Sentinel;
	}

protected:
	unique_resource_storage(Resource const resource = Sentinel)
		: m_resource(resource)
	{
	}

	unique_resource_storage(unique_resource_storage&& source)
		: m_resource(source.m_resource)
	{
		source.m_resource = Sentinel;
	}

	unique_resource_storage& operator=(unique_resource_storage&& source) &
	{
		m_resource = source.m_resource;
		source.m_resource = Sentinel;
		return *this;
	}

	void set(Resource const resource)
	{
		m_resource = resource;
	}

	void clear()
	{
		m_resource = Sentinel;
	}

	template<typename, typename, auto>
	friend class unique_resource;
};

template<typename Resource>
class unique_resource_storage<Resource, unique_resource_sentinel>
{
	std::optional<Resource> m_resource;

public:
	[[nodiscard]] Resource const& get() const
	{
		return *m_resource;
	}

	explicit operator bool() const
	{
		return m_resource.has_value();
	}

protected:
	unique_resource_storage() = default;

	unique_resource_storage(Resource const resource)
		: m_resource(resource)
	{
	}

	unique_resource_storage(unique_resource_storage&& source)
		: m_resource(source.m_resource)
	{
		source.m_resource.reset();
	}

	unique_resource_storage& operator=(unique_resource_storage&& source) &
	{
		m_resource = source.m_resource;
		source.m_resource.reset();
		return *this;
	}

	void set(Resource const resource)
	{
		m_resource = resource;
	}

	void clear()
	{
		m_resource.reset();
	}

	template<typename, typename, auto>
	friend class unique_resource;
};

template<typename Resource, typename Deleter, auto Sentinel>
class unique_resource : public unique_resource_storage<Resource, Sentinel>, Deleter
{
	using storage_type = unique_resource_storage<Resource, Sentinel>;

	static_assert(std::is_trivially_copyable_v<Resource>);
	static_assert(std::is_nothrow_move_constructible_v<Deleter>);
	static_assert(std::is_nothrow_move_assignable_v<Deleter>);

public:
	unique_resource() = default;

	explicit unique_resource(Resource const resource)
		: storage_type(resource)
	{
	}

	explicit unique_resource(Resource const resource, Deleter&& deleter)
		: storage_type(resource)
		, Deleter(static_cast<Deleter&&>(deleter))
	{
	}

	explicit unique_resource(Resource const resource, Deleter const& deleter)
		: storage_type(resource)
		, Deleter(deleter)
	{
	}

	unique_resource(unique_resource&& source)
		: storage_type(static_cast<storage_type&&>(source))
		, Deleter(static_cast<Deleter&&>(source))
	{
	}

	unique_resource& operator=(unique_resource&& source) &
	{
		if (*this)
		{
			static_cast<Deleter const&>(*this)(this->get());
		}
		static_cast<storage_type&>(*this) = static_cast<storage_type&&>(source);
		static_cast<Deleter&>(*this) = static_cast<Deleter&&>(source);
		return *this;
	}

	~unique_resource()
	{
		if (*this)
		{
			static_cast<Deleter const&>(*this)(this->get());
		}
	}


	[[nodiscard]] Deleter const& get_deleter() const
	{
		return static_cast<Deleter const&>(*this);
	}


	[[nodiscard]] Resource release()
	{
		Resource resource = this->get();
		this->clear();
		return resource;
	}

	void reset()
	{
		if (*this)
		{
			static_cast<Deleter const&>(*this)(this->get());
		}
		this->clear();
	}

	void reset(Resource const resource)
	{
		if (*this)
		{
			static_cast<Deleter const&>(*this)(this->get());
		}
		this->set(resource);
	}
};


template<typename Resource, typename Deleter, auto Sentinel, typename Consumer>
std::invoke_result_t<Consumer&&, Resource> consume_resource(detail::unique_resource<Resource, Deleter, Sentinel>&& resource, Consumer&& consumer)
{
	auto r = static_cast<Consumer&&>(consumer)(resource.get());
	if (r)
	{
		(void)resource.release();
	}
	return r;
}

} // namespace allio::detail
