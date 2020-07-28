import sys
import xml.etree.ElementTree as etree

def parse_parameter(reg_parameter):
    # XXX placeholder
    """
    parameter = []

    reg_type = reg_parameter.find("type")
    reg_name = reg_parameter.find("name")
    reg_enum = reg_parameter.find("enum")

    parameter.append((NAME, reg_name.text))

    if reg_type.tail:
        assert reg_type.tail in ["*", "["]

        if reg_type.tail == "*":

            parameter["pointer"] = true

        elif reg_type.tail == "[":

            parameter["array"] = true
            assert reg_enum.tail == "]"
            parameter["array_size"] = reg_enum.text

    if reg_parameter.text:  # const
        assert reg_parameter.text == "const"
        parameter.append((CONST,))
    """

    return reg_parameter

def parameter_to_name(command_name, parameter):
    return parameter.find("name").text 

def parameter_to_cdecl(command_name, parameter):
    paramdecl = str(parameter.text or "")
    for element in parameter:
        paramdecl += str(element.text or "") + str(element.tail or "")
    assert len(parameter.tail.strip()) == 0
    return paramdecl

    # cdecl = ""
    # if "const" in parameter:
        # cdecl += "const "
    # cdecl += parameter["type"] + " "
    # if "array" in parameter:
        # cdecl += "[" + parameter["array_size"] + "]"

def api_layer_name_for_command(command):
    return LayerName + command[2:]

def dispatch_name_for_command(command):
    return command[2:]


def dump(file, indent, depth, element):
    # if element.tag == "type" and element.attrib.get("name", "") == "XrEventDataBuffer":
        # file.write("%s\n" % dir(element[2]))
    if depth == 0:
        return
    file.write("%s%s : %s\n" % (" " * indent, element.tag, element.attrib))
    if element.text:
        if len(element.text.strip()) > 0:
            file.write("%stext = \"%s\"\n" % (" " * (indent + 4), element.text.strip()))
    for child in element:
        dump(indent + 4, depth - 1, child)
    if element.tail:
        if len(element.tail.strip()) > 0:
            file.write("%stail = \"%s\"\n" % (" " * (indent + 4), element.tail.strip()))

LayerName = "OverlaysLayer"

registryFilename = sys.argv[1]
outputFilename = sys.argv[2]

tree = etree.parse(registryFilename)
root = tree.getroot()

if False:
    out = open("output.dump", "w")
    dump(out, 0, 100, root)
    out.close()

commands = {}

reg_commands = root.find("commands")

for reg_command in reg_commands:
    # print("%s" % (reg_command.find("proto").find("name").text))
    command = {}
    command["return_type"] = reg_command.find("proto").find("type").text
    command_name = reg_command.find("proto").find("name").text
    command["name"] = command_name
    command["parameters"] = []
    for reg_parameter in reg_command.findall("param"):
        command["parameters"].append(parse_parameter(reg_parameter))
    commands[command_name] = command

reg_types = root.find("types")

handles = {} # value is a tuple of handle name and parent handle name

atoms = set()

for reg_type in reg_types:

    if reg_type.attrib.get("category", "") == "handle":
        handle_name = reg_type.find("name").text
        handles[handle_name] = (handle_name, reg_type.attrib.get("parent", ""))

    if reg_type.attrib.get("category", "") == "basetype" and reg_type.find("type").text == "XR_DEFINE_ATOM":
        atom_name = reg_type.find("name").text
        atoms.add(atom_name)

