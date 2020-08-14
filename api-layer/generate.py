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
        dump(file, indent + 4, depth - 1, child)
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

structs = {} # value is a tuple of struct name, type enum, extends struct name, and list of members
    # members are dict of "name", "type", other goop depending on type

have_protection = {
    "XR_USE_PLATFORM_WIN32",
    "XR_USE_GRAPHICS_API_D3D11",
}

for reg_type in reg_types:

    protect = reg_type.attrib.get("protect", "")
    if protect and not protect in have_protection:
        continue

    if reg_type.attrib.get("category", "") == "handle":
        handle_name = reg_type.find("name").text
        handles[handle_name] = (handle_name, reg_type.attrib.get("parent", ""))

    if reg_type.attrib.get("category", "") == "basetype" and reg_type.find("type").text == "XR_DEFINE_ATOM":
        atom_name = reg_type.find("name").text
        atoms.add(atom_name)

for reg_type in reg_types:

    protect = reg_type.attrib.get("protect", "")
    if protect and not protect in have_protection:
        continue

    if reg_type.attrib.get("category", "") == "struct":
        struct_name = reg_type.attrib["name"]
        extends = reg_type.attrib.get("structextends", "")
        typeenum = reg_type.findall("member")[0].attrib.get("values", "")
        members = []
        # print(struct_name)
        for reg_member in reg_type.findall("member"):
            member = {}

            reg_member_text = (reg_member.text or "").strip()
            reg_member_tail = (reg_member.tail or "").strip()

            member["is_const"] = reg_member_text and (reg_member.text[0:5] == "const")

            type_text = (reg_member.find("type").text or "").strip()
            type_tail = (reg_member.find("type").tail or "").strip()
            name_text = (reg_member.find("name").text or "").strip()
            name_tail = (reg_member.find("name").tail or "").strip()
            if not reg_member.find("enum") is None:
                enum_text = (reg_member.find("enum").text or "").strip()
                enum_tail = (reg_member.find("enum").tail or "").strip()
            else:
                enum_text = ""
                enum_tail = ""

            member["name"] = name_text

            member_type = type_text
            if type_tail:
                member_type += " " + type_tail

            if type_text == "char" and name_tail == "[" and enum_tail == "]":
                # member["type"] = "char_array"
                # member["size"] = enum_text
                member["type"] = "fixed_array"
                member["base_type"] = "char"
                member["size"] = enum_text

            elif member_type == "char * const*":
                member["type"] = "string_list"
                size_and_len = reg_member.attrib["len"].strip()
                member["size"] = size_and_len.split(",")[0].strip()

            elif type_tail == "* const*":
                member["type"] = "list_of_struct_pointers"
                member["struct_type"] = type_text
                size_and_len = reg_member.attrib["len"].strip()
                member["size"] = size_and_len.split(",")[0].strip()

            elif member_type == "void *":
                if reg_member_text == "const struct" or reg_member_text == "struct":
                    member["type"] = "xr_struct_pointer"
                else:
                    member["type"] = "void_pointer"

            elif name_tail and name_tail[0] == "[" and name_tail[-1] == "]":
                member["type"] = "fixed_array"
                member["base_type"] = type_text
                member["size"] = name_tail[1:-1]

            elif not type_tail and not name_tail:
                member["type"] = "POD"
                member["pod_type"] = type_text

            elif type_tail == "*" and not name_tail : # and reg_member_text == "const struct" or reg_member_text == "struct":
                size_and_len = reg_member.attrib.get("len", "").strip()
                if size_and_len:
                    if size_and_len == "null-terminated":
                        member["type"] = "c_string"
                    else:
                        if type_text in atoms or member_type in handles.keys():
                            member["type"] = "pointer_to_atom_or_handle"
                            member["size"] = size_and_len.split(",")[0].strip()
                        elif type_text in structs and structs[type_text][1]:
                            member["type"] = "pointer_to_xr_struct_array"
                            member["size"] = size_and_len.split(",")[0].strip()
                        else:
                            member["type"] = "pointer_to_struct_array"
                            member["size"] = size_and_len.split(",")[0].strip()
                else:
                    member["type"] = "pointer_to_struct"
                member["struct_type"] = type_text
                member["member_text"] = reg_member_text

            # elif type_tail == "*" and not name_tail and not reg_member_text:
                # member["type"] = "pointer_to_opaque"
                # member["opaque_type"] = type_text

            else:
                print("didn't parse %s" % (", ".join((reg_member_text, type_text, type_tail, name_text, name_tail, enum_text, enum_tail, reg_member_tail))))
                dump(sys.stdout, 4, 100, reg_member)
                sys.exit(-1)

            foo = """
            member : {}
                type : {}
                    text = "XrStructureType"
                name : {}
                    text = "type"
            member : {}
                text = "const"
                type : {}
                    text = "void"
                    tail = "*"
                name : {}
                    text = "next"
            member : {}
                type : {}
                    text = "char"
                name : {}
                    text = "layerName"
                    tail = "["
                enum : {}
                    text = "XR_MAX_API_LAYER_NAME_SIZE"
                    tail = "]"
"""
            members.append(member)
        structs[struct_name] = (struct_name, typeenum, extends, members)


