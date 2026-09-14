#define _CRT_SECURE_NO_WARNINGS
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <restartmanager.h>
#include <aclapi.h>
#include <sddl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _MSC_VER
    #pragma comment(lib, "rstrtmgr.lib")
    #pragma comment(lib, "advapi32.lib")
    #pragma comment(lib, "shell32.lib")
    #pragma comment(lib, "ole32.lib")
    #pragma comment(lib, "user32.lib")
    #pragma comment(lib, "comctl32.lib")
    #pragma comment(lib, "gdi32.lib")
#else
    #define _snprintf_s snprintf
#endif

#define SNPRINTF _snprintf_s

/* =================================================================
     GLOBAL STATE
   ================================================================= */

static BOOL g_silentMode = FALSE;
static BOOL g_hasConsole = FALSE;


/* =================================================================
     ICON CONFIG
   ================================================================= */

#define ICON_MAIN_DLL       "shell32.dll"
#define ICON_MAIN_INDEX     21
#define ICON_DELETE_DLL     "shell32.dll"
#define ICON_DELETE_INDEX   131
#define ICON_UNLOCK_DLL     "shell32.dll"
#define ICON_UNLOCK_INDEX   165
#define ICON_GRANT_DLL      "shell32.dll"
#define ICON_GRANT_INDEX    48
#define ICON_OWNER_DLL      "shell32.dll"
#define ICON_OWNER_INDEX    46
#define ICON_LOCK_DLL       "shell32.dll"
#define ICON_LOCK_INDEX     23
#define CONSOLE_ICON_DLL    "shell32.dll"
#define CONSOLE_ICON_INDEX  21


/* =================================================================
    CONSOLE MANAGEMENT
   ================================================================= */

static BOOL attach_parent_console(void)
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return FALSE;

    FILE *fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$",  "r", stdin);

    HWND hwnd = GetConsoleWindow();
    if (hwnd)
    {
        HICON hBig = NULL, hSmall = NULL;
        ExtractIconExA(CONSOLE_ICON_DLL, CONSOLE_ICON_INDEX,
                       &hBig, &hSmall, 1);
        if (hBig)
            SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hBig);
        if (hSmall)
            SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);
    }

    return TRUE;
}

static void alloc_own_console(void)
{
    if (GetConsoleWindow())
    {
        g_hasConsole = TRUE;
        return;
    }

    if (!AllocConsole())
    {
        g_hasConsole = FALSE;
        return;
    }

    FILE *fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$",  "r", stdin);

    SetConsoleTitleA("Unlocker Pro");

    HWND hwnd = GetConsoleWindow();
    if (hwnd)
    {
        HICON hBig = NULL, hSmall = NULL;
        ExtractIconExA(CONSOLE_ICON_DLL, CONSOLE_ICON_INDEX,
                       &hBig, &hSmall, 1);
        if (hBig)
            SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hBig);
        if (hSmall)
            SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);
    }

    g_hasConsole = TRUE;
}

static BOOL have_console(void)
{
    return (GetConsoleWindow() != NULL) || g_hasConsole;
}


/* =================================================================
     ICON LOADING
   ================================================================= */

static void apply_icon_to_console(void)
{
    HWND hwnd = GetConsoleWindow();
    if (!hwnd) return;

    HICON hBig = NULL, hSmall = NULL;
    ExtractIconExA(CONSOLE_ICON_DLL, CONSOLE_ICON_INDEX,
                   &hBig, &hSmall, 1);

    if (hBig)
        SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hBig);
    if (hSmall)
        SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);

    if (hBig)
        SetClassLongPtrA(hwnd, GCLP_HICON, (LONG_PTR)hBig);
    if (hSmall)
        SetClassLongPtrA(hwnd, GCLP_HICONSM, (LONG_PTR)hSmall);
}


/* =================================================================
     POPUP ALERTS
   ================================================================= */

static void show_popup(HWND owner, const char *title,
                       const char *msg, UINT flags)
{
    if (g_silentMode) return;
    MessageBoxA(owner, msg, title, flags);
}

static void popup_success(const char *operation, const char *details)
{
    char msg[2048];
    SNPRINTF(msg, sizeof(msg), "%s\n\n%s", operation, details);
    show_popup(NULL, "Unlocker Pro - Success", msg,
               MB_OK | MB_ICONINFORMATION);
}

static void popup_error(const char *operation, const char *details)
{
    char msg[2048];
    SNPRINTF(msg, sizeof(msg), "%s\n\n%s", operation, details);
    show_popup(NULL, "Unlocker Pro - Failed", msg,
               MB_OK | MB_ICONERROR);
}

static void popup_info(const char *operation, const char *details)
{
    char msg[2048];
    SNPRINTF(msg, sizeof(msg), "%s\n\n%s", operation, details);
    show_popup(NULL, "Unlocker Pro", msg,
               MB_OK | MB_ICONINFORMATION);
}


/* =================================================================
   Case-insensitive compare
   ================================================================= */

