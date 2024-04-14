#include "IObitUnlocker.hpp"

#include "IObitDriver.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <fstream>

IObitUnlocker::IObitUnlocker()
{
	this->driver_path = "";
}

bool IObitUnlocker::Start()
{
	if (!CreateDriverFile())
		return false;

	if (!CreateDriverService())
		return false;

	if (!StartDriver())
		return false;

	return true;
}

bool IObitUnlocker::Stop()
{
	if (!StopDriver())
		return false;

	if (!DeleteDriverService())
		return false;

	if (!RemoveDriverFile())
		return false;

	return true;
}

std::string IObitUnlocker::DumpInfo(std::string path)
{
	HANDLE io_bit = CreateFileA("\\\\.\\IObitUnlockerDevice", 0xC0100000, 3u, 0, 3u, 0, 0);
	if (io_bit == INVALID_HANDLE_VALUE)
		return {};

	char* input_buffer = (char*)malloc(1064);
	char* output_buffer = (char*)malloc(1024);

	std::string result;
	if (input_buffer && output_buffer)
	{
		memset(input_buffer, 0, 1064);
		memset(output_buffer, 0, 1024);

		if (!path.empty())
		{
			wchar_t wide_path[MAX_PATH] = {};
			if (!ToWideStr(path, wide_path))
			{
				printf("[!] Failed To Convert Path To Wide String");
				free(input_buffer);
				free(output_buffer);
				CloseHandle(io_bit);
				return {};
			}
			wcscpy_s((wchar_t*)input_buffer, sizeof(wide_path), wide_path);
		}

		unsigned long bytes_returned = 0;
		if (DeviceIoControl(io_bit, IOCTL_DUMP_INFO, input_buffer, 1064, output_buffer, 1024, &bytes_returned, nullptr))
			result = WideToStr((wchar_t*)output_buffer);
	}

	free(input_buffer);
	free(output_buffer);
	CloseHandle(io_bit);
	return result;
}

bool IObitUnlocker::UnlockFile(std::string path)
{
	return SendUnlockRequestWrapper(path, "", 0, 3);
}

bool IObitUnlocker::UnlockDeleteFile(std::string path)
{
	return SendUnlockRequestWrapper(path, "", 1, 3);
}

bool IObitUnlocker::UnlockRenameFile(std::string path, std::string new_name)
{
	return SendUnlockRequestWrapper(path, new_name, 2, 3);
}

bool IObitUnlocker::UnlockMoveFile(std::string path, std::string new_path)
{
	return SendUnlockRequestWrapper(path, new_path, 3, 3);
}

bool IObitUnlocker::UnlockCopyFile(std::string path, std::string new_path)
{
	return SendUnlockRequestWrapper(path, new_path, 4, 3);
}

bool IObitUnlocker::CreateDriverFile()
{
	char path[MAX_PATH];
	GetModuleFileNameA(0, path, MAX_PATH);
	if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
	{
		printf("[!] Failed ERROR_INSUFFICIENT_BUFFER\n");
		return 0;
	}

	this->driver_path = path;
	this->driver_path = this->driver_path.substr(0, this->driver_path.find_last_of('\\') + 1);
	this->driver_path += DRIVER_NAME;

	//create a file from the driver byte array
	std::ofstream out_file(this->driver_path.c_str(), std::ios::out | std::ios::binary);
	out_file.write((const char*)io_bit_unlocker_driver, sizeof(io_bit_unlocker_driver));
	out_file.close();
}

bool IObitUnlocker::RemoveDriverFile()
{
	return DeleteFileA(this->driver_path.c_str());
}

