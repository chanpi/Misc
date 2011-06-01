// Miscellaneous.cpp : DLL アプリケーション用にエクスポートされる関数を定義します。
//

#include "stdafx.h"
#include "Miscellaneous.h"
//#include "I4C3DModulesDefs.h"
//#include "I4C3DAnalyzeXML.h"
#include <time.h>
#include <Shlwapi.h>
#include <MMSystem.h>

static TCHAR szTitle[] = _T("Miscellaneous");
#define BUFFER_SIZE		256

static LOG_LEVEL g_logLevel	= Log_Error;
static HANDLE g_hMutex = NULL;

static LARGE_INTEGER g_liPrev = {0};
static LARGE_INTEGER g_liNow = {0};

static BOOL PrepareLogFile(PWSTR szFileName, SIZE_T cchLength, PCSTR szExtension);
static void GetCurrentTimeForLogW(LOG_LEVEL logLevel, PTSTR szTime, SIZE_T cchLength);
static void GetCurrentTimeForLogA(LOG_LEVEL logLevel, PSTR szTime, SIZE_T cchLength);

#if defined UNICODE || _UNICODE
#define GetCurrentTimeForLog GetCurrentTimeForLogW
#else
#define GetCurrentTimeForLog GetCurrentTimeForLogA
#endif


BOOL ExecuteOnce(LPCTSTR szAppName)
{
	g_hMutex = CreateMutex(NULL, TRUE, szAppName);
	if (g_hMutex == NULL) {
		return FALSE;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		return FALSE;
	}
	return TRUE;
}

void CleanupMutex()
{
	if (g_hMutex != NULL) {
		CloseHandle(g_hMutex);
		g_hMutex = NULL;
	}
}


void LogInitialize(LOG_LEVEL logLevel)
{
	g_logLevel = logLevel;
}

BOOL GetModuleFileWithExtension(PTSTR szFilePath, SIZE_T cchLength, PCTSTR szExtension) {
	TCHAR* ptr = NULL;
	if (!GetModuleFileName(NULL, szFilePath, cchLength)) {
		ReportError(_T("[ERROR] 実行ファイル名の取得に失敗しました。"));
	}
	ptr = _tcsrchr(szFilePath, _T('.'));
	if (ptr != NULL) {
		_tcscpy_s(ptr+1, cchLength-(ptr+1-szFilePath), szExtension);
		return TRUE;
	}
	return FALSE;
}

void ReportError(PCTSTR szMessage) {
	MessageBox(NULL, szMessage, szTitle, MB_OK | MB_ICONERROR);
}

BOOL PrepareLogFile(PWSTR szFileName, SIZE_T cchLength, PCSTR szExtension)
{
	if (!PathIsDirectory(_T("logs"))) {
		if (!CreateDirectory(_T("logs"), NULL)) {
			return FALSE;
		}
	}

	time_t timer;
	struct tm date = {0};
	char buffer[BUFFER_SIZE];

	timer = time(NULL);
	localtime_s(&date, &timer);
	sprintf_s(buffer, _countof(buffer), "logs\\%04d%02d%02d%s", date.tm_year+1900, date.tm_mon+1, date.tm_mday, szExtension);

	MultiByteToWideChar(CP_ACP, 0, buffer, -1, szFileName, cchLength);
	return TRUE;
}

void LogDebugMessageW(LOG_LEVEL logLevel, PCWSTR szMessage)
{
	TCHAR szLogFileName[BUFFER_SIZE];

	if (g_logLevel <= logLevel) {
		HANDLE hLogFile;
		unsigned char szBOM[] = { 0xFF, 0xFE };
		DWORD dwNumberOfBytesWritten = 0;

		if (!PrepareLogFile(szLogFileName, _countof(szLogFileName), ".log")) {
			//ReportError(_T("log専用ディレクトリを作成できません。ログは出力されません。"));
			return;
		}

		hLogFile = CreateFile(szLogFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hLogFile == INVALID_HANDLE_VALUE) {
			// 書き込みが混雑してファイルが開けない場合がある（排他オープンのため）
			//WCHAR szError[I4C3D_BUFFER_SIZE];
			//swprintf_s(szError, _countof(szError), L"[ERROR] ログファイルのオープンに失敗しています。: %d", GetLastError());
			//ReportError(szError);
			//ReportError(szMessage);
			return;
		}

		WriteFile(hLogFile, szBOM, 2, &dwNumberOfBytesWritten, NULL);
		SetFilePointer(hLogFile, 0, NULL, FILE_END);

		WCHAR szTime[MAX_PATH] = {0};
		GetCurrentTimeForLog(logLevel, szTime, _countof(szTime));
		WriteFile(hLogFile, szTime, wcslen(szTime) * sizeof(WCHAR), &dwNumberOfBytesWritten, NULL);
		WriteFile(hLogFile, szMessage, wcslen(szMessage) * sizeof(WCHAR), &dwNumberOfBytesWritten, NULL);
		WriteFile(hLogFile, L"\r\n", 2 * sizeof(WCHAR), &dwNumberOfBytesWritten, NULL);
		CloseHandle(hLogFile);
	}
}

