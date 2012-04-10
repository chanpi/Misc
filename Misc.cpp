// Misc.cpp : DLL アプリケーション用にエクスポートされる関数を定義します。
//

#include "stdafx.h"
#include "Constants.h"
#include "Misc.h"
#include <time.h>
#include <Shlwapi.h>
#include <MMSystem.h>

#define BUFFER_SIZE		256

static PCTSTR g_szTitle		= _T("Misc");
static PCSTR g_szLogExt		= ".log";
static PCTSTR g_MutexNameA	= _T("Global\\LogFileOpenA");
static PCTSTR g_MutexNameW	= _T("Global\\LogFileOpenW");
static char g_szWDirectory[BUFFER_SIZE] = "";
static char g_szADirectory[BUFFER_SIZE] = "";
static char g_szWTitle[BUFFER_SIZE] = "";
static char g_szATitle[BUFFER_SIZE] = "";
static LOG_LEVEL g_logLevel	= Log_Error;

static HANDLE g_hMutex = NULL;
static HANDLE g_hFileHandleW = NULL;
static HANDLE g_hFileHandleA = NULL;
static HANDLE g_hFileLockW = NULL;
static HANDLE g_hFileLockA = NULL;

static LARGE_INTEGER g_liPrev = {0};
static LARGE_INTEGER g_liNow = {0};

static BOOL WINAPI LogDebugMessageW(LOG_LEVEL logLevel, PCWSTR szMessage);
static BOOL WINAPI LogDebugMessageA(LOG_LEVEL logLevel, PCSTR szMessage);
static BOOL WINAPI PrepareLogFile(PCSTR szDirectory, PWSTR szFileName, SIZE_T cchLength, PCSTR szExtension, PCSTR szTitle);
static BOOL WINAPI CompareDate(struct tm* lastDate, const struct tm* date);
static BOOL WINAPI GetCurrentTimeForLogW(LOG_LEVEL logLevel, PTSTR szTime, SIZE_T cchLength);
static BOOL WINAPI GetCurrentTimeForLogA(LOG_LEVEL logLevel, PSTR szTime, SIZE_T cchLength);
static void WINAPI CreateLogMessageW(LPWSTR destMessage, int length, LPCWSTR srcMessage, DWORD dwErrorCode, LPCWSTR szFILE, int line);
static void WINAPI CreateLogMessageA(LPSTR destMessage, int length, LPCSTR srcMessage, DWORD dwErrorCode, LPCSTR szFILE, int line);


#if defined UNICODE || _UNICODE
#define GetCurrentTimeForLog GetCurrentTimeForLogW
#define LogDebugMessage LogDebugMessageW
#define CreateLogMessage CreateLogMessageW
#else
#define GetCurrentTimeForLog GetCurrentTimeForLogA
#define LogDebugMessage LogDebugMessageA
#define CreateLogMessage CreateLogMessageA
#endif

BOOL WINAPI CreateFileMutex(HANDLE* phMutex, LPCTSTR szAppName)
{
	*phMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, szAppName);
	if (*phMutex) {
		return TRUE;
	}
	*phMutex = CreateMutex(NULL, FALSE, szAppName);
	if (*phMutex == NULL) {
		return FALSE;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		return FALSE;
	}
	return TRUE;
}

void WINAPI CleanupFileMutex(HANDLE hMutex)
{
	if (hMutex != NULL) {
		CloseHandle(hMutex);
		hMutex = NULL;
	}
}

BOOL WINAPI ExecuteOnce(LPCTSTR szAppName)
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

void WINAPI CleanupMutex()
{
	if (g_hMutex != NULL) {
		CloseHandle(g_hMutex);
		g_hMutex = NULL;
	}
}

BOOL WINAPI GetModuleFileWithExtension(PTSTR szFilePath, SIZE_T cchLength, PCTSTR szExtension) {
	TCHAR* ptr = NULL;
	if (!GetModuleFileName(NULL, szFilePath, cchLength)) {
		ReportError(_T(MISC_MESSAGE_ERROR_GETMODULEFILENAME));
	}
	ptr = _tcsrchr(szFilePath, _T('.'));
	if (ptr != NULL) {
		_tcscpy_s(ptr+1, cchLength-(ptr+1-szFilePath), szExtension);
		return TRUE;
	}
	return FALSE;
}

