# High Level Concepts

## Object

Objects are the things on which I/O can be performed. Some examples of objects are files, directories, sockets, processes, threads among others. Note that in most cases these objects are not directly represented by any C++ objects. Instead objects generally exist only in the host operating system and are managed by the program through the use of handles. Different kinds of objects are represented by types derived from `object_t`.

## Handle

Handles are what the program uses to manage objects. Depending on the platform and the kind of object in question, a handle usually contains a platform specific host operating system handle such as a file descriptor on UNIX based operating systems or a HANDLE on Windows. Unless otherwise specified, handles are non-unique owners of the objects they refer to. This means that the object is released when the last handle referring to it is destroyed. What this release entails depends on the kind of object. Note that unlike say the `std::shared_ptr`, another non-unique owner type, handles are not copyable. Shared ownership of an object is achieved through handle duplication. Handles can even be duplicated across multiple processes.

## Operation

It is through operations that the program interacts with objects. Like objects, operations are represented by types describing the requirements and effects of the operation.

## Multiplexer

The time taken by many operations is unbounded. This means that the operation is not guaranteed to complete in any finite amount of time. For this reason it is especially desirable to perform operations asynchronously without blocking an expensive thread of execution. Multiplexers are what facilitate this asynchronous operation. Multiplexers usually represent some operating system object providing a stream of completion information for the results of asynchronous operations. Some multiplexers, such as the Linux io_uring multiplexer, also provide a channel for the submission of operations to the operating system.

Before the program can submit asynchronous operations, the relevant object, via its handle, must be attached to a multiplexer instance. Depending on the multiplexer this coupling may be exclusive, meaning that the object can only be attached to a single multiplexer at a time. The coupling may also be permanent, meaning that once attached, the object could not be decoupled from the multiplexer. The most prominent example of such restrictions is the Windows I/O Completion Port multiplexer.
