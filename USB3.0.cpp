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
#define SPEED_CNT_CYCLE 0x100
//#define DEBUG

// Function Declarations
FT_STATUS Initialization(FT_HANDLE &);
FT_STATUS WINAPI ReadThread(LPVOID);
FT_STATUS WINAPI WriteThread(LPVOID);
BOOL LoopBackTest(UCHAR *, UCHAR *, ULONG);

// Global Variables
ifstream fin;
ofstream fout;
ULONG INFRAME, OUTFRAME;
#ifdef DEBUG
HANDLE WrFinEvent;
HANDLE RdFinEvent;
HANDLE WrStartEvent;
HANDLE RdStartEvent;
#endif // DEBUG
HANDLE WrSemaphore, RdSemaphore;
CRITICAL_SECTION RdCritSect;
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
	INFRAME = 0x1000;
	OUTFRAME = 0x1000;
	// ----- Initialization Stage -------
	ftStatus = Initialization(ftHandle);
	printf("ftHandle=0x%x\n", ftHandle);
	if (FT_FAILED(ftStatus))
	{
		printf("Initialization Failed\n");		
		return 0;
	}
	system("pause");
	WrSemaphore = CreateSemaphore(
		NULL,           // default security attributes
		2,  // initial count
		2,  // maximum count
		NULL);          // name
	RdSemaphore = CreateSemaphore(
		NULL,           // default security attributes
		0,  // initial count
		2,  // maximum count
		NULL);          // name
	if (!InitializeCriticalSectionAndSpinCount(&RdCritSect,	0x00000400)) // Init Critical Section
		return 0;
#ifdef DEBUG
	UCHAR *wrBuf = new UCHAR[INFRAME * SPEED_CNT_CYCLE];
	UCHAR *rdBuf = new UCHAR[INFRAME * SPEED_CNT_CYCLE];
	for (ULONG i = 0; i < INFRAME * SPEED_CNT_CYCLE; i++)
	{
		wrBuf[i] = i;
		//rdBuf[i] = i;
	}
	WrFinEvent = CreateEvent(
		NULL,               // default security attributes
		FALSE,              // non-manual-reset event
		FALSE,              // initial state is nonsignaled
		NULL				// object name
	);
	RdFinEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	WrStartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	RdStartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
#else
	UCHAR *wrBuf = 0;
	UCHAR *rdBuf = 0;
	fout.open("input.txt");	
	for (ULONG i = 0; i < INFRAME * 0x100; i++)
		for (char j = '0'; j <= '9'; j++)
			fout.put(j);
	fout.close();
	fin.open("input.txt");	
	fout.open("output.txt");
#endif // DEBUG
	//-----------------------------------
	// ----- Self-Test Stage ------------
	UCHAR *rdbuf = new UCHAR[0x1000];
	UCHAR *wrbuf = new UCHAR[0x1000];
	for (ULONG i = 0; i < 0x1000; i++)
		wrbuf[i] = i;
	ULONG ulBytesTransferred;
	ftStatus = FT_WritePipe(ftHandle, 0x02, wrbuf, 0x1000, &ulBytesTransferred, NULL);
	if (FT_FAILED(ftStatus))
	{
		printf("Write Failed\n");
		FT_AbortPipe(ftHandle,0x02);
	}
	ftStatus = FT_ReadPipe(ftHandle, 0x82, rdbuf, 0x1000, &ulBytesTransferred, NULL);
	if (FT_FAILED(ftStatus))
	{
		printf("Read Failed\n");
		FT_AbortPipe(ftHandle, 0x82);
	}
	if (!LoopBackTest(wrbuf, rdbuf, 0x1000))
		printf("Self-Test Failed\n");
	else
		printf("Self-Test Complete\n");	
	delete rdbuf;
	delete wrbuf;
	system("pause");
	FT_AbortPipe(ftHandle, 0x02);
	FT_AbortPipe(ftHandle, 0x82);
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
		NULL,                   
		0,                     
		ReadThread,				
		&rdStruct,				
		0,                      
		NULL);					
	if (hThread[0] == NULL || hThread[1] == NULL)
	{
		printf("Create Thread Failed\n");
		ExitProcess(0);
	}	
