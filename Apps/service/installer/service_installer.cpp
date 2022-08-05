#include "../pch.h"
#include <wincred.h>
#pragma comment (lib, "Credui.lib")

// this program needs admin mode(requireAdministrator (/level='requireAdministrator'))

SC_HANDLE inatallServiceForUser(SC_HANDLE m, const WCHAR* path)
{
    {
        CREDUI_INFO cui = { .cbSize = sizeof(CREDUI_INFO), .pszMessageText = L"login for creating service", .pszCaptionText = L"Login" };
        TCHAR user[CREDUI_MAX_USERNAME_LENGTH + 1] = { 0 };
        TCHAR password[CREDUI_MAX_PASSWORD_LENGTH + 1] = { 0 };
        BOOL save = FALSE;

        if (CredUIPromptForCredentialsW(
            &cui,
            L".",
            NULL,
            0,
            user,
            CREDUI_MAX_USERNAME_LENGTH + 1,
            password,
            CREDUI_MAX_PASSWORD_LENGTH + 1,
            &save,
            CREDUI_FLAGS_GENERIC_CREDENTIALS |
            CREDUI_FLAGS_COMPLETE_USERNAME |
            CREDUI_FLAGS_ALWAYS_SHOW_UI |
            CREDUI_FLAGS_DO_NOT_PERSIST |
            CREDUI_FLAGS_VALIDATE_USERNAME) == 0) {
            return CreateServiceW(
                m,
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
                user,
                password);
        }
        else {
            MessageBoxW(NULL, L"CredUIPromptForCredentials failed!", L"Service Installer", 0);
            return NULL;
        }
    }
}

void run(const WCHAR* path, bool admin)
{
    SC_HANDLE m = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    SC_HANDLE service;

    if (NULL == m)
    {
        MessageBoxW(NULL, L"OpenSCManager failed\nNote: check you have administrator mode", L"Service installer", 0);
        return;
    }
    if (admin) {
        service = CreateServiceW(
            m,
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
    }
    else { 
        service = inatallServiceForUser(m, path); 
    }

    if (service == NULL)
    {
        MessageBoxW(NULL, L"Failed to install service: CreateService failed", L"Service installer", 0);
        CloseServiceHandle(m);
        return;
    }
    if (StartServiceW(service, 0, NULL)==FALSE) {
        MessageBoxW(NULL, L"Warning:\nStartService failed!\nCannot start service", L"Service installer", 0);
    }
    MessageBoxW(NULL, L"Service is now insalled in your computer!\nrun \"sc start httpserver\" to start, and \"sc stop httpserver\" to stop.", L"Service installer", 0);
    CloseServiceHandle(service);
    CloseServiceHandle(m);
}

int wmain(int argc, WCHAR **argv){
    if (argc == 1) {
        MessageBoxW(NULL, L"Too few arguments\nUsage: [user] or [admin]", L"Service installer", 0);
        return 1;
    }
    bool admin;
    if (lstrcmpW(argv[1], L"user")==0) {
        admin = false;
    }
    else if (lstrcmpW(argv[1], L"admin")==0) {
        admin = true;
    }
    else {
        MessageBoxW(NULL, L"Bad argument\nUsage: [user] or [admin]", L"Service installer", 0);
        return 1;
    }
	run(L"C:\\Program Files\\WebServer\\service.exe", admin);
    return (int)GetLastError();
}

