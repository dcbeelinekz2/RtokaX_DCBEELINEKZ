/*
 * PtokaX - hub server for Direct Connect peer to peer network.

 * Copyright (C) 2002-2005  Ptaczek, Ptaczek at PtokaX dot org
 * Copyright (C) 2004-2013  Petr Kozelka, PPK at PtokaX dot org

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3
 * as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//---------------------------------------------------------------------------
#include "stdinc.h"
//---------------------------------------------------------------------------
#include "ExceptionHandling.h"
//---------------------------------------------------------------------------
#include "ServerManager.h"
#include "utility.h"
//---------------------------------------------------------------------------
#pragma hdrstop
//---------------------------------------------------------------------------
#include <Dbghelp.h>
#include <delayimp.h>
//---------------------------------------------------------------------------
LPTOP_LEVEL_EXCEPTION_FILTER pOldTLEF = NULL;
string sLogPath = "", sDebugSymbolsFile = "";
static char sDebugBuf[512];
//---------------------------------------------------------------------------

void AppendLog(const char * sData) {
	FILE * fw  = fopen((sLogPath + "system.log").c_str(), "a");

	if(fw == NULL) {
		return;
	}

	time_t acc_time;
	time(&acc_time);

	struct tm * acc_tm;
	acc_tm = localtime(&acc_time);

	strftime(sDebugBuf, 128, "%d.%m.%Y %H:%M:%S", acc_tm);

	fprintf(fw, "%s - %s\n", sDebugBuf, sData);

	fclose(fw);
}
//---------------------------------------------------------------------------

FARPROC WINAPI PtokaX_FailHook(unsigned /*dliNotify*/, PDelayLoadInfo /*pdli*/) {
#ifdef _BUILD_GUI
    ::MessageBox(NULL, "Something bad happen and PtokaX crashed. PtokaX was not able to collect any information why this happen because your operating system"
		" don't support functionality needed for that. If you know why this crash happen then please report it as bug to PPK@PtokaX.org!",
        "PtokaX crashed!", MB_OK | MB_ICONERROR);
#else
    AppendLog("Something bad happen and PtokaX crashed. PtokaX was not able to collect any information why this happen because your operating system"
        " don't support functionality needed for that. If you know why this crash happen then please report it as bug to PPK@PtokaX.org!");
#endif

    exit(EXIT_FAILURE);
}

//---------------------------------------------------------------------------

void GetSourceFileInfo(DWORD64 dw64Address, FILE * fw) {
    IMAGEHLP_LINE64 il64LineInfo;
	memset(&il64LineInfo, 0, sizeof(IMAGEHLP_LINE64));
	il64LineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    DWORD dwDisplacement = 0;

	if(SymGetLineFromAddr64(GetCurrentProcess(), dw64Address, &dwDisplacement, &il64LineInfo) == TRUE) {
        // We have sourcefile and linenumber info, write it.
        fprintf(fw, "%s(%d): ", il64LineInfo.FileName, il64LineInfo.LineNumber);
	} else {
        // We don't have sourcefile and linenumber info, let's try module name

        IMAGEHLP_MODULE64 im64ModuleInfo;
        memset(&im64ModuleInfo, 0, sizeof(IMAGEHLP_MODULE64));
        im64ModuleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);

        if(SymGetModuleInfo64(GetCurrentProcess(), dw64Address, &im64ModuleInfo)) {
            // We found module name, write module name and address
            fprintf(fw, "%s|0x%08I64X: ", im64ModuleInfo.ModuleName, dw64Address);
        } else {
            // We don't found module. Write address, it's better than nothing
            fprintf(fw, "0x%08I64X: ", dw64Address);
        }
	}
}
//---------------------------------------------------------------------------

void GetFunctionInfo(DWORD64 dw64Address, FILE * fw) {
	DWORD64 dw64Displacement = 0;

    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO pSym = (PSYMBOL_INFO)buffer;

    pSym->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSym->MaxNameLen = MAX_SYM_NAME;

	if(SymFromAddr(GetCurrentProcess(), dw64Address, &dw64Displacement, pSym) == TRUE) {
        // We have decorated name, try to make it readable
        if(UnDecorateSymbolName(pSym->Name, sDebugBuf, 512, UNDNAME_COMPLETE | UNDNAME_NO_THISTYPE | UNDNAME_NO_SPECIAL_SYMS | UNDNAME_NO_MEMBER_TYPE |
            UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_ACCESS_SPECIFIERS) > 0) {
            // We have readable name, write it
            fprintf(fw, "%s\n", sDebugBuf);
            return;
        }
	}

    // We don't found any info, write '?'
    fprintf(fw, "?\n");
}
//---------------------------------------------------------------------------

