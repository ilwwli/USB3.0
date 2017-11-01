// USB3.0.cpp : �������̨Ӧ�ó������ڵ㡣

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
#define BUFFER_SIZE 0x1000000

// Function Declarations
FT_STATUS Initialization(FT_HANDLE &);
FT_STATUS WINAPI ReadThread(LPVOID);
FT_STATUS WINAPI WriteThread(LPVOID);
BOOL LoopBackTest(UCHAR *, UCHAR *, ULONG);

// Global Variables
ifstream fin;
ofstream fout;
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
	// ----- Initialization Stage -------
	ftStatus = Initialization(ftHandle);
	printf("ftHandle=0x%x\n", ftHandle);
	if (FT_FAILED(ftStatus))
	{
		printf("Initialization Failed\n");
		system("pause");
		return 0;
	}
	system("pause");
	fin.open("input.txt");
	fout.open("output.txt");
	//-----------------------------------
	// ----- Self-Test Stage ------------
	UCHAR *rdBuf = new UCHAR[BUFFER_SIZE];
	UCHAR *wrBuf = new UCHAR[BUFFER_SIZE];
	for (ULONG i = 0; i < BUFFER_SIZE; i++)
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
	// ----- Main Stage -----------------
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
	//TerminateThread(hThread[0], 0);
	//TerminateThread(hThread[1], 0);
	printf("WrStatus : %d, RdStatus: %d\n", wrStruct.ftStatus, rdStruct.ftStatus);
	system("pause");

	//--------------------------------
	// ----- Clear Stage -------------
	delete rdBuf;
	delete wrBuf;
	FT_AbortPipe(ftHandle, 0x02);
	FT_AbortPipe(ftHandle, 0x82);
	fin.close();
	fout.close();
	//--------------------------------
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
	OVERLAPPED rdOverlapped = { 0 };
	ftStatus = FT_InitializeOverlapped(ftHandle, &rdOverlapped);
	if (FT_FAILED(ftStatus))
	{
		pftStruct->ftStatus = ftStatus;
		return ftStatus;
	}	
	while (1)
	{
		ULONG Counter = 0;
		ULONG ulBytesTransferred = 0;
		ULONG ulBytesTransferredLast = 0;
		timeLapse = clock();
		for (int i = 0; i < 0x100; i++)
		{
			//ftStatus = FT_ReadPipe(ftHandle, 0x82, acBuf + 0x1000 * i, 0x1000, &ulBytesTransferred, NULL);
			ulBytesTransferredLast = ulBytesTransferred;
			ftStatus = FT_ReadPipe(ftHandle, 0x82, acBuf + 0x1000 * i, 0x1000, &ulBytesTransferred, &rdOverlapped);
			if (fout.is_open())
			{
				ULONG j = i ? i - 1 : 0x99;
				fout.write(((char *)acBuf + 0x1000 * j), ulBytesTransferredLast);
			}
			if (ftStatus == FT_IO_PENDING)
			{							
				ftStatus = FT_GetOverlappedResult(ftHandle, &rdOverlapped, &ulBytesTransferred, TRUE);
			}
			if (FT_FAILED(ftStatus))
			{
				FT_AbortPipe(ftHandle, 0x82);
				pftStruct->ftStatus = ftStatus;				
				return ftStatus;
			}
			Counter += ulBytesTransferred;
			FT_ReleaseOverlapped(ftHandle, &rdOverlapped);			
		}
		timeLapse = clock() - timeLapse;
		// Print the result using thread-safe functions.
		size_t cchStringSize;
		DWORD dwChars;
		StringCchPrintf(msgBuf, 255, TEXT("RxSpeed:%.2fMB/s, %.4fsec Elapsed for Receiving %.2fMB Data\n"),
			(((double)Counter) / 0x100000) / (((double)timeLapse) / CLOCKS_PER_SEC), 
			((double)timeLapse) / CLOCKS_PER_SEC, 
			((double)Counter) / 0x100000
		);
		StringCchLength(msgBuf, 255, &cchStringSize);
		WriteConsole(hStdout, msgBuf, (DWORD)cchStringSize, &dwChars, NULL);
	}	
	return ftStatus;
}

FT_STATUS WINAPI WriteThread(LPVOID lpParam)
{	
	FT_STATUS ftStatus;
	FT_HANDLE ftHandle;
	pFT_STRUCT pftStruct = (pFT_STRUCT)lpParam;
	ftHandle = pftStruct->ftHandle;	
	UCHAR *acBuf = pftStruct->acBuf;	
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
	OVERLAPPED wrOverlapped = { 0 };
	ftStatus = FT_InitializeOverlapped(ftHandle, &wrOverlapped);
	if (FT_FAILED(ftStatus))
	{
		pftStruct->ftStatus = ftStatus;
		return ftStatus;
	}	
	//BOOL final = 0;
	while (1)
	{
		ULONG Counter = 0;
		ULONG ulBytesTransferred = 0;
		ULONG ulBytesReaded = 0x1000;
		timeLapse = clock();
		for (ULONG i = 0; i < 0x100; i++)
		{
			//ftStatus = FT_WritePipe(ftHandle, 0x02, acBuf + 0x1000 * i, 0x1000, &ulBytesTransferred, NULL);
			if (fin.is_open())
			{
				if (!fin)
				{
					pftStruct->ftStatus = ftStatus;
					return ftStatus;
				}
				fin.read(((char *)acBuf + 0x1000 * i), 0x1000);
				ulBytesReaded = fin.gcount();
			}
			ftStatus = FT_WritePipe(ftHandle, 0x02, acBuf + 0x1000 * i, ulBytesReaded, &ulBytesTransferred, &wrOverlapped);
			if (ftStatus == FT_IO_PENDING)
			{
				ftStatus = FT_GetOverlappedResult(ftHandle, &wrOverlapped, &ulBytesTransferred, TRUE);
			}			
			if (FT_FAILED(ftStatus))
			{
				FT_AbortPipe(ftHandle, 0x02);
				pftStruct->ftStatus = ftStatus;				
				return ftStatus;
			}
			Counter += ulBytesTransferred;
			FT_ReleaseOverlapped(ftHandle, &wrOverlapped);			
		}
		timeLapse = clock() - timeLapse;
		// Print the result using thread-safe functions.
		size_t cchStringSize;
		DWORD dwChars;
		StringCchPrintf(msgBuf, 255, TEXT("TxSpeed:%.2fMB/s, %.4fsec Elapsed for Transmitting %.2fMB Data\n"),
			(((double)Counter) / 0x100000) / (((double)timeLapse) / CLOCKS_PER_SEC),
			((double)timeLapse) / CLOCKS_PER_SEC,
			((double)Counter) /0x100000
		);
		StringCchLength(msgBuf, 255, &cchStringSize);
		WriteConsole(hStdout, msgBuf, (DWORD)cchStringSize, &dwChars, NULL);
	}	
	return ftStatus;
}

BOOL LoopBackTest(UCHAR *wrBuf, UCHAR *rdBuf, ULONG size)
{
	for (ULONG i = 0; i < size; i++)
		if (wrBuf[i] != rdBuf[i])
			return 0;
	return 1;	
}