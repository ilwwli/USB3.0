#pragma once
//------------------------
// *** * THIS HEADER FILE IS NOT USED * ***
// DO NOT INCLUDE THIS FILE TO .CPP FILE
//------------------------
#ifndef USB30
#define USB30
#include <fstream>
#include <thread>


typedef unsigned int ULONG; 
typedef unsigned char UCHAR;

// --- Input Class ---
class TxDataSource;			// Abstruct Class
class TxFromFile;
class TxFromFunction;
// --- Output Class ---
class RxDataDestination;	// Abstruct Class
class RxToFile;
class RxToMemory;
// Thread Class
class ReadThreadClass;
class WriteThreadClass;

// Entry Point
class USBtop;

class USBtop {
private:
	ReadThreadClass *rdt;
	WriteThreadClass *wrt;
public:
	USBtop();
};

class TxDataSource {
public:	
	virtual ULONG ReadFromSource(ULONG, UCHAR *) = 0;	
};

class TxFromFile : public TxDataSource {
private:
	std::istream &fin;
public:
	TxFromFile(ULONG bufl, ULONG total, std::istream &f) : fin(f) {}
	ULONG ReadFromSource(ULONG n, UCHAR *buf);
};

class TxFromFunction : public TxDataSource {
private:
	ULONG(*func)(ULONG, UCHAR *);
public:
	TxFromFunction(ULONG bufl, ULONG total, decltype(func)f) : func(f) {}
	ULONG ReadFromSource(ULONG n, UCHAR *buf);
};

class RxDataDestination {
	virtual ULONG WriteToDestination(ULONG, UCHAR *);
};

class RxToFile : public RxDataDestination{
private:
	std::ofstream &fout;
public:	
	RxToFile(std::ofstream &f) : fout(f) {}
	ULONG WriteToDestination(ULONG n, UCHAR *buf);
};
class RxToMemory : public RxDataDestination {
private:
	UCHAR *Buf;
	ULONG BufferLength;
public:
	RxToMemory(UCHAR *buf, ULONG bufl) : Buf(buf), BufferLength(bufl) {}
	ULONG WriteToDestination(ULONG n, UCHAR *buf);
};
class WriteThreadClass {
private:
	UCHAR *Buf;
	TxDataSource *pSource;
public:
	WriteThreadClass(ULONG bufsize, TxDataSource *ps) : pSource(ps) {
		Buf = new UCHAR[bufsize];
		WriteThreadClassInit();
	}
	void WriteThreadClassInit();
	void MainLoop();
	~WriteThreadClass() {
		delete Buf;
	}
};

#endif // !USB30