supported_structs = [
    "XrVector2f",
    "XrVector3f",
    "XrVector4f",
    "XrColor4f",
    "XrQuaternionf",
    "XrPosef",
    "XrOffset2Df",
    "XrExtent2Df",
    "XrRect2Df",
    "XrOffset2Di",
    "XrExtent2Di",
    "XrRect2Di",
    "XrBaseInStructure",
    "XrBaseOutStructure",
    "XrApiLayerProperties",
    "XrExtensionProperties",
    "XrApplicationInfo",
    "XrInstanceCreateInfo",
    "XrInstanceProperties",
    "XrSystemGetInfo",
    "XrSystemProperties",
    "XrSystemGraphicsProperties",
    "XrSystemTrackingProperties",
    "XrGraphicsBindingD3D11KHR",
    "XrSessionCreateInfo",
    "XrSessionBeginInfo",
    "XrSwapchainCreateInfo",
    "XrSwapchainImageBaseHeader",
    "XrSwapchainImageD3D11KHR",
    "XrSwapchainImageAcquireInfo",
    "XrSwapchainImageWaitInfo",
    "XrSwapchainImageReleaseInfo",
    "XrReferenceSpaceCreateInfo",
    "XrActionSpaceCreateInfo",
    "XrSpaceLocation",
    "XrSpaceVelocity",
    "XrFovf",
    "XrView",
    "XrViewLocateInfo",
    "XrViewState",
    "XrViewConfigurationView",
    "XrSwapchainSubImage",
    "XrCompositionLayerBaseHeader",
    "XrCompositionLayerProjectionView",
    "XrCompositionLayerProjection",
    "XrCompositionLayerQuad",
    "XrFrameBeginInfo",
    "XrFrameEndInfo",
    "XrFrameWaitInfo",
    "XrFrameState",
    "XrHapticBaseHeader",
    "XrHapticVibration",
    "XrEventDataBaseHeader",
    "XrEventDataBuffer",
    "XrEventDataEventsLost",
    "XrEventDataInstanceLossPending",
    "XrEventDataSessionStateChanged",
    "XrEventDataReferenceSpaceChangePending",
    "XrViewConfigurationProperties",
    "XrActionStateBoolean",
    "XrActionStateFloat",
    "XrActionStateVector2f",
    "XrActionStatePose",
    "XrActionStateGetInfo",
    "XrHapticActionInfo",
    "XrActionSetCreateInfo",
    "XrActionSuggestedBinding",
    "XrInteractionProfileSuggestedBinding",
    "XrActiveActionSet",
    "XrSessionActionSetsAttachInfo",
    "XrActionsSyncInfo",
    "XrBoundSourcesForActionEnumerateInfo",
    "XrInputSourceLocalizedNameGetInfo",
    "XrEventDataInteractionProfileChanged",
    "XrInteractionProfileState",
    "XrActionCreateInfo",
    "XrDebugUtilsObjectNameInfoEXT",
    "XrDebugUtilsLabelEXT",
    "XrDebugUtilsMessengerCallbackDataEXT",
    "XrDebugUtilsMessengerCreateInfoEXT",
    "XrGraphicsRequirementsD3D11KHR",
]

manually_implemented_commands = [
    "xrCreateSession",
]

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

# We know the opaque objects of these types are returned by the system as pointers that do not need to be deep copied
special_functions = {
    "ID3D11Device": {"ref" : "%(name)s->AddRef()", "unref": "%(name)s->Release()"},
    "ID3D11Texture2D": {"ref" : "%(name)s->AddRef()", "unref": "%(name)s->Release()"},
}

