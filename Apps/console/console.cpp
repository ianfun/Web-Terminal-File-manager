#define PORT 80
#define CONSOLE_APP
#include "../../src/run.cpp"

BOOL WINAPI ConsoleHandler(DWORD event)
{
	switch (event) {
	case CTRL_C_EVENT:
		for (int i = 0; i < N_THREADS; ++i) {
			PostQueuedCompletionStatus(iocp, 0, (ULONG_PTR)RunIOCPLoop, 0);
		}
		return TRUE;
	}
	return FALSE;
}

int main() {
	SetCurrentDirectoryW(L"../../static");
	SetConsoleCtrlHandler(ConsoleHandler, TRUE);
	run(NULL);
}