#ifdef DEBUG
	HANDLE EventList[2] = { WrFinEvent , RdFinEvent };
	for (int i = 0; i < 0x10; i++)
	{
		if (!SetEvent(WrStartEvent) || !SetEvent(RdStartEvent))
			printf("Restart Failed\n");
		WaitForMultipleObjects(2, EventList, TRUE, INFINITE);
		if (!LoopBackTest(wrBuf, rdBuf, INFRAME * SPEED_CNT_CYCLE))
		{
			//for (ULONG i = 0; i < 0x100; i++)			
			//	printf("%x, %x\t", wrBuf[i], rdBuf[i]);			
			printf("LoopBack Test Failed\n");
			//TerminateThread(hThread[0], 0);
			//TerminateThread(hThread[1], 0);
			//break;
		}
		else
			printf("LoopBack Test Complete\n");			
	}
	TerminateThread(hThread[0], 0);
	TerminateThread(hThread[1], 0);
	CloseHandle(WrStartEvent);
	CloseHandle(RdStartEvent);
	CloseHandle(WrFinEvent);
	CloseHandle(RdFinEvent);
#endif // DEBUG
	WaitForMultipleObjects(2, hThread, TRUE, INFINITE); // Wait for ReadThread & WriteThread to join
	CloseHandle(RdSemaphore);
	CloseHandle(WrSemaphore);
	//TerminateThread(hThread[0], 0);
	//TerminateThread(hThread[1], 0);
	printf("WrStatus : %d, RdStatus: %d\n", wrStruct.ftStatus, rdStruct.ftStatus);
	system("pause");
	//--------------------------------
	// ----- Clear Stage -------------	
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
	UCHAR *sourceBuf = pftStruct->acBuf;
	UCHAR *threadBuf = new UCHAR[OUTFRAME];
	clock_t timeLapse;	
	OVERLAPPED rdOverlapped = { 0 };
	ftStatus = FT_InitializeOverlapped(ftHandle, &rdOverlapped);
	if (FT_FAILED(ftStatus))
	{
		pftStruct->ftStatus = ftStatus;
		return ftStatus;
	}	
	ULONG ulBytesTransferred = 0;
	while (1)
	{
		ULONG Counter = 0;		
#ifdef DEBUG
		WaitForSingleObject(RdStartEvent, INFINITE);
#endif // DEBUG
		timeLapse = clock();
		for (int i = 0; i < SPEED_CNT_CYCLE; i++)
		{					
			WaitForSingleObject(RdSemaphore, INFINITE);			
#ifdef DEBUG			
			ftStatus = FT_ReadPipe(ftHandle, 0x82, sourceBuf + OUTFRAME * i, OUTFRAME, &ulBytesTransferred, &rdOverlapped);
#else			
			ftStatus = FT_ReadPipe(ftHandle, 0x82, threadBuf, OUTFRAME, &ulBytesTransferred, &rdOverlapped);				
#endif // DEBUG			
			if (ftStatus == FT_IO_PENDING)
			{							
				ftStatus = FT_GetOverlappedResult(ftHandle, &rdOverlapped, &ulBytesTransferred, TRUE);
			}
#ifndef DEBUG
			if (!fout)
			{
				ftStatus = FT_OTHER_ERROR;
				break;
			}
			fout.write(((char *)threadBuf), ulBytesTransferred);
#endif // !DEBUG						
			while (!ReleaseSemaphore(WrSemaphore, 1, NULL))
				Sleep(0);		// give up time slice
			if (FT_FAILED(ftStatus))
			{
				if (fout.is_open())
					fout.write(((char *)threadBuf[(i + 1) % 2]), ulBytesTransferred);
				FT_ReleaseOverlapped(ftHandle, &rdOverlapped);
				FT_AbortPipe(ftHandle, 0x82);
				pftStruct->ftStatus = ftStatus;					
				break;
			}
			Counter += ulBytesTransferred;			
		}
		timeLapse = clock() - timeLapse;

		// Print the result using thread-safe functions.
		TCHAR msgBuf[255];
		HANDLE hStdout;
		// Make sure there is a console to receive output results. 
		hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hStdout == INVALID_HANDLE_VALUE)
		{
			ftStatus = FT_OTHER_ERROR;			
			pftStruct->ftStatus = ftStatus;			
		}
		size_t cchStringSize;
		DWORD dwChars;
		StringCchPrintf(msgBuf, 255, TEXT("RxSpeed:%.2fMB/s, %.4fsec for 0x%xByte Data\n"),
			(((double)Counter) / 0x100000) / (((double)timeLapse) / CLOCKS_PER_SEC), 
			((double)timeLapse) / CLOCKS_PER_SEC, 
			Counter//(((double)Counter) / 0x100000)
		);
		StringCchLength(msgBuf, 255, &cchStringSize);
		WriteConsole(hStdout, msgBuf, (DWORD)cchStringSize, &dwChars, NULL);
		if (FT_FAILED(ftStatus))
			break;