static int my_stricmp(const char *a, const char *b)
{
    while (*a && *b)
    {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return (int)(unsigned char)ca - (int)(unsigned char)cb;
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

#define _stricmp my_stricmp


/* =================================================================
   Constants
   ================================================================= */

#define APP_DISPLAYNAME  "Unlocker Pro By Alex"
#define APP_VERSION      "1.0.0"
#define APP_PUBLISHER    "Rahul Singh Kanasiya Alex"
#define APP_UNINST_KEY   "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\UnlockerPro"
#define INSTALL_DIR      "C:\\Program Files\\UnlockerPro"
#define INSTALL_EXE      "C:\\Program Files\\UnlockerPro\\unlocker.exe"

#define CTX_KEY_FILE     "Software\\Classes\\*\\shell\\UnlockerPro"
#define CTX_KEY_DIR      "Software\\Classes\\Directory\\shell\\UnlockerPro"
#define CTX_KEY_DRIVE    "Software\\Classes\\Drive\\shell\\UnlockerPro"
#define CTX_KEY_DIRBG    "Software\\Classes\\Directory\\Background\\shell\\UnlockerPro"
#define CTX_EXT_KEY      "Software\\Classes\\UnlockerProMenu"

#define CTX_ICON_MAIN     "shell32.dll,21"
#define CTX_ICON_DELETE   "shell32.dll,131"
#define CTX_ICON_UNLOCK   "shell32.dll,165"
#define CTX_ICON_GRANT    "shell32.dll,48"
#define CTX_ICON_OWNER    "shell32.dll,46"
#define CTX_ICON_LOCK     "shell32.dll,23"


/* =================================================================
   Utility Helpers
   ================================================================= */

static void safe_strcpy(char *dst, size_t dstSize, const char *src)
{
    if (dstSize == 0) return;
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

static BOOL paths_equal(const char *a, const char *b)
{
    char na[MAX_PATH * 4], nb[MAX_PATH * 4];
    safe_strcpy(na, sizeof(na), a);
    safe_strcpy(nb, sizeof(nb), b);

    for (char *p = na; *p; p++) if (*p == '/') *p = '\\';
    for (char *p = nb; *p; p++) if (*p == '/') *p = '\\';

    size_t la = strlen(na);
    if (la > 0 && na[la - 1] == '\\') na[la - 1] = '\0';
    size_t lb = strlen(nb);
    if (lb > 0 && nb[lb - 1] == '\\') nb[lb - 1] = '\0';

    return _stricmp(na, nb) == 0;
}

static void print_json_escaped(const char *s)
{
    while (*s)
    {
        switch (*s)
        {
        case '\\': printf("\\\\"); break;
        case '"':  printf("\\\""); break;
        case '\n': printf("\\n");  break;
        case '\r': printf("\\r");  break;
        case '\t': printf("\\t");  break;
        default:   putchar(*s);    break;
        }
        s++;
    }
}

void print_error(const char *operation)
{
    DWORD err = GetLastError();
    char *msg = NULL;

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, 0,
        (LPSTR)&msg, 0, NULL
    );

    if (msg)
    {
        if (have_console())
            fprintf(stderr, "%s failed. Error %lu: %s\n", operation, err, msg);
        LocalFree(msg);
    }
    else
    {
        if (have_console())
            fprintf(stderr, "%s failed. Error %lu\n", operation, err);
    }
}

void print_process_name(DWORD pid)
{
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

    if (!hProcess) { printf("unknown.exe"); return; }

    char path[MAX_PATH * 4];
    DWORD size = sizeof(path);

    if (QueryFullProcessImageNameA(hProcess, 0, path, &size))
    {
        const char *name = strrchr(path, '\\');
        printf("%s", name ? name + 1 : path);
    }
    else
    {
        printf("unknown.exe");
    }

    CloseHandle(hProcess);
}


/* =================================================================
   Installation State Check
   ================================================================= */

int check_installation_state(void)
{
    BOOL exeExists = (GetFileAttributesA(INSTALL_EXE) != INVALID_FILE_ATTRIBUTES);

    HKEY hKey = NULL;
    BOOL fileMenu = (RegOpenKeyExA(HKEY_LOCAL_MACHINE, CTX_KEY_FILE,
                        0, KEY_READ, &hKey) == ERROR_SUCCESS);
    if (fileMenu) RegCloseKey(hKey);

    BOOL uninstEntry = (RegOpenKeyExA(HKEY_LOCAL_MACHINE, APP_UNINST_KEY,
                            0, KEY_READ, &hKey) == ERROR_SUCCESS);
    if (uninstEntry) RegCloseKey(hKey);

    if (exeExists && fileMenu && uninstEntry) return 1;
    if (!exeExists && !fileMenu && !uninstEntry) return 0;
    return 2;
}

void cleanup_all_registry(void)
{
    RegDeleteTreeA(HKEY_LOCAL_MACHINE, CTX_KEY_FILE);
    RegDeleteTreeA(HKEY_LOCAL_MACHINE, CTX_KEY_DIR);
    RegDeleteTreeA(HKEY_LOCAL_MACHINE, CTX_KEY_DRIVE);
    RegDeleteTreeA(HKEY_LOCAL_MACHINE, CTX_KEY_DIRBG);
    RegDeleteTreeA(HKEY_LOCAL_MACHINE, CTX_EXT_KEY);
    RegDeleteTreeA(HKEY_LOCAL_MACHINE, APP_UNINST_KEY);

    RegDeleteTreeA(HKEY_CURRENT_USER, CTX_KEY_FILE);
    RegDeleteTreeA(HKEY_CURRENT_USER, CTX_KEY_DIR);
    RegDeleteTreeA(HKEY_CURRENT_USER, CTX_KEY_DRIVE);
    RegDeleteTreeA(HKEY_CURRENT_USER, CTX_KEY_DIRBG);
    RegDeleteTreeA(HKEY_CURRENT_USER, APP_UNINST_KEY);
}


/* =================================================================
   Admin & Elevation
   ================================================================= */

BOOL is_admin(void)
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(
            &ntAuth, 2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0,
            &adminGroup))
    {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

BOOL elevate_self(const char *args)
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    SHELLEXECUTEINFOA sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS
                     | SEE_MASK_NO_CONSOLE
                     | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb       = "runas";
    sei.lpFile       = exePath;
    sei.lpParameters = args;
    sei.nShow        = SW_HIDE;
    sei.hwnd         = NULL;

    if (ShellExecuteExA(&sei))
    {
        WaitForSingleObject(sei.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(sei.hProcess, &exitCode);
        CloseHandle(sei.hProcess);
        ExitProcess(exitCode);
    }
    return FALSE;
}


/* =================================================================
   Privileges
   ================================================================= */

BOOL enable_privilege(const char *name)
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!OpenProcessToken(GetCurrentProcess(),
            TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return FALSE;

    if (!LookupPrivilegeValueA(NULL, name, &luid))
    {
        CloseHandle(hToken);
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL ok = AdjustTokenPrivileges(
        hToken, FALSE, &tp, sizeof(tp), NULL, NULL);

    CloseHandle(hToken);
    return ok && (GetLastError() == ERROR_SUCCESS);
}

void enable_all_privileges(void)
{
    enable_privilege(SE_DEBUG_NAME);
    enable_privilege(SE_TAKE_OWNERSHIP_NAME);
    enable_privilege(SE_BACKUP_NAME);
    enable_privilege(SE_RESTORE_NAME);
}


/* =================================================================
   Handle Enumeration
   ================================================================= */

typedef LONG NTSTATUS;

typedef NTSTATUS (NTAPI *pNtQuerySystemInformation)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength);

typedef struct _SYSTEM_HANDLE_ENTRY {
    ULONG  ProcessId;
    BYTE   ObjectTypeIndex;
    BYTE   HandleAttributes;
    USHORT HandleValue;
    PVOID  Object;
    ULONG  GrantedAccess;
} SYSTEM_HANDLE_ENTRY;

typedef struct _SYSTEM_HANDLE_INFORMATION {
    ULONG               HandleCount;
    SYSTEM_HANDLE_ENTRY Handles[1];
} SYSTEM_HANDLE_INFORMATION;

int force_close_handles(const char *targetFile)
{
    enable_all_privileges();

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return 0;

    pNtQuerySystemInformation NtQSI =
        (pNtQuerySystemInformation)GetProcAddress(ntdll,
            "NtQuerySystemInformation");
    if (!NtQSI) return 0;

    ULONG bufSize = 1024 * 1024;
    PVOID buffer = NULL;
    NTSTATUS status;

    for (int tries = 0; tries < 8; tries++)
    {
        buffer = malloc(bufSize);
        if (!buffer) return 0;

        status = NtQSI(16, buffer, bufSize, &bufSize);
        if (status == 0) break;

        free(buffer);
        buffer = NULL;
        bufSize *= 2;
    }

    if (!buffer) return 0;

    SYSTEM_HANDLE_INFORMATION *shi = (SYSTEM_HANDLE_INFORMATION *)buffer;
    int closedCount = 0;

    char targetFull[MAX_PATH * 4];
    DWORD len = GetFullPathNameA(targetFile, sizeof(targetFull),
                                 targetFull, NULL);
    if (len == 0 || len >= sizeof(targetFull))
    {
        free(buffer);
        return 0;
    }

    for (ULONG i = 0; i < shi->HandleCount; i++)
    {
        SYSTEM_HANDLE_ENTRY *e = &shi->Handles[i];

        if (e->ProcessId == 0 || e->ProcessId == 4) continue;

        HANDLE hProc = OpenProcess(
            PROCESS_DUP_HANDLE, FALSE, e->ProcessId);
        if (!hProc) continue;

        HANDLE hDup = NULL;
        if (DuplicateHandle(hProc, (HANDLE)(ULONG_PTR)e->HandleValue,
                GetCurrentProcess(), &hDup,
                0, FALSE, DUPLICATE_SAME_ACCESS))
        {
            char nameBuf[MAX_PATH * 4];
            DWORD nameLen = GetFinalPathNameByHandleA(
                hDup, nameBuf, sizeof(nameBuf), 0);

            if (nameLen > 0 && nameLen < sizeof(nameBuf))
            {
                char *cleanName = nameBuf;
                if (strncmp(cleanName, "\\\\?\\", 4) == 0)
                    cleanName += 4;

                if (_stricmp(cleanName, targetFull) == 0)
                {
                    HANDLE hRemote = NULL;
                    if (DuplicateHandle(hProc,
                            (HANDLE)(ULONG_PTR)e->HandleValue,
                            hProc, &hRemote, 0, FALSE,
                            DUPLICATE_CLOSE_SOURCE))
                    {
                        CloseHandle(hRemote);
                        closedCount++;
                    }
                }
            }
            CloseHandle(hDup);
        }
        CloseHandle(hProc);
    }

    free(buffer);
    return closedCount;
}


/* =================================================================
   Ownership & ACL
   ================================================================= */

BOOL take_ownership(const char *file)
{
    enable_all_privileges();

    WCHAR wFile[MAX_PATH * 4];
    if (!MultiByteToWideChar(CP_UTF8, 0, file, -1,
            wFile, (int)(sizeof(wFile) / sizeof(WCHAR))))
        return FALSE;

    DWORD result = SetNamedSecurityInfoW(
        wFile, SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION,
        NULL, NULL, NULL, NULL);

    return result == ERROR_SUCCESS;
}

BOOL grant_everyone_full(const char *file)
{
    enable_all_privileges();
    take_ownership(file);

    EXPLICIT_ACCESSW ea;
    ZeroMemory(&ea, sizeof(ea));

    ea.grfAccessPermissions = GENERIC_ALL;
    ea.grfAccessMode        = SET_ACCESS;
    ea.grfInheritance       = SUB_CONTAINERS_AND_OBJECTS_INHERIT;

    PSID everyoneSid = NULL;
    if (!ConvertStringSidToSidW(L"S-1-1-0", &everyoneSid))
    {
        print_error("ConvertStringSidToSid");
        return FALSE;
    }

    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea.Trustee.ptstrName   = (LPWSTR)everyoneSid;

    PACL newAcl = NULL;
    DWORD result = SetEntriesInAclW(1, &ea, NULL, &newAcl);
    if (result != ERROR_SUCCESS)
    {
        LocalFree(everyoneSid);
        return FALSE;
    }

    WCHAR wFile[MAX_PATH * 4];
    if (!MultiByteToWideChar(CP_UTF8, 0, file, -1,
            wFile, (int)(sizeof(wFile) / sizeof(WCHAR))))
    {
        LocalFree(newAcl);
        LocalFree(everyoneSid);
        return FALSE;
    }

    result = SetNamedSecurityInfoW(
        wFile, SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
        NULL, NULL, newAcl, NULL);

    BOOL ok = (result == ERROR_SUCCESS);

    LocalFree(newAcl);
    LocalFree(everyoneSid);
    return ok;
}


/* =================================================================
   PendingFileRenameOperations
   ================================================================= */

BOOL schedule_delete_on_reboot(const char *file)
{
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SYSTEM\\CurrentControlSet\\Control\\Session Manager",
            0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return FALSE;

    DWORD type = 0, size = 0;
    RegQueryValueExA(hKey, "PendingFileRenameOperations",
                     NULL, &type, NULL, &size);

    BYTE *oldData = NULL;
    if (size > 0)
    {
        oldData = (BYTE *)malloc(size);
        RegQueryValueExA(hKey, "PendingFileRenameOperations",
                         NULL, &type, oldData, &size);
    }

    char fullPath[MAX_PATH * 2];
    SNPRINTF(fullPath, sizeof(fullPath), "\\??\\%s", file);

    DWORD pathLen = (DWORD)strlen(fullPath);
    DWORD newSize = size + pathLen + 2;
    BYTE *newData = (BYTE *)calloc(1, newSize);

    if (oldData) memcpy(newData, oldData, size);
    memcpy(newData + size, fullPath, pathLen);
    newData[size + pathLen + 1] = '\0';

    LONG res = RegSetValueExA(hKey, "PendingFileRenameOperations",
                   0, REG_MULTI_SZ, newData, newSize);

    free(oldData);
    free(newData);
    RegCloseKey(hKey);
    return res == ERROR_SUCCESS;
}


/* =================================================================
   Show Locking Processes — Common: gather process list
   -----------------------------------------------------------------
   Returns number of processes (0 = none, -1 = error)
   Fills names[] with process names, pids[] with PIDs,
   apps[] with application names.
   ================================================================= */

#define MAX_LOCK_PROCS 32

typedef struct {
    int   count;
    int   error;
    DWORD errorCode;
    char  names[MAX_LOCK_PROCS][MAX_PATH];
    DWORD pids[MAX_LOCK_PROCS];
    char  apps[MAX_LOCK_PROCS][256];
} LockList;

static void gather_locking_processes(const char *file, LockList *out)
{
    out->count = 0;
    out->error = 0;
    out->errorCode = 0;

    DWORD sessionHandle = 0;
    WCHAR sessionKey[CCH_RM_SESSION_KEY + 1];

    DWORD result = RmStartSession(&sessionHandle, 0, sessionKey);
    if (result != ERROR_SUCCESS)
    {
        out->error = 1;
        out->errorCode = result;
        return;
    }

    WCHAR wFile[MAX_PATH * 4];
    if (!MultiByteToWideChar(CP_UTF8, 0, file, -1,
            wFile, (int)(sizeof(wFile) / sizeof(WCHAR))))
    {
        RmEndSession(sessionHandle);
        out->error = 1;
        return;
    }

    LPCWSTR files[1] = { wFile };
    result = RmRegisterResources(sessionHandle, 1, files, 0, NULL, 0, NULL);
    if (result != ERROR_SUCCESS)
    {
        RmEndSession(sessionHandle);
        out->error = 1;
        out->errorCode = result;
        return;
    }

    UINT needed = 0, count = 0;
    DWORD rebootReasons = 0;

    result = RmGetList(sessionHandle, &needed, &count, NULL, &rebootReasons);

    if (result == ERROR_SUCCESS && needed == 0)
    {
        RmEndSession(sessionHandle);
        out->count = 0;
        return;
    }

    if (result != ERROR_MORE_DATA)
    {
        RmEndSession(sessionHandle);
        out->error = 1;
        out->errorCode = result;
        return;
    }

    RM_PROCESS_INFO *processes =
        (RM_PROCESS_INFO *)malloc(needed * sizeof(RM_PROCESS_INFO));
    if (!processes)
    {
        RmEndSession(sessionHandle);
        out->error = 1;
        return;
    }

    count = needed;
    result = RmGetList(sessionHandle, &needed, &count,
                       processes, &rebootReasons);

    if (result != ERROR_SUCCESS)
    {
        free(processes);
        RmEndSession(sessionHandle);
        out->error = 1;
        out->errorCode = result;
        return;
    }

    for (UINT i = 0; i < count && out->count < MAX_LOCK_PROCS; i++)
    {
        DWORD pid = processes[i].Process.dwProcessId;
        out->pids[out->count] = pid;

        /* Process name */
        safe_strcpy(out->names[out->count], MAX_PATH, "unknown.exe");
        HANDLE hProcess = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess)
        {
            char path[MAX_PATH * 4];
            DWORD size = sizeof(path);
            if (QueryFullProcessImageNameA(hProcess, 0, path, &size))
            {
                const char *name = strrchr(path, '\\');
                if (name)
                    safe_strcpy(out->names[out->count], MAX_PATH, name + 1);
            }
            CloseHandle(hProcess);
        }

        /* Application name */
        out->apps[out->count][0] = '\0';
        WideCharToMultiByte(CP_UTF8, 0, processes[i].strAppName, -1,
                            out->apps[out->count], 256, NULL, NULL);

        out->count++;
    }

    free(processes);
    RmEndSession(sessionHandle);
}


/* =================================================================
   P = Show locking processes — JSON output (for -silent / CLI)
   ================================================================= */

int show_locking_processes_json(const char *file)
{
    LockList list;
    gather_locking_processes(file, &list);

    if (list.error)
    {
        if (have_console())
            printf("{\n  \"file\": \"%s\",\n  \"error\": %lu,\n"
                   "  \"locking_processes\": []\n}\n",
                   file, list.errorCode);
        return 0;
    }

    if (!have_console())
        return (list.count > 0) ? 1 : 1;

    printf("{\n  \"file\": \"");
    print_json_escaped(file);
    printf("\",\n  \"locking_count\": %d,\n", list.count);

    if (list.count == 0)
    {
        printf("  \"locking_processes\": []\n}\n");
        return 1;
    }

    printf("  \"locking_processes\": [\n");

    for (int i = 0; i < list.count; i++)
    {
        printf("    {\n");
        printf("      \"pid\": %lu,\n", list.pids[i]);
        printf("      \"process\": \"");
        print_json_escaped(list.names[i]);
        printf("\",\n");
        printf("      \"application\": \"");
        print_json_escaped(list.apps[i][0] ? list.apps[i] : "");
        printf("\"\n    }");

        if (i + 1 < list.count) printf(",");
        printf("\n");
    }

    printf("  ]\n}\n");
    return 1;
}


/* =================================================================
   P = Show locking processes — POPUP (for context menu)
   -----------------------------------------------------------------
   Show only Process names - simple list.
   ================================================================= */

int show_locking_processes_popup(const char *file)
{
    LockList list;
    gather_locking_processes(file, &list);

    char msg[4096];
    msg[0] = '\0';

    if (list.error)
    {
        SNPRINTF(msg, sizeof(msg),
                 "Could not query locking processes.\n\n"
                 "Error code: %lu", list.errorCode);
        popup_error("Show Locking Apps Failed", msg);
        return 0;
    }

    if (list.count == 0)
    {
        SNPRINTF(msg, sizeof(msg),
                 "No processes are locking this file.\n\n"
                 "File:\n%s", file);
        popup_info("Locking Apps", msg);
        return 1;
    }

    if (list.count == 1)
    {
        SNPRINTF(msg, sizeof(msg),
                 "1 process is locking this file:\n\n"
                 "  %s", list.names[0]);
        popup_info("Locking Apps", msg);
        return 1;
    }

    SNPRINTF(msg, sizeof(msg),
             "%d processes are locking this file:\n\n", list.count);

    for (int i = 0; i < list.count; i++)
    {
        char line[MAX_PATH + 8];
        SNPRINTF(line, sizeof(line), "  %s\n", list.names[i]);
        strncat(msg, line, sizeof(msg) - strlen(msg) - 1);
    }

    popup_info("Locking Apps", msg);
    return 1;
}


/* =================================================================
   U = Unlock
   ================================================================= */

int unlock_file(const char *file)
{
    enable_all_privileges();

    if (have_console())
        printf("[*] Trying Restart Manager unlock...\n");

    DWORD sessionHandle = 0;
    WCHAR sessionKey[CCH_RM_SESSION_KEY + 1];

    DWORD result = RmStartSession(&sessionHandle, 0, sessionKey);
    if (result == ERROR_SUCCESS)
    {
        WCHAR wFile[MAX_PATH * 4];
        if (MultiByteToWideChar(CP_UTF8, 0, file, -1,
                wFile, (int)(sizeof(wFile) / sizeof(WCHAR))))
        {
            LPCWSTR files[1] = { wFile };
            result = RmRegisterResources(sessionHandle, 1, files, 0, NULL, 0, NULL);

            if (result == ERROR_SUCCESS)
            {
                UINT needed = 0, count = 0;
                DWORD reasons = 0;
                result = RmGetList(sessionHandle, &needed, &count, NULL, &reasons);

                if (result == ERROR_SUCCESS && needed == 0)
                {
                    if (have_console())
                        printf("[+] File is not locked.\n");
                    RmEndSession(sessionHandle);
                    return 1;
                }

                if (result == ERROR_MORE_DATA)
                {
                    RM_PROCESS_INFO *procs = (RM_PROCESS_INFO *)
                        malloc(needed * sizeof(RM_PROCESS_INFO));
                    if (procs)
                    {
                        count = needed;
                        if (RmGetList(sessionHandle, &needed, &count,
                                procs, &reasons) == ERROR_SUCCESS)
                        {
                            if (have_console())
                                printf("[*] Locking processes:\n");
                            for (UINT i = 0; i < count; i++)
                            {
                                if (have_console())
                                {
                                    printf("    PID %lu - ",
                                        procs[i].Process.dwProcessId);
                                    print_process_name(procs[i].Process.dwProcessId);
                                    printf("\n");
                                }
                            }
                        }
                        free(procs);
                    }
                }

                if (have_console())
                    printf("[*] Requesting RmShutdown...\n");
                RmShutdown(sessionHandle, 0, NULL);
            }
        }
        RmEndSession(sessionHandle);
    }

    HANDLE h = CreateFileA(file, GENERIC_READ | GENERIC_WRITE,
                           0, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(h);
        if (have_console())
            printf("[+] File is now unlocked.\n");
        return 1;
    }

    if (have_console())
        printf("[*] Trying handle enumeration...\n");
    int closed = force_close_handles(file);
    if (have_console())
        printf("[*] Closed %d handle(s).\n", closed);

    h = CreateFileA(file, GENERIC_READ | GENERIC_WRITE,
                    0, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(h);
        if (have_console())
            printf("[+] File is now unlocked.\n");
        return 1;
    }

    if (have_console())
        printf("[!] File may still be locked.\n");
    return 0;
}


/* =================================================================
   D = Force Delete
   ================================================================= */

int force_delete(const char *file)
{
    enable_all_privileges();

    if (have_console())
    {
        printf("========================================\n");
        printf("  Force Delete: %s\n", file);
        printf("========================================\n");
    }

    DWORD attr = GetFileAttributesA(file);
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        char buf[1024];
        SNPRINTF(buf, sizeof(buf),
                 "File not found or inaccessible:\n%s", file);
        popup_error("Force Delete Failed", buf);
        print_error("GetFileAttributes");
        return 0;
    }

    if (have_console())
        printf("[Stage 1] Fixing attributes...\n");

    if (attr & (FILE_ATTRIBUTE_READONLY |
                FILE_ATTRIBUTE_HIDDEN |
                FILE_ATTRIBUTE_SYSTEM))
    {
        SetFileAttributesA(file,
            attr & ~(FILE_ATTRIBUTE_READONLY |
                     FILE_ATTRIBUTE_HIDDEN |
                     FILE_ATTRIBUTE_SYSTEM));
    }

    if (DeleteFileA(file))
    {
        if (have_console())
            printf("[+] Deleted directly.\n");
        {
            char buf[1024];
            SNPRINTF(buf, sizeof(buf),
                     "File deleted successfully:\n\n%s", file);
            popup_success("File Deleted", buf);
        }
        return 1;
    }

    if (have_console())
        printf("    Failed (Error %lu)\n", GetLastError());

    if (have_console())
        printf("[Stage 2] Granting Everyone 777...\n");
    grant_everyone_full(file);

    if (DeleteFileA(file))
    {
        if (have_console())
            printf("[+] Deleted after permission fix.\n");
        {
            char buf[1024];
            SNPRINTF(buf, sizeof(buf),
                     "File deleted successfully:\n\n%s", file);
            popup_success("File Deleted", buf);
        }
        return 1;
    }

    if (have_console())
        printf("    Failed (Error %lu)\n", GetLastError());

    if (have_console())
        printf("[Stage 3] Trying Restart Manager unlock...\n");
    unlock_file(file);

    if (DeleteFileA(file))
    {
        if (have_console())
            printf("[+] Deleted after unlock.\n");
        {
            char buf[1024];
            SNPRINTF(buf, sizeof(buf),
                     "File deleted successfully:\n\n%s", file);
            popup_success("File Deleted", buf);
        }
        return 1;
    }

    if (have_console())
        printf("    Failed (Error %lu)\n", GetLastError());

    if (have_console())
        printf("[Stage 4] Aggressive handle close...\n");
    int closed = force_close_handles(file);
    if (have_console())
        printf("    Closed %d handle(s).\n", closed);

    if (DeleteFileA(file))
    {
        if (have_console())
            printf("[+] Deleted after handle close.\n");
        {
            char buf[1024];
            SNPRINTF(buf, sizeof(buf),
                     "File deleted successfully:\n\n%s", file);
            popup_success("File Deleted", buf);
        }
        return 1;
    }

    if (have_console())
        printf("    Failed (Error %lu)\n", GetLastError());

    if (have_console())
        printf("[Stage 5] Taking ownership...\n");
    take_ownership(file);
    grant_everyone_full(file);

    if (DeleteFileA(file))
    {
        if (have_console())
            printf("[+] Deleted after ownership.\n");
        {
            char buf[1024];
            SNPRINTF(buf, sizeof(buf),
                     "File deleted successfully:\n\n%s", file);
            popup_success("File Deleted", buf);
        }
        return 1;
    }

    if (have_console())
        printf("    Failed (Error %lu)\n", GetLastError());

    if (have_console())
        printf("[Stage 6] Scheduling delete on next reboot...\n");

    if (schedule_delete_on_reboot(file))
    {
        if (have_console())
            printf("[+] File will be deleted on next reboot.\n");
        {
            char buf[1024];
            SNPRINTF(buf, sizeof(buf),
                     "File is currently locked and cannot be deleted now."
                     "\n\nIt has been scheduled for deletion on the NEXT"
                     " RESTART.\n\nFile:\n%s", file);
            popup_info("Reboot Required", buf);
        }
        return 1;
    }

    if (have_console())
        printf("[!] All stages failed.\n");
    {
        char buf[1024];
        SNPRINTF(buf, sizeof(buf),
                 "Could not delete file even after trying all methods."
                 "\n\nFile:\n%s", file);
        popup_error("Force Delete Failed", buf);
    }
    return 0;
}