supported_commands = [
    "xrDestroyInstance",
    "xrGetInstanceProperties",
    "xrPollEvent",
    "xrResultToString",
    "xrStructureTypeToString",
    "xrGetSystem",
    "xrGetSystemProperties",
    "xrEnumerateEnvironmentBlendModes",
    "xrCreateSession",
    "xrDestroySession",
    "xrEnumerateReferenceSpaces",
    "xrCreateReferenceSpace",
    "xrGetReferenceSpaceBoundsRect",
    "xrCreateActionSpace",
    "xrLocateSpace",
    "xrDestroySpace",
    "xrEnumerateViewConfigurations",
    "xrGetViewConfigurationProperties",
    "xrEnumerateViewConfigurationViews",
    "xrEnumerateSwapchainFormats",
    "xrCreateSwapchain",
    "xrDestroySwapchain",
    "xrEnumerateSwapchainImages",
    "xrAcquireSwapchainImage",
    "xrWaitSwapchainImage",
    "xrReleaseSwapchainImage",
    "xrBeginSession",
    "xrEndSession",
    "xrRequestExitSession",
    "xrWaitFrame",
    "xrBeginFrame",
    "xrEndFrame",
    "xrLocateViews",
    "xrStringToPath",
    "xrPathToString",
    "xrCreateActionSet",
    "xrDestroyActionSet",
    "xrCreateAction",
    "xrDestroyAction",
    "xrSuggestInteractionProfileBindings",
    "xrAttachSessionActionSets",
    "xrGetCurrentInteractionProfile",
    "xrGetActionStateBoolean",
    "xrGetActionStateFloat",
    "xrGetActionStateVector2f",
    "xrGetActionStatePose",
    "xrSyncActions",
    "xrEnumerateBoundSourcesForAction",
    "xrGetInputSourceLocalizedName",
    "xrApplyHapticFeedback",
    "xrStopHapticFeedback",
    "xrGetD3D11GraphicsRequirementsKHR",
    "xrSetDebugUtilsObjectNameEXT",
    "xrCreateDebugUtilsMessengerEXT",
    "xrDestroyDebugUtilsMessengerEXT",
    "xrSubmitDebugUtilsMessageEXT",
    "xrSessionBeginDebugUtilsLabelRegionEXT",
    "xrSessionEndDebugUtilsLabelRegionEXT",
    "xrSessionInsertDebugUtilsLabelEXT",
]

supported_handles = [
    "XrInstance",
    "XrSession",
    "XrActionSet",
    "XrAction",
    "XrSwapchain",
    "XrSpace",
    "XrDebugUtilsMessengerEXT",
]

# XXX development placeholder
if False:
    for command_name in supported_commands:
        command = commands[command_name]
        parameters = ", ".join([parameter_to_cdecl(command_name, parameter) for parameter in command["parameters"]])
        print("%s %s(%s)" % (command["return_type"], command["name"], parameters))

if False:
    for handle in supported_handles:
        if not handles[handle][1]:
            print("handle %s" % (handles[handle][0]))
        else:
            print("handle %s has parent type %s" % (handles[handle][0], handles[handle][1]))


# store preambles of generated header and source -----------------------------


before_downchain = {}

after_downchain = {}

add_to_handle_struct = {}

# after_downchain[

add_to_handle_struct["XrInstance"] = {
    "members" : """
        std::set<XrDebugUtilsMessengerEXT> debugUtilsMessengers;
    """,
    }

add_to_handle_struct["XrDebugUtilsMessengerEXT"] = {
    "members" : """
        XrDebugUtilsMessengerCreateInfoEXT createInfo;
    """,
    }

after_downchain["xrCreateDebugUtilsMessengerEXT"] = """
    gOverlaysLayerXrDebugUtilsMessengerEXTToHandleInfo.at(*messenger).createInfo = *createInfo;
    // Already have lock on gOverlaysLayerXrInstanceToHandleInfo.at
    gOverlaysLayerXrInstanceToHandleInfo.at(instance).debugUtilsMessengers.insert(*messenger);
"""

after_downchain["xrCreateSession"] = """
    OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, "xrCreateSession", OverlaysLayerNoObjectInfo, "Hi, a session was created.\\n");
"""

after_downchain["xrStringToPath"] = """
        gOverlaysLayerPathtoAtomInfo.emplace(std::piecewise_construct, std::forward_as_tuple(*path), std::forward_as_tuple(pathString));

"""

after_downchain["xrGetSystem"] = """
        gOverlaysLayerSystemIdtoAtomInfo.emplace(std::piecewise_construct, std::forward_as_tuple(*systemId), std::forward_as_tuple(getInfo));

"""

