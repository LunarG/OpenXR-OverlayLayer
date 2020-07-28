#ifndef _OVERLAYS_H_
#define _OVERLAYS_H_

#include <openxr/openxr.h>
#include <new>
#include <set>
#include <functional>

enum CopyType {
    COPY_EVERYTHING,       // XR command will consume (aka input)
    COPY_ONLY_TYPE_NEXT,   // XR command will fill (aka output)
};

typedef std::function<void* (size_t size)> AllocateFunc;
typedef std::function<void (const void* p)> FreeFunc;
XrBaseInStructure *CopyXrStructChain(XrInstance instance, const XrBaseInStructure* srcbase, CopyType copyType, AllocateFunc alloc, std::function<void (void* pointerToPointer)> addOffsetToPointer);
void FreeXrStructChain(XrInstance instance, const XrBaseInStructure* p, FreeFunc free);
XrBaseInStructure* CopyEventChainIntoBuffer(XrInstance instance, const XrEventDataBaseHeader* eventData, XrEventDataBuffer* buffer);
XrBaseInStructure* CopyXrStructChainWithMalloc(XrInstance instance, const void* xrstruct);
void FreeXrStructChainWithFree(XrInstance instance, const void* xrstruct);

typedef std::pair<uint64_t, XrObjectType> HandleTypePair;

extern const std::set<HandleTypePair> OverlaysLayerNoObjectInfo;

void OverlaysLayerLogMessage(XrInstance instance,
                         XrDebugUtilsMessageSeverityFlagsEXT message_severity, const char* command_name,
                         const std::set<HandleTypePair>& objects_info, const char* message);

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

#endif /* _OVERLAYS_H_ */