/* =================================================================
   Grant Everyone (with popups)
   ================================================================= */

int grant_everyone_with_popup(const char *file)
{
    if (grant_everyone_full(file))
    {
        char buf[1024];
        SNPRINTF(buf, sizeof(buf),
                 "Full access (777) granted to Everyone:\n\n%s", file);
        popup_success("Permissions Updated", buf);
        return 1;
    }

    char buf[1024];
    SNPRINTF(buf, sizeof(buf),
             "Failed to grant permissions:\n\n%s", file);
    popup_error("Permission Grant Failed", buf);
    return 0;
}


/* =================================================================
   Take Ownership (with popups)
   ================================================================= */

int take_ownership_with_popup(const char *file)
{
    if (take_ownership(file))
    {
        char buf[1024];
        SNPRINTF(buf, sizeof(buf),
                 "Ownership transferred to current user:\n\n%s", file);
        popup_success("Ownership Taken", buf);
        return 1;
    }

    char buf[1024];
    SNPRINTF(buf, sizeof(buf),
             "Failed to take ownership:\n\n%s", file);
    popup_error("Take Ownership Failed", buf);
    return 0;
}


/* =================================================================
   Unlock (with popups)
   ================================================================= */

int unlock_with_popup(const char *file)
{
    if (unlock_file(file))
    {
        char buf[1024];
        SNPRINTF(buf, sizeof(buf),
                 "File lock released successfully:\n\n%s", file);
        popup_success("File Unlocked", buf);
        return 1;
    }

    char buf[1024];
    SNPRINTF(buf, sizeof(buf),
             "Could not release file lock:\n\n%s", file);
    popup_error("Unlock Failed", buf);
    return 0;
}