void LogDebugMessageA(LOG_LEVEL logLevel, PCSTR szMessage)
{
	TCHAR szLogFileNameA[MAX_PATH];

	if (g_logLevel <= logLevel) {
		HANDLE hLogFile;
		DWORD dwNumberOfBytesWritten = 0;

		if (!PrepareLogFile(szLogFileNameA, _countof(szLogFileNameA), "_info.log")) {
			//ReportError(_T("log専用ディレクトリを作成できません。ログは出力されません。"));
			return;
		}

		hLogFile = CreateFile(szLogFileNameA, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hLogFile == INVALID_HANDLE_VALUE) {
			// 書き込みが混雑してファイルが開けない場合がある（排他オープンのため）
			//TCHAR szError[I4C3D_BUFFER_SIZE];
			//_stprintf_s(szError, _countof(szError), _T("[ERROR] ログファイルのオープンに失敗しています。: %d"), GetLastError());
			//ReportError(szError);
			return;
		}

		SetFilePointer(hLogFile, 0, NULL, FILE_END);

		char szTime[BUFFER_SIZE] = {0};
		GetCurrentTimeForLogA(logLevel, szTime, _countof(szTime));
		WriteFile(hLogFile, szTime, strlen(szTime), &dwNumberOfBytesWritten, NULL);
		WriteFile(hLogFile, szMessage, strlen(szMessage), &dwNumberOfBytesWritten, NULL);
		WriteFile(hLogFile, "\r\n", 2, &dwNumberOfBytesWritten, NULL);
		CloseHandle(hLogFile);
	}
}

void RemoveWhiteSpaceW(PWSTR szBuffer)
{
	WCHAR* pStart = szBuffer;
	WCHAR* pEnd = szBuffer + wcslen(szBuffer) - 1;
	WORD wCharType = 0;

	while (pStart < pEnd && GetStringTypeEx(LOCALE_USER_DEFAULT, CT_CTYPE1, pStart, 1, &wCharType)) {
		if (wCharType & C1_SPACE) {
			pStart++;
		} else {
			break;
		}
	}

	while (pStart < pEnd && GetStringTypeEx(LOCALE_USER_DEFAULT, CT_CTYPE1, pEnd, 1, &wCharType)) {
		if (wCharType & C1_SPACE) {
			pEnd--;
		} else {
			break;
		}
	}

	MoveMemory(szBuffer, pStart, pEnd - pStart + 1);
	*(szBuffer + (pEnd - pStart + 1)) = L'\0';
}

void RemoveWhiteSpaceA(PSTR szBuffer)
{
	char* pStart = szBuffer;
	char* pEnd = szBuffer + strlen(szBuffer) - 1;
	WORD wCharType = 0;

	while (pStart < pEnd && GetStringTypeExA(LOCALE_USER_DEFAULT, CT_CTYPE1, pStart, 1, &wCharType)) {
		if (wCharType & C1_SPACE) {
			pStart++;
		} else {
			break;
		}
	}

	while (pStart < pEnd && GetStringTypeExA(LOCALE_USER_DEFAULT, CT_CTYPE1, pEnd, 1, &wCharType)) {
		if (wCharType & C1_SPACE) {
			pEnd--;
		} else {
			break;
		}
	}

	MoveMemory(szBuffer, pStart, pEnd - pStart + 1);
	*(szBuffer + (pEnd - pStart + 1)) = '\0';
}

void DebugProfileMonitor(PCSTR position) {
	if (g_logLevel < Log_Info) {
		return;
	}
	DOUBLE dTime = 0;
	LARGE_INTEGER liFrequency;
	QueryPerformanceFrequency(&liFrequency);
	QueryPerformanceCounter(&g_liNow);
	dTime = (DOUBLE)((g_liNow.QuadPart - g_liPrev.QuadPart) * 1000 / liFrequency.QuadPart);

	if (dTime > 1.) {
		char buffer[256];
		sprintf_s(buffer, _countof(buffer), "%s %9.4lf[ms]", position, dTime);
		LogDebugMessageA(Log_Info, buffer);
	}
	g_liPrev = g_liNow;
}

////////////////////////////////////////////////////////

inline PCSTR GetLogLevelString(LOG_LEVEL logLevel) {
	PCSTR szLogLevel = "?????";
	switch (logLevel) {
	case Log_Debug:
		szLogLevel = "DEBUG";
		break;

	case Log_Info:
		szLogLevel = " INFO";
		break;

	case Log_Error:
		szLogLevel = "ERROR";
		break;
	}
	return szLogLevel;
}

void GetCurrentTimeForLogW(LOG_LEVEL logLevel, PWSTR szTime, SIZE_T cchLength)
{
	time_t timer;
	struct tm date = {0};
	char buffer[BUFFER_SIZE];
	PCSTR szLogLevel = GetLogLevelString(logLevel);

	timer = time(NULL);
	localtime_s(&date, &timer);
	sprintf_s(buffer, _countof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d %s] ",
		date.tm_year+1900, date.tm_mon+1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec, szLogLevel);

	MultiByteToWideChar(CP_ACP, 0, buffer, -1, szTime, cchLength);
}

void GetCurrentTimeForLogA(LOG_LEVEL logLevel, PSTR szTime, SIZE_T cchLength)
{
	time_t timer;
	struct tm date = {0};
	char buffer[BUFFER_SIZE];
	PCSTR szLogLevel = GetLogLevelString(logLevel);

	timer = time(NULL);
	localtime_s(&date, &timer);
	sprintf_s(buffer, _countof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d %s] ",
		date.tm_year+1900, date.tm_mon+1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec, szLogLevel);

	strcpy_s(szTime, cchLength, buffer);
}
