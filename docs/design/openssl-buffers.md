# OpenSSL I/O Buffer Allocation
Socket I/O using OpenSSL requires the use of intermediate buffers to store
encrypted data sent or received via the underlying raw socket. The allocation
of these buffers has been causing some headache. Ideally the wrapping socket
would perform all of its raw byte I/O using registered buffers.

### On registered I/O buffers:
* Acquire buffer operation: waits when no buffers are available
* Release buffer operation: releases the buffer, causes an acquire operation to be completed

### On non-registered OpenSSL socket I/O:
* The handle stores a type-erased memory pool reference.
* The default pool should just use a global allocator.
* If the user wants to use different pools for the same object, the handle can be duplicated with a different pool.