/* =================================================================
   Context Menu Registration
   ================================================================= */

BOOL register_extended_subcommands(void)
{
    RegDeleteTreeA(HKEY_LOCAL_MACHINE, CTX_EXT_KEY);

    HKEY hExt = NULL;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, CTX_EXT_KEY,
            0, NULL, 0, KEY_WRITE, NULL, &hExt, NULL) != ERROR_SUCCESS)
        return FALSE;

    HKEY hExtShell = NULL;
    if (RegCreateKeyExA(hExt, "shell",
            0, NULL, 0, KEY_WRITE, NULL, &hExtShell, NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hExt);
        return FALSE;
    }

    struct {
        const char *name;
        const char *label;
        const char *args;
        const char *icon;
    } subs[] = {
        { "01Delete",   "Force Delete",           "-f \"%1\" D", CTX_ICON_DELETE },
        { "02Unlock",   "Unlock (release lock)",  "-f \"%1\" U", CTX_ICON_UNLOCK },
        { "03Grant777", "Grant Everyone (777)",   "-f \"%1\" A", CTX_ICON_GRANT  },
        { "04TakeOwn",  "Take Ownership",         "-f \"%1\" O", CTX_ICON_OWNER  },
        { "05ShowLock", "Show locking apps",      "-f \"%1\" P", CTX_ICON_LOCK   },
    };

    for (int i = 0; i < 5; i++)
    {
        HKEY hSub = NULL;
        if (RegCreateKeyExA(hExtShell, subs[i].name,
                0, NULL, 0, KEY_WRITE, NULL, &hSub, NULL) != ERROR_SUCCESS)
            continue;

        RegSetValueExA(hSub, NULL, 0, REG_SZ,
            (const BYTE *)subs[i].label,
            (DWORD)(strlen(subs[i].label) + 1));

        RegSetValueExA(hSub, "Icon", 0, REG_SZ,
            (const BYTE *)subs[i].icon,
            (DWORD)(strlen(subs[i].icon) + 1));

        HKEY hCmd = NULL;
        if (RegCreateKeyExA(hSub, "command",
                0, NULL, 0, KEY_WRITE, NULL, &hCmd, NULL) == ERROR_SUCCESS)
        {
            char cmdLine[1024];
            SNPRINTF(cmdLine, sizeof(cmdLine),
                     "\"%s\" %s", INSTALL_EXE, subs[i].args);

            RegSetValueExA(hCmd, NULL, 0, REG_SZ,
                (const BYTE *)cmdLine, (DWORD)(strlen(cmdLine) + 1));
            RegCloseKey(hCmd);
        }
        RegCloseKey(hSub);
    }

    RegCloseKey(hExtShell);
    RegCloseKey(hExt);
    return TRUE;
}

