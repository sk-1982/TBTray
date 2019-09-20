/*
 * Traktouch loader
 *
 * Finds the active Traktor window, or starts Traktor if there is no window yet,
 * then loads the companion DLL into Traktor via a Windows hook.
 *
 * Copyright (c) 2019 by Joachim Fenkes <github@dojoe.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tchar.h>
#include <Shlwapi.h>


TCHAR *GetErrorMessage()
{
	static TCHAR msgbuf[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msgbuf, _countof(msgbuf), nullptr);
	return msgbuf;
}


static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	if(hwnd)
	{
		TCHAR name[256] = {};
		if(::GetClassName(hwnd, name, 256) > 0)
		{
			if(!_tcscmp(name, _T("MozillaWindowClass")))
			{
				GetWindowText(hwnd, name, 256);
				if(_tcsstr(name, _T("- Mozilla Thunderbird")))
				{
					*reinterpret_cast<HWND *>(lParam) = hwnd;
					return FALSE; // done
				}
			}
		}
	}
	return TRUE; // continue
}


static void CALLBACK TimerProc(HWND, UINT, UINT_PTR idTimer, DWORD)
{
	static HWND hwnd = nullptr;
	if(IsWindow(hwnd))
		return; // We already hooked it

	hwnd = nullptr;
	::EnumWindows(&EnumWindowsProc, reinterpret_cast<LPARAM>(&hwnd));
	if(hwnd == nullptr)
		return; // Thunderbird is not running

	// Construct the DLL filename from our own filename, then attempt to load the DLL and find the entry hook
	TCHAR dllName[1024];
	int dllNameLen = GetModuleFileName(nullptr, dllName, sizeof(dllName));
	strcpy(dllName + dllNameLen - 3, "dll");
	HMODULE dll = LoadLibrary(dllName);
	HOOKPROC hookProc = (HOOKPROC)GetProcAddress(dll, "_EntryHook@12");
	if(!dll || !hookProc)
	{
		MessageBox(nullptr, _T("Could not find the companion DLL. Make sure it's in the same directory as the EXE and has the same name."), _T("TBTray"), MB_ICONEXCLAMATION);
		hwnd = nullptr;
		return;
	}

	DWORD threadID = GetWindowThreadProcessId(hwnd, nullptr);
	HHOOK hook = SetWindowsHookEx(WH_CALLWNDPROC, hookProc, dll, threadID);
	if(!hook)
	{
		MessageBox(nullptr, GetErrorMessage(), _T("Failed to hook Thunderbird UI thread"), MB_ICONEXCLAMATION);
		hwnd = nullptr;
		return;
	}
	FreeLibrary(dll);

	// Send a message to the hooked WindowProc to make sure the DLL install code ran before we exit
	SendMessage(hwnd, WM_NULL, 0, 0);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
	auto timerID = SetTimer(nullptr, 1, 1000, TimerProc);

	MSG msg;
	while(::GetMessage(&msg, nullptr, 0, 0))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

#ifdef _DEBUG
	MessageBox(nullptr, _T("DLL injected - hit OK to unload"), _T("TBTray"), MB_ICONINFORMATION);

	/* Tell the DLL to unhook and unload itself */
	/*if (0xBABE != SendMessage(hTraktorWindow, WM_APP + 9999, 0xCAFE, 0xDEADBEEF))
		MessageBox(0, "Failed to unhook", "Yikes", MB_ICONEXCLAMATION);
	UnhookWindowsHookEx(hook);*/
#endif

	return 0;
}
