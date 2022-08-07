#include "../pch.h"
#include <wincred.h>
#include <strsafe.h>
#pragma comment (lib, "Credui.lib")

// this program needs admin mode(requireAdministrator (/level='requireAdministrator'))

static DWORD err = 0;

enum Option {
    UI=1, Cmd=2, Nt=3
};

SC_HANDLE inatallServiceForUser(SC_HANDLE m, const WCHAR* path, Option options)
{
    {
        WCHAR cname[MAX_COMPUTERNAME_LENGTH + 1]; DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
        if (GetComputerNameW(cname, &size) == FALSE) {
            err = GetLastError();
            MessageBoxW(0, L"GetComputerName failed!", L"Service Installer", MB_ICONERROR);
            return NULL;
        }
        WCHAR user[CREDUI_MAX_USERNAME_LENGTH + 1] = { 0 }, user_domain[CREDUI_MAX_USERNAME_LENGTH + 1] = {0};
        WCHAR password[CREDUI_MAX_PASSWORD_LENGTH + 1] = { 0 };
        WCHAR domain[CREDUI_MAX_DOMAIN_TARGET_LENGTH] = {0};
        BOOL save = FALSE;
        DWORD ret; BOOL ok; DWORD lerr = 0; BOOL once = TRUE; DWORD status;
    AGAIN:
        save = FALSE;
        if (lerr) {
            if (lerr == ERROR_ACCOUNT_RESTRICTION) {
                system("start https://stackoverflow.com/questions/1047854/cant-run-a-service-under-an-account-which-has-no-password");
                MessageBoxW(NULL, L"ERROR_ACCOUNT_RESTRICTION:\nthis may due to empty password, see https://stackoverflow.com/questions/1047854/cant-run-a-service-under-an-account-which-has-no-password for details.", L"Service Installer", MB_ICONERROR);
            }
            else if (lerr == ERROR_LOGON_TYPE_NOT_GRANTED) {
                system("start https://docs.microsoft.com/en-us/system-center/scsm/enable-service-log-on-sm");
                MessageBoxW(NULL, L"ERROR_LOGON_TYPE_NOT_GRANTED:\nthis may due to your account don't have logon as service rights, see https://docs.microsoft.com/en-us/system-center/scsm/enable-service-log-on-sm for details.", L"Service Installer", MB_ICONERROR);
            }
            LPWSTR p = NULL;
            size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, lerr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&p, 0, NULL);
            MessageBoxW(NULL, p,L"Service Installer: Login failure", MB_ICONERROR);
            LocalFree(p);
        }

        CREDUI_INFO cui = { .cbSize = sizeof(CREDUI_INFO), 
            .pszMessageText = once ? L"Logon is required for creating process in user mode.\nWe will clear password from memory and not save to anywhere." : L"Logon Failed." , 
            .pszCaptionText = L"Logon for create service" };
        
        if ((status=
            
            ((options == UI) ? CredUIPromptForCredentialsW(
            &cui,
            cname,
            NULL,
            lerr,
            user_domain,
            sizeof(user_domain) / sizeof(WCHAR),
            password,
            sizeof(password) / sizeof(WCHAR),
            &save,
            CREDUI_FLAGS_GENERIC_CREDENTIALS |
            CREDUI_FLAGS_COMPLETE_USERNAME |
            CREDUI_FLAGS_ALWAYS_SHOW_UI |
            CREDUI_FLAGS_DO_NOT_PERSIST |
            CREDUI_FLAGS_VALIDATE_USERNAME)
             : CredUICmdLinePromptForCredentialsW(
                 cname, NULL, lerr, 
                 user_domain, sizeof(user_domain) / sizeof(WCHAR), 
                 password, sizeof(password) / sizeof(WCHAR), &save, CREDUI_FLAGS_DO_NOT_PERSIST  | CREDUI_FLAGS_EXCLUDE_CERTIFICATES))
            
            )== 0) {
            ret = CredUIParseUserNameW(user_domain, user, sizeof(user) / 2, domain, sizeof(domain) / 2);
            if (ret !=NO_ERROR) {
                puts("CredUIParseUserName failed!");
                SecureZeroMemory(password, sizeof password);
                SecureZeroMemory(user, sizeof user);
                SecureZeroMemory(user_domain, sizeof user_domain);
                once = FALSE;
                goto AGAIN;
            }
            {
                HANDLE hToken = NULL;
                ok = LogonUserW(user, domain, password, LOGON32_LOGON_SERVICE, LOGON32_PROVIDER_DEFAULT, &hToken);
                lerr = GetLastError();
                if (hToken && hToken!=INVALID_HANDLE_VALUE) {
                    CloseHandle(hToken);
                }
            }
            if (ok == FALSE) {
                SecureZeroMemory(password, sizeof password);
                SecureZeroMemory(user, sizeof user);
                SecureZeroMemory(user_domain, sizeof user_domain);
                once = FALSE;
                goto AGAIN;
            }
            {
                StringCbPrintfW(user_domain, sizeof(user_domain), L".\\%s", user);
                SecureZeroMemory(user, sizeof user);
                SC_HANDLE ret = CreateServiceW(
                    m,
                    SVCNAME,
                    L"A HTTP static file & websocket terminal server.",
                    SERVICE_ALL_ACCESS,
                    SERVICE_WIN32_OWN_PROCESS,
                    SERVICE_AUTO_START,
                    SERVICE_ERROR_NORMAL,
                    path,
                    NULL,
                    NULL,
                    NULL,
                    user_domain,
                    password);
                err = GetLastError();
                SecureZeroMemory(password, sizeof password);
                SecureZeroMemory(user_domain, sizeof user_domain);
                return ret;
            }
        }
        else {
            SecureZeroMemory(password, sizeof password);
            SecureZeroMemory(user_domain, sizeof user_domain);
            printf("status: %lu\n", status);
            if (status == ERROR_CANCELLED) {
                MessageBoxW(NULL, L"User logon canceled.", L"Service Installer", MB_ICONERROR);
            }
            else {
                MessageBoxW(NULL, L"logon failed!", L"Service Installer", MB_ICONERROR);
            }
            return NULL;
        }
    }

}