BOOL register_one_menu(const char *baseKey, BOOL isBackground)
{
    RegDeleteTreeA(HKEY_LOCAL_MACHINE, baseKey);

    HKEY hKey = NULL;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, baseKey,
            0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return FALSE;

    const char *label = "Unlocker Pro";

    RegSetValueExA(hKey, "MUIVerb", 0, REG_SZ,
        (const BYTE *)label, (DWORD)(strlen(label) + 1));

    RegSetValueExA(hKey, "Icon", 0, REG_SZ,
        (const BYTE *)CTX_ICON_MAIN,
        (DWORD)(strlen(CTX_ICON_MAIN) + 1));

    const char *emptySub = "";
    RegSetValueExA(hKey, "SubCommands", 0, REG_SZ,
        (const BYTE *)emptySub, 1);

    const char *extKeyName = "UnlockerProMenu";
    RegSetValueExA(hKey, "ExtendedSubCommandsKey", 0, REG_SZ,
        (const BYTE *)extKeyName, (DWORD)(strlen(extKeyName) + 1));

    if (isBackground)
    {
        const char *noWorkingDir = "";
        RegSetValueExA(hKey, "NoWorkingDirectory", 0, REG_SZ,
            (const BYTE *)noWorkingDir, 1);
    }

    RegCloseKey(hKey);
    return TRUE;
}

BOOL register_all_context_menus(void)
{
    if (!register_extended_subcommands())
        return FALSE;

    register_one_menu(CTX_KEY_FILE,  FALSE);
    register_one_menu(CTX_KEY_DIR,   FALSE);
    register_one_menu(CTX_KEY_DRIVE, FALSE);
    register_one_menu(CTX_KEY_DIRBG, TRUE);
    return TRUE;
}


/* =================================================================
   Control Panel Entry
   ================================================================= */

BOOL register_uninstall_entry(void)
{
    HKEY hKey = NULL;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, APP_UNINST_KEY,
            0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
    {
        print_error("RegCreateKeyEx (uninstall)");
        return FALSE;
    }

    RegSetValueExA(hKey, "DisplayName", 0, REG_SZ,
        (const BYTE *)APP_DISPLAYNAME, (DWORD)(strlen(APP_DISPLAYNAME) + 1));

    char uninstCmd[1024];
    SNPRINTF(uninstCmd, sizeof(uninstCmd),
             "\"%s\" -uninstall", INSTALL_EXE);

    RegSetValueExA(hKey, "UninstallString", 0, REG_SZ,
        (const BYTE *)uninstCmd, (DWORD)(strlen(uninstCmd) + 1));

    RegSetValueExA(hKey, "QuietUninstallString", 0, REG_SZ,
        (const BYTE *)uninstCmd, (DWORD)(strlen(uninstCmd) + 1));

    RegSetValueExA(hKey, "DisplayVersion", 0, REG_SZ,
        (const BYTE *)APP_VERSION, (DWORD)(strlen(APP_VERSION) + 1));

    RegSetValueExA(hKey, "Publisher", 0, REG_SZ,
        (const BYTE *)APP_PUBLISHER, (DWORD)(strlen(APP_PUBLISHER) + 1));

    RegSetValueExA(hKey, "InstallLocation", 0, REG_SZ,
        (const BYTE *)INSTALL_DIR, (DWORD)(strlen(INSTALL_DIR) + 1));

    DWORD estimatedSize = 800;
    RegSetValueExA(hKey, "EstimatedSize", 0, REG_DWORD,
        (const BYTE *)&estimatedSize, sizeof(estimatedSize));

    DWORD noModify = 1, noRepair = 1;
    RegSetValueExA(hKey, "NoModify", 0, REG_DWORD,
        (const BYTE *)&noModify, sizeof(noModify));
    RegSetValueExA(hKey, "NoRepair", 0, REG_DWORD,
        (const BYTE *)&noRepair, sizeof(noRepair));

    RegCloseKey(hKey);
    return TRUE;
}


/* =================================================================
   Install
   ================================================================= */

BOOL do_install(void)
{
    if (!is_admin())
    {
        if (have_console())
            printf("[!] Administrator permission required.\n");
        popup_error("Admin Required",
                    "Administrator privileges are required for installation.");
        return FALSE;
    }

    if (have_console())
    {
        printf("\n========================================\n");
        printf("  Installing Unlocker Pro %s\n", APP_VERSION);
        printf("========================================\n\n");
        printf("[*] Cleaning old registry entries...\n");
    }

    cleanup_all_registry();

    if (have_console())
        printf("[+] Clean.\n\n[*] Creating install folder...\n");

    if (!CreateDirectoryA(INSTALL_DIR, NULL))
    {
        if (GetLastError() != ERROR_ALREADY_EXISTS)
        {
            print_error("CreateDirectory");
            return FALSE;
        }
    }

    if (have_console())
        printf("[+] Folder ready: %s\n", INSTALL_DIR);

    char selfPath[MAX_PATH];
    GetModuleFileNameA(NULL, selfPath, MAX_PATH);

    if (have_console())
    {
        printf("[*] Source: %s\n", selfPath);
        printf("[*] Target: %s\n", INSTALL_EXE);
    }

    if (!paths_equal(selfPath, INSTALL_EXE))
    {
        DeleteFileA(INSTALL_EXE);
        Sleep(200);

        if (!CopyFileA(selfPath, INSTALL_EXE, FALSE))
        {
            print_error("CopyFile");
            if (have_console())
                printf("\n[!] INSTALL FAILED: Cannot copy exe.\n");
            popup_error("Install Failed",
                        "Could not copy executable to install directory.");
            return FALSE;
        }
    }

    if (have_console())
        printf("[*] Verifying copy...\n");

    Sleep(500);
    if (GetFileAttributesA(INSTALL_EXE) == INVALID_FILE_ATTRIBUTES)
    {
        if (have_console())
            printf("[!] VERIFICATION FAILED: exe not found after copy!\n");
        popup_error("Install Failed",
                    "Executable not found after copy.");
        return FALSE;
    }

    if (have_console())
        printf("[+] Copy verified.\n\n[*] Registering context menus...\n");

    if (!register_all_context_menus())
    {
        if (have_console())
            printf("[!] Context menu registration failed.\n");
        popup_error("Install Failed",
                    "Could not register context menu.");
        return FALSE;
    }

    if (have_console())
        printf("[+] Right-click menu installed.\n"
               "[*] Registering Control Panel entry...\n");

    if (!register_uninstall_entry())
    {
        if (have_console())
            printf("[!] Control Panel entry failed.\n");
        return FALSE;
    }

    if (have_console())
    {
        printf("[+] Added to Control Panel.\n");
        printf("\n========================================\n");
        printf("  [SUCCESS] Installation complete!\n");
        printf("========================================\n\n");
    }

    popup_success("Installation Complete",
                  "Unlocker Pro has been installed successfully.\n\n"
                  "Right-click any file to see the new menu.");
    return TRUE;
}


/* =================================================================
   Uninstall
   ================================================================= */

BOOL do_uninstall(void)
{
    if (!is_admin())
    {
        if (have_console())
            printf("[!] Administrator permission required.\n");
        return FALSE;
    }

    if (have_console())
        printf("\n[*] Uninstalling Unlocker Pro...\n");

    cleanup_all_registry();

    if (have_console())
        printf("[+] Registry cleaned.\n");

    char cmd[1024];
    SNPRINTF(cmd, sizeof(cmd),
        "cmd.exe /c timeout /t 2 /nobreak >nul & "
        "del /f /q \"%s\" & "
        "rmdir /q \"%s\"",
        INSTALL_EXE, INSTALL_DIR);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    popup_success("Uninstalled",
                  "Unlocker Pro has been uninstalled.");
    return TRUE;
}


/* =================================================================
   Auto-Repair
   ================================================================= */

BOOL do_repair(void)
{
    if (!is_admin())
    {
        if (elevate_self("-repair")) return TRUE;
        return FALSE;
    }

    if (have_console())
        printf("\n[*] Running auto-repair...\n\n");

    int state = check_installation_state();

    if (have_console())
    {
        printf("[*] Detected state: ");
        switch (state) {
            case 0: printf("Not installed\n"); break;
            case 1: printf("Fully installed\n"); break;
            case 2: printf("Broken (partial)\n"); break;
        }
    }

    if (state == 1) {
        if (have_console())
            printf("[+] Installation is healthy. Nothing to repair.\n");
        popup_info("Repair", "Installation is healthy. Nothing to repair.");
        return TRUE;
    }

    if (have_console())
        printf("\n[*] Re-installing cleanly...\n");

    return do_install();
}


/* =================================================================
   Interactive Mode
   ================================================================= */

int interactive_mode(void)
{
    if (!GetConsoleWindow())
    {
        alloc_own_console();
    }
    else
    {
        g_hasConsole = TRUE;
    }

    apply_icon_to_console();

    printf("\n========================================\n");
    printf("   Unlocker Pro %s\n", APP_VERSION);
    printf("========================================\n\n");

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    BOOL runningFromInstall = paths_equal(exePath, INSTALL_EXE);

    int state = check_installation_state();

    printf("[*] Running from: %s\n", exePath);
    printf("[*] Installation state: ");
    switch (state) {
        case 0: printf("Not installed\n\n"); break;
        case 1: printf("Fully installed\n\n"); break;
        case 2: printf("BROKEN (registry/exe mismatch)\n\n"); break;
    }

    if (state == 1 && runningFromInstall)
    {
        printf("Unlocker Pro is installed and healthy.\n\n");
        printf("  [U] Uninstall\n");
        printf("  [R] Repair / Reinstall\n");
        printf("  [Q] Quit\n\n");
        printf("Choice: ");

        char line[16];
        if (!fgets(line, sizeof(line), stdin)) return 0;
        char c = (char)toupper((unsigned char)line[0]);

        if (c == 'U') {
            if (!is_admin()) { if (elevate_self("-uninstall")) return 0; return 1; }
            return do_uninstall() ? 0 : 1;
        }
        if (c == 'R') {
            if (!is_admin()) { if (elevate_self("-install")) return 0; return 1; }
            return do_install() ? 0 : 1;
        }
        return 0;
    }

    if (state == 2)
    {
        printf("WARNING: Installation is BROKEN.\n");
        printf("Registry entries exist but exe is missing (or vice versa).\n\n");
        printf("  [F] Fix automatically (recommended)\n");
        printf("  [U] Uninstall completely\n");
        printf("  [Q] Quit\n\n");
        printf("Choice: ");

        char line[16];
        if (!fgets(line, sizeof(line), stdin)) return 0;
        char c = (char)toupper((unsigned char)line[0]);

        if (c == 'F') {
            if (!is_admin()) { if (elevate_self("-repair")) return 0; return 1; }
            return do_repair() ? 0 : 1;
        }
        if (c == 'U') {
            if (!is_admin()) { if (elevate_self("-uninstall")) return 0; return 1; }
            return do_uninstall() ? 0 : 1;
        }
        return 0;
    }

    printf("Unlocker Pro is not installed.\n\n");
    printf("  This tool removes file-locks and permissions blocks.\n");
    printf("  Use it to delete/modify stubborn files that\n");
    printf("  Windows otherwise refuses to touch.\n\n");
    printf("  Type Y and press Enter to install.\n");
    printf("  Type N and press Enter to exit.\n\n");
    printf("Install? [Y/N]: ");

    char line[16];
    if (!fgets(line, sizeof(line), stdin)) return 0;
    char c = (char)toupper((unsigned char)line[0]);

    if (c == 'Y') {
        if (!is_admin()) {
            if (elevate_self("-install")) return 0;
            printf("[!] Elevation failed.\n");
            return 1;
        }
        return do_install() ? 0 : 1;
    }
    printf("Cancelled.\n");
    return 0;
}


/* =================================================================
   Help
   ================================================================= */

void show_help(void)
{
    if (have_console())
    {
        printf("\nUnlocker Pro %s\n", APP_VERSION);
        printf("------------------------------\n\n");
        printf("Usage:\n");
        printf("  unlocker.exe                             (interactive)\n");
        printf("  unlocker.exe -install                    (install)\n");
        printf("  unlocker.exe -uninstall                  (uninstall)\n");
        printf("  unlocker.exe -repair                     (auto-fix)\n");
        printf("  unlocker.exe -status                     (check state)\n");
        printf("  unlocker.exe -f \"file\" COMMAND [options]\n\n");
        printf("Options:\n");
        printf("  -silent    Suppress popups + JSON output for P\n\n");
        printf("Commands:\n");
        printf("  P   Show locking processes\n");
        printf("      (popup normally, JSON in -silent mode)\n");
        printf("  U   Unlock file\n");
        printf("  D   Force delete\n");
        printf("  A   Grant Everyone full access (777)\n");
        printf("  O   Take ownership\n");
        printf("  R   Rename (needs new name)\n");
        printf("  M   Move   (needs directory)\n");
        printf("  C   Copy   (needs directory)\n\n");
    }
    else
    {
        popup_info("Unlocker Pro",
                   "Usage:\n\n"
                   "unlocker.exe -install\n"
                   "unlocker.exe -uninstall\n"
                   "unlocker.exe -repair\n"
                   "unlocker.exe -status\n"
                   "unlocker.exe -f \"file\" COMMAND");
    }
}


/* =================================================================
   Command Line Parser
   ================================================================= */

#define MAX_ARGS 64

