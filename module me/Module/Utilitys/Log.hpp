#pragma once

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <windows.h>

#include <managers/logging/logging.h>

// Unified logging
//
//   LOG();                                          -> appends "CALL <functionname>" to Logs.log
//   LOGF("TaskScheduler", "found WHSJ at 0x%llX", addr); -> OutputDebugString + ImGui log + Logs.log
//
// Logs.log is created next to Module.dll (the directory this dll was loaded from)
// and is (re)created empty every time the dll is loaded.

#ifdef _DEBUG
    #define LOG_ENABLED 1
#else
    #define LOG_ENABLED 0
#endif

namespace LogFile
{
    struct State
    {
        CRITICAL_SECTION lock;
        HANDLE handle;
        char path[MAX_PATH];
        // relógio de parede derivado (base + tick) e controle do flush periódico
        ULONGLONG bootTick;
        ULONGLONG baseMs;
        DWORD pendingLines;
        ULONGLONG lastFlush;
    };

    inline State MakeState()
    {
        State state{};

        InitializeCriticalSection(&state.lock);
        state.handle = INVALID_HANDLE_VALUE;
        state.path[0] = '\0';

        state.bootTick = GetTickCount64();
        state.pendingLines = 0;
        state.lastFlush = state.bootTick;

        SYSTEMTIME now{};
        GetLocalTime(&now);
        state.baseMs = static_cast<ULONGLONG>(now.wHour) * 3600000ULL +
            static_cast<ULONGLONG>(now.wMinute) * 60000ULL +
            static_cast<ULONGLONG>(now.wSecond) * 1000ULL +
            static_cast<ULONGLONG>(now.wMilliseconds);

        char module_path[MAX_PATH]{};
        HMODULE module = nullptr;

        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&MakeState), &module) && module)
        {
            GetModuleFileNameA(module, module_path, MAX_PATH);
        }

        char* slash = strrchr(module_path, '\\');
        if (slash)
        {
            slash[1] = '\0';
            snprintf(state.path, MAX_PATH, "%sLogs.log", module_path);

            state.handle = CreateFileA(state.path, FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (state.handle != INVALID_HANDLE_VALUE)
                SetFilePointer(state.handle, 0, nullptr, FILE_END);
        }

        return state;
    }

    inline State& Get()
    {
        static State state = MakeState();
        return state;
    }

    inline void Init()
    {
        Get();
    }

    inline const char* Path()
    {
        return Get().path;
    }

    // "HH:MM:SS.mmm" do relógio de parede, derivado de base + GetTickCount64 —
    // sem syscalls custosos por linha. Toda linha ganha timestamp para medir a
    // duração de cada fase (init, sync de FFlags, compile de script) pela
    // diferença entre linhas consecutivas do Logs.log.
    inline void Stamp(char* Out, size_t Size)
    {
        const State& state = Get();
        const ULONGLONG Ms = (state.baseMs + (GetTickCount64() - state.bootTick)) % 86400000ULL;
        snprintf(Out, Size, "[%02u:%02u:%02u.%03u] ",
            static_cast<unsigned>(Ms / 3600000ULL),
            static_cast<unsigned>((Ms / 60000ULL) % 60ULL),
            static_cast<unsigned>((Ms / 1000ULL) % 60ULL),
            static_cast<unsigned>(Ms % 1000ULL));
    }

    inline void Write(const char* text, int length)
    {
        if (!text || length <= 0)
            return;

        State& state = Get();
        if (state.handle == INVALID_HANDLE_VALUE)
            return;

        EnterCriticalSection(&state.lock);

        DWORD written = 0;
        WriteFile(state.handle, text, static_cast<DWORD>(length), &written, nullptr);

        // Flush periódico (≤200 ms ou 32 linhas) em vez de um por linha: um
        // FlushFileBuffers a cada LOGF sincroniza o disco — e o OneDrive, que
        // indexa esta pasta — dezenas de vezes por segundo durante a execução
        // de scripts. Cada linha de log virava um travamento potencial.
        const ULONGLONG Now = GetTickCount64();
        if (++state.pendingLines >= 32 || Now - state.lastFlush >= 200)
        {
            FlushFileBuffers(state.handle);
            state.pendingLines = 0;
            state.lastFlush = Now;
        }

        LeaveCriticalSection(&state.lock);
    }

    // called by LOG() — one line per entry: "CALL <functionname>"
    inline void Call(const char* name)
    {
        if (!name)
            return;

        char line[512]{};
        char stamp[24]{};
        Stamp(stamp, sizeof(stamp));
        const int length = snprintf(line, sizeof(line), "%sCALL %s\n", stamp, name);
        Write(line, length);
    }

    // called by LOGF() — one line per entry: "[source] message"
    inline void Message(const char* source, const char* text)
    {
        if (!text)
            return;

        char line[2400]{};
        char stamp[24]{};
        Stamp(stamp, sizeof(stamp));
        const int length = snprintf(line, sizeof(line), "%s[%s] %s\n", stamp, source ? source : "log", text);
        Write(line, length);
    }
}

inline void Log_Internal(const char* source, const char* fmt, ...)
{
    char msg[2048]{};

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    // OutputDebugStringA — visible in DbgView / debugger
    char dbg[2300]{};
    snprintf(dbg, sizeof(dbg), "[justafatguy67][%s] %s\n", source, msg);
    OutputDebugStringA(dbg);

    // ImGui in-app log
    log_mgr.add_log(dbg);

    // Logs.log next to the dll
    LogFile::Message(source, msg);
}

// call at the top of a function: writes "CALL <functionname>" into Logs.log
#define LOG() LogFile::Call(__FUNCTION__)

// formatted log: OutputDebugString + ImGui + Logs.log
#define LOGF(source, ...) do { Log_Internal(source, __VA_ARGS__); } while(0)
