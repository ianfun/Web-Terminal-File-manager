#include "../pch.h"

// this program needs admin mode(requireAdministrator (/level='requireAdministrator'))

VOID SvcInstall(const WCHAR* path)
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;

    schSCManager = OpenSCManagerW(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        MessageBoxW(NULL, L"OpenSCManager failed", L"Service installer", 0);
        return;
    }
    schService = CreateServiceW(
        schSCManager,              // SCM database 
        SVCNAME,                   // name of service 
        SVCNAME,                   // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        SERVICE_DEMAND_START,      // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        path,                    // path to service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        NULL,                      // no dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 

    if (schService == NULL)
    {
        MessageBoxW(NULL, L"CreateService failed", L"Service installer", 0);
        CloseServiceHandle(schSCManager);
        return;
    }
    else MessageBoxW(NULL, L"Service installed successfully", L"Service installer", 0);

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

int main(){
	SvcInstall(L"C:\\Program Files\\HTTPServer\\service.exe");
}
