#include "IObitUnlocker.hpp"

int main()
{
	IObitUnlocker io_bit_unlocker = IObitUnlocker();
	
	if (io_bit_unlocker.Start())
	{
		io_bit_unlocker.UnlockDeleteFile("C:\\Users\\Admin\\Desktop\\New Text Document.txt");
		io_bit_unlocker.Stop();
	}
	return 0;
}