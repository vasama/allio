# ABI Stability

ALLIO does not provide a stable ABI, except where explicitly specified.

## native_opaque_handle

`allio_abi_object` is an intentionally ABI stable C-compatible struct. It represents an opaque asynchronously pollable object. If you require ABI stability in your DLL/SO interface, this type provides that guarantee.

For RAII you may wrap a `allio_abi_object` in an `opaque_handle` on the user side before returning it. That means doing so in your public header or module.
