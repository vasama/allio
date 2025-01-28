cmake_minimum_required(VERSION 3.24)
project(allio_senders)

find_package(vsm.cmake REQUIRED)
vsm_define_package(allio.senders)

vsm_add_library(
	allio::senders

	HEADERS
		include/allio/detail/execution.hpp
		include/allio/detail/senders/get_multiplexer.hpp
		include/allio/detail/senders/senders.hpp
		include/allio/detail/senders/sync_wait.hpp
		include/allio/detail/senders/task.hpp

		include/allio/senders/get_multiplexer.hpp
		include/allio/senders/sync_wait.hpp
		include/allio/senders/task.hpp
		include/allio/senders/traits.hpp
		include/allio/senders/via.hpp

		# Handles
		include/allio/senders/directory.hpp
		include/allio/senders/event.hpp
		include/allio/senders/file.hpp
		include/allio/senders/pipe.hpp
		include/allio/senders/process.hpp
		include/allio/senders/raw_datagram_socket.hpp
		include/allio/senders/raw_listen_socket.hpp
		include/allio/senders/raw_socket.hpp

	HEADER_DEPENDENCIES
		allio::core
		vsm::intrusive
		stdexec::stdexec
)
