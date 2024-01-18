# Windows Paths

## Win32 Paths

For Win32 paths ALLIO always treats both accepted separators (`\` and `/`) as equivalent. In some very niche cases Win32 APIs treat the two differently, but any such paths are rejected by ALLIO for other reasons.

### Relative Paths

These are paths of the form `x/./y/../z`.

Within a Windows user mode program, such paths are relative to the current working directory.
Users of ALLIO can specify a root directory via a handle to be used in place of the current working directory.

### Drive Relative Paths

These paths are of the form `C:x/y/z`.

They are prefixed with a drive letter (`C:`) which is not followed by a separator and is optionally followed by a relative path.
Within a Windows user mode program, such paths are relative to the current working directory on the specified drive.

### Current Drive Absolute Paths

These paths are of the form `/x/y/z`.

They are prefixed with a separator and optionally followed by a relative path.
Within a Windows user mode program, such paths are relative to the root directory of the current drive.

### Drive Absolute Paths

These paths are of the form `C:/x/y/z`.

They are prefixed with a drive letter (`C:`) followed by a separator and optionally followed by a relative path.

### UNC Absolute Paths

These are paths of the form `//server/share/x/y/z`.

They are prefixed with two separators followed by a server name (`server`) followed by a separator and a share name (`share`) and optionally followed by a separator and a relative path.

### Local Device Paths

These are paths of the form `//./x/y/z`.

They are prefixed with the local device prefix `//.` and optionally followed by a separator and a relative path.
Within a Windows user mode program, such paths are relative to the NT object manager.

### Root Local Device Paths

These are paths of the form `//?/x/y/z`.

They are prefixed with the local device prefix `//?` and optionally followed by a separator and a relative path.
They behave similarly to local device paths, but are not canonicalized during conversion to NT object paths.
In contrast to the Win32 APIs, any such paths which are not already in canonical form are rejected by ALLIO.

## NT Kernel Compatible Paths

### Canonical Relative Paths

These are paths of the form `x\y\z`.

They do not contain any of the following:
* Relative components (`.` or `..`).
* Non-canonical separators (`/`).
* Duplicate separators (`\\`).

Such paths are accepted by both Win32 and NT APIs, but when passed to the kernel require specifying a root directory handle.

### NT Object Paths

These are paths of the form `\??\x\y\z`.

They are prefixed with the NT object path prefix (`\??\`) and followed by a canonical relative path.
Such paths are accepted by both Win32 and NT APIs and are always passed to the kernel unchanged and without validation by Win32 APIs. Note that the kernel still does its own validation.

## Differences Between Win32 and ALLIO Path Handling

As a general rule, ALLIO does not accept any paths which would be rejected by Win32 APIs, except when they would be rejected due to length restrictions related to `MAX_PATH`.
On the other hand, ALLIO does reject some paths which would be accepted by Win32 APIs, when the Win32 behaviour can be considered harmful.

This means that all files produced by ALLIO can also be opened using Win32 APIs, and vice versa, though to open certain files using ALLIO may require a .

### Win32 Paths Rejected by ALLIO

* Any path containing a relative component with trailing dots and/or spaces. E.g. (`x/y./z`).
  The reason for this is that Win32 APIs will quietly strip any trailing dots and spaces.
* Any path containing a relative component with a special device name, optionally followed by spaces, a dot or colon and other characters. E.g. (`C:/NUL`, `C:/NUL.txt`, `C:/NUL:txt`)
  The reason for this is that Win32 APIs will either reject or quietly convert any such paths to paths referring to the specified device.

  The special device names are:
  * `AUX`
  * `PRN`
  * `NUL`
  * `COM0` to `COM9`
  * `LPT0` to `LPT9`
  * `CON`
  * `CONIN$`
  * `CONOUT$`
* Any root local device paths containing a non-canonical relative path.
  The reason for this is that Win32 APIs will sometimes skip canonicalization of such paths.
  ALLIO could follow suit and simply convert such paths to NT object paths, but passing non-canonical paths to the kernel is not particularly useful. For passing paths to the kernel directly, the NT object path prefix (`\??\`) can be used.
* Any relative paths when the current working directory path is a local device path.
  The reason for this is that Win32 APIs have unpredictable behaviour when the current working directory path is a local device path.
  E.g. when the current working directory path is `//./C:/`, the path `/` is converted to `\??\UNC\` by the Win32 APIs.
* Any drive relative paths on a drive which is not the drive of the current working directory and when the path on that drive is not a drive absolute path on that drive. E.g. the path `X:Y` is rejected when the path on drive `X:` is `Z:\`. Win32 APIs in contrast would interpret the path as `Z:\Y`.