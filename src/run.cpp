#define _CRT_SECURE_NO_WARNINGS // sprintf
#define N_THREADS 1
#include <winsock2.h>
#include <vector>
#include <Ws2tcpip.h>
#include <mstcpip.h>
#include <mswsock.h>
#include <iphlpapi.h>
#include <crtdbg.h>
#include <strsafe.h>
#include <wchar.h>
#include <WinInet.h>
#include <map>
#include <string>
#include <shlwapi.h>
#include "NtAPI.h"
#include <llhttp.h>
#include "types.h"
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock") 
#pragma comment(lib, "wininet")
#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment (lib, "Shlwapi.lib")
static CHAR* computer_name;
static SIZE_T computer_name_len;
static LPFN_ACCEPTEX pAcceptEx;
static LPFN_DISCONNECTEX pDisconnectEx;
static HANDLE heap, iocp;
static HMODULE ntdll;
static OVERLAPPED dumyOL = { 0 };
struct IOCP;

#if N_THREADS > 1
static HANDLE hThs[N_THREADS-1];
#endif

#ifdef _DEBUG
#define log_puts(x) puts(x)
#define log_fmt printf
#define assert(x) {if (!(x)){printf("[error] %s.%d: %s, err=%d\n", __FILE__, __LINE__, #x, WSAGetLastError());}}
#else
#define assert(x) (x)
#define log_puts(x) 
#define log_fmt(x, ...)
#endif // _DEBUG
//#define TRACE
#ifdef TRACE
#define WSASend(a, b, c, d, e, f, g) {puts("WSASend");WSASend(a, b, c, d, e, f, g);}
#define WSARecv(a, b, c, d, e, f, g) {puts("WSARecv");WSARecv(a, b, c, d, e, f, g);}
#define CloseClient(a) {puts("CloseClient");closeClient(a);}
#else
#define CloseClient closeClient
#endif

__declspec(noreturn) void fatal(const char* msg) {
	log_fmt("[fatal error] %s\nWSAGetLastError=%d\n", msg, WSAGetLastError());
	ExitProcess(1);
}
void closeClient(IOCP* ctx);
inline LPCWSTR urlToPath(IOCP* ctx) {
	if (ctx->url)
		return ctx->url;
	return L"index.html";
}
#include "Mine.h"
#include "fs.cpp"
void accept_next();
#include "handshake.cpp"
#include "frame.cpp"
#include "pipe.cpp"
#include "server.cpp"
#include "accept.cpp"

#pragma warning(push)
#pragma warning(disable: 6308)
#pragma warning(disable: 28182)

DWORD __stdcall run(LPVOID param)
{
	heap = GetProcessHeap();
	if (heap == NULL) {
		fatal("GetProcessHeap");
	}
	ntdll = LoadLibraryW(L"ntdll");
	if (ntdll == NULL) {
		fatal("LoadLibrary(\"ntdll\")");
	}
	pNtQueryDirectoryFile = (decltype(pNtQueryDirectoryFile))GetProcAddress(ntdll, "NtQueryDirectoryFile");
	if (pNtQueryDirectoryFile == NULL) {
		fatal("GetProcAddress(\"NtQueryDirectoryFile\")");
	}
	{
		DWORD size = 0;
		WCHAR* tmp = NULL;
		if (GetComputerNameW(tmp, &size) == FALSE) {
			if (GetLastError() != ERROR_BUFFER_OVERFLOW) {
				fatal("GetComputerNameW");
			}
			tmp = (WCHAR*)HeapAlloc(heap, 0, (SIZE_T)size * 2);
			if (tmp == NULL) {
				fatal("HeapAlloc");
			}
			if (GetComputerNameW(tmp, &size) == FALSE) {
				fatal("GetComputerNameW");
			}
			int len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, tmp, -1, NULL, 0, NULL, NULL);
			if (len <= 0) {
				fatal("WideCharToMultiByte");
			}
			computer_name = (CHAR*)HeapAlloc(heap, 0, len);
			if (computer_name == NULL) {
				fatal("HeapAlloc");
			}
			len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, tmp, -1, computer_name, len, NULL, NULL);
			if (len <= 0) {
				fatal("WideCharToMultiByte");
			}
			computer_name_len = len-1; // no '\0'
			HeapFree(heap, 0, tmp);
		}
	}
	//system("chcp 65001");
	SetConsoleTitleW(L"Web Server");
	{
		WSADATA wsaData{};
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			fatal("WSAStartup");
		}
	}
	iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, N_THREADS);
	if (iocp == NULL) {
		fatal("CreateIoCompletionPort");
	}
	if (!initHash()) {
		fatal("initHash");
	}