bool IObitUnlocker::CreateDriverService()
{
	SC_HANDLE sc_manager = OpenSCManagerA(0, 0, SC_MANAGER_ALL_ACCESS);
	if (!sc_manager)
	{
		printf("[!] Failed To Open A Handle To SCManager\n");
		return false;
	}

	SC_HANDLE service = CreateServiceA(sc_manager, "IObitUnlocker", "IObitUnlocker", SERVICE_ALL_ACCESS, 1, 3, 0, this->driver_path.c_str(), 0, 0, 0, 0, "IObitUnlocker");

	unsigned long last_error = GetLastError();
	if (last_error == ERROR_SERVICE_EXISTS)
	{
		service = OpenServiceA(sc_manager, "IObitUnlocker", SC_MANAGER_ALL_ACCESS);
		if (!service)
		{
			CloseServiceHandle(sc_manager);
			return false;
		}

		bool change_service_status = ChangeServiceConfigA(service, 1u, 3u, 0, this->driver_path.c_str(), 0, 0, 0, 0, 0, "IObitUnlocker");
		if (!change_service_status)
		{
			CloseServiceHandle(service);
			CloseServiceHandle(sc_manager);
			return false;
		}
	}
	else if (last_error != ERROR_SUCCESS)
	{
		CloseServiceHandle(service);
		CloseServiceHandle(sc_manager);
		return false;
	}

	const char* new_path = "File driver for IObitUnlocker";
	bool change_service_status = ChangeServiceConfig2W(service, 1u, &new_path);

	CloseServiceHandle(service);
	CloseServiceHandle(sc_manager);
	return change_service_status;
}

bool IObitUnlocker::DeleteDriverService()
{
	SC_HANDLE sc_manager = OpenSCManagerA(0, 0, SC_MANAGER_ALL_ACCESS);
	if (!sc_manager)
	{
		printf("[!] Failed To Open A Handle To SCManager\n");
		return false;
	}

	SC_HANDLE service = OpenServiceA(sc_manager, "IObitUnlocker", SERVICE_ALL_ACCESS);
	if (service)
	{
		bool exit_status = DeleteService(service);
		CloseServiceHandle(service);
		CloseServiceHandle(sc_manager);
		return exit_status;
	}

	if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
	{
		CloseServiceHandle(sc_manager);
		return true;
	}

	CloseServiceHandle(sc_manager);
	return false;
}

bool IObitUnlocker::StartDriver()
{
	SC_HANDLE sc_manager = OpenSCManagerA(0, 0, SC_MANAGER_ALL_ACCESS);
	if (!sc_manager)
	{
		printf("[!] Failed To Open A Handle To SCManager\n");
		return false;
	}

	SC_HANDLE service = OpenServiceA(sc_manager, "IObitUnlocker", SERVICE_ALL_ACCESS);
	if (!service)
	{
		CloseServiceHandle(sc_manager);
		return false;
	}

	_SERVICE_STATUS service_status = {};
	if (QueryServiceStatus(service, &service_status))
	{
		if (service_status.dwCurrentState == 1)
		{
			if (!ChangeServiceConfigA(service, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, 0, 0, 0, 0, 0, 0, "IObitUnlocker") || !StartServiceA(service, 0, 0) || !QueryServiceStatus(service, &service_status))
			{
				CloseServiceHandle(service);
				CloseServiceHandle(sc_manager);
				return true;
			}

			while (true)
			{
				Sleep(service_status.dwWaitHint);
				if (service_status.dwCurrentState == 4 || !service_status.dwWaitHint)
					break;

				if (!QueryServiceStatus(service, &service_status))
				{
					CloseServiceHandle(service);
					CloseServiceHandle(sc_manager);
					return false;
				}
			}

			if (service_status.dwCurrentState == 4)
			{
				CloseServiceHandle(service);
				CloseServiceHandle(sc_manager);
				return true;
			}
		}
	}
	CloseServiceHandle(service);
	CloseServiceHandle(sc_manager);
	return true;
}

