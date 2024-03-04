# Registered byte I/O

### Linux `io_uring`
* Can be used for all types of byte I/O.
* Buffer registration is scoped to a specific `io_uring` instance.
* Accepts user provided memory for buffer storage.

### Windows WSA RIO extensions
* Can only be used with sockets created specifically for registered I/O.
* Registered I/O sockets cannot use regular unregistered buffer I/O.
* Buffer registeration is global and can be shared between queues.
* Accepts user provided memory for buffer storage.
