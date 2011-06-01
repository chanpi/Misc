#ifndef _MISCELLANEOUS_H_
#define _MISCELLANEOUS_H_

#include <Windows.h>

#undef DLL_EXPORT

#ifdef MISCELLANEOUS_EXPORTS
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
	DLL_EXPORT void LogInitialize(LOG_LEVEL logLevel);

	DLL_EXPORT void LogDebugMessageW(LOG_LEVEL logLevel, PCWSTR szMessage);
	DLL_EXPORT void LogDebugMessageA(LOG_LEVEL logLevel, PCSTR szMessage);

	// �擪�����̃X�y�[�X�������B
	DLL_EXPORT void RemoveWhiteSpaceW(PWSTR szBuffer);
	DLL_EXPORT void RemoveWhiteSpaceA(PSTR szBuffer);

	DLL_EXPORT void DebugProfileMonitor(PCSTR position);

#if defined UNICODE || _UNICODE

#define LogDebugMessage LogDebugMessageW
#define RemoveWhiteSpace RemoveWhiteSpaceW

#else

#define LogDebugMessage LogDebugMessageA
#define RemoveWhiteSpace RemoveWhiteSpaceA

#endif

#ifdef __cplusplus
}
#endif

#endif /* _MISCELLANEOUS_H_ */