LONG WINAPI PtokaX_UnhandledExceptionFilter(LPEXCEPTION_POINTERS ExceptionInfo) {
    static volatile LONG PermLock = 0;

    // When unhandled exception happen then permanently 'lock' here. We terminate after first exception.
    while(InterlockedExchange(&PermLock, 1) == 1) {
		::Sleep(10);
	}

    // Set failure hook
	__pfnDliFailureHook2 = PtokaX_FailHook;

    // Check if we have debug symbols
    if(FileExist(sDebugSymbolsFile.c_str()) == false) {
#ifdef _BUILD_GUI
        ::MessageBox(NULL, "Something bad happen and PtokaX crashed. PtokaX was not able to collect any information why this happen because file with debug symbols"
			" (PtokaX.pdb) is missing. If you know why this crash happen then please report it as bug to PPK@PtokaX.org!",
            "PtokaX crashed!", MB_OK | MB_ICONERROR);
#else
        AppendLog("Something bad happen and PtokaX crashed. PtokaX was not able to collect any information why this happen because file with debug symbols"
            " (PtokaX.pdb) is missing. If you know why this crash happen then please report it as bug to PPK@PtokaX.org!");
#endif

        ExceptionHandlingUnitialize();

		exit(EXIT_FAILURE);
	}

	// Initialize debug symbols
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_LINES);
    if(SymInitialize(GetCurrentProcess(), clsServerManager::sPath.c_str(), TRUE) == FALSE) {
#ifdef _BUILD_GUI
        ::MessageBox(NULL, "Something bad happen and PtokaX crashed. PtokaX was not able to collect any information why this happen because initializatin of"
			" debug symbols failed. If you know why this crash happen then please report it as bug to PPK@PtokaX.org!",
            "PtokaX crashed!", MB_OK | MB_ICONERROR);
#else
        AppendLog("Something bad happen and PtokaX crashed. PtokaX was not able to collect any information why this happen because initializatin of"
            " debug symbols failed. If you know why this crash happen then please report it as bug to PPK@PtokaX.org!");
#endif

        ExceptionHandlingUnitialize();

		exit(EXIT_FAILURE);
    }

    // Generate crash log filename
    time_t acc_time;
    time(&acc_time);

    struct tm *tm = localtime(&acc_time);
    strftime(sDebugBuf, 128, "Crash-%d.%m.%Y-%H.%M.%S.log", tm);

    // Open crash file
    FILE * fw = fopen((sLogPath + sDebugBuf).c_str(), "w");
    if(fw == NULL) {
#ifdef _BUILD_GUI
        ::MessageBox(NULL, "Something bad happen and PtokaX crashed. PtokaX was not able to create file with information why this crash happen."
			" If you know why this crash happen then please report it as bug to PPK@PtokaX.org!",
            "PtokaX crashed!", MB_OK | MB_ICONERROR);
#else
        AppendLog("Something bad happen and PtokaX crashed. PtokaX was not able to create file with information why this crash happen."
            " If you know why this crash happen then please report it as bug to PPK@PtokaX.org!");
#endif

        ExceptionHandlingUnitialize();
        SymCleanup(GetCurrentProcess());

		exit(EXIT_FAILURE);
    }

    string sCrashMsg = "Something bad happen and PtokaX crashed. PtokaX collected information why this crash happen to file ";
    sCrashMsg += string(sDebugBuf);
    sCrashMsg += ", please send that file to PPK@PtokaX.org!";

    // Write PtokaX version, build and exception code
	fprintf(fw, "PtokaX version: " PtokaXVersionString " [build " BUILD_NUMBER "]"
#ifdef _M_X64
        " (x64)"
#endif
        "\nException Code: %x\n", ExceptionInfo->ExceptionRecord->ExceptionCode);

    {
        // Write windoze version where we crashed if is possible
        OSVERSIONINFOEX ver;
        memset(&ver, 0, sizeof(OSVERSIONINFOEX));
        ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

        if(GetVersionEx((OSVERSIONINFO*)&ver) != 0) {
			fprintf(fw, "Windows version: %d.%d SP: %d\n", ver.dwMajorVersion, ver.dwMinorVersion, ver.wServicePackMajor);
        }
    }

    // Write date and time when crash happen
    strftime(sDebugBuf, 128, "Date and time: %d.%m.%Y %H:%M:%S\n\n", tm);
    fprintf(fw, sDebugBuf);

    STACKFRAME64 sf64CallStack;
    memset(&sf64CallStack, 0, sizeof(STACKFRAME64));

    sf64CallStack.AddrPC.Mode      = AddrModeFlat;
    sf64CallStack.AddrStack.Mode   = AddrModeFlat;
    sf64CallStack.AddrFrame.Mode   = AddrModeFlat;