# store preambles of generated header and source -----------------------------


source_text = """
#include "xr_generated_overlays.hpp"
#include "hex_and_handles.h"

#include <cstring>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <unordered_map>

"""

header_text = """
// #include "api_layer_platform_defines.h"
#include "xr_generated_dispatch_table.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <set>

#include "overlays.h"

"""


# make types, structs, variables ---------------------------------------------


# XrPath

header_text += f"struct {LayerName}PathAtomInfo\n"
header_text += """
{
    std::string pathString;
"""
header_text += f"    {LayerName}PathAtomInfo(const char *pathString_) :\n"
header_text += """
        pathString(pathString_)
    {
    }
    // map of remote XrPath by XrSession
    std::unordered_map<uint64_t, uint64_t> remotePathByLocalSession;
};

"""

header_text += f"extern std::unordered_map<XrPath, {LayerName}PathAtomInfo> g{LayerName}PathtoAtomInfo;\n\n"
source_text += f"std::unordered_map<XrPath, {LayerName}PathAtomInfo> g{LayerName}PathtoAtomInfo;\n\n"


# XrSystemId

header_text += f"struct {LayerName}SystemIdAtomInfo\n"
header_text += """
{
    XrSystemGetInfo *getInfo;
"""
header_text += f"    {LayerName}SystemIdAtomInfo(const XrSystemGetInfo *getInfo_)\n"
header_text += """
    {
        getInfo = reinterpret_cast<XrSystemGetInfo*>(CopyXrStructChainWithMalloc(getInfo_));
    }
    // map of remote XrSystemId by XrSession
    std::unordered_map<uint64_t, uint64_t> remoteSystemIdByLocalSession;
};

"""

header_text += f"extern std::unordered_map<XrSystemId, {LayerName}SystemIdAtomInfo> g{LayerName}SystemIdtoAtomInfo;\n\n"
source_text += f"std::unordered_map<XrSystemId, {LayerName}SystemIdAtomInfo> g{LayerName}SystemIdtoAtomInfo;\n\n"


only_child_handles = [h for h in supported_handles if h != "XrInstance"]

for handle_type in supported_handles:
    header_text += f"struct {LayerName}{handle_type}HandleInfo\n"
    header_text += "{\n"
    # XXX header_text += "        std::set<HandleTypePair> childHandles;\n"
    parent_type = str(handles[handle_type][1] or "")
    if parent_type:
        header_text += f"        {parent_type} parentHandle;\n"
    header_text += "        XrGeneratedDispatchTable *downchain;\n"
    header_text += "        bool invalid = false;\n"
    header_text += add_to_handle_struct.get(handle_type, {}).get("members", "")
    if parent_type:
        header_text += f"        {LayerName}{handle_type}HandleInfo({parent_type} parent, XrGeneratedDispatchTable *downchain_) : \n"
        header_text += "            parentHandle(parent),\n"
        header_text += "            downchain(downchain_)\n"
        header_text += "        {}\n"
    else:
        # we know this is XrInstance...
        header_text += f"        {LayerName}{handle_type}HandleInfo(XrGeneratedDispatchTable *downchain_): \n"
        header_text += "            downchain(downchain_)\n"
        header_text += "        {}\n"
        header_text += f"        ~{LayerName}{handle_type}HandleInfo()\n"
        header_text += "        {\n"
        header_text += "            delete downchain;\n"
        header_text += "        }\n"
    header_text += f"        void Destroy() /* For OpenXR's intents, not a dtor */\n"
    header_text += "        {\n"
    header_text += "            invalid = true;\n"
    # XXX header_text += "            ;\n" delete all child handles, and how will we do that?
    header_text += "        }\n"
    header_text += "};\n"

    source_text += f"std::unordered_map<{handle_type}, {LayerName}{handle_type}HandleInfo> g{LayerName}{handle_type}ToHandleInfo;\n"
    header_text += f"extern std::unordered_map<{handle_type}, {LayerName}{handle_type}HandleInfo> g{LayerName}{handle_type}ToHandleInfo;\n"

    source_text += f"std::mutex g{LayerName}{handle_type}ToHandleInfoMutex;\n"
    header_text += f"extern std::mutex g{LayerName}{handle_type}ToHandleInfoMutex;\n"

    source_text += "\n"


