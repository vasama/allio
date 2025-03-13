# ALLIO: Asynchronous Low Level I/O

## Key Features

* Both synchronous (blocking) and asynchronous I/O is supported.
* Easy to use file I/O, including memory mapped files.
* Sockets and other networking primitives with support for TLS.
* Race-free filesystem access using directory handles.
* A low level API for spawning and managing processes.
* User friendly interfaces. Customizable with good defaults.
* Can be used without exceptions or runtime type information.

## Feature Showcase

### Memory mapped file

```CPP
namespace io = allio::blocking; // allio::blocking functions throw on error.

io::mapped<char> mapping = io::map_file_as<char>(allio::path_view("./hello.txt"));
std::print("{}", std::string_view(mapping));
```

### Memory mapped file without exceptions

```CPP
namespace io = allio::nothrow; // allio::nothrow functions return std::expected.

std::expected mapping = io::map_file_as<char>(allio::path_view("./hello.txt"));
if (!mapping)
    return std::unexpected(mapping.error());
std::print("{}", std::string_view(*mapping));
```

### Blocking socket client

```CPP
namespace io = allio::blocking;
using namespace allio::network_literals;

auto socket = io::connect("192.168.0.7:50000"_ipv4);
```

### Echo server using C++26 `std::execution`

```CPP
namespace io = allio::senders; // allio::senders functions return senders.
namespace ex = std::execution;

io::task<void> echo_server(allio::any_endpoint_view endpoint) {
    io::listen_socket_handle listen_socket = co_await io::listen(endpoint);

    ex::async_scope scope;
    while (true) {
        io::socket_handle socket = co_await listen_socket.accept();
        scope.spawn(
            handle_client(std::move(socket)) |
            ex::upon_error([](auto e) { std::print("error: {}", e); }));
    }
}

io::task<void> handle_client(io::socket_handle socket) {
    for (std::byte buffer[1024];;) {
        size_t size = co_await socket.read_some(as_read_buffer(buffer));
        co_await socket.write(as_write_buffer(buffer, size));
    }
}
```

### Spawning a subprocess

```CPP
namespace io = allio::blocking;

auto child_stdout = io::create_pipe(inheritable);
io::process_handle process = io::create_process(
    allio::path_view("/usr/bin/echo"),
    allio::process_arguments({ "hello" }),
    allio::redirect_stdout(child_stdout));

if (auto exit_code = process.wait().get_exit_code(); exit_code != 0)
    std::print("exit_code = {}\n", exit_code);
else
    std::print("{}\n", io::read_until_end<std::string>(child_stdout));
```

## Installation

<details>
<summary>Conan installation instructions</summary>

#### Local recipes index

Conan recipes for ALLIO are automatically generated at [vasama/conan-index](https://github.com/vasama/conan-index). To use the recipes locally, a Conan [local recipes index](https://docs.conan.io/2/tutorial/conan_repositories/setup_local_recipes_index.html) can be used:

```
git clone https://github.com/vasama/conan-index vasama-conan-index
conan remote add local-vasama-conan-index ./vasama-conan-index
```

This is the recommended solution for any scenario where high availability and protection against supply chain attacks is important.

The recipe index structure also makes it easy to host the packages on a private Artifactory instance if desired.

#### vasama.org remote

All recipes at [vasama/conan-index](https://github.com/vasama/conan-index) - including the ALLIO recipes - are also available at [conan.vasama.org](https://conan.vasama.org).

```
conan remote add vasama.org https://conan.vasama.org
```

This method should only be used for local development purposes and trying out the library. High availability of the remote cannot be guaranteed at this time.

</details>

<details>
<summary>vcpkg installation instructions</summary>

</details>