#ifdef DEBUG
		if (!SetEvent(RdFinEvent))		
			printf("RdT SetEvent failed (%d)\n", GetLastError());		
#endif // DEBUG
	}	
	ReleaseSemaphore(WrSemaphore, 1, NULL);	
	return ftStatus;
}

FT_STATUS WINAPI WriteThread(LPVOID lpParam)
{	
	FT_STATUS ftStatus;
	FT_HANDLE ftHandle;
	pFT_STRUCT pftStruct = (pFT_STRUCT)lpParam;
	ftHandle = pftStruct->ftHandle;	
	UCHAR *sourceBuf = pftStruct->acBuf;
	UCHAR *threadBuf = new UCHAR[INFRAME];
	clock_t timeLapse;	
	OVERLAPPED wrOverlapped = { 0 };
	ftStatus = FT_InitializeOverlapped(ftHandle, &wrOverlapped);
	if (FT_FAILED(ftStatus))
	{
		pftStruct->ftStatus = ftStatus;
		return ftStatus;
	}		
	ULONG ulBytesTransferred = 0;
	ULONG ulBytesReaded = 0;	
	
	// *** * Main Loop * ***
	while (1)
	{
		ULONG Counter = 0;
#ifdef DEBUG
		WaitForSingleObject(WrStartEvent, INFINITE);
#endif // DEBUG		
		timeLapse = clock();
		for (ULONG i = 0; i < SPEED_CNT_CYCLE; i++)
		{		
			WaitForSingleObject(WrSemaphore, INFINITE);
#ifdef DEBUG			
			ftStatus = FT_WritePipe(ftHandle, 0x02, sourceBuf + INFRAME * i, INFRAME, &ulBytesTransferred, &wrOverlapped);
#else			
			if (!fin)
			{
				ftStatus = FT_OTHER_ERROR;
				break;
			}
			fin.read(((char *)threadBuf), INFRAME);
			ulBytesReaded = fin.gcount();					
			ftStatus = FT_WritePipe(ftHandle, 0x02, threadBuf, ulBytesReaded, &ulBytesTransferred, &wrOverlapped);
#endif // DEBUG			
			if (ftStatus == FT_IO_PENDING)
			{
				ftStatus = FT_GetOverlappedResult(ftHandle, &wrOverlapped, &ulBytesTransferred, TRUE);
			}
			while (!ReleaseSemaphore(RdSemaphore, 1, NULL))			
				Sleep(0);		// give up time slice			
			if (FT_FAILED(ftStatus))
			{
				FT_ReleaseOverlapped(ftHandle, &wrOverlapped);
				FT_AbortPipe(ftHandle, 0x02);
				pftStruct->ftStatus = ftStatus;				
				break;
			}
			Counter += ulBytesTransferred;
		}
		timeLapse = clock() - timeLapse;

		// Print the result using thread-safe functions.
		TCHAR msgBuf[255];
		HANDLE hStdout;
		// Make sure there is a console to receive output results. 
		hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hStdout == INVALID_HANDLE_VALUE)
		{
			ftStatus = FT_OTHER_ERROR;
			pftStruct->ftStatus = ftStatus;
			break;
		}
		size_t cchStringSize;
		DWORD dwChars;
		StringCchPrintf(msgBuf, 255, TEXT("TxSpeed:%.2fMB/s, %.4fsec for 0x%xByte Data\n"),
			(((double)Counter) / 0x100000) / (((double)timeLapse) / CLOCKS_PER_SEC),
			((double)timeLapse) / CLOCKS_PER_SEC,
			Counter//(((double)Counter) / 0x100000)
		);
		StringCchLength(msgBuf, 255, &cchStringSize);
		WriteConsole(hStdout, msgBuf, (DWORD)cchStringSize, &dwChars, NULL);
		if (FT_FAILED(ftStatus))
			break;
#ifdef DEBUG
		if (!SetEvent(WrFinEvent))		
			printf("WrT SetEvent failed (%d)\n", GetLastError());			
#endif // DEBUG
	}	
	ReleaseSemaphore(RdSemaphore, 1, NULL);	
	return ftStatus;
}

BOOL LoopBackTest(UCHAR *wrBuf, UCHAR *rdBuf, ULONG size)
{
	for (ULONG i = 0; i < size; i++)
		if (wrBuf[i] != rdBuf[i])
		{		
			return 0;
		}
	return 1;	
}