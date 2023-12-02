# Library Design Overview


## Handle

A handle represents some I/O resource such as file, directory, socket, etc.

Some examples of concrete handle types:
* `file_handle`: Represents a file on a filesystem and is capable of scatter/gather random access byte I/O.
* `stream_socket_handle`: Represents a stream socket and is capable of scatter/gather stream byte I/O.
* `listen_socket_handle`: Represents a listening socket and is capable of accepting stream socket connections.


## Multiplexer

A multiplexer represents a mechanism for submitting asynchronous operations on handles and for communicating the completion of those operations back to the user.

Some examples of concrete root multiplexer types:
* `win32::iocp_multiplexer`: A Windows specific multiplexer employing the I/O Completion Port API.
* `linux::io_uring_multiplexer`: A Linux specific multiplexer employing the io_uring API.
* `manual_multiplexer`: A generic multiplexer which leaves signalling of operation completion to the user. One use case is in freestanding applications where completion may be signaled by an interrupt handler.
