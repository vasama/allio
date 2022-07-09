# ABI Stability

ALLIO does not provide a stable ABI, except where explicitly specified.

## native_opaque_handle

`native_opaque_handle` is an intentionally ABI stable C-compatible struct. It represents an opaque asynchronously pollable object. If you require ABI stability in your DLL/SO interface, this type provides that guarantee.

For RAII you may wrap a `native_opaque_handle` in an `opaque_handle` on the user side before returning it. That means doing so in your public header or module.

If you wish to use a C API for the ABI stable interface of your library, the C type `allio_opaque_handle` exists to facilitate this. The C++ `native_opaque_handle` type is an alias for the former.
