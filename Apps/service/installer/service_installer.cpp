#include "../pch.h"

// this program needs admin mode(requireAdministrator (/level='requireAdministrator'))

VOID SvcInstall(const WCHAR* path)
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;

    schSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (NULL == schSCManager)
    {
        MessageBoxW(NULL, L"OpenSCManager failed\nNote: check you have administrator mode", L"Service installer", 0);
        return;
    }
    schService = CreateServiceW(
        schSCManager,
        SVCNAME,
        SVCNAME,  
        SERVICE_ALL_ACCESS,  
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        path,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL);

    if (schService == NULL)
    {
        MessageBoxW(NULL, L"Failed to install service: CreateService failed", L"Service installer", 0);
        CloseServiceHandle(schSCManager);
        return;
    }
    MessageBoxW(NULL, L"Service is insalled in your computer!", L"Service installer", 0);
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

int main(int argc, wchar_t**argv){
	SvcInstall(L"C:\\Program Files\\WebServer\\service.exe");
    return (int)GetLastError();
}
