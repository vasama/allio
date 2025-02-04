# OpenSSL I/O Buffer Allocation
Socket I/O using OpenSSL requires the use of intermediate buffers to store encrypted data sent or received via the underlying raw socket. The allocation of these buffers has been causing some headache. Ideally the wrapping socket would perform all of its raw byte I/O using registered buffers.

### When performing registered I/O:
* Acquire buffer operation: waits when no buffers are available
* Release buffer operation: releases the buffer, causes an acquire operation to be completed
* TODO: Acquire/release on what handle type? New registered_buffer_pool_t?

### When performing non-registered I/O:
* The handle stores a type-erased memory pool reference.
* The default pool should just use a global allocator.
* If the user wants to use different pools for the same object, the handle can be duplicated with a different pool.
