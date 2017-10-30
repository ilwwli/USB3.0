// USB3.0.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "FTD3XX.h" 
#pragma comment(lib, "FTD3XX.lib")

int main()
{
    return 0;
}

FT_HANDLE Initialization()
{
	FT_STATUS ftStatus; 
	FT_HANDLE ftHandle;
	DWORD numDevs = 0;
	ftStatus = FT_CreateDeviceInfoList(&numDevs); 
	if (!FT_FAILED(ftStatus) && numDevs > 0) 
	{
		FT_DEVICE_LIST_INFO_NODE *devInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE) * numDevs);
		ftStatus = FT_GetDeviceInfoList(devInfo, &numDevs); 
		if (!FT_FAILED(ftStatus)) 
		{ 
			printf("List of Connected Devices!\n\n"); 
			for (DWORD i = 0; i < numDevs; i++) 
			{ 
				printf("Device[%d]\n", i); 
				printf("\tFlags: 0x%x %s | Type: %d | ID: 0x%08X | ftHandle=0x%x\n", 
					devInfo[i].Flags, 
					devInfo[i].Flags & FT_FLAGS_SUPERSPEED ? "[USB 3]" : 
						devInfo[i].Flags & FT_FLAGS_HISPEED ? "[USB 2]" : 
						devInfo[i].Flags & FT_FLAGS_OPENED ? "[OPENED]" : "", 
					devInfo[i].Type, 
					devInfo[i].ID, 
					devInfo[i].ftHandle); 
				printf("\tSerialNumber=%s\n", devInfo[i].SerialNumber); 
				printf("\tDescription=%s\n", devInfo[i].Description); 
			} 
		}
		else
			printf("Get Device Info List Failed\n");		 
		ftStatus = FT_Create(devInfo[0].SerialNumber, FT_OPEN_BY_SERIAL_NUMBER, &ftHandle);
		if (!FT_FAILED(ftStatus))
		{	}
		else
		{
			printf("Create Failed\n");
			FT_Close(ftHandle);
		}
		free(devInfo);
	}
	else
		printf("Create Device Info List Failed\n");
	return ftHandle;
}

