// Miscellaneous.cpp : DLL アプリケーション用にエクスポートされる関数を定義します。
//

#include "stdafx.h"
#include "Miscellaneous.h"
#include <time.h>
#include <Shlwapi.h>
#include <MMSystem.h>

static TCHAR szTitle[] = _T("Miscellaneous");
#define BUFFER_SIZE		256

static char g_szWTitle[BUFFER_SIZE] = "";
static char g_szATitle[BUFFER_SIZE] = "";
static LOG_LEVEL g_logLevel	= Log_Error;

static HANDLE g_hMutex = NULL;
static HANDLE g_hFileHandleW = NULL;
static HANDLE g_hFileHandleA = NULL;

static LARGE_INTEGER g_liPrev = {0};
static LARGE_INTEGER g_liNow = {0};

static BOOL PrepareLogFile(PWSTR szFileName, SIZE_T cchLength, PCSTR szExtension, PCSTR szTitle);
static BOOL CompareDate(struct tm* lastDate, const struct tm* date);
static BOOL GetCurrentTimeForLogW(LOG_LEVEL logLevel, PTSTR szTime, SIZE_T cchLength);
static BOOL GetCurrentTimeForLogA(LOG_LEVEL logLevel, PSTR szTime, SIZE_T cchLength);

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

BOOL PrepareLogFile(PWSTR szFileName, SIZE_T cchLength, PCSTR szExtension, PCSTR szTitle)
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
	sprintf_s(buffer, _countof(buffer), "logs\\%s_%04d%02d%02d%s", szTitle, date.tm_year+1900, date.tm_mon+1, date.tm_mday, szExtension);

	MultiByteToWideChar(CP_ACP, 0, buffer, -1, szFileName, cchLength);
	return TRUE;
}

