# Library Design Overview


## Handle

A handle represents some I/O resource such as file, directory, socket, etc.

Some examples of concrete handle types:
* `file_handle`
  <br>
  Represents a file on a filesystem and is capable of random access scatter-gather byte I/O.
* `stream_socket_handle`
  <br>
  Represents a stream socket and is capable of scatter-gather byte I/O.
* `listen_socket_handle`
  <br>
  Represents a listening socket and is capable of accepting stream socket connections.


## Multiplexer

A multiplexer represents a mechanism for submitting asynchronous operations on handles and for communicating the completion of those operations back to the user.

Some examples of concrete multiplexer types:
* `win32::iocp_multiplexer`
  <br>
  A Windows specific multiplexer employing the I/O Completion Port API.
* `linux::io_uring_multiplexer`
  <br>
  A Linux specific multiplexer employing the io_uring API.
* `manual_multiplexer`
  <br>
  A generic multiplexer which leaves signalling of operation completion to the user. One use case is in freestanding applications where completion may be signaled by an interrupt handler.


## Error handling




## Namespaces