void WINAPI ReportError(PCTSTR szMessage) {
	MessageBox(NULL, szMessage, g_szTitle, MB_OK | MB_ICONERROR);
}

BOOL WINAPI PrepareLogFile(PCSTR szDirectory, PWSTR szFileName, SIZE_T cchLength, PCSTR szExtension, PCSTR szTitle)
{
	CHAR szLogDirectory[BUFFER_SIZE] = {0};
	sprintf_s(szLogDirectory, _countof(szLogDirectory), "%s\\logs", szDirectory);
	if (!PathIsDirectoryA(szLogDirectory)) {
		if (!CreateDirectoryA(szLogDirectory, NULL)) {
			return FALSE;
		}
	}

	time_t timer;
	struct tm date = {0};
	char buffer[BUFFER_SIZE];

	timer = time(NULL);
	localtime_s(&date, &timer);
	sprintf_s(buffer, _countof(buffer), "%s\\logs\\%s_%04d%02d%02d%s", szDirectory, szTitle, date.tm_year+1900, date.tm_mon+1, date.tm_mday, szExtension);

	MultiByteToWideChar(CP_ACP, 0, buffer, -1, szFileName, cchLength);
	return TRUE;
}

BOOL WINAPI LogFileOpenW(PCSTR szDirectory, PCSTR szTitle, LOG_LEVEL logLevel)
{
	TCHAR szLogFileName[BUFFER_SIZE];
	unsigned char szBOM[] = { 0xFF, 0xFE };
	DWORD dwNumberOfBytesWritten = 0;

	CreateFileMutex(&g_hFileLockW, g_MutexNameW);

	if (!PrepareLogFile(szDirectory, szLogFileName, _countof(szLogFileName), g_szLogExt, szTitle)) {
		ReportError(_T(MISC_MESSAGE_ERROR_CREATE_DIR));
		return FALSE;
	}

	g_hFileHandleW = CreateFile(szLogFileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (g_hFileHandleW == INVALID_HANDLE_VALUE) {
		WCHAR szError[BUFFER_SIZE];
		swprintf_s(szError, _countof(szError), _T(MISC_MESSAGE_ERROR_OPEN_FILE), szLogFileName, GetLastError());
		ReportError(szError);
		return FALSE;
	}

	// 共有ファイルへの書き込みのため、ファイルポインタの移動、書き込み、フラッシュは排他制御した上で行う。
	WaitForSingleObject(g_hFileLockW, INFINITE);
	SetFilePointer(g_hFileHandleW, 0, NULL, FILE_END);
	WriteFile(g_hFileHandleW, szBOM, 2, &dwNumberOfBytesWritten, NULL);
	FlushFileBuffers(g_hFileHandleW);
	ReleaseMutex(g_hFileLockW);

	g_logLevel = logLevel;
	strcpy_s(g_szWDirectory, _countof(g_szWDirectory), szDirectory);
	strcpy_s(g_szWTitle, _countof(g_szWTitle), szTitle);
	return TRUE;
}

BOOL WINAPI LogFileOpenA(PCSTR szDirectory, PCSTR szTitle, LOG_LEVEL logLevel)
{
	TCHAR szLogFileName[BUFFER_SIZE] = {0};

	CreateFileMutex(&g_hFileLockA, g_MutexNameA);

	if (!PrepareLogFile(szDirectory, szLogFileName, _countof(szLogFileName), g_szLogExt, szTitle)) {
		ReportError(_T(MISC_MESSAGE_ERROR_CREATE_DIR));
		return FALSE;
	}

	g_hFileHandleA = CreateFile(szLogFileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (g_hFileHandleA == INVALID_HANDLE_VALUE) {
		TCHAR szError[BUFFER_SIZE] = {0};
		_stprintf_s(szError, _countof(szError), _T(MISC_MESSAGE_ERROR_OPEN_FILE), szLogFileName, GetLastError());
		ReportError(szError);
		return FALSE;
	}

	// 共有ファイルへの書き込みのため、ファイルポインタの移動、書き込み、フラッシュは排他制御した上で行う。
	WaitForSingleObject(g_hFileLockA, INFINITE);
	SetFilePointer(g_hFileHandleA, 0, NULL, FILE_END);
	ReleaseMutex(g_hFileLockA);

	g_logLevel = logLevel;
	strcpy_s(g_szADirectory, _countof(g_szADirectory), szDirectory);
	strcpy_s(g_szATitle, _countof(g_szATitle), szTitle);
	return TRUE;
}

BOOL WINAPI LogFileCloseW()
{
	if (g_hFileHandleW != NULL) {
		if (!CloseHandle(g_hFileHandleW)) {
			TCHAR szError[BUFFER_SIZE] = {0};
			_stprintf_s(szError, _countof(szError), _T(MISC_MESSAGE_ERROR_CLOSE_FILE), GetLastError());
			ReportError(szError);
			return FALSE;
		}
		g_hFileHandleW = NULL;
	}
	CleanupFileMutex(g_hFileLockW);
	return TRUE;
}

BOOL WINAPI LogFileCloseA()
{
	if (g_hFileHandleA != NULL) {
		if (!CloseHandle(g_hFileHandleA)) {
			TCHAR szError[BUFFER_SIZE] = {0};
			_stprintf_s(szError, _countof(szError), _T(MISC_MESSAGE_ERROR_CLOSE_FILE), GetLastError());
			ReportError(szError);
			return FALSE;
		}
		g_hFileHandleA = NULL;
	}
	CleanupFileMutex(g_hFileLockA);
	return TRUE;
}

BOOL WINAPI LogDebugMessageW(LOG_LEVEL logLevel, PCWSTR szMessage)
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
			if (!LogFileOpenW(g_szWDirectory, g_szWTitle, g_logLevel)) {
				return FALSE;
			}
		}

		WCHAR szBuffer[BUFFER_SIZE*2] = {0};
		swprintf_s(szBuffer, _countof(szBuffer), L"%s%s\r\n", szTime, szMessage);
		
		// 共有ファイルへの書き込みのため、ファイルポインタの移動、書き込み、フラッシュは排他制御した上で行う。
		WaitForSingleObject(g_hFileLockW, INFINITE);
		SetFilePointer(g_hFileHandleW, 0, NULL, FILE_END);
		WriteFile(g_hFileHandleW, szBuffer, wcslen(szBuffer) * sizeof(WCHAR), &dwNumberOfBytesWritten, NULL);
		FlushFileBuffers(g_hFileHandleW);
		ReleaseMutex(g_hFileLockW);

		return TRUE;
	}
	return TRUE;
}

