[[nodiscard]] auto create_event(detail::event_mode const mode, auto&&... args)
{
	using namespace detail;
	return io_traits::produce<event_t, event_io::create_t>(
		make_args<io_parameters_t<event_t, event_io::create_t>>(
			event_mode_t{ mode },
			vsm_forward(args)...));
}
