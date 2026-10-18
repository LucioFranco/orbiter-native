// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#define __LOG_CPP

#include <string.h>
#include <fstream>
#include <chrono>
#include <cerrno>
#ifdef _WIN32
#include <Windows.h>
#include <Psapi.h>
#endif
#include "Log.h"
#include "TimeData.h"

using namespace std;

extern char DBG_MSG[256];
extern TimeData td;

static std::string logname = "Orbiter.log";
static char logs[256] = "";
static bool finelog = false;
static auto t0 = std::chrono::steady_clock::now();

static double LogTime()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

static LogOutFunc logOut = 0;

void InitLog (const char *logfile, bool append)
{
	logname = logfile;
	ofstream ofs (logname, append ? ios::app : ios::out);
	ofs << "**** " << logname << endl;
	t0 = std::chrono::steady_clock::now();
}

void SetLogOutFunc(LogOutFunc func)
{
	logOut = func;
}

void SetLogVerbosity (bool verbose)
{
	finelog = verbose;
}

void LogOut (const char *msg, ...)
{
	va_list ap;
	va_start (ap, msg);
	LogOutVA(msg, ap);
	va_end (ap);
}

void LogOutVA(const char *format, va_list ap)
{
    // vfprintf consumes its va_list; clone it before formatting a second time.
    FILE *f = fopen(logname.c_str(), "a");
    if (f) {
        va_list copy;
        va_copy(copy, ap);
        fprintf(f, "%010.3f: ", LogTime());
        vfprintf(f, format, copy);
        va_end(copy);
        fputc('\n', f);
        fclose(f);
    }
    if (logOut) {
        va_list copy;
        va_copy(copy, ap);
        vsnprintf(logs, sizeof logs, format, copy);
        va_end(copy);
        (*logOut)(logs);
    }
}

void LogOutFine (const char *msg, ...)
{
    if (!finelog) return;
    va_list ap;
    va_start(ap, msg);
    LogOutVA(msg, ap);
    va_end(ap);
}

void LogOut ()
{
	LogOut ("%s", logs);
}

void LogOut_Error (const char *func, const char *file, int line, const char *msg, ...)
{
	va_list ap;
	va_start (ap, msg);
	LogOut_ErrorVA(func, file, line, msg, ap);
	va_end(ap);
}

void LogOut_Error_Start()
{
	LogOut("============================ ERROR: ===========================");
}

void LogOut_Error_End()
{
	LogOut("===============================================================");
}

void LogOut_Warning_Start()
{
	LogOut("--------------------------- WARNING: --------------------------");
}

void LogOut_Warning_End()
{
	LogOut("---------------------------------------------------------------");
}

void LogOut_Obsolete_Start()
{
	LogOut("-------------------------- OBSOLETE: --------------------------");
}

void LogOut_Location(const char* func, const char* file, int line)
{
	LogOut("[%s | %s | %d]", func, file, line);

}

void LogOut_ErrorVA(const char *func, const char *file, int line, const char *msg, va_list ap)
{
	LogOut_Error_Start();
	LogOutVA(msg, ap);
	LogOut_Location(func, file, line);
	LogOut_Error_End();
}

void LogOut_WarningVA(const char* func, const char* file, int line, const char* msg, va_list ap)
{
	LogOut_Warning_Start();
	LogOutVA(msg, ap);
	LogOut_Location(func, file, line);
	LogOut_Warning_End();
}

void LogOut_Warning(const char *func, const char *file, int line, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	LogOut_WarningVA(func, file, line, msg, ap);
	va_end(ap);
}

void LogOut_LastError (const char *func, const char *file, int line)
{
#ifdef _WIN32
	DWORD err = GetLastError();
	LPTSTR errString = NULL;
	FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,err,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&errString,0,NULL);
	LogOut_Error (func, file, line, "%s", errString ? errString : "Unknown system error");
	if (errString) LocalFree(errString);
#else
	const int err = errno;
	LogOut_Error(func, file, line, "%s", std::strerror(err));