# dump out debugging information on structs
if True:
    out = open("output.txt", "w")

    for name in supported_structs: # structs.keys():
        struct = structs[name]
        s = name + " "
        if struct[1]:
            s += "has type %s " % struct[1]
        if struct[2]:
            s += "extends struct %s " % struct[2]
        out.write(s + "\n")

        for member in struct[3]:
            s = "    (%s) " % (member["type"])

            if member["type"] == "char_array":
                s += "char %s[%s]" % (member["name"], member["size"])
            elif member["type"] == "c_string":
                s += "char *%s" % (member["name"])
            elif member["type"] == "string_list":
                s += "char** %s (size in %s)" % (member["name"], member["size"])
            elif member["type"] == "list_of_struct_pointers":
                s += "%s* const * %s (size in %s)" % (member["struct_type"], member["name"], member["size"])
            elif member["type"] == "pointer_to_struct":
                s += "%s* %s (reg_member.text \"%s\")" % (member["struct_type"], member["name"], member["member_text"])
            elif member["type"] == "pointer_to_atom_or_handle":
                s += "%s* %s (size in %s)" % (member["struct_type"], member["name"], member["size"])
            elif member["type"] == "pointer_to_xr_struct_array":
                s += "%s* %s (size in %s) (reg_member.text \"%s\")" % (member["struct_type"], member["name"], member["size"], member["member_text"])
            elif member["type"] == "pointer_to_struct_array":
                s += "%s* %s (size in %s) (reg_member.text \"%s\")" % (member["struct_type"], member["name"], member["size"], member["member_text"])
            elif member["type"] == "pointer_to_opaque":
                s += "%s* %s" % (member["opaque_type"], member["name"])
            elif member["type"] == "xrstruct_pointer":
                s += "void* %s (XR struct)" % (member["name"])
            elif member["type"] == "void_pointer":
                s += "void* %s" % (member["name"])
            elif member["type"] == "fixed_array":
                s += "%s %s[%s]" % (member["base_type"], member["name"], member["size"])
            elif member["type"] == "POD":
                s += "%s %s" % (member["pod_type"], member["name"])
            else:
                s += "XXX \"%s\" %s" % (member["type"], member["name"])

            if member["is_const"]:
                s += " is const"

            out.write(s + "\n")

    out.close()

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

in_destructor = {}

add_to_handle_struct = {}

add_to_handle_struct["XrInstance"] = {
    "members" : """
        const XrInstanceCreateInfo *createInfo = nullptr;
        std::set<XrDebugUtilsMessengerEXT> debugUtilsMessengers;
""",
}

add_to_handle_struct["XrSession"] = {
    "members" : """
        const XrSessionCreateInfo *createInfo = nullptr;
        XrSession actualHandle;
        bool isOverlaySession = false; // The XrSession is only valid in the Main XrInstance (i.e. in the Main Process)
""",
}

add_to_handle_struct["XrDebugUtilsMessengerEXT"] = {
    "members" : """
        XrDebugUtilsMessengerCreateInfoEXT *createInfo = nullptr;
""",
}

in_destructor["XrDebugUtilsMessengerEXT"] = "    if(createInfo) { FreeXrStructChainWithFree(instance, createInfo); }\n";

add_to_handle_struct["XrActionSet"] = {
    "members" : """
        XrActionSetCreateInfo *createInfo = nullptr;
        std::unordered_map<uint64_t, uint64_t> remoteActionSetByLocalSession;
""",
}

in_destructor["XrActionSet"] = "    if(createInfo) { FreeXrStructChainWithFree(instance, createInfo); }\n";

add_to_handle_struct["XrAction"] = {
    "members" : """
        XrActionCreateInfo *createInfo = nullptr;
        std::unordered_map<uint64_t, uint64_t> remoteActionByLocalSession;
""",
}

in_destructor["XrAction"] = "    if(createInfo) { FreeXrStructChainWithFree(instance, createInfo); }\n";

add_to_handle_struct["XrSpace"] = {
    "members" : """
        bool isReferenceSpace;
        XrReferenceSpaceCreateInfo *referenceSpaceCreateInfo = nullptr;
        XrActionSpaceCreateInfo *actionSpaceCreateInfo = nullptr;
""",
}

in_destructor["XrSpace"] = "    if(referenceSpaceCreateInfo) { FreeXrStructChainWithFree(instance, referenceSpaceCreateInfo); }\n";
in_destructor["XrSpace"] += "    if(actionSpaceCreateInfo) { FreeXrStructChainWithFree(instance, actionSpaceCreateInfo); }\n";

after_downchain["xrCreateDebugUtilsMessengerEXT"] = """
    mlock.lock();
    gOverlaysLayerXrInstanceToHandleInfo[instance]->debugUtilsMessengers.insert(*messenger);
    mlock.unlock();
    OverlaysLayerXrDebugUtilsMessengerEXTHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrDebugUtilsMessengerEXTHandleInfo>(instance, instance, instanceInfo->downchain);
    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrDebugUtilsMessengerEXTToHandleInfoMutex);
    gOverlaysLayerXrDebugUtilsMessengerEXTToHandleInfo[*messenger] = info;
    info->createInfo = reinterpret_cast<XrDebugUtilsMessengerCreateInfoEXT*>(CopyXrStructChainWithMalloc(instance, createInfo));
    mlock2.unlock();
"""

