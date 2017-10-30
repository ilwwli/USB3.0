// USB3.0.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "FTD3XX.h" 
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <ctime>
#pragma comment(lib, "FTD3XX.lib")

#define BUFFER_SIZE 0x10000000

FT_STATUS Initialization(FT_HANDLE &);
FT_STATUS WINAPI ReadThread(LPVOID);
typedef struct FT_STRUCT
{
	FT_HANDLE ftHandle;
	FT_STATUS ftStatus;
} *pFT_STRUCT;

int main()
{
	FT_STATUS ftStatus;
	FT_HANDLE ftHandle;
	FT_STRUCT ftStruct;
	ftStatus = Initialization(ftHandle);
	if (FT_FAILED(ftStatus))
	{
		printf("Initialization Failed\n");
		system("pause");
		return 0;
	}	
	HANDLE hThread;
	hThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		ReadThread,				// thread function name
		&ftStruct,				// argument to thread function 
		0,                      // use default creation flags 
		NULL);					// returns the thread identifier 								

	if (hThread == NULL)
	{
		printf("Create Thread Failed\n");
		ExitProcess(3);
	}
	//WaitForMultipleObjects(MAX_THREADS, hThreadArray, TRUE, INFINITE);
	WaitForSingleObject(hThread, INFINITE);
	// End of main thread creation loop.
	system("pause");
	return 0;
}

FT_STATUS Initialization(FT_HANDLE &ftHandle)
{
	FT_STATUS ftStatus; 	
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
		if (FT_FAILED(ftStatus))
		{
			printf("Create Failed\n");
			FT_Close(ftHandle);
		}
		free(devInfo);
	}
	else
		printf("Create Device Info List Failed\n");
	return ftStatus;
}

FT_STATUS WINAPI ReadThread(LPVOID lpParam)
{
	FT_STATUS ftStatus;
	FT_HANDLE ftHandle;
	pFT_STRUCT pftStruct = (pFT_STRUCT)lpParam;
	ftHandle = pftStruct->ftHandle;
	UCHAR acBuf[BUFFER_SIZE] = { 0xFF };
	ULONG ulBytesTransferred = 0;
	clock_t timeLapse;
	TCHAR msgBuf[255];

	HANDLE hStdout;
	// Make sure there is a console to receive output results. 
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdout == INVALID_HANDLE_VALUE)
	{
		ftStatus =  FT_OTHER_ERROR;
		pftStruct->ftStatus = ftStatus;
		return ftStatus;
	}
	
	
	while (1)
	{
		//ULONG Counter = BUFFER_SIZE;
		timeLapse = clock();
		ftStatus = FT_WritePipe(ftHandle, 0x02, acBuf, BUFFER_SIZE, &ulBytesTransferred, NULL);
		if (FT_FAILED(ftStatus))
		{
			ftStatus = FT_AbortPipe(ftHandle, 0x02);
			if (FT_FAILED(ftStatus))
			{
				pftStruct->ftStatus = ftStatus;
				return ftStatus;
			}				
		}
		timeLapse = clock() - timeLapse;
		// Print the result using thread-safe functions.
		size_t cchStringSize;
		DWORD dwChars;
		StringCchPrintf(msgBuf, 255, TEXT("%dsec Elapsed for Transmitting %dMB Data\n"),
			((double)timeLapse) / CLOCKS_PER_SEC, ulBytesTransferred);
		StringCchLength(msgBuf, 255, &cchStringSize);
		WriteConsole(hStdout, msgBuf, (DWORD)cchStringSize, &dwChars, NULL);
	}
	return 0;
}
