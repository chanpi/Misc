#ifndef __MISC_H__
#define __MISC_H__

#include <Windows.h>

#undef DLL_EXPORT

#ifdef MISC_EXPORTS
#define DLL_EXPORT		__declspec(dllexport)
#else
#define DLL_EXPORT		__declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum { Log_Debug, Log_Info, Log_Error, Log_Off } LOG_LEVEL;

	DLL_EXPORT BOOL GetModuleFileWithExtension(PTSTR szFilePath, SIZE_T cchLength, PCTSTR szExtension);
	DLL_EXPORT void ReportError(PCTSTR szMessage);

	DLL_EXPORT BOOL ExecuteOnce(LPCTSTR szAppName);
	DLL_EXPORT void CleanupMutex();

	// �ݒ�t�@�C����ǂݍ��݁A���O���x����ݒ肷��B
	// �ǂݍ��݂Ɏ��s�����ꍇ��A�{�֐����Ăяo���Ȃ������ꍇ�́ALog_Error���x���ɐݒ肷��B
	// main�Ȃǂōŏ��Ɉ�x�����Ăяo���̂��悢�B
	DLL_EXPORT BOOL LogFileOpenW(PCSTR szTitle, LOG_LEVEL logLevel);
	DLL_EXPORT BOOL LogFileCloseW();
	DLL_EXPORT BOOL LogFileOpenA(PCSTR szTitle, LOG_LEVEL logLevel);
	DLL_EXPORT BOOL LogFileCloseA();

	// �擪�����̃X�y�[�X�������B
	DLL_EXPORT void RemoveWhiteSpaceW(PWSTR szBuffer);
	DLL_EXPORT void RemoveWhiteSpaceA(PSTR szBuffer);

	DLL_EXPORT void DebugProfileMonitor(PCSTR position);

	DLL_EXPORT void LoggingMessageW(LOG_LEVEL logLevel, LPCWSTR srcMessage, DWORD dwErrorCode, LPCWSTR szFILE, int line);
	DLL_EXPORT void LoggingMessageA(LOG_LEVEL logLevel, LPCSTR srcMessage, DWORD dwErrorCode, LPCSTR szFILE, int line);

#if defined UNICODE || _UNICODE

#define RemoveWhiteSpace RemoveWhiteSpaceW
#define LoggingMessage LoggingMessageW

#else

#define RemoveWhiteSpace RemoveWhiteSpaceA
#define LoggingMessage LoggingMessageA

#endif

#ifdef __cplusplus
}
#endif

#endif /* _MISC_H_ */