after_downchain["xrCreateActionSet"] = """
    OverlaysLayerXrActionSetHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrActionSetHandleInfo>(instance, instance, instanceInfo->downchain);
    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrActionSetToHandleInfoMutex);
    gOverlaysLayerXrActionSetToHandleInfo[*actionSet] = info;
    info->createInfo = reinterpret_cast<XrActionSetCreateInfo*>(CopyXrStructChainWithMalloc(instance, createInfo));
    mlock2.unlock();

"""
after_downchain["xrCreateAction"] = """
    OverlaysLayerXrActionHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrActionHandleInfo>(actionSet, actionSetInfo->parentInstance, actionSetInfo->downchain);
    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrActionToHandleInfoMutex);
    gOverlaysLayerXrActionToHandleInfo[*action] = info;
    info->createInfo = reinterpret_cast<XrActionCreateInfo*>(CopyXrStructChainWithMalloc(actionSetInfo->parentInstance, createInfo));
    mlock2.unlock();

"""

after_downchain["xrStringToPath"] = """
    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerPathToAtomInfoMutex);
    gOverlaysLayerPathToAtomInfo[*path] = std::make_shared<OverlaysLayerPathAtomInfo>(pathString);
    mlock2.unlock();

"""

after_downchain["xrGetSystem"] = """
    XrSystemGetInfo* getInfoCopy = reinterpret_cast<XrSystemGetInfo*>(CopyXrStructChainWithMalloc(instance, getInfo));
    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerSystemIdToAtomInfoMutex);
    gOverlaysLayerSystemIdToAtomInfo[*systemId] = std::make_shared<OverlaysLayerSystemIdAtomInfo>(getInfoCopy);
    mlock2.unlock();
"""

after_downchain["xrSuggestInteractionProfileBindings"] = """
    auto search = gPathToSuggestedInteractionProfileBinding.find(suggestedBindings->interactionProfile);
    if(search != gPathToSuggestedInteractionProfileBinding.end()) {
        FreeXrStructChainWithFree(instance, search->second);
    }
    // XXX this needs to be by profile, not the whole thing
    gPathToSuggestedInteractionProfileBinding[suggestedBindings->interactionProfile] = reinterpret_cast<XrInteractionProfileSuggestedBinding*>(CopyXrStructChainWithMalloc(instance, suggestedBindings));

"""

after_downchain["xrCreateReferenceSpace"] = """
    OverlaysLayerXrSpaceHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrSpaceHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);
    info->referenceSpaceCreateInfo = reinterpret_cast<XrReferenceSpaceCreateInfo*>(CopyXrStructChainWithMalloc(sessionInfo->parentInstance, createInfo));
    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrSpaceToHandleInfoMutex);
    gOverlaysLayerXrSpaceToHandleInfo[*space] = info;
    mlock2.unlock();
"""

after_downchain["xrCreateActionSpace"] = """
    OverlaysLayerXrSpaceHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrSpaceHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);
    info->actionSpaceCreateInfo = reinterpret_cast<XrActionSpaceCreateInfo*>(CopyXrStructChainWithMalloc(sessionInfo->parentInstance, createInfo));
    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrSpaceToHandleInfoMutex);
    gOverlaysLayerXrSpaceToHandleInfo[*space] = info;
    mlock2.unlock();
"""

after_downchain["xrPollEvent"] = """
    // An XrSession in an event must be "valid", which means it cannot have
    // been destroyed prior, so implies runtimes never give us back
    // an event that contains a destroyed session.  So we will be able to find the local handle.
    // Let validation catch bad behavior.
    if(eventData->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
        auto p = reinterpret_cast<XrEventDataSessionStateChanged*>(eventData);
        std::unique_lock<std::recursive_mutex> lock(gActualSessionToLocalHandleMutex);
        p->session = gActualSessionToLocalHandle[p->session];
    } else if(eventData->type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
        auto p = reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(eventData);
        std::unique_lock<std::recursive_mutex> lock(gActualSessionToLocalHandleMutex);
        p->session = gActualSessionToLocalHandle[p->session];
    } else if(eventData->type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {
        auto p = reinterpret_cast<XrEventDataInteractionProfileChanged*>(eventData);
        std::unique_lock<std::recursive_mutex> lock(gActualSessionToLocalHandleMutex);
        p->session = gActualSessionToLocalHandle[p->session];
    }
"""

# store preambles of generated header and source -----------------------------


source_text = """
#include "xr_generated_overlays.hpp"
#include "hex_and_handles.h"

#include <cstring>
#include <sstream>
#include <iomanip>
#include <unordered_map>

std::unordered_map<XrPath, XrInteractionProfileSuggestedBinding*> gPathToSuggestedInteractionProfileBinding;

"""

header_text = """
// #include "api_layer_platform_defines.h"
#include "xr_generated_dispatch_table.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <mutex>
#include <string>
#include <memory>
#include <tuple>
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
    typedef std::shared_ptr<OverlaysLayerPathAtomInfo> Ptr;
};

"""

