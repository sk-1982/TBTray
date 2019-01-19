// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <stdio.h>
#include <vector>

char *GetErrorMessage()
{
	static char msgbuf[1024];
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msgbuf, sizeof(msgbuf), NULL);
	return msgbuf;
}

std::vector<HHOOK> gMouseHooks;

typedef BOOL (WINAPI *SetCursorPos_t)(_In_ int X, _In_ int Y);
SetCursorPos_t OrigSetCursorPos = (SetCursorPos_t)GetProcAddress(GetModuleHandle(L"user32"), "SetCursorPos");
BOOL WINAPI MySetCursorPos(_In_ int x, _In_ int y)
{
	printf("scp %i %i\n", x, y);
	return OrigSetCursorPos(x, y);
}

HHOOK hHook;
LRESULT CALLBACK MouseHook(int nCode, WPARAM wParam, LPARAM lParam)
{
	MOUSEHOOKSTRUCT &hs = *(MOUSEHOOKSTRUCT *)lParam;
	printf("maus %i %i\n", hs.pt.x, hs.pt.y);
	return CallNextHookEx(hHook, nCode, wParam, lParam);
}

void hookAllThreads(int idHook, HOOKPROC lpfn, HINSTANCE hmod)
{
	printf("Hook hook\n");
	const HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hSnap == INVALID_HANDLE_VALUE)
		printf("Failed to create snapshot! %i\n", GetLastError());

	const DWORD pid = GetCurrentProcessId();
	THREADENTRY32 te;
	te.dwSize = sizeof(te);
	BOOL more = Thread32First(hSnap, &te);
	while (more) {
		if (te.th32OwnerProcessID == pid) {
			HHOOK hHook = SetWindowsHookEx(idHook, lpfn, hmod, te.th32ThreadID);
			if (hHook) // We can't hook non-GUI threads, that's okay.
				gMouseHooks.push_back(hHook);
		}

		more = Thread32Next(hSnap, &te);
	}

	CloseHandle(hSnap);
}

HANDLE hHookThread;
DWORD idHookThread;
void HookThread(HMODULE hModule)
{
	hookAllThreads(WH_MOUSE, MouseHook, hModule);
	Mhook_SetHook((PVOID *)&OrigSetCursorPos, MySetCursorPos);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	Mhook_Unhook((PVOID *)&OrigSetCursorPos);
	for (HHOOK hook : gMouseHooks)
		if (!UnhookWindowsHookEx(hook))
			printf("Failed to unhook %p: %s\n", hook, GetErrorMessage());
}

void selfUnload(HMODULE hModule)
{
	printf("My handle: %p\n", hModule);
	if (IDYES == MessageBoxA(0, "Click Yes to unload DLL again", "Traktor Touch DLL", MB_ICONQUESTION | MB_YESNO)) {
		PostThreadMessage(idHookThread, WM_QUIT, 0, 0);
		WaitForSingleObject(hHookThread, INFINITE);
		FreeLibraryAndExitThread(hModule, 0);
	}
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
#ifdef _DEBUG
		RedirectIOToConsole();
#endif
		hHookThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)HookThread, hModule, 0, &idHookThread);
		if (!hHookThread)
			printf("Can't create hook thread: %s", GetErrorMessage());
#ifdef _DEBUG
		if (!CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)selfUnload, hModule, 0, NULL))
			printf("Can't create unload thread: %s", GetErrorMessage());
#endif
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
		break;
    case DLL_PROCESS_DETACH:
#ifdef _DEBUG
		CloseConsole();
#endif
        break;
    }
    return TRUE;
}

