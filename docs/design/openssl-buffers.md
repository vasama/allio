# OpenSSL I/O Buffer Allocation

Socket I/O using OpenSSL requires the use of intermediate buffers to store
encrypted data sent or received via the underlying raw socket. The allocation
of these buffers has been causing some headache. Ideally the wrapping socket
would perform all of its raw byte I/O using registered buffers.
