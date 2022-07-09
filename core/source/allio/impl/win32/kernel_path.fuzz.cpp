#include <allio/impl/win32/kernel_path.hpp>

#include <allio/impl/win32/kernel.hpp>
#include <allio/win32/kernel_error.hpp>

#include <string>

using namespace allio;
using namespace allio::win32;

static FARPROC ntdll_import(char const* const name)
{
	static HMODULE const ntdll = GetModuleHandleA("NTDLL.DLL");
	return GetProcAddress(ntdll, name);
}

enum RTL_PATH_TYPE
{
	RtlPathTypeUnknown,
	RtlPathTypeUncAbsolute,
	RtlPathTypeDriveAbsolute,
	RtlPathTypeDriveRelative,
	RtlPathTypeRooted,
	RtlPathTypeRelative,
	RtlPathTypeLocalDevice,
	RtlPathTypeRootLocalDevice
};

static RTL_PATH_TYPE RtlDetermineDosPathNameType_U(
	_In_ PCWSTR Path)
{
	using function_ptr = decltype(RtlDetermineDosPathNameType_U) NTAPI*;
	static function_ptr const function = reinterpret_cast<function_ptr>(ntdll_import("RtlDetermineDosPathNameType_U"));
	return function(Path);
}

static NTSTATUS RtlDosPathNameToRelativeNtPathName_U_WithStatus(
	_In_ PCWSTR DosFileName,
	_Out_ PUNICODE_STRING NtFileName,
	_Out_opt_ PWSTR* FilePath,
	_Out_opt_ PRTL_RELATIVE_NAME RelativeName)
{
	using function_ptr = decltype(RtlDosPathNameToRelativeNtPathName_U_WithStatus) NTAPI*;
	static function_ptr const function = reinterpret_cast<function_ptr>(ntdll_import("RtlDosPathNameToRelativeNtPathName_U_WithStatus"));
	return function(DosFileName, NtFileName, FilePath, RelativeName);
}


using rtl_kernel_path_storage = std::wstring;

static vsm::result<kernel_path> rtl_convert_path(rtl_kernel_path_storage& storage, wchar_t const* const win32_path)
{
	UNICODE_STRING rtl_full_kernel_path = {};
	RTL_RELATIVE_NAME rtl_relative_name = {};

	NTSTATUS const status = RtlDosPathNameToRelativeNtPathName_U_WithStatus(
		win32_path, &rtl_full_kernel_path, nullptr, &rtl_relative_name);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	UNICODE_STRING const rtl_kernel_path =
		rtl_relative_name.ContainingDirectory != NULL
			? rtl_relative_name.RelativeName
			: rtl_full_kernel_path;

	storage.resize(rtl_kernel_path.Length / sizeof(wchar_t));
	memcpy(storage.data(), rtl_kernel_path.Buffer, rtl_kernel_path.Length);
	HeapFree(NtCurrentPeb()->ProcessHeap, 0, rtl_full_kernel_path.Buffer);

	return kernel_path
	{
		.handle = rtl_relative_name.ContainingDirectory,
		.path = std::wstring_view(storage),
	};
}

static vsm::result<kernel_path> rtl_make_kernel_path(rtl_kernel_path_storage& storage, kernel_path_parameters const& args)
{
	wchar_t const* const win32_path = args.path.wide_c_string();

	switch (RtlDetermineDosPathNameType_U(win32_path))
	{
	case RtlPathTypeUnknown:
		return vsm::unexpected(error::invalid_path);

	case RtlPathTypeRelative:
		{
			std::wstring string = L"//./";
			string += args.path.wide_view();

			vsm_try_discard(rtl_convert_path(storage, string.c_str()));
			storage.erase(storage.begin(), storage.begin() + 4);

			return kernel_path
			{
				.handle = args.handle,
				.path = storage,
			};
		}

	case RtlPathTypeDriveRelative:
	case RtlPathTypeRooted:
		if (args.handle == NULL)
		{
			return vsm::unexpected(error::invalid_path);
		}

	case RtlPathTypeUncAbsolute:
	case RtlPathTypeDriveAbsolute:
	case RtlPathTypeLocalDevice:
	case RtlPathTypeRootLocalDevice:
		return rtl_convert_path(storage, win32_path);
	}

	return vsm::result<kernel_path>(vsm::result_value);
}

static vsm::result<void> fuzz_main(uint8_t const* data_beg, uint8_t const* data_end)
{
	uint8_t const flags = data_beg != data_end ? *data_beg++ : 0;
	HANDLE const handle = flags & 0x01 ? GetCurrentProcess() : NULL;
	bool const wide_string = flags & 0x02;
	bool const win32_api_form = flags & 0x04;

	if (wide_string && (data_end - data_beg) % 2)
	{
		--data_end;
	}

	size_t const data_size = data_end - data_beg;
	input_string_view const input_string = wide_string
		? input_string_view(reinterpret_cast<wchar_t const*>(data_beg), data_size / sizeof(wchar_t))
		: input_string_view(reinterpret_cast<char const*>(data_beg), data_size / sizeof(char));

	kernel_path_parameters const args =
	{
		.handle = handle,
		.path = input_path_view(input_string),
		.win32_api_form = win32_api_form,
	};

	kernel_path_storage storage_1;
	auto const kernel_path_r1 = make_kernel_path(storage_1, args);

	rtl_kernel_path_storage storage_2;
	auto const kernel_path_r2 = rtl_make_kernel_path(storage_2, args);

	if (kernel_path_r2)
	{
		if (kernel_path_r1)
		{
			if (kernel_path_r2->path != kernel_path_r2->path ||
				kernel_path_r1->handle != kernel_path_r2->handle)
			{
				return vsm::unexpected(error::unsupported_operation);
			}
		}
	}
	else
	{
		if (kernel_path_r1)
		{
			return vsm::unexpected(error::unsupported_operation);
		}
	}

	return {};
}


extern "C"
int LLVMFuzzerInitialize(int* const argc, char const* const* const* const argv)
{
	return 0;
}

extern "C"
int LLVMFuzzerTestOneInput(uint8_t const* const data, size_t const size)
{
	if (auto const r = fuzz_main(data, data + size); !r)
	{
		__debugbreak();
	}
	return 0;
}