BOOL LogFileOpenW(PCSTR szTitle, LOG_LEVEL logLevel)
{
	TCHAR szLogFileName[BUFFER_SIZE];
	unsigned char szBOM[] = { 0xFF, 0xFE };
	DWORD dwNumberOfBytesWritten = 0;

	if (!PrepareLogFile(szLogFileName, _countof(szLogFileName), ".log", szTitle)) {
		ReportError(_T("log専用ディレクトリを作成できません。ログは出力されません。"));
		return FALSE;
	}

	g_hFileHandleW = CreateFile(szLogFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (g_hFileHandleW == INVALID_HANDLE_VALUE) {
		// 書き込みが混雑してファイルが開けない場合がある（排他オープンのため）
		WCHAR szError[BUFFER_SIZE];
		swprintf_s(szError, _countof(szError), L"[ERROR] %sファイルのオープンに失敗しています。: %d", szLogFileName, GetLastError());
		ReportError(szError);
		return FALSE;
	}
	SetFilePointer(g_hFileHandleW, 0, NULL, FILE_END);

	WriteFile(g_hFileHandleW, szBOM, 2, &dwNumberOfBytesWritten, NULL);
	FlushFileBuffers(g_hFileHandleW);
	g_logLevel = logLevel;
	strcpy_s(g_szWTitle, _countof(g_szWTitle), szTitle);
	return TRUE;
}

BOOL LogFileOpenA(PCSTR szTitle, LOG_LEVEL logLevel)
{
	TCHAR szLogFileName[BUFFER_SIZE] = {0};

	if (!PrepareLogFile(szLogFileName, _countof(szLogFileName), ".log", szTitle)) {
		ReportError(_T("log専用ディレクトリを作成できません。ログは出力されません。"));
		return FALSE;
	}

	g_hFileHandleA = CreateFile(szLogFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (g_hFileHandleA == INVALID_HANDLE_VALUE) {
		// 書き込みが混雑してファイルが開けない場合がある（排他オープンのため）
		WCHAR szError[BUFFER_SIZE] = {0};
		swprintf_s(szError, _countof(szError), L"[ERROR] %sファイルのオープンに失敗しています。: %d", szLogFileName, GetLastError());
		ReportError(szError);
		return FALSE;
	}
	SetFilePointer(g_hFileHandleA, 0, NULL, FILE_END);

	g_logLevel = logLevel;
	strcpy_s(g_szATitle, _countof(g_szATitle), szTitle);
	return TRUE;
}

BOOL LogFileCloseW()
{
	if (g_hFileHandleW != NULL) {
		if (!CloseHandle(g_hFileHandleW)) {
			TCHAR szError[BUFFER_SIZE] = {0};
			_stprintf_s(szError, _countof(szError), _T("ログファイルをクローズできません。 ErrorNo.%d"), GetLastError());
			ReportError(szError);
			return FALSE;
		}
		g_hFileHandleW = NULL;
	}
	return TRUE;
}

BOOL LogFileCloseA()
{
	if (g_hFileHandleA != NULL) {
		if (!CloseHandle(g_hFileHandleA)) {
			TCHAR szError[BUFFER_SIZE] = {0};
			_stprintf_s(szError, _countof(szError), _T("ログファイルをクローズできません。 ErrorNo.%d"), GetLastError());
			ReportError(szError);
			return FALSE;
		}
		g_hFileHandleA = NULL;
	}
	return TRUE;
}

BOOL LogDebugMessageW(LOG_LEVEL logLevel, PCWSTR szMessage)
{
	if (g_logLevel <= logLevel) {
		if (g_hFileHandleW == NULL) {
			return FALSE;
		}

		DWORD dwNumberOfBytesWritten = 0;
		WCHAR szTime[MAX_PATH] = {0};
		if (!GetCurrentTimeForLogW(logLevel, szTime, _countof(szTime))) {
			if (!LogFileCloseW()) {
				return FALSE;
			}
			if (!LogFileOpenW(g_szWTitle, g_logLevel)) {
				return FALSE;
			}
		}
		WCHAR szBuffer[BUFFER_SIZE*2] = {0};
		swprintf_s(szBuffer, _countof(szBuffer), _T("%s%s\r\n"), szTime, szMessage);
		WriteFile(g_hFileHandleW, szBuffer, wcslen(szBuffer) * sizeof(WCHAR), &dwNumberOfBytesWritten, NULL);
		return FlushFileBuffers(g_hFileHandleW);
	}
	return TRUE;
}

BOOL LogDebugMessageA(LOG_LEVEL logLevel, PCSTR szMessage)
{
	if (g_logLevel <= logLevel) {
		if (g_hFileHandleA == NULL) {
			return FALSE;
		}

		DWORD dwNumberOfBytesWritten = 0;
		char szTime[BUFFER_SIZE] = {0};
		if (!GetCurrentTimeForLogA(logLevel, szTime, _countof(szTime))) {
			if (!LogFileCloseA()) {
				return FALSE;
			}
			if (!LogFileOpenA(g_szATitle, g_logLevel)) {
				return FALSE;
			}
		}

		char szBuffer[BUFFER_SIZE*2] = {0};
		sprintf_s(szBuffer, _countof(szBuffer), "%s%s\r\n", szTime, szMessage);
		WriteFile(g_hFileHandleA, szBuffer, strlen(szBuffer), &dwNumberOfBytesWritten, NULL);
		return FlushFileBuffers(g_hFileHandleA);
	}
	return TRUE;
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

static BOOL CompareDate(struct tm* lastDate, const struct tm* date)
{
	BOOL ret = TRUE;
	if (lastDate->tm_year != 0 &&
		(lastDate->tm_year != date->tm_year || lastDate->tm_mon != date->tm_mon || lastDate->tm_mday != date->tm_mday)) {
			// 前回と日付が異なる
			ret = FALSE;
	}
	lastDate->tm_year = date->tm_year;
	lastDate->tm_mon = date->tm_mon;
	lastDate->tm_mday = date->tm_mday;
	return ret;
}

// 前回と日付が異なっている場合はFALSEを返します。
BOOL GetCurrentTimeForLogW(LOG_LEVEL logLevel, PWSTR szTime, SIZE_T cchLength)
{
	time_t timer;
	struct tm date = {0};
	static struct tm lastDate = {0};

	char buffer[BUFFER_SIZE] = {0};
	PCSTR szLogLevel = GetLogLevelString(logLevel);

	timer = time(NULL);
	localtime_s(&date, &timer);
	sprintf_s(buffer, _countof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d %s] ",
		date.tm_year+1900, date.tm_mon+1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec, szLogLevel);

	MultiByteToWideChar(CP_ACP, 0, buffer, -1, szTime, cchLength);

	return CompareDate(&lastDate, &date);
}

// 前回と日付が異なっている場合はFALSEを返します。
BOOL GetCurrentTimeForLogA(LOG_LEVEL logLevel, PSTR szTime, SIZE_T cchLength)
{
	time_t timer;
	struct tm date = {0};
	static struct tm lastDate = {0};

	char buffer[BUFFER_SIZE] = {0};
	PCSTR szLogLevel = GetLogLevelString(logLevel);

	timer = time(NULL);
	localtime_s(&date, &timer);
	sprintf_s(buffer, _countof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d %s] ",
		date.tm_year+1900, date.tm_mon+1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec, szLogLevel);

	strcpy_s(szTime, cchLength, buffer);

	return CompareDate(&lastDate, &date);
}