#endif
}

#ifdef _WIN32
void LogOut_DDErr(HRESULT hr, const char *func, const char *file, int line)
{
    if (hr != 0) LogOut_Error(func, file, line, "DirectDraw error 0x%08x", static_cast<unsigned int>(hr));
}

void LogOut_DIErr(HRESULT hr, const char *func, const char *file, int line)
{
    if (hr != 0) LogOut_Error(func, file, line, "DirectInput error 0x%08x", static_cast<unsigned int>(hr));
}

#endif

void LogOut_Obsolete(const char* func, const char* msg)
{
	LogOut_Obsolete_Start();
	LogOut("Obsolete API function used: %s", func);
	if (msg)
		LogOut("%s", msg);
	else {
		LogOut("At least one active module is accessing an obsolete interface function.");
		LogOut("Addons which rely on obsolete functions may not be compatible with");
		LogOut("future versions of Orbiter.");
	}
	LogOut_Warning_End();
}


#ifdef _WIN32
void PrintModules()
{
	HMODULE hMods[4096];
	HANDLE hProcess;
	DWORD cbNeeded;
	unsigned int i;

	// Get a handle to the process.

	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetProcessId(GetCurrentProcess()));

	if (NULL == hProcess) return;

	if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
	{
		for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
		{
			char szModName[MAX_PATH];

			if (GetModuleFileNameExA(hProcess, hMods[i], szModName, sizeof(szModName)))
			{
				MODULEINFO mi;

				GetModuleInformation(hProcess, hMods[i], &mi, sizeof(MODULEINFO));

				DWORD vs = GetFileVersionInfoSize(szModName, NULL);

				if (vs == 0) continue;

				BYTE* Data = new BYTE[vs];

				if (!GetFileVersionInfo(szModName, 0, vs, Data))
				{
					LogOut("GetFileVersionInfo Failed %u", GetLastError());
					delete[]Data;
					continue;
				}

				VS_FIXEDFILEINFO *Info = NULL;
				UINT size = 0;

				if (!VerQueryValueA(Data, TEXT("\\"), (void**)&Info, &size))
				{
					LogOut("VerQueryValueA Failed %u", GetLastError());
					delete[]Data;
					continue;
				}

				if (Info->dwSignature == 0xFEEF04BD)
				{
					unsigned short a = Info->dwFileVersionMS >> 16;
					unsigned short b = Info->dwFileVersionMS & 0xFFFF;
					unsigned short c = Info->dwFileVersionLS >> 16;
					unsigned short d = Info->dwFileVersionLS & 0xFFFF;
					LogOut("Module linked [%s]  Version=%hu.%hu.%hu.%hu  Size=%u", szModName, a, b, c, d, mi.SizeOfImage);
				}

				delete[]Data;
			}
		}
	}
	CloseHandle(hProcess);
	return;
}



#endif

void tracenew (char *fname, int line)
{
#define TESTALLOC 2
#if TESTALLOC == 1
	ofstream ofs("tracenew.txt", ios::app);
	sprintf (DBG_MSG, "T=%f, %s: %d", SimT, fname, line);
	ofs << DBG_MSG << endl;
	ofs.close();
#elif TESTALLOC == 2
	sprintf (DBG_MSG, "T=%f, %s: %d", td.SimT0, fname, line);
#else
	MessageBeep (-1);
#endif
}


// =======================================================================
// Profiler methods
// =======================================================================

static double prof_sum = 0.0;
static uint32_t prof_count = 0;
static std::chrono::time_point<std::chrono::steady_clock> prof_t0;

void StartProf ()
{
	prof_t0 = std::chrono::steady_clock::now();
}

double EndProf (DWORD *count)
{
	auto t1 = std::chrono::steady_clock::now();
	std::chrono::duration<double> time_delta = t1 - prof_t0;

	double dt = time_delta.count();
	if (dt > 0.0) {
		prof_sum += dt;
		prof_count++;
	}
	if (count) *count = prof_count;
	return prof_sum/(double)prof_count;
}