BOOL WINAPI LogDebugMessageA(LOG_LEVEL logLevel, PCSTR szMessage)
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
			if (!LogFileOpenA(g_szADirectory, g_szATitle, g_logLevel)) {
				return FALSE;
			}
		}

		char szBuffer[BUFFER_SIZE*2] = {0};
		sprintf_s(szBuffer, _countof(szBuffer), "%s%s\r\n", szTime, szMessage);

		// 共有ファイルへの書き込みのため、ファイルポインタの移動、書き込み、フラッシュは排他制御した上で行う。
		WaitForSingleObject(g_hFileLockA, INFINITE);
		SetFilePointer(g_hFileHandleA, 0, NULL, FILE_END);
		WriteFile(g_hFileHandleA, szBuffer, strlen(szBuffer), &dwNumberOfBytesWritten, NULL);
		FlushFileBuffers(g_hFileHandleA);
		ReleaseMutex(g_hFileLockA);

		return TRUE;
	}
	return TRUE;
}

void WINAPI RemoveWhiteSpaceW(PWSTR szBuffer)
{
	WCHAR* pStart = szBuffer;
	WCHAR* pEnd = szBuffer + wcslen(szBuffer) - 1;
	WORD wCharType = 0;

	while (pStart < pEnd && GetStringTypeEx(LOCALE_USER_DEFAULT, CT_CTYPE1, pStart, 1, &wCharType)) {
		if (wCharType & C1_SPACE || wCharType & C1_CNTRL) {
			pStart++;
		} else {
			break;
		}
	}

	while (pStart < pEnd && GetStringTypeEx(LOCALE_USER_DEFAULT, CT_CTYPE1, pEnd, 1, &wCharType)) {
		if (wCharType & C1_SPACE || wCharType & C1_CNTRL) {
			pEnd--;
		} else {
			break;
		}
	}

	MoveMemory(szBuffer, pStart, pEnd - pStart + 1);
	*(szBuffer + (pEnd - pStart + 1)) = L'\0';
}

