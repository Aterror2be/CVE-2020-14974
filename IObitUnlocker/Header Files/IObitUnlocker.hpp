#ifndef IO_BIT_UNLOCKER_HPP
#define IO_BIT_UNLOCKER_HPP

#include <string>

class IObitUnlocker
{
public:
	IObitUnlocker();

	bool Start();
	bool Stop();

	std::string DumpInfo(std::string path);

	bool UnlockFile(std::string path);
	bool UnlockDeleteFile(std::string path);
	bool UnlockRenameFile(std::string path, std::string new_name);
	bool UnlockCopyFile(std::string path, std::string new_path);
	bool UnlockMoveFile(std::string path, std::string new_path);
private:
	bool CreateDriverFile();
	bool RemoveDriverFile();
	bool CreateDriverService();
	bool DeleteDriverService();
	bool StartDriver();
	bool StopDriver();

	bool SendUnlockRequestWrapper(std::string path, std::string new_path, unsigned long param_1, unsigned long param_2);
	bool ToWideStr(const std::string& src, wchar_t* dst);
	std::string WideToStr(wchar_t* src);

	const unsigned long IOCTL_UNLOCK = 0x222124;
	const unsigned long IOCTL_DUMP_INFO = 0x222128;
	const std::string DRIVER_NAME = "IObitUnlocker.sys";
	std::string driver_path;
};

#endif // !IO_BIT_UNLOCKER_HPP