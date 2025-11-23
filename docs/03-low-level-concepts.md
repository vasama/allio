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

Each multiplexer type provides a member type named `operation_type`, from which any concrete `async_operation` specialization for that multiplexer should be derived. The appropriate `async_operation` specialization must be defined in order to perform the requested operation. Static member functions `submit`, `notify`, and `cancel` provide the actual implementation of the operation.

```CPP
template<>
struct async_operation<io_uring_multiplexer, my_object_t, my_operation_t>
    : io_uring_multiplexer::operation_type
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

## Asynchronous operation flow

### Submission

The first step in the lifecycle of an asynchronous operation is submission. What the submission of an operation entails naturally depends on the object, the operation, and the multiplexer in question. Submission can generally be divided into three phases:
* #### Argument validation
  In most cases this mirrors the usual argument validation performed when the same operation is executed synchronously.
  <br>
  Depending on the object and multiplexer, asynchronous execution of some operations may place additional requirements on their arguments, as compared to simple synchronous execution.
* #### Resource allocation
  TODO: The need for resource allocation is not unique to asynchronous I/O. Move this explanation elsewhere.
  <br>
  <br>
  In many cases no per-operation resource allocations are needed. On the other hand, ALLIO is fairly permissive in the data formats accepted by operations, and in some cases using a non-native format may incur additional memory allocations.
  <br>
  <br>
  Common cases leading to per-operation dynamic memory allocations are:
  * Scatter-gather byte I/O using non-native buffer formats.
    <br>
    To avoid allocations, use the native buffer format (see `allio::io_buffer_layour`).
    <br>
    TODO: There is currently no way to query the format used by an operation.
  * Network operations involving endpoints when generic endpoints are used.
    <br>
    To avoid allocations, use opaque platform endpoints (see `allio::platform_endpoint`).

  In some cases additional temporary system resources such as kernel event objects may be required. It is not generally possible to avoid the allocation of these objects, but the cases where they are needed are rare and tend to involve complex and inherently costly operations such as process creation. Where possible, ALLIO attempts to pool and reuse such objects in order to reduce per-operation overhead.
* #### Initiation
  Once everything is in place, the operation is initiated using a multiplexer specific mechanism. Usually the operation then enters a pending state and progress notifications are made via the associated I/O handler.
  <br>
  In some cases an operation may complete synchronously. If this happens, the final operation result is returned from `submit_io` and no progress notifications are made.
  <br>
  See the section on notification for information on how to handle the `submit_io` result.

### Notification

The implementation of a single user-facing operation may involve one or more concrete I/O operations. For example, if the platform does not natively support scatter-gather byte I/O, multiple single buffer operations are needed.

Any time a concrete I/O operation completion is handled, the associated `io_handler` is invoked. It is the job of the handler to notify the I/O operation by calling `notify_io` with arguments equivalent to those used for the submission of the operation. There is no limit on the number of times an operation may have to be notified before completion.

Where most fallible APIs in ALLIO return `expected<T, error_code>`, `notify_io` (as well as `submit_io`) returns `expected<T, io_error_code>`. `io_error_code` is derived from `error_code`, and carries an extra enum value describing the status of an asynchronous operation. That enum is `io_notify_status`.

`io_notify_status` has three values:
1. #### `completed`
   The operation has completed, either successfully or erroneously.
   <br>
   Note that when an operation completes successfully, the resulting expected carries a value instead of an error, and so no `io_notify_status` is returned.
2. #### `submitted`
   The operation has been submitted and is pending completion.
   <br>
   In this state, the `io_handler` may be invoked to handle notifications.
3. #### `cancelled`
   The operation has completed erroneously due to cancellation.
   <br>
   It is sometimes important to know whether the operation failed or was cancelled. Due to the variety of error codes resulting from I/O cancellation, it is difficult to detect cancellation by inspecting the error code. One use of this information is found in the I/O senders which complete via `set_stopped` instead of `set_error` upon cancellation.

### Cancellation