header_text += f"extern std::unordered_map<XrPath, {LayerName}PathAtomInfo::Ptr> g{LayerName}PathToAtomInfo;\n\n"
source_text += f"std::unordered_map<XrPath, {LayerName}PathAtomInfo::Ptr> g{LayerName}PathToAtomInfo;\n\n"
header_text += f"extern std::recursive_mutex g{LayerName}PathToAtomInfoMutex;\n"
source_text += f"std::recursive_mutex g{LayerName}PathToAtomInfoMutex;\n"


# XrSystemId

header_text += f"struct {LayerName}SystemIdAtomInfo\n"
header_text += """
{
    const XrSystemGetInfo *getInfo;
"""
header_text += f"    {LayerName}SystemIdAtomInfo(const XrSystemGetInfo *copyOfGetInfo)\n"
header_text += """
    : getInfo(copyOfGetInfo)
    {}
    // map of remote XrSystemId by XrSession
    typedef std::shared_ptr<OverlaysLayerSystemIdAtomInfo> Ptr;
};

"""

header_text += f"extern std::unordered_map<XrSystemId, {LayerName}SystemIdAtomInfo::Ptr> g{LayerName}SystemIdToAtomInfo;\n\n"
source_text += f"std::unordered_map<XrSystemId, {LayerName}SystemIdAtomInfo::Ptr> g{LayerName}SystemIdToAtomInfo;\n\n"
header_text += f"extern std::recursive_mutex g{LayerName}SystemIdToAtomInfoMutex;\n"
source_text += f"std::recursive_mutex g{LayerName}SystemIdToAtomInfoMutex;\n"


only_child_handles = [h for h in supported_handles if h != "XrInstance"]

for handle_type in supported_handles:
    header_text += f"struct {LayerName}{handle_type}HandleInfo\n"
    header_text += "{\n"
    # XXX header_text += "        std::set<HandleTypePair> childHandles;\n"
    parent_type = str(handles[handle_type][1] or "")
    if parent_type:
        header_text += f"        {parent_type} parentHandle;\n"
        header_text += "        XrInstance parentInstance;\n"
    header_text += "        XrGeneratedDispatchTable *downchain;\n"
    header_text += "        bool invalid = false;\n"
    header_text += add_to_handle_struct.get(handle_type, {}).get("members", "")
    if parent_type:
        header_text += f"        {LayerName}{handle_type}HandleInfo({parent_type} parent, XrInstance parentInstance_, XrGeneratedDispatchTable *downchain_) : \n"
        header_text += "            parentHandle(parent),\n"
        header_text += "            parentInstance(parentInstance_),\n"
        header_text += "            downchain(downchain_)\n"
        header_text += "        {}\n"
        header_text += f"        ~{LayerName}{handle_type}HandleInfo()\n"
        header_text += "        {\n"
        header_text += in_destructor.get(command_name, "")
        header_text += "        }\n"
    else:
        # we know this is XrInstance...
        header_text += f"        {LayerName}{handle_type}HandleInfo(XrGeneratedDispatchTable *downchain_): \n"
        header_text += "            downchain(downchain_)\n"
        header_text += "        {}\n"
        header_text += f"        ~{LayerName}{handle_type}HandleInfo()\n"
        header_text += "        {\n"
        header_text += in_destructor.get(command_name, "")
        header_text += "            delete downchain;\n"
        header_text += "        }\n"
    header_text += f"        void Destroy() /* For OpenXR's intents, not a dtor */\n"
    header_text += "        {\n"
    header_text += "            invalid = true;\n"
    # XXX header_text += "            ;\n" delete all child handles, and how will we do that?
    header_text += "        }\n"
    header_text += f"        typedef std::shared_ptr<{LayerName}{handle_type}HandleInfo> Ptr;\n"
    header_text += "};\n"

    source_text += f"std::unordered_map<{handle_type}, {LayerName}{handle_type}HandleInfo::Ptr> g{LayerName}{handle_type}ToHandleInfo;\n"
    header_text += f"extern std::unordered_map<{handle_type}, {LayerName}{handle_type}HandleInfo::Ptr> g{LayerName}{handle_type}ToHandleInfo;\n"

    source_text += f"std::recursive_mutex g{LayerName}{handle_type}ToHandleInfoMutex;\n"
    header_text += f"extern std::recursive_mutex g{LayerName}{handle_type}ToHandleInfoMutex;\n"

    source_text += "\n"


# deep copy and free functions -----------------------------------------------

xr_typed_structs = [name for name in supported_structs if structs[name][1]]

copy_function_case_bodies = ""

free_function_case_bodies = ""

