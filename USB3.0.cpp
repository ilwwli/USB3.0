// USB3.0.cpp : 定义控制台应用程序的入口点。

#include "stdafx.h"
#include "FTD3XX.h" 
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <ctime>
#include <fstream>
#pragma comment(lib, "FTD3XX.lib")
using namespace std;
// Macro Definitions
#define BUFFER_SIZE 0x10000000

// Function Declarations
FT_STATUS Initialization(FT_HANDLE &);
FT_STATUS WINAPI ReadThread(LPVOID);
FT_STATUS WINAPI WriteThread(LPVOID);
BOOL LoopBackTest(UCHAR *, UCHAR *, ULONG);

// Thread Parameter
typedef struct __FT_STRUCT
{
	__FT_STRUCT(FT_HANDLE h, UCHAR *b, FT_STATUS s = FT_OK) : ftHandle(h), acBuf(b), ftStatus(s) {};
	FT_HANDLE ftHandle;
	FT_STATUS ftStatus;
	UCHAR *acBuf;
} FT_STRUCT, *pFT_STRUCT;

int main()
{
	FT_STATUS ftStatus;
	FT_HANDLE ftHandle;	
	//------Initialization Stage---------
	ftStatus = Initialization(ftHandle);
	printf("ftHandle=0x%x\n", ftHandle);
	if (FT_FAILED(ftStatus))
	{
		printf("Initialization Failed\n");
		system("pause");
		return 0;
	}
	system("pause");
	//-----------------------------------
	//----------Self-Test Stage----------
	UCHAR *rdBuf = new UCHAR[BUFFER_SIZE];
	UCHAR *wrBuf = new UCHAR[BUFFER_SIZE];
	for (int i = 0; i < BUFFER_SIZE; i++)
		wrBuf[i] = i;
	ULONG ulBytesTransferred;
	ftStatus = FT_WritePipe(ftHandle, 0x02, wrBuf, 4096, &ulBytesTransferred, NULL);
	if (FT_FAILED(ftStatus))
	{
		printf("Write Failed\n");
		FT_AbortPipe(ftHandle,0x02);
	}
	ftStatus = FT_ReadPipe(ftHandle, 0x82, rdBuf, 4096, &ulBytesTransferred, NULL);
	if (FT_FAILED(ftStatus))
	{
		printf("Read Failed\n");
		FT_AbortPipe(ftHandle, 0x82);
	}
	if (!LoopBackTest(wrBuf, rdBuf, 4096))
		printf("LoopBack Test Failed\n");
	else
		printf("LoopBack Test Complete\n");
	system("pause");
	//-----------------------------------
	//----------Main Stage---------------
	HANDLE hThread[2];
	// Write Thread
	FT_STRUCT wrStruct(ftHandle, wrBuf);
	hThread[1] = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		WriteThread,			// thread function name
		&wrStruct,				// argument to thread function 
		0,                      // use default creation flags 
		NULL);					// returns the thread identifier
	// Read Thread
	FT_STRUCT rdStruct(ftHandle, rdBuf);
	hThread[0] = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		ReadThread,				// thread function name
		&rdStruct,				// argument to thread function 
		0,                      // use default creation flags 
		NULL);					// returns the thread identifier	
	if (hThread[0] == NULL || hThread[1] == NULL)
	{
		printf("Create Thread Failed\n");
		ExitProcess(3);
	}

	WaitForMultipleObjects(2, hThread, TRUE, INFINITE);
	//WaitForSingleObject(hThread, INFINITE);
	// End of main thread creation loop.
	printf("WrStatus : %d, RdStatus: %d\n", wrStruct.ftStatus, rdStruct.ftStatus);
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
	{
		printf("Create Device Info List Failed\n");
		ftStatus = FT_OTHER_ERROR;
	}
	return ftStatus;
}

FT_STATUS WINAPI ReadThread(LPVOID lpParam)
{
	FT_STATUS ftStatus;
	FT_HANDLE ftHandle;
	pFT_STRUCT pftStruct = (pFT_STRUCT)lpParam;
	ftHandle = pftStruct->ftHandle;	
	UCHAR *acBuf = pftStruct->acBuf;
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
		ULONG Counter = 0;
		timeLapse = clock();
		for (int i = 0; i < 0x100; i++)
		{
			ftStatus = FT_ReadPipe(ftHandle, 0x82, acBuf + 0x10000 * i, 0x10000, &ulBytesTransferred, NULL);
			if (FT_FAILED(ftStatus))
			{
				FT_AbortPipe(ftHandle, 0x82);
				pftStruct->ftStatus = ftStatus;
				delete acBuf;
				return ftStatus;
			}
			Counter += ulBytesTransferred;
		}
		timeLapse = clock() - timeLapse;
		// Print the result using thread-safe functions.
		size_t cchStringSize;
		DWORD dwChars;
		StringCchPrintf(msgBuf, 255, TEXT("TxSpeed:%.4f, %.4fsec Elapsed for Receiving %dMB Data\n"),
			(Counter / 0x100000) / (((double)timeLapse) / CLOCKS_PER_SEC), ((double)timeLapse) / CLOCKS_PER_SEC, Counter / 0x100000);
		StringCchLength(msgBuf, 255, &cchStringSize);
		WriteConsole(hStdout, msgBuf, (DWORD)cchStringSize, &dwChars, NULL);
	}
	delete acBuf;
	return ftStatus;
}

FT_STATUS WINAPI WriteThread(LPVOID lpParam)
{
	FT_STATUS ftStatus;
	FT_HANDLE ftHandle;
	pFT_STRUCT pftStruct = (pFT_STRUCT)lpParam;
	ftHandle = pftStruct->ftHandle;	
	UCHAR *acBuf = pftStruct->acBuf;	
	ULONG ulBytesTransferred = 0;
	clock_t timeLapse;
	TCHAR msgBuf[255];

	HANDLE hStdout;
	// Make sure there is a console to receive output results. 
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdout == INVALID_HANDLE_VALUE)
	{
		ftStatus = FT_OTHER_ERROR;
		pftStruct->ftStatus = ftStatus;
		return ftStatus;
	}
	while (1)
	{
		ULONG Counter = 0;
		timeLapse = clock();
		for (int i = 0; i < 0x100; i++)
		{
			ftStatus = FT_WritePipe(ftHandle, 0x02, acBuf + 0x10000 * i, 0x10000, &ulBytesTransferred, NULL);
			if (FT_FAILED(ftStatus))
			{
				FT_AbortPipe(ftHandle, 0x02);
				pftStruct->ftStatus = ftStatus;
				delete acBuf;
				return ftStatus;
			}
			Counter += ulBytesTransferred;
		}
		timeLapse = clock() - timeLapse;
		// Print the result using thread-safe functions.
		size_t cchStringSize;
		DWORD dwChars;
		StringCchPrintf(msgBuf, 255, TEXT("TxSpeed:%.4f, MB/s%.4fsec Elapsed for Transmitting %dMB Data\n"),
			(Counter / 0x100000) / (((double)timeLapse) / CLOCKS_PER_SEC), ((double)timeLapse) / CLOCKS_PER_SEC, Counter /0x100000);
		StringCchLength(msgBuf, 255, &cchStringSize);
		WriteConsole(hStdout, msgBuf, (DWORD)cchStringSize, &dwChars, NULL);
	}
	delete acBuf;
	return ftStatus;
}

BOOL LoopBackTest(UCHAR *wrBuf, UCHAR *rdBuf, ULONG size)
{
	for (int i = 0; i < size; i++)
		if (wrBuf[i] != rdBuf[i])
			return 0;
	return 1;	
}