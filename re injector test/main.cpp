#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <tlhelp32.h>
#include <psapi.h>

HWND hwndTextBox, hwndButton, hwndComboBox, hwndInjectButton, hwndDiscordButton, hwndRefreshButton;
HWND hwndComboBoxList; 

std::vector<DWORD> processIds; 
std::vector<std::wstring> processNames; 

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void refreshProcessList();
void injectDLL(const std::wstring& processName, const std::wstring& dllPath);
std::wstring GetDlgItemTextW(HWND hwnd, int id);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"WinFormsApp", NULL };
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindow(L"WinFormsApp", L"Simple Injector", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 400, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!hwnd)
        return -1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

void refreshProcessList()
{
    processIds.clear();
    processNames.clear();
    SendMessage(hwndComboBox, CB_RESETCONTENT, 0, 0); 

    DWORD processes[1024];
    DWORD needed;
    if (EnumProcesses(processes, sizeof(processes), &needed))
    {
        DWORD numProcesses = needed / sizeof(DWORD);

        for (DWORD i = 0; i < numProcesses; ++i)
        {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processes[i]);
            if (hProcess)
            {
                WCHAR szProcessName[MAX_PATH];
                if (GetProcessImageFileName(hProcess, szProcessName, sizeof(szProcessName) / sizeof(WCHAR)))
                {
                    std::wstring processName = szProcessName;
                    size_t lastBackslash = processName.rfind(L'\\');
                    if (lastBackslash != std::wstring::npos && lastBackslash < processName.length() - 1)
                    {
                        processName = processName.substr(lastBackslash + 1);
                    }
                    processIds.push_back(processes[i]);
                    processNames.push_back(processName);
                    SendMessage(hwndComboBox, CB_ADDSTRING, 0, (LPARAM)processName.c_str());
                }
                CloseHandle(hProcess);
            }
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        hwndTextBox = CreateWindow(
            L"EDIT", L"Path to DLL", WS_VISIBLE | WS_CHILD | WS_BORDER,
            12, 12, 385, 20, hwnd, NULL, NULL, NULL);

        hwndButton = CreateWindow(
            L"BUTTON", L"Choose DLL", WS_VISIBLE | WS_CHILD,
            403, 12, 75, 47, hwnd, NULL, NULL, NULL);

        hwndInjectButton = CreateWindow(
            L"BUTTON", L"Inject DLL", WS_VISIBLE | WS_CHILD,
            12, 65, 75, 23, hwnd, NULL, NULL, NULL);

        hwndDiscordButton = CreateWindow(
            L"BUTTON", L"Discord", WS_VISIBLE | WS_CHILD,
            12, 94, 75, 23, hwnd, NULL, NULL, NULL);

        hwndComboBox = CreateWindow(
            L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
            12, 38, 385, 200, hwnd, NULL, NULL, NULL);

        hwndRefreshButton = CreateWindow(
            L"BUTTON", L"Refresh", WS_VISIBLE | WS_CHILD,
            95, 94, 75, 23, hwnd, NULL, NULL, NULL);

        refreshProcessList();

        break;

    case WM_COMMAND:
        if ((HWND)lParam == hwndComboBox && HIWORD(wParam) == CBN_SELCHANGE)
        {
            int selectedIndex = SendMessage(hwndComboBox, CB_GETCURSEL, 0, 0);
            if (selectedIndex != CB_ERR)
            {
                SetWindowText(hwndTextBox, processNames[selectedIndex].c_str());
            }
        }
        else if ((HWND)lParam == hwndInjectButton)
        {
            int selectedIndex = SendMessage(hwndComboBoxList, CB_GETCURSEL, 0, 0);
            if (selectedIndex != CB_ERR)
            {
                std::wstring selectedProcessName = processNames[selectedIndex];
                std::wstring dllPath = GetDlgItemTextW(hwnd, GetDlgCtrlID(hwndTextBox));

                if (!dllPath.empty())
                {
                    injectDLL(selectedProcessName, dllPath);
                }
                else
                {
                    MessageBox(NULL, L"Please Pick a DLL file!", L"Error", MB_ICONERROR);
                }
            }
            else
            {
                MessageBox(NULL, L"No process selected", L"Error", MB_ICONERROR);
            }
        }
        else if ((HWND)lParam == hwndDiscordButton)
        {
            ShellExecute(NULL, L"open", L"https://discord.gg/e7hcEqdG8a", NULL, NULL, SW_SHOWNORMAL);
        }
        else if ((HWND)lParam == hwndButton) 
        {
            OPENFILENAME ofn;
            WCHAR szFileName[MAX_PATH] = L"";

            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"DLL Files (*.dll)\0*.dll\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = szFileName;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

            if (GetOpenFileName(&ofn))
            {
                SetWindowText(hwndTextBox, szFileName);
            }
        }
        else if ((HWND)lParam == hwndRefreshButton) 
        {
            refreshProcessList();
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

void injectDLL(const std::wstring& processName, const std::wstring& dllPath)
{
    DWORD processId = 0;
    for (size_t i = 0; i < processNames.size(); ++i)
    {
        if (processNames[i] == processName)
        {
            processId = processIds[i];
            break;
        }
    }

    if (processId == 0)
    {
        MessageBox(NULL, L"Process Does Not Exist", L"Error", MB_ICONERROR);
        return;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == NULL)
    {
        MessageBox(NULL, L"Failed to open process", L"Error", MB_ICONERROR);
        return;
    }

    void* remoteMemory = VirtualAllocEx(hProcess, NULL, dllPath.size() * sizeof(wchar_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remoteMemory == NULL)
    {
        CloseHandle(hProcess);
        MessageBox(NULL, L"Failed to allocate memory in the remote process", L"Error", MB_ICONERROR);
        return;
    }

    if (!WriteProcessMemory(hProcess, remoteMemory, dllPath.c_str(), dllPath.size() * sizeof(wchar_t), NULL))
    {
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        MessageBox(NULL, L"Failed to write to remote process memory", L"Error", MB_ICONERROR);
        return;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryW, remoteMemory, 0, NULL);
    if (hThread == NULL)
    {
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        MessageBox(NULL, L"Failed to create a remote thread", L"Error", MB_ICONERROR);
        return;
    }

    WaitForSingleObject(hThread, INFINITE); 
    CloseHandle(hThread);

    VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    MessageBox(NULL, L"Injection Succeeded", L"Success", MB_ICONINFORMATION);
}


std::wstring GetDlgItemTextW(HWND hwnd, int id)
{
    HWND hwndCtrl = GetDlgItem(hwnd, id);
    if (hwndCtrl != NULL)
    {
        int len = GetWindowTextLengthW(hwndCtrl);
        if (len > 0)
        {
            std::vector<wchar_t> buffer(len + 1);
            GetWindowTextW(hwndCtrl, buffer.data(), len + 1);
            return std::wstring(buffer.data());
        }
    }
    return L"";
}

