Standard streams (stdin, stdout, stderr) might use a console object on Windows. This requires some special handling. Probably a separate `standard_stream_handle` type. Windows uses IOCTLs to interact with condrv (Console Driver). It is likely possible to implement asynchronous console read by calling `NtDeviceIoControlFile` with an event and using a wait packet to wait for I/O completion.

https://devblogs.microsoft.com/commandline/windows-command-line-inside-the-windows-console/
https://learn.microsoft.com/pdf?url=https%3A%2F%2Flearn.microsoft.com%2Fen-us%2Fwindows%2Fconsole%2Ftoc.json
