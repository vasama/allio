# Low Level Concepts

## Native Handle

Native handles are plain data structs which store the data needed for interacting with objects. Each native handle type is a specialization of the class template `native_handle<Object>`. The inheritance hierarchy of any native handle type is parallel to the inheritance hierarchy of corresponding object type.

For the most basic example, the native handle of `platform_object_t` contains a `native_platform_handle`, which itself is an enum wrapping a file descriptor, `HANDLE`, or other platform-specific handle type. It is derived from the native handle of `object_t`, from which `platform_object_t` itself is derived.

```CPP
template<>
struct native_handle<platform_object_t> : native_handle<platform_object_t::base_type>
{
    native_platform_handle platform_handle;
};
```

## Asynchronous Connector State

In order to perform asynchronous operations on an object, a multiplexer is needed. The coupling of a handle and multiplexer may require state. That state is stored in the class template `async_connector<Multiplexer, Object>` for a given pairing of multiplexer and object types. The connector may be stored entirely separate from the native handle. In fact it is possible to associate a single *native* handle with more than one multiplexer, in which case separate connectors are required for each coupling.

Each multiplexer type provides a member type named `connector_type`, from which any concrete `async_connector` specialization for that multiplexer should be derived. The appropriate `async_connector` specialization must be defined in order to couple a handle to a multiplexer, but in most cases there is no need for any data members specific to that specialization.

```CPP
template<>
struct native_handle<io_uring_multiplexer, my_object_t> : io_uring_multiplexer::connector_type
{
};
```

A prominent example of connector state comes from the Linux `io_uring_multiplexer`, which stores direct file indices in its `connector_type`. See `io_uring_register` documentation for more information.

```CPP
struct io_uring_multiplexer::connector_type
{
    int file_index;
};
```

## Asynchronous Operation State

Each asynchronous operation requires individual state. That state is stored in the class template `async_operation<Multiplexer, Object, Operation>` for a given triplet of multiplexer, object, and operation types.

Each multiplexer type provides a member type named `operation_type`, from which any concrete `async_operation` specialization for that multiplexer should be derived. The appropriate `async_operation` specialization must be defined in order to perform the requested operation. Static member functions `submit`, `notify`, and `cancel` provide the actual implementation of the operation. For 

```CPP
template<>
struct async_operation<io_uring_multiplexer, my_object_t, my_operation_t> : io_uring_multiplexer::operation_type
{
    using M = io_uring_multiplexer;
    using H = native_handle<my_object_t> const;
    using C = async_connector<io_uring_multiplexer, my_object_t> const;
    using S = async_operation<io_uring_multiplexer, my_object_t, my_operation_t>;
    using A = io_parameters_t<my_object_t, my_operation_t>;

    static io_result<R> submit(
        M& m,
        H& h,
        C& c,
        S& s,
        A const& a,
        io_handler<M>& handler);

    static io_result<R> notify(
        M& m,
        H& h,
        C& c,
        S& s,
        A const& a,
        io_handler<M>& handler,
        M::io_status_type status);

    static void cancel(
        M& m,
        H const& h,
        C const& c,
        S& s);
};
```

## `submit_io`

## `notify_io`

## `cancel_io`