bool IObitUnlocker::StopDriver()
{
	SC_HANDLE sc_manager = OpenSCManagerA(0, 0, SC_MANAGER_ALL_ACCESS);
	if (!sc_manager)
	{
		printf("[!] Failed To Open A Handle To SCManager\n");
		return false;
	}

	SC_HANDLE service = OpenServiceA(sc_manager, "IObitUnlocker", SERVICE_ALL_ACCESS);
	if (!service)
	{
		CloseServiceHandle(sc_manager);
		return false;
	}

	_SERVICE_STATUS service_status = {};
	if (QueryServiceStatus(service, &service_status))
	{
		if (service_status.dwCurrentState == 4)
		{
			if (!ControlService(service, 1u, &service_status) || !QueryServiceStatus(service, &service_status))
			{
				CloseServiceHandle(service);
				CloseServiceHandle(sc_manager);
				return false;
			}

			while (true)
			{
				Sleep(service_status.dwWaitHint);
				if (service_status.dwCurrentState == 1 || !service_status.dwWaitHint)
					break;

				if (!QueryServiceStatus(service, &service_status))
				{
					CloseServiceHandle(service);
					CloseServiceHandle(sc_manager);
					return false;
				}
			}
		}

		if (service_status.dwCurrentState == 1)
		{
			bool exit_status = ChangeServiceConfigW(service, SERVICE_KERNEL_DRIVER, SERVICE_DISABLED, SERVICE_ERROR_IGNORE, 0, 0, 0, 0, 0, 0, L"IObitUnlocker");
			CloseServiceHandle(service);
			CloseServiceHandle(sc_manager);
			return exit_status;
		}
	}
	CloseServiceHandle(service);
	CloseServiceHandle(sc_manager);
	return false;
}

bool IObitUnlocker::SendUnlockRequestWrapper(std::string path, std::string new_path, unsigned long param_1, unsigned long param_2)
{
	HANDLE io_bit = CreateFileA("\\\\.\\IObitUnlockerDevice", 0xC0100000, 3u, 0, 3u, 0, 0);
	if (io_bit == INVALID_HANDLE_VALUE)
		return false;

	char* input_buffer = (char*)malloc(1064);
	if (!input_buffer)
	{
		CloseHandle(io_bit);
		return false;
	}

	char* output_buffer = (char*)malloc(1024);
	if (!output_buffer)
	{
		CloseHandle(io_bit);
		return false;
	}

	memset(input_buffer, 0, 1064);
	memset(output_buffer, 0, 1024);

	if (!path.empty())
	{
		wchar_t wide_path[MAX_PATH] = {};
		if (!ToWideStr(path, wide_path))
		{
			printf("[!] Failed To Convert Path To Wide String");
			free(input_buffer);
			free(output_buffer);
			return false;
		}
		wcscpy_s((wchar_t*)input_buffer, wcslen(wide_path) + 1, wide_path);
	}

	if (!new_path.empty())
	{
		wchar_t wide_new_path[MAX_PATH] = {};
		if (!ToWideStr(new_path, wide_new_path))
		{
			printf("[!] Failed To Convert Path To Wide String");
			free(input_buffer);
			free(output_buffer);
			return false;
		}
		wcscpy_s((wchar_t*)(input_buffer + 0x210), wcslen(wide_new_path) + 1, wide_new_path);
	}

	input_buffer[0x420] = param_1;
	input_buffer[0x424] = param_2;

	unsigned long bytes_returned = 0;
	DeviceIoControl(io_bit, IOCTL_UNLOCK, input_buffer, 1064, &output_buffer, 1024, &bytes_returned, 0);

	free(input_buffer);
	free(output_buffer);
	CloseHandle(io_bit);
	return true;
}

bool IObitUnlocker::ToWideStr(const std::string& src, wchar_t* dst)
{
	if (src.empty() || dst == nullptr)
		return false;

	int result = MultiByteToWideChar(CP_UTF8, 0, src.c_str(), -1, dst, src.length());
	if (result != 0)
		return false;

	return true;
}

std::string IObitUnlocker::WideToStr(wchar_t* src)
{
	int size = WideCharToMultiByte(CP_UTF8, 0, src, -1, nullptr, 0, nullptr, nullptr);
	if (size == NULL)
		return {};

	std::string str(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, src, -1, &str[0], size, nullptr, nullptr);

	return str;
}