static int parse_command_line(LPSTR cmdLine, char **argv, int maxArgs)
{
    int argc = 0;
    char *p = cmdLine;

    while (*p && argc < maxArgs - 1)
    {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        if (*p == '"')
        {
            p++;
            argv[argc++] = p;
            while (*p && *p != '"') p++;
            if (*p == '"') *p++ = '\0';
        }
        else
        {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
    }

    argv[argc] = NULL;
    return argc;
}


/* =================================================================
   WinMain — Entry Point
   ================================================================= */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)nCmdShow;

    g_hasConsole = attach_parent_console();

    static char cmdLineBuf[8192];
    static char *argv[MAX_ARGS];

    safe_strcpy(cmdLineBuf, sizeof(cmdLineBuf),
                lpCmdLine ? lpCmdLine : "");

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    argv[0] = exePath;
    int argc = 1;

    static char *parsedArgs[MAX_ARGS];
    int parsedCount = parse_command_line(cmdLineBuf, parsedArgs, MAX_ARGS - 1);

    for (int i = 0; i < parsedCount && argc < MAX_ARGS - 1; i++)
    {
        argv[argc++] = parsedArgs[i];
    }
    argv[argc] = NULL;

    /* Silent mode check */
    for (int i = 1; i < argc; i++)
    {
        if (_stricmp(argv[i], "-silent") == 0)
        {
            g_silentMode = TRUE;
        }
    }

    if (argc < 2) return interactive_mode();

    if (argc == 2 && _stricmp(argv[1], "-silent") == 0)
        return interactive_mode();

    if (_stricmp(argv[1], "-status") == 0) {
        int state = check_installation_state();
        if (have_console())
        {
            printf("Installation state: ");
            switch (state) {
                case 0: printf("Not installed\n"); break;
                case 1: printf("Fully installed\n"); break;
                case 2: printf("BROKEN\n"); break;
            }
        }
        else
        {
            const char *msg =
                (state == 0) ? "Not installed" :
                (state == 1) ? "Fully installed" : "BROKEN";
            popup_info("Unlocker Pro - Status", msg);
        }
        return 0;
    }

    if (_stricmp(argv[1], "-repair") == 0) {
        if (!is_admin()) {
            if (elevate_self("-repair")) return 0;
            return 1;
        }
        return do_repair() ? 0 : 1;
    }

    if (_stricmp(argv[1], "-install") == 0 || _stricmp(argv[1], "/install") == 0) {
        if (!is_admin()) {
            if (elevate_self("-install")) return 0;
            if (have_console()) printf("Elevation failed.\n");
            return 1;
        }
        return do_install() ? 0 : 1;
    }

    if (_stricmp(argv[1], "-uninstall") == 0 || _stricmp(argv[1], "/uninstall") == 0) {
        if (!is_admin()) {
            if (elevate_self("-uninstall")) return 0;
            if (have_console()) printf("Elevation failed.\n");
            return 1;
        }
        return do_uninstall() ? 0 : 1;
    }

    if (argc < 4 || _stricmp(argv[1], "-f") != 0) {
        show_help();
        return 1;
    }

    const char *file = argv[2];
    const char *command = argv[3];

    if (_stricmp(command, "D") == 0 || _stricmp(command, "U") == 0 ||
        _stricmp(command, "A") == 0 || _stricmp(command, "O") == 0) {
        if (!is_admin()) {
            char args[2048];
            SNPRINTF(args, sizeof(args), "-f \"%s\" %s%s",
                     file, command, g_silentMode ? " -silent" : "");
            if (elevate_self(args)) return 0;
        }
    }

    DWORD attributes = GetFileAttributesA(file);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        char buf[1024];
        SNPRINTF(buf, sizeof(buf),
                 "File not found or inaccessible:\n\n%s", file);
        popup_error("Operation Failed", buf);
        if (have_console())
            print_error("GetFileAttributes");
        return 1;
    }

    /* ★ P — Silent mode me JSON, warna popup ★ */
    if (_stricmp(command, "P") == 0)
    {
        if (g_silentMode)
            return show_locking_processes_json(file) ? 0 : 1;
        else
            return show_locking_processes_popup(file) ? 0 : 1;
    }

    if (_stricmp(command, "U") == 0)
        return unlock_with_popup(file) ? 0 : 1;

    if (_stricmp(command, "D") == 0)
        return force_delete(file) ? 0 : 1;

    if (_stricmp(command, "A") == 0)
        return grant_everyone_with_popup(file) ? 0 : 1;

    if (_stricmp(command, "O") == 0)
        return take_ownership_with_popup(file) ? 0 : 1;

    if (_stricmp(command, "R") == 0) {
        if (argc < 5) {
            if (have_console()) printf("R requires new filename.\n");
            popup_error("Rename Failed", "New filename not provided.");
            return 1;
        }
        char dir[MAX_PATH * 4];
        safe_strcpy(dir, sizeof(dir), file);
        char *slash = strrchr(dir, '\\');
        if (!slash) safe_strcpy(dir, sizeof(dir), ".");
        else *slash = '\0';
        char dest[MAX_PATH * 4];
        SNPRINTF(dest, sizeof(dest), "%s\\%s", dir, argv[4]);
        if (MoveFileExA(file, dest,
                MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)) {
            if (have_console()) printf("Renamed to: %s\n", dest);
            char buf[1024];
            SNPRINTF(buf, sizeof(buf), "Renamed to:\n\n%s", dest);
            popup_success("Rename Successful", buf);
            return 0;
        }
        print_error("MoveFileEx");
        popup_error("Rename Failed", "Could not rename the file.");
        return 1;
    }

    if (_stricmp(command, "M") == 0) {
        if (argc < 5) {
            if (have_console()) printf("M requires destination.\n");
            popup_error("Move Failed", "Destination not provided.");
            return 1;
        }
        const char *n = strrchr(file, '\\');
        n = n ? n + 1 : file;
        char dest[MAX_PATH * 4];
        SNPRINTF(dest, sizeof(dest), "%s\\%s", argv[4], n);
        if (MoveFileExA(file, dest,
                MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)) {
            if (have_console()) printf("Moved to: %s\n", dest);
            char buf[1024];
            SNPRINTF(buf, sizeof(buf), "Moved to:\n\n%s", dest);
            popup_success("Move Successful", buf);
            return 0;
        }
        print_error("MoveFileEx");
        popup_error("Move Failed", "Could not move the file.");
        return 1;
    }

    if (_stricmp(command, "C") == 0) {
        if (argc < 5) {
            if (have_console()) printf("C requires destination.\n");
            popup_error("Copy Failed", "Destination not provided.");
            return 1;
        }
        const char *n = strrchr(file, '\\');
        n = n ? n + 1 : file;
        char dest[MAX_PATH * 4];
        SNPRINTF(dest, sizeof(dest), "%s\\%s", argv[4], n);
        if (CopyFileA(file, dest, FALSE)) {
            if (have_console()) printf("Copied to: %s\n", dest);
            char buf[1024];
            SNPRINTF(buf, sizeof(buf), "Copied to:\n\n%s", dest);
            popup_success("Copy Successful", buf);
            return 0;
        }
        print_error("CopyFile");
        popup_error("Copy Failed", "Could not copy the file.");
        return 1;
    }

    if (have_console()) printf("Unknown command: %s\n", command);
    show_help();
    return 1;
}
