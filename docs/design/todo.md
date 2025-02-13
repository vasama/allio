* Network endpoint redesign
* Async file I/O implementation
* OpenSSL TLS implementation
* File I/O stream interface.
* Full byte I/O interfaces. (read_all, read_until, read_while, ...)
* Registered buffer I/O.
* Split io_flags into separate general/create/byte-io flags. Maybe similar to handle_flags.
* DONE: Use overlapped I/O as default on Windows. Make synchronous I/O opt-in.
