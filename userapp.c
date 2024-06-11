#include <windows.h>
#include <stdio.h>
#include <wincred.h>
#include <shellapi.h>
#include <tchar.h>

// Function declaration
BOOL AuthenticateUser();
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void MonitorApplications(HANDLE hPipe);
BOOL CheckForKernelSignal(HANDLE hPipe);

// Function to prompt for authentication
BOOL AuthenticateUser() {
    CREDUI_INFO cui;
    TCHAR pszName[CREDUI_MAX_USERNAME_LENGTH] = _T("");
    TCHAR pszPwd[CREDUI_MAX_PASSWORD_LENGTH] = _T("");
    BOOL fSave = FALSE;
    DWORD dwErr;
    ULONG authPackage = 0;
    LPVOID outAuthBuffer = NULL;
    ULONG outAuthBufferSize = 0;

    cui.cbSize = sizeof(CREDUI_INFO);
    cui.hwndParent = NULL;
    TCHAR messageText[] = _T("Please enter your credentials.");
    TCHAR captionText[] = _T("Authentication Required");
    cui.pszMessageText = messageText;
    cui.pszCaptionText = captionText;
    cui.hbmBanner = NULL;

    dwErr = CredUIPromptForWindowsCredentials(
        &cui,                              // CREDUI_INFO structure
        0,                                 // Error code to display
        &authPackage,                      // Authentication package
        NULL,                              // Inauth buffer
        0,                                 // Size of inauth buffer
        &outAuthBuffer,                    // Output buffer for auth data
        &outAuthBufferSize,                // Size of out auth buffer
        &fSave,                            // State of save check box
        CREDUIWIN_GENERIC                  // Flags
    );

    if (dwErr == NO_ERROR) {
        // Authentication successful
        SecureZeroMemory(outAuthBuffer, outAuthBufferSize);
        CoTaskMemFree(outAuthBuffer);
        return TRUE;
    }
    else {
        // Authentication failed
        printf("Authentication failed. Error: %d\n", dwErr);
        SecureZeroMemory(outAuthBuffer, outAuthBufferSize);
        CoTaskMemFree(outAuthBuffer);
        return FALSE;
    }
}

// Detects when a new window is created
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    TCHAR className[256];
    GetClassName(hwnd, className, sizeof(className) / sizeof(TCHAR));

    // Check if the window belongs to an application
    if (_tcscmp(className, _T("ApplicationFrameWindow")) == 0 || _tcscmp(className, _T("Windows.UI.Core.CoreWindow")) == 0) {
        // Prompt for authentication
        if (!AuthenticateUser()) {
            // Authentication failed, close the window
            SendMessage(hwnd, WM_CLOSE, 0, 0);
        }
    }

    return TRUE;
}

// Window procedure for the hidden window
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_USER + 1) {
        // New window creation detected
        EnumWindows(EnumWindowsProc, 0);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Monitors for new applications and checks for kernel signals
void MonitorApplications(HANDLE hPipe) {
    while (1) {
        if (CheckForKernelSignal(hPipe)) {
            EnumWindows(EnumWindowsProc, 0);
        }
        Sleep(500);  // Check every half second
    }
}

// Check for signals from the kernel driver
BOOL CheckForKernelSignal(HANDLE hPipe) {
    char buffer[128];
    DWORD bytesRead;
    BOOL result = ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, NULL);

    if (result && bytesRead > 0) {
        return TRUE;
    }
    return FALSE;
}

// Main function
int main() {
    // Register window class
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = _T("MyWindowClass");
    RegisterClass(&wc);

    // Create hidden window
    HWND hwnd = CreateWindow(wc.lpszClassName, _T(""), 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    if (!hwnd) {
        printf("Failed to create window. Error: %lu\n", GetLastError());
        return 1;
    }

    // Initialize the NOTIFYICONDATA structure
    NOTIFYICONDATA nid = { 0 };
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_GUID | NIF_MESSAGE;
    nid.uCallbackMessage = WM_USER + 1;
    Shell_NotifyIcon(NIM_ADD, &nid);

    // Create a named pipe for communication with the kernel driver
    HANDLE hPipe = CreateNamedPipe(
        _T("\\\\.\\pipe\\KernelModeAuthPipe"),
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        0,
        0,
        0,
        NULL
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        printf("Failed to create named pipe. Error: %lu\n", GetLastError());
        return 1;
    }

    printf("Waiting for kernel driver connection...\n");
    BOOL connected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
    if (!connected) {
        printf("Failed to connect to named pipe. Error: %lu\n", GetLastError());
        CloseHandle(hPipe);
        return 1;
    }

    printf("Kernel driver connected.\n");

    // Start monitoring applications
    MonitorApplications(hPipe);

    // Message loop to keep the application running
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Remove the icon from the system tray
    Shell_NotifyIcon(NIM_DELETE, &nid);

    CloseHandle(hPipe);

    return 0;
}