#ifdef _M_X64
    sf64CallStack.AddrPC.Offset    = ExceptionInfo->ContextRecord->Rip;
    sf64CallStack.AddrStack.Offset = ExceptionInfo->ContextRecord->Rsp;
    sf64CallStack.AddrFrame.Offset = ExceptionInfo->ContextRecord->Rbp;
#else
    sf64CallStack.AddrPC.Offset    = ExceptionInfo->ContextRecord->Eip;
    sf64CallStack.AddrStack.Offset = ExceptionInfo->ContextRecord->Esp;
    sf64CallStack.AddrFrame.Offset = ExceptionInfo->ContextRecord->Ebp;
#endif

	// Write where crash happen
    fprintf(fw, "Exception location:\n");

    GetSourceFileInfo(sf64CallStack.AddrPC.Offset, fw);
	GetFunctionInfo(sf64CallStack.AddrPC.Offset, fw);

	// Try to write callstack
    fprintf(fw, "\nCall stack:\n");

    // We don't want it like never ending story, limit call stack to 100 lines
	for(uint32_t ui32i = 0; ui32i < 100; ui32i++) {
		if(StackWalk64(
#ifdef _M_X64
			IMAGE_FILE_MACHINE_AMD64,
#else
			IMAGE_FILE_MACHINE_I386,
#endif
			GetCurrentProcess(), GetCurrentThread(), &sf64CallStack, ExceptionInfo->ContextRecord, NULL,
            SymFunctionTableAccess64, SymGetModuleBase64, NULL) == FALSE || sf64CallStack.AddrFrame.Offset == 0) {
            break;
        }

        GetSourceFileInfo(sf64CallStack.AddrPC.Offset, fw);
        GetFunctionInfo(sf64CallStack.AddrPC.Offset, fw);
	}

	fclose(fw);

#ifdef _BUILD_GUI
    ::MessageBox(NULL, sCrashMsg.c_str(), "PtokaX crashed!", MB_OK | MB_ICONERROR);
#else
    AppendLog(sCrashMsg.c_str());
#endif

    ExceptionHandlingUnitialize();
    SymCleanup(GetCurrentProcess());

    exit(EXIT_FAILURE);
}
//---------------------------------------------------------------------------

void ExceptionHandlingInitialize(const string &sPath, char * sAppPath) {
    sLogPath = sPath+ "\\logs\\";

    size_t szBufLen = strlen(sAppPath);
    if(szBufLen > 3  && tolower(sAppPath[szBufLen-3]) == 'e' && tolower(sAppPath[szBufLen-2]) == 'x' && tolower(sAppPath[szBufLen-1]) == 'e') {
        sAppPath[szBufLen-3] = 'p';
        sAppPath[szBufLen-2] = 'd';
        sAppPath[szBufLen-1] = 'b';
    }

    sDebugSymbolsFile = sAppPath;

    // Set PtokaX unhandled exception filter
    pOldTLEF = SetUnhandledExceptionFilter(&PtokaX_UnhandledExceptionFilter);
}
//---------------------------------------------------------------------------

void ExceptionHandlingUnitialize() {
    // Restore old Top Level Exception Filter
    SetUnhandledExceptionFilter(pOldTLEF);
}
//---------------------------------------------------------------------------