void WINAPI RemoveWhiteSpaceA(PSTR szBuffer)
{
	char* pStart = szBuffer;
	char* pEnd = szBuffer + strlen(szBuffer) - 1;
	WORD wCharType = 0;

	while (pStart < pEnd && GetStringTypeExA(LOCALE_USER_DEFAULT, CT_CTYPE1, pStart, 1, &wCharType)) {
		if (wCharType & C1_SPACE || wCharType & C1_CNTRL) {
			pStart++;
		} else {
			break;
		}
	}

	while (pStart < pEnd && GetStringTypeExA(LOCALE_USER_DEFAULT, CT_CTYPE1, pEnd, 1, &wCharType)) {
		if (wCharType & C1_SPACE || wCharType & C1_CNTRL) {
			pEnd--;
		} else {
			break;
		}
	}

	MoveMemory(szBuffer, pStart, pEnd - pStart + 1);
	*(szBuffer + (pEnd - pStart + 1)) = '\0';
}

void WINAPI DebugProfileMonitor(PCSTR position) {
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

void WINAPI CreateLogMessageA(LPSTR destMessage, int length, LPCSTR srcMessage, DWORD dwErrorCode, LPCSTR szFILE, int line)
{
	CHAR szFileName[MAX_PATH] = {0};
	CHAR szExt[8] = {0};
	_splitpath_s(szFILE, NULL, 0, NULL, 0, szFileName, _countof(szFileName), szExt, _countof(szExt));
	sprintf_s(destMessage, length, "%s <%s%s:line(%d) error(%u)>", srcMessage, szFileName, szExt, line, dwErrorCode);
}

void WINAPI LoggingMessageA(LOG_LEVEL logLevel, LPCSTR srcMessage,  DWORD dwErrorCode, LPCSTR szFILE, int line)
{
	CHAR szError[BUFFER_SIZE] = {0};
	CreateLogMessageA(szError, _countof(szError), srcMessage, dwErrorCode, szFILE, line);
	LogDebugMessageA(logLevel, szError);
}

void WINAPI CreateLogMessageW(LPWSTR destMessage, int length, LPCWSTR srcMessage, DWORD dwErrorCode, LPCWSTR szFILE, int line)
{
	TCHAR szFileName[MAX_PATH] = {0};
	TCHAR szExt[8] = {0};
	_wsplitpath_s(szFILE, NULL, 0, NULL, 0, szFileName, _countof(szFileName), szExt, _countof(szExt));
	swprintf_s(destMessage, length, L"%s <%s%s:line(%d) error(%u)>", srcMessage, szFileName, szExt, line, dwErrorCode);
}

void WINAPI LoggingMessageW(LOG_LEVEL logLevel, LPCWSTR srcMessage,  DWORD dwErrorCode, LPCWSTR szFILE, int line)
{
	WCHAR szError[BUFFER_SIZE] = {0};
	CreateLogMessageW(szError, _countof(szError), srcMessage, dwErrorCode, szFILE, line);
	LogDebugMessageW(logLevel, szError);
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

static BOOL WINAPI CompareDate(struct tm* lastDate, const struct tm* date)
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
BOOL WINAPI GetCurrentTimeForLogW(LOG_LEVEL logLevel, PWSTR szTime, SIZE_T cchLength)
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
BOOL WINAPI GetCurrentTimeForLogA(LOG_LEVEL logLevel, PSTR szTime, SIZE_T cchLength)
{
	time_t timer = 0;
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
