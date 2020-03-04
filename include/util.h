#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif  // !WIN32_LEAN_AND_MEAN

#ifndef NOMINMAX
#define NOMINMAX
#endif  // !NOMINMAX

#include <windows.h>
#include <openxr/openxr.h>

inline std::string fmt(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int size = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    if(size >= 0) {
        int provided = size + 1;
        std::unique_ptr<char[]> buf(new char[provided]);

        va_start(args, fmt);
        vsnprintf(buf.get(), provided, fmt, args);
        va_end(args);

        return std::string(buf.get());
    }
    return "(fmt() failed, vsnprintf returned -1)";
}

inline void outputDebugF(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int size = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    if(size >= 0) {
        int provided = size + 1;
        std::unique_ptr<char[]> buf(new char[provided]);

        va_start(args, fmt);
        vsnprintf(buf.get(), provided, fmt, args);
        va_end(args);

        OutputDebugStringA(buf.get());
    } else {
        OutputDebugStringA("vsnprintf in outputDebugF() failed, format was: ");
        OutputDebugStringA(fmt);
    }
}

inline void CheckResultWithLastError(bool success, const char* what, const char *file, int line)
{
    if(!success) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        outputDebugF("%s at %s:%d failed with %d (%s)\n", what, file, line, lastError, messageBuf);
        DebugBreak();
        LocalFree(messageBuf);
    }
}

inline void CheckResult(HRESULT result, const char* what, const char *file, int line)
{
    if(result != S_OK) {
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        outputDebugF("%s at %s:%d failed with %d (%s)\n", what, file, line, result, messageBuf);
        DebugBreak();
        LocalFree(messageBuf);
    }
}

inline void CheckXrResult(XrResult a, const char* what, const char *file, int line)
{
    if(a != XR_SUCCESS) {
        outputDebugF("%s at %s:%d failed with %d\n", what, file, line, a);
        DebugBreak();
    }
}

// Use this macro to test if HANDLE or pointer functions succeeded that also update LastError
#define CHECK_NOT_NULL(a) CheckResultWithLastError(((a) != NULL), #a, __FILE__, __LINE__)

// Use this macro to test if functions succeeded that also update LastError
#define CHECK_LAST_ERROR(a) CheckResultWithLastError((a), #a, __FILE__, __LINE__)

// Use this macro to test Direct3D functions
#define CHECK(a) CheckResult(a, #a, __FILE__, __LINE__)

// Use this macro to test OpenXR functions
#define CHECK_XR(a) CheckXrResult(a, #a, __FILE__, __LINE__)