for name in xr_typed_structs:
    struct = structs[name]

    copy_function = """
bool CopyXrStructChain(XrInstance instance, const %(name)s* src, %(name)s *dst, CopyType copyType, AllocateFunc alloc, std::function<void (void* pointerToPointer)> addOffsetToPointer)
{
""" % {"name" : struct[0]}

    free_function = """
void FreeXrStructChain(XrInstance instance, const %(name)s* p, FreeFunc freefunc)
{
""" % {"name" : struct[0]}

    for member in struct[3]:

        if member["type"] == "char_array":

            copy_function += "    memcpy(dst->%(name)s, src->%(name)s, %(size)s);\n" % member

        elif member["type"] == "c_string":

            copy_function += "    char *%(name)s = (char *)alloc(strlen(src->%(name)s) + 1);\n" % member
            copy_function += "    memcpy(%(name)s, src->%(name)s, strlen(src->%(name)s + 1));\n" % member
            copy_function += "    dst->%(name)s = %(name)s;\n" % member
            free_function += "    freefunc(p->%(name)s);\n" % member

        elif member["type"] == "string_list":

            copy_function += """    
    // array of pointers to C strings for %(name)s
    auto %(name)s = (char **)alloc(sizeof(char *) * src->%(size)s);
    dst->%(name)s = %(name)s;
    addOffsetToPointer(&dst->%(name)s);
    for(uint32_t i = 0; i < dst->%(size)s; i++) {
        %(name)s[i] = (char *)alloc(strlen(src->%(name)s[i]) + 1);
        strncpy_s(%(name)s[i], strlen(src->%(name)s[i]) + 1, src->%(name)s[i], strlen(src->%(name)s[i]) + 1);
        addOffsetToPointer(&%(name)s[i]);
    }

""" % member

            free_function += """    
    // array of pointers to C strings for %(name)s
    for(uint32_t i = 0; i < p->%(size)s; i++) {
        freefunc(p->%(name)s[i]);
    }
    freefunc(p->%(name)s);

""" % member

        elif member["type"] == "list_of_struct_pointers":

            copy_function += """    
    // array of pointers to XR structs for %(name)s
    auto %(name)s = (%(struct_type)s **)alloc(sizeof(%(struct_type)s) * src->%(size)s);
    dst->%(name)s = %(name)s;
    addOffsetToPointer(&dst->%(name)s);
    for(uint32_t i = 0; i < dst->%(size)s; i++) {
        %(name)s[i] = reinterpret_cast<%(struct_type)s *>(CopyXrStructChain(instance, reinterpret_cast<const XrBaseInStructure*>(src->%(name)s[i]), copyType, alloc, addOffsetToPointer));
        addOffsetToPointer(&%(name)s[i]);
    }

""" % member

            free_function += """    
    // array of pointers to XR structs for %(name)s
    for(uint32_t i = 0; i < p->%(size)s; i++) {
        freefunc(p->%(name)s);
    }
    freefunc(p->%(name)s);

""" % member

        elif member["type"] == "pointer_to_struct":

            if member["struct_type"] in special_functions:
                copy_function += "    dst->%(name)s = src->%(name)s;\n" % member
                copy = special_functions[member["struct_type"]]["ref"]
                copy_function += "    %s;\n" % (copy % {"name" : ("src->%(name)s" % member)})
                free = special_functions[member["struct_type"]]["unref"]
                free_function += "    %s;\n" % (free % {"name" : ("p->%(name)s" % member)})
            else:
                copy_function += "    // XXX struct %(struct_type)s* %(name)s (reg_member.text \"%(member_text)s\")\n" % member
                free_function += "    // XXX struct %(struct_type)s* %(name)s (reg_member.text \"%(member_text)s\")\n" % member

        elif member["type"] == "pointer_to_atom_or_handle":

            copy_function += "    %(struct_type)s *%(name)s = reinterpret_cast<%(struct_type)s*>(alloc(sizeof(%(struct_type)s) * src->%(size)s));\n" % member
            copy_function += "    memcpy(%(name)s, src->%(name)s, sizeof(%(struct_type)s) * src->%(size)s);\n" % member
            copy_function += "    dst->%(name)s = %(name)s;\n" % member
            copy_function += "    addOffsetToPointer(&dst->%(name)s);\n" % member
            free_function += "    freefunc(&p->%(name)s);\n" % member

        elif member["type"] == "pointer_to_xr_struct_array":

            copy_function += """
    // Lay down %(name)s...
    auto %(name)s = (%(struct_type)s*)alloc(sizeof(%(struct_type)s) * src->%(size)s);
    dst->%(name)s = %(name)s;
    addOffsetToPointer(&dst->%(name)s);
    for(uint32_t i = 0; i < dst->%(size)s; i++) {
        bool result = CopyXrStructChain(instance, &src->%(name)s[i], &%(name)s[i], copyType, alloc, addOffsetToPointer);
        if(!result) {
            return result;
        }
    }
""" % member

            free_function += """
    freefunc(p->%(name)s);
""" % member
        elif member["type"] == "pointer_to_struct_array":
            copy_function += "    %(struct_type)s *%(name)s = reinterpret_cast<%(struct_type)s*>(alloc(sizeof(%(struct_type)s) * src->%(size)s));\n" % member
            copy_function += "    memcpy(%(name)s, src->%(name)s, sizeof(%(struct_type)s) * src->%(size)s);\n" % member
            copy_function += "    dst->%(name)s = %(name)s;\n" % member
            free_function += "    freefunc(p->%(name)s);\n" % member
        elif member["type"] == "pointer_to_opaque":
            copy_function += "    // XXX opaque %s* %s\n" % (member["opaque_type"], member["name"])
            free_function += "    // XXX opaque %s* %s\n" % (member["opaque_type"], member["name"])
        elif member["type"] == "void_pointer":
            if member["name"] == "next":
                pass
            else:
                copy_function += "    dst->%(name)s = src->%(name)s; // We only know this is void*, so we can only copy.\n" % member
        elif member["type"] == "fixed_array":
            copy_function += "    memcpy(dst->%(name)s, src->%(name)s, sizeof(%(base_type)s) * %(size)s);\n" % member
        elif member["type"] == "POD":
            copy_function += "    dst->%(name)s = src->%(name)s;\n" % member
        else:
            copy_function += "    // XXX XXX \"%s\" %s\n" % (member["type"], member["name"])
            free_function += "    // XXX XXX \"%s\" %s\n" % (member["type"], member["name"])

#     if member["is_const"]:
#         copy_function += " is const"


    copy_function += "    dst->next = reinterpret_cast<XrBaseInStructure*>(CopyXrStructChain(instance, reinterpret_cast<const XrBaseInStructure*>(src->next), copyType, alloc, addOffsetToPointer));\n"
    copy_function += "    addOffsetToPointer(&dst->next);\n"
    copy_function += "    return true;\n"
    copy_function += "}\n\n"

    free_function += "    FreeXrStructChain(instance, reinterpret_cast<const XrBaseInStructure*>(p->next), freefunc);\n"
    free_function += "}\n\n"

    source_text += copy_function
    source_text += free_function

    copy_function_case_bodies += """
            case %(enum)s: {
                auto src = reinterpret_cast<const %(name)s*>(srcbase);
                auto dst = reinterpret_cast<%(name)s*>(alloc(sizeof(%(name)s)));
                dstbase = reinterpret_cast<XrBaseInStructure*>(dst);
                bool result = CopyXrStructChain(instance, src, dst, copyType, alloc, addOffsetToPointer);
                if(!result) {
                    return nullptr;
                }
                break;
            }
""" % {"name" : name, "enum" : struct[1]}

    free_function_case_bodies += """
            case %(enum)s: {
                FreeXrStructChain(instance, reinterpret_cast<const %(name)s*>(p), freefunc);
                break;
            }
""" % {"name" : name, "enum" : struct[1]}


source_text += """
XrBaseInStructure *CopyXrStructChain(XrInstance instance, const XrBaseInStructure* srcbase, CopyType copyType, AllocateFunc alloc, std::function<void (void* pointerToPointer)> addOffsetToPointer)
{
    XrBaseInStructure *dstbase = nullptr;
    bool skipped;

    do {
        skipped = false;

        if(!srcbase) {
            return nullptr;
        }

        // next pointer of struct is copied after switch statement

        switch(srcbase->type) {
"""
source_text += copy_function_case_bodies

source_text += """

            default: {
                // I don't know what this is, skip it and try the next one
                std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
                OverlaysLayerXrInstanceHandleInfo::Ptr info = gOverlaysLayerXrInstanceToHandleInfo.at(instance);
                mlock.unlock();
                char structTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
                structTypeName[0] = '\\0';
                if(info->downchain->StructureTypeToString(instance, srcbase->type, structTypeName) != XR_SUCCESS) {
                    sprintf(structTypeName, "<type %08X>", srcbase->type);
                }
                OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                         nullptr, OverlaysLayerNoObjectInfo, fmt("CopyXrStructChain called on %p of unhandled type %s - dropped from \\"next\\" chain.", srcbase, structTypeName).c_str());
                srcbase = srcbase->next;
                skipped = true;
                break;
            }
        }
    } while(skipped);

    return dstbase;
}
"""

source_text += """
void FreeXrStructChain(XrInstance instance, const XrBaseInStructure* p, FreeFunc freefunc)
{
    if(!p) {
        return;
    }

    switch(p->type) {

"""

source_text += free_function_case_bodies

source_text += """

        default: {
            // I don't know what this is, skip it and try the next one
            std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
            OverlaysLayerXrInstanceHandleInfo::Ptr info = gOverlaysLayerXrInstanceToHandleInfo.at(instance);
            mlock.unlock();
            char structTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
            structTypeName[0] = '\\0';
            if(info->downchain->StructureTypeToString(instance, p->type, structTypeName) != XR_SUCCESS) {
                sprintf(structTypeName, "<type %08X>", p->type);
            }
            OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                 nullptr, OverlaysLayerNoObjectInfo, fmt("Warning: Free called on %p of unknown type %d - will descend \\"next\\" but don't know any other pointers.\\n", p, structTypeName).c_str());
            break;
        }
    }

    freefunc(p);
}

XrBaseInStructure* CopyEventChainIntoBuffer(XrInstance instance, const XrEventDataBaseHeader* eventData, XrEventDataBuffer* buffer)
{
    size_t remaining = sizeof(XrEventDataBuffer);
    unsigned char* next = reinterpret_cast<unsigned char *>(buffer);
    return CopyXrStructChain(instance, reinterpret_cast<const XrBaseInStructure*>(eventData), COPY_EVERYTHING,
            [&buffer,&remaining,&next](size_t s){unsigned char* cur = next; next += s; return cur; },
            [](void *){ });
}

XrBaseInStructure* CopyXrStructChainWithMalloc(XrInstance instance, const void* xrstruct)
{
    return CopyXrStructChain(instance, reinterpret_cast<const XrBaseInStructure*>(xrstruct), COPY_EVERYTHING,
            [](size_t s){return malloc(s); },
            [](void *){ });
}

void FreeXrStructChainWithFree(XrInstance instance, const void* xrstruct)
{
    FreeXrStructChain(instance, reinterpret_cast<const XrBaseInStructure*>(xrstruct),
            [](const void *p){free(const_cast<void*>(p));});
}
"""


# make functions returned by xrGetInstanceProcAddr ---------------------------

handles_needing_substitution = ['XrSession'] # ['XrSwapchain', 'XrSpace']

for command_name in [c for c in supported_commands if c not in manually_implemented_commands]:
    command = commands[command_name]

    parameters = ", ".join([parameter_to_cdecl(command_name, parameter) for parameter in command["parameters"]])
    command_type = command["return_type"]
    layer_command = api_layer_name_for_command(command_name)
    source_text += f"{command_type} {layer_command}({parameters})\n"
    source_text += "{\n"

    handle_type = command["parameters"][0].find("type").text
    handle_name = command["parameters"][0].find("name").text

    # handles are guaranteed not to be destroyed while in another command according to the spec...
    source_text += f"    std::unique_lock<std::recursive_mutex> mlock(g{LayerName}{handle_type}ToHandleInfoMutex);\n"
    source_text += f"    {LayerName}{handle_type}HandleInfo::Ptr {handle_name}Info = g{LayerName}{handle_type}ToHandleInfo.at({handle_name});\n\n"

    if handle_type in handles_needing_substitution:
        source_text += f"    // substitute the real handle in\n";
        source_text += f"    {handle_type} localHandleStore = {handle_name};\n"
        source_text += f"    {handle_name} = {handle_name}Info->actualHandle;\n\n"

    source_text += f"    mlock.unlock();\n\n"

    source_text += before_downchain.get(command_name, "")

    parameter_names = ", ".join([parameter_to_name(command_name, parameter) for parameter in command["parameters"]])
    dispatch_command = dispatch_name_for_command(command_name)
    source_text += f"    XrResult result = {handle_name}Info->downchain->{dispatch_command}({parameter_names});\n\n"

    if handle_type in handles_needing_substitution:
        source_text += f"    // put the local handle back\n";
        source_text += f"    {handle_name} = localHandleStore;\n"

    if command_name in after_downchain:

        source_text += after_downchain.get(command_name, "")

    else:

        # Attempt generation of special cases

        # Special Case Create functions
        if command_name.find("Create") >= 0:
            created_type = command["parameters"][-1].find("type").text
            created_name = command["parameters"][-1].find("name").text
            # just assume we hand-coded Instance creation so these are all child handle types
            # in C++17 this would be an initializer list but before that we have to use the forwarding syntax.
            parent_type = str(handles[created_type][1] or "")
            source_text += f"    std::unique_lock<std::recursive_mutex> mlock2(g{LayerName}{created_type}ToHandleInfoMutex);\n"
            if parent_type == "XrInstance":
                source_text += f"    {LayerName}{created_type}HandleInfo::Ptr {created_name}Info = std::make_shared<{LayerName}{created_type}HandleInfo>({handle_name}, instance, {handle_name}Info->downchain);\n"
                source_text += f"    g{LayerName}{created_type}ToHandleInfo[*{created_name}] = {created_name}Info;\n"
            else:
                source_text += f"    {LayerName}{created_type}HandleInfo::Ptr {created_name}Info = std::make_shared<{LayerName}{created_type}HandleInfo>({handle_name}, {handle_name}Info->parentInstance, {handle_name}Info->downchain);\n"
                source_text += f"    g{LayerName}{created_type}ToHandleInfo[*{created_name}] = {created_name}Info;\n"
            # XXX source_text += f"    {handle_name}Info.childHandles.insert(*{created_name});\n"
            source_text += f"    mlock2.unlock();\n"


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