void run(const WCHAR* path, Option options)
{
    SC_HANDLE m = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    SC_HANDLE service;

    if (NULL == m)
    {
        err = GetLastError();
        MessageBoxW(NULL, L"OpenSCManager failed\nNote: check you have administrator mode", L"Service installer", 0);
        return;
    }
    if (options == Nt) {
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
        err = GetLastError();
    }
    else { 
        service = inatallServiceForUser(m, path, options);
    }

    if (service == NULL)
    {
        printf("CreateService failed with error: %lu\n", GetLastError());
        MessageBoxW(NULL, L"Failed to install service: CreateService failed", L"Service installer", MB_ICONERROR);
        CloseServiceHandle(m);
        return;
    }
    if (StartServiceW(service, 0, NULL)==FALSE) {
        err = GetLastError();
        MessageBoxW(NULL, L"Warning:\nStartService failed!\nCannot start service", L"Service installer", MB_ICONWARNING);
    }
    MessageBoxW(NULL, L"Congratulations!\nService is now insalled in your computer!\nrun \"sc start httpserver\" to start, and \"sc stop httpserver\" to stop.", L"Service installer", MB_ICONINFORMATION);
    CloseServiceHandle(service);
    CloseServiceHandle(m);
}

int wmain(int argc, WCHAR **argv){
    if (argc == 1) {
        MessageBoxW(NULL, L"Too few arguments\nUsage: [user] | [admin] | [cmd]", L"Service installer", MB_ICONERROR);
        return ERROR_INVALID_PARAMETER;
    }
    Option options = (Option)0;
    if (lstrcmpW(argv[1], L"user")==0) {
        options = UI;
    }
    else if (lstrcmpW(argv[1], L"admin")==0) {
        options = Nt;
    }
    else if (lstrcmpW(argv[1], L"cmd")==0) {
        options = Cmd;
    }
    else {
        MessageBoxW(NULL, L"Bad argument\nUsage: [user] | [admin] | [cmd]", L"Service installer", MB_ICONERROR);
        return ERROR_INVALID_PARAMETER;
    }
	run(L"C:\\Program Files\\WebServer\\service.exe", options);
    return err;
}