#ifndef PORT
#define PORT 80
#endif
	sockaddr_in ip4{ .sin_family = AF_INET, .sin_port = htons(PORT) };
#ifdef  CONSOLE_APP
	{
		PIP_ADAPTER_INFO pAdapter = (IP_ADAPTER_INFO*)HeapAlloc(heap, 0, sizeof(IP_ADAPTER_INFO));
		ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
		if (pAdapter == NULL) {
			fatal("HeapAlloc");
		}
		if (GetAdaptersInfo(pAdapter, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
			pAdapter = (IP_ADAPTER_INFO*)HeapReAlloc(heap, 0, pAdapter, ulOutBufLen);

			if (pAdapter == NULL) {
				fatal("HeapReAlloc");
				return 1;
			}
		}
		if (GetAdaptersInfo(pAdapter, &ulOutBufLen) == NO_ERROR) {
			PIP_ADAPTER_INFO head = pAdapter;
			DWORD i = 0;
			puts("select your ip address");
			while (pAdapter) {
				printf("[%d] %s\n", i, pAdapter->IpAddressList.IpAddress.String);
				pAdapter = pAdapter->Next;
				i++;
			}
			DWORD num;
			puts("enter number:");
			while (scanf_s("%u", &num) != 1 || num > i) { puts("invalid number, try again"); }
			while (num != 0) {
				num--;
				head = head->Next;
			}
			log_fmt("selected address: %s\n", head->IpAddressList.IpAddress.String);
			if (inet_pton(AF_INET, head->IpAddressList.IpAddress.String, (SOCKADDR*)&ip4) != 1) {
				fatal("inet_pton");
			}
			USHORT port;
			puts("enter port");
			while (scanf_s("%hu", &port) != 1) { puts("invalid port, try again"); }
			printf("select port: %hu\n", port);
			ip4.sin_port = htons(port);
			ip4.sin_family = AF_INET;
		}
		else {
			fatal("GetAdaptersInfo");
		}
		HeapFree(heap, 0, pAdapter);
	}
#endif
	acceptIOCP.server = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (acceptIOCP.server == INVALID_SOCKET) {
		fatal("WSASocketW");
	}
	if (bind(acceptIOCP.server, (struct sockaddr*)&ip4, sizeof(ip4)) == SOCKET_ERROR) {
		fatal("bind");
	}
	if (listen(acceptIOCP.server, SOMAXCONN) == SOCKET_ERROR) {
		fatal("listen");
	}
	{
		DWORD dwBytes = 0;
		GUID ga = WSAID_ACCEPTEX, gd = WSAID_DISCONNECTEX;
		if (WSAIoctl(acceptIOCP.server, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&ga, sizeof(ga),
			&pAcceptEx, sizeof(pAcceptEx),
			&dwBytes, NULL, NULL) == SOCKET_ERROR || WSAIoctl(acceptIOCP.server, SIO_GET_EXTENSION_FUNCTION_POINTER,
				&gd, sizeof(gd),
				&pDisconnectEx, sizeof(pDisconnectEx),
				&dwBytes, NULL, NULL)) {
			fatal("WSAIoctl: get AcceptEx & DisconnectEx function pointer");
		}
	}
	if (CreateIoCompletionPort((HANDLE)acceptIOCP.server, iocp, (ULONG_PTR)pAcceptEx, N_THREADS) == NULL) {
		fatal("CreateIoCompletionPort");
	}
	accept_next();
	
#if N_THREADS > 1
	for (int i = 0; i < N_THREADS-1; ++i) {
		DWORD id;
		hThs[i] = CreateThread(NULL, 0, RunIOCPLoop, NULL, 0, &id);
		log_fmt("[info] spawn thrad: (id=%u)\n", id);
	}
#endif
	log_fmt("[info] running main iocp thread: (id=%u)\n", GetCurrentThreadId());
	(void)RunIOCPLoop(NULL);
	log_fmt("[info] exit main iocp loop (id=%u), wait for all threads exit\n", GetCurrentThreadId());
#if N_THREADS > 1
	WaitForMultipleObjects(N_THREADS - 1, hThs, TRUE, INFINITE);
#endif
	shutdown(acceptIOCP.server, SD_BOTH);
	closesocket(acceptIOCP.server);
	WSACleanup();
	
	closeHash();
	_ASSERT(_CrtCheckMemory());
	CloseHandle(iocp);
	FreeLibrary(ntdll);
	puts("[info] exit process");
	return 0;
}
#pragma warning(pop)