# make functions returned by xrGetInstanceProcAddr ---------------------------


for command_name in supported_commands:
    command = commands[command_name]

    parameters = ", ".join([parameter_to_cdecl(command_name, parameter) for parameter in command["parameters"]])
    command_type = command["return_type"]
    layer_command = api_layer_name_for_command(command_name)
    source_text += f"{command_type} {layer_command}({parameters})\n"
    source_text += "{\n"

    handle_type = command["parameters"][0].find("type").text
    handle_name = command["parameters"][0].find("name").text

    # handles are guaranteed not to be destroyed while in another command according to the spec...
    source_text += f"    std::unique_lock<std::mutex> mlock(g{LayerName}{handle_type}ToHandleInfoMutex);\n"
    source_text += f"    {LayerName}{handle_type}HandleInfo& {handle_name}Info = g{LayerName}{handle_type}ToHandleInfo.at({handle_name});\n\n"

    source_text += before_downchain.get(command_name, "")

    parameter_names = ", ".join([parameter_to_name(command_name, parameter) for parameter in command["parameters"]])
    dispatch_command = dispatch_name_for_command(command_name)
    source_text += f"    XrResult result = {handle_name}Info.downchain->{dispatch_command}({parameter_names});\n\n"

    if command_name.find("Create") >= 0:
        created_type = command["parameters"][-1].find("type").text
        created_name = command["parameters"][-1].find("name").text
        # just assume we hand-coded Instance creation so these are all child handle types
        # in C++17 this would be an initializer list but before that we have to use the forwarding syntax.
        source_text += f"    g{LayerName}{created_type}ToHandleInfo.emplace(std::piecewise_construct, std::forward_as_tuple(*{created_name}), std::forward_as_tuple({handle_name}, {handle_name}Info.downchain));\n"
        # XXX source_text += f"    {handle_name}Info.childHandles.insert(*{created_name});\n"

    source_text += f"    mlock.unlock();\n"

    source_text += after_downchain.get(command_name, "")

    source_text += "    return result;\n"

    source_text += "}\n"
    source_text += "\n"

# make GetInstanceProcAddr ---------------------------------------------------

header_text += f"XrResult {LayerName}XrGetInstanceProcAddr("
header_text += """
    XrInstance                                  instance,
    const char*                                 name,
    PFN_xrVoidFunction*                         function);
"""

source_text += f"XrResult {LayerName}XrGetInstanceProcAddr("
source_text += """
    XrInstance                                  instance,
    const char*                                 name,
    PFN_xrVoidFunction*                         function)
{
    // Set the function pointer to NULL so that the fall-through below actually works:
    *function = nullptr;

"""
first = True
for command_name in supported_commands:
    command = commands[command_name]
    layer_command = api_layer_name_for_command(command_name)
    if not first:
        source_text += "    } else "
    source_text += f"    if (strcmp(name, \"{command_name}\") == 0) " + "{\n"
    source_text += f"        *function = reinterpret_cast<PFN_xrVoidFunction>({layer_command});\n"
    first = False

unsupported_command_names = [c for c in commands.keys() if not c in supported_commands]

for command_name in unsupported_command_names:
    command = commands[command_name]
    layer_command = api_layer_name_for_command(command_name)
    source_text += "    } else " + f"if (strcmp(name, \"{command_name}\") == 0) " + "{\n"
    source_text += f"        *function = nullptr;\n"

source_text += """
    }

    // If we set up the function, just return
    if (*function != nullptr) {
        return XR_SUCCESS;
    }

    // Since Overlays proxies Session and Session children over IPC
    // and has special handling for some objects,
    // if we didn't set up the function, we must not offer it.
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}
"""


if outputFilename == "xr_generated_overlays.cpp":
    open(outputFilename, "w").write(source_text)
else:
    open(outputFilename, "w").write(header_text)
