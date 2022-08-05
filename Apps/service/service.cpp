// the HTTPServer service - iinstall the service before you run it

#pragma comment(lib, "advapi32.lib")

wchar_t SVCNAME[] = L"HTTPServer";
#define PORT 80
#include "../../src/run.cpp"
#define SVC_ERROR                        ((DWORD)0xC0020001L)

SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;

VOID WINAPI SvcCtrlHandler(DWORD);
VOID WINAPI SvcMain(DWORD, LPTSTR*);
VOID ReportSvcStatus(DWORD, DWORD, DWORD);

int __cdecl wmain(int argc, WCHAR** argv)
{
    SERVICE_TABLE_ENTRYW f[] =
    {
        { SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
        { NULL, NULL }
    };
    if (StartServiceCtrlDispatcherW(f)) {
        return 0;
    }
    switch (GetLastError())
    {
    case ERROR_FAILED_SERVICE_CONTROLLER_CONNECT:
        fputs("StartServiceCtrlDispatcherW: ERROR_FAILED_SERVICE_CONTROLLER_CONNECT: the program is running as a console application", stderr);
        break;
    case ERROR_SERVICE_ALREADY_RUNNING:
        fputs("ERROR_SERVICE_ALREADY_RUNNING: the service is already running", stderr);
        break;
    default:
        fprintf(stderr, "StartServiceCtrlDispatcherW: error=%lu\n", GetLastError());
        break;
    }
    return 1;
}

VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
    gSvcStatusHandle = RegisterServiceCtrlHandler(
        SVCNAME,
        SvcCtrlHandler);
    if (!gSvcStatusHandle)
    {
        return;
    }
    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;
    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);
    SetCurrentDirectoryW(L"C:\\Program Files\\WebServer");
    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);
    run(NULL);
    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
}


VOID ReportSvcStatus(DWORD dwCurrentState,
    DWORD dwWin32ExitCode,
    DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;
    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else gSvcStatus.dwCheckPoint = dwCheckPoint++;
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
    switch (dwCtrl)
    {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
        PostQueuedCompletionStatus(iocp, 0, (ULONG_PTR)RunIOCPLoop, 0);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
        return;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    default:
        break;
    }

}