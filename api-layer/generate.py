import sys
import re
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
    return layer_name + command[2:]

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

layer_name = "OverlaysLayer"

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
    # if it's an alias (e.g. xrGetVulkanGraphicsRequirements2KHR), skip it for now
    if not "alias" in reg_command.keys():
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

    if reg_type.attrib.get("category", "") == "struct" and not "alias" in reg_type.keys():
        struct_name = reg_type.attrib["name"]
        print("%s" % struct_name)
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

                is_an_xr_type = (type_text[0:2] == 'Xr')

                if is_an_xr_type and type_text in structs:
                    member["type"] = "xr_simple_struct"
                    member["struct_type"] = type_text
                else:
                    member["type"] = "POD"
                    member["pod_type"] = type_text

            elif type_tail == "*" and not name_tail : # and reg_member_text == "const struct" or reg_member_text == "struct":
                size_and_len = reg_member.attrib.get("len", "").strip()
                if size_and_len:
                    if size_and_len == "null-terminated":
                        member["type"] = "c_string"
                    else:
                        if type_text in atoms or type_text in handles:
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
                sys.exit(1)

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
    # "XrSwapchainImageBaseHeader",
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
    # "XrCompositionLayerBaseHeader",
    "XrCompositionLayerProjectionView",
    "XrCompositionLayerProjection",
    "XrCompositionLayerQuad",
    "XrFrameBeginInfo",
    "XrFrameEndInfo",
    "XrFrameWaitInfo",
    "XrFrameState",
    # "XrHapticBaseHeader",
    "XrHapticVibration",
    # "XrEventDataBaseHeader",
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
    "XrSessionCreateInfoOverlayEXTX",
    "XrEventDataMainSessionVisibilityChangedEXTX",
]

manually_implemented_commands = [
    "xrApplyHapticFeedback",
    "xrStopHapticFeedback",
    "xrLocateSpace",
    "xrCreateSession",
    "xrPollEvent",
    "xrEndFrame",
    "xrCreateActionSet",
    "xrCreateAction",
    "xrCreateActionSpace",
    "xrGetActionStateBoolean",
    "xrGetActionStateFloat",
    "xrGetActionStateVector2f",
    "xrGetActionStatePose",
    "xrSyncActions",
    "xrSuggestInteractionProfileBindings",
    "xrAttachSessionActionSets",
    "xrGetCurrentInteractionProfile",
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
    "xrGetD3D11GraphicsRequirementsKHR",
    "xrSetDebugUtilsObjectNameEXT",
    "xrCreateDebugUtilsMessengerEXT",
    "xrDestroyDebugUtilsMessengerEXT",
    "xrSubmitDebugUtilsMessageEXT",
    "xrSessionBeginDebugUtilsLabelRegionEXT",
    "xrSessionEndDebugUtilsLabelRegionEXT",
    "xrSessionInsertDebugUtilsLabelEXT",
    "xrApplyHapticFeedback",
    "xrStopHapticFeedback",
]

supported_handles = [
    "XrAction",
    "XrActionSet",
    "XrSwapchain",
    "XrSpace",
    "XrSession",
    "XrDebugUtilsMessengerEXT",
    "XrInstance",
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
            elif member["type"] == "xr_struct_pointer":
                s += "void* %s (XR struct)" % (member["name"])
            elif member["type"] == "void_pointer":
                s += "void* %s" % (member["name"])
            elif member["type"] == "fixed_array":
                s += "%s %s[%s]" % (member["base_type"], member["name"], member["size"])
            elif member["type"] == "POD":
                s += "%s %s" % (member["pod_type"], member["name"])
            elif member["type"] == "xr_simple_struct":
                s += "%s %s" % (member["struct_type"], member["name"])
            else:
                s += "XXX \"%s\" %s" % (member["type"], member["name"])

            if member["is_const"]:
                s += " is const"

            out.write(s + "\n")

    out.close()


# dump debugging information
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

def is_an_xr_type(type) :
    return (parameter_type[0:2] == 'Xr')

def is_an_xr_struct(type) :
    return is_an_xr_type and (type in structs)

def is_an_xr_chained_struct(type) :
    # argh baseheader!!
    return (is_an_xr_struct and structs[type][2]) or (type in "XrSwapchainImageBaseHeader")

def is_an_xr_handle(type) :
    return is_an_xr_type and (type in handles)


# store preambles of generated header and source -----------------------------


# only invoked if downchain returned XR_SUCCEEDED(result)
before_downchain = {}

after_downchain_main = {}

in_destructor = {}

in_constructor = {}

in_destroy = {}

add_to_handle_struct = {}


# XrInstance

add_to_handle_struct["XrInstance"] = {
    "members" : """
    const XrInstanceCreateInfo *createInfo = nullptr;
    std::set<XrDebugUtilsMessengerEXT> debugUtilsMessengers;
    std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>> profilesToBindings;
    std::set<OverlaysLayerXrActionSetHandleInfo::Ptr> childActionSets;
    std::set<OverlaysLayerXrSessionHandleInfo::Ptr> childSessions;
    std::set<OverlaysLayerXrDebugUtilsMessengerEXTHandleInfo::Ptr> childDebugUtilsMessengerEXTs;
    std::unordered_map<WellKnownStringIndex, XrPath> OverlaysLayerWellKnownStringToPath;
    std::unordered_map<XrPath, WellKnownStringIndex> OverlaysLayerPathToWellKnownString;
    std::unordered_map<XrPath, XrPath> OverlaysLayerBindingToSubaction;
    std::set<XrPath> OverlaysLayerAllSubactionPaths;

""",
}


# XrSession

add_to_handle_struct["XrSession"] = {
    "members" : """
    ID3D11Device*   d3d11Device;
    XrSession localHandle;
    const XrSessionCreateInfo *createInfo = nullptr;
    std::set<OverlaysLayerXrSwapchainHandleInfo::Ptr> childSwapchains;
    std::set<OverlaysLayerXrSpaceHandleInfo::Ptr> childSpaces;
    XrActionSet placeholderActionSet;
    std::unordered_map<XrAction, std::string> placeholderActionNames;
    std::unordered_map<XrPath, std::pair<XrAction, XrActionType>> placeholderActions;
    std::map<std::pair<XrPath /* interaction profile */, XrPath /* full binding */>, std::pair<XrAction, XrActionType>> placeholderActionsByProfileAndFullBinding;
    std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>> bindingsByProfile;
    std::unordered_map<XrAction, XrPath> bindingsByAction;
    std::vector<XrActiveActionSet> lastSyncedActiveActionSets;
    bool actionSetsWereAttached = false;
    std::set<XrPath> interactionProfiles;
    std::unordered_map<XrPath,XrPath> currentInteractionProfileBySubactionPath;
    bool interactionProfileChangePending = false;
""",
}

in_destroy["XrSession"] = """
    childSwapchains.clear();
    childSpaces.clear();
"""

after_downchain_main["xrBeginSession"] = """
    auto mainSession = gMainSessionContext;
    if(mainSession) {
        auto l = mainSession->GetLock();
        mainSession->sessionState.DoCommand(OpenXRCommand::BEGIN_SESSION);
    }
"""

after_downchain_main["xrDestroySession"] = """
    // XXX tell overlay app that session was lost

"""

after_downchain_main["xrEndSession"] = """
    auto mainSession = gMainSessionContext;
    if(mainSession) {
        auto l = mainSession->GetLock();
        mainSession->sessionState.DoCommand(OpenXRCommand::END_SESSION);
    }
"""

after_downchain_main["xrRequestExitSession"] = """
    auto mainSession = gMainSessionContext;
    if(mainSession) {
        auto l = mainSession->GetLock();
        mainSession->sessionState.DoCommand(OpenXRCommand::REQUEST_EXIT_SESSION);
    }
"""

# XrSwapchain

add_to_handle_struct["XrSwapchain"] = {
    "members" : """
    OverlaySwapchain::Ptr overlaySwapchain;             // Swapchain data on Overlay side
    SwapchainCachedData::Ptr mainAsOverlaySwapchain;   // Swapchain data on Main side
    XrSwapchain localHandle;
""",
}

after_downchain_main["xrCreateSwapchain"] = """
    auto info = OverlaysLayerGetHandleInfoFromXrSwapchain(*swapchain));
    info->localHandle = *swapchain;
"""

after_downchain_main["xrCreateSwapchain"] = """
    sessionInfo->childSwapchains.insert(OverlaysLayerGetHandleInfoFromXrSwapchain(*swapchain));
"""

after_downchain_main["xrWaitFrame"] = """
    auto mainSession = gMainSessionContext;
    XrInstance instance = sessionInfo->parentInstance;
    if(mainSession) {
        auto l = mainSession->GetLock();

        std::shared_ptr<XrFrameState> savedFrameState(reinterpret_cast<XrFrameState*>(CopyXrStructChainWithMalloc(instance, frameState)), [instance](XrFrameState*p){ FreeXrStructChainWithFree(instance, p);});
        mainSession->sessionState.savedFrameState = savedFrameState;

        mainSession->sessionState.DoCommand(OpenXRCommand::WAIT_FRAME);

        std::unique_lock<std::recursive_mutex> lock(gConnectionsToOverlayByProcessIdMutex);
        if(!gConnectionsToOverlayByProcessId.empty()) {
            for(auto& overlayconn: gConnectionsToOverlayByProcessId) {
                auto conn = overlayconn.second;
                auto lock = conn->GetLock();
                if(conn->ctx) {
                    // XXX if this overlay app's WaitFrameMainAsOverlay is waiting on WaitFrameMain, release it.
                }
            }
        }
}
"""

# XrDebugUtilsMessenger

add_to_handle_struct["XrDebugUtilsMessengerEXT"] = {
    "members" : """
    XrDebugUtilsMessengerEXT handle;
    XrDebugUtilsMessengerCreateInfoEXT *createInfo = nullptr;
""",
}

in_destructor["XrDebugUtilsMessengerEXT"] = "    if(createInfo) { FreeXrStructChainWithFree(parentInstance, createInfo); }\n"

after_downchain_main["xrCreateDebugUtilsMessengerEXT"] = f"""
    gOverlaysLayerXrInstanceToHandleInfo.at(instance)->debugUtilsMessengers.insert(*messenger);
    OverlaysLayerXrDebugUtilsMessengerEXTHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrDebugUtilsMessengerEXTHandleInfo>(instance, instance, instanceInfo->downchain);
    info->createInfo = reinterpret_cast<XrDebugUtilsMessengerCreateInfoEXT*>(CopyXrStructChainWithMalloc(instance, createInfo));
    info->handle = *messenger; // XXX should be part of autogenerated ctor
    OverlaysLayerAddHandleInfoForXrDebugUtilsMessengerEXT(*messenger, info);
"""


# XrActionSet

add_to_handle_struct["XrActionSet"] = {
    "members" : """
    XrActionSet handle;
    XrActionSetCreateInfo *createInfo = nullptr;
    ActionBindLocation bindLocation = BIND_PENDING;
    std::set<OverlaysLayerXrActionHandleInfo::Ptr> childActions;
""",
}

in_destructor["XrActionSet"] = "    if(createInfo) { FreeXrStructChainWithFree(parentInstance, createInfo); }\n"

# left here as breadcrumbs - CreateActionSet is completely hand-written
if False:
    after_downchain_main["xrCreateActionSet"] = f"""
    OverlaysLayerXrActionSetHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrActionSetHandleInfo>(instance, instance, instanceInfo->downchain);
    info->createInfo = reinterpret_cast<XrActionSetCreateInfo*>(CopyXrStructChainWithMalloc(instance, createInfo));
    OverlaysLayerAddHandleInfoForXrActionSet(*actionSet, info);
"""


# XrAction
add_to_handle_struct["XrAction"] = {
    "members" : """
    XrAction handle;
    XrActionCreateInfo *createInfo = nullptr;
    ActionBindLocation bindLocation = BIND_PENDING;
    std::set<XrPath> subactionPaths;
    std::set<XrPath> suggestedBindings;
    std::unordered_map<XrPath /* interaction Profile */, std::set<XrPath>> suggestedBindingsByProfile;
    std::unordered_map<XrPath, ActionStateUnion> stateBySubactionPath;
""",
}

# left here as breadcrumbs - CreateAction is completely hand-written
if False:
    after_downchain_main["xrCreateAction"] = f"""
    OverlaysLayerXrActionHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrActionHandleInfo>(actionSet, actionSetInfo->parentInstance, actionSetInfo->downchain);
    info->createInfo = reinterpret_cast<XrActionCreateInfo*>(CopyXrStructChainWithMalloc(actionSetInfo->parentInstance, createInfo));
    info->subactionPaths.insert(info->createInfo->subactionPaths, info->createInfo->subactionPaths + info->createInfo->countSubactionPaths);
    OverlaysLayerAddHandleInfoForXrAction(*action, info);

"""

in_destructor["XrAction"] = "    if(createInfo) { FreeXrStructChainWithFree(parentInstance, createInfo); }\n"


# XrSpace

add_to_handle_struct["XrSpace"] = {
    "members" : """
    XrSpace localHandle;
    SpaceType spaceType;
    OverlaysLayerXrActionHandleInfo::Ptr action;
    XrAction placeholderAction;
    std::shared_ptr<const XrActionSpaceCreateInfo> actionSpaceCreateInfo;
    XrPath createdWithInteractionProfile = XR_NULL_PATH;
""",
}

after_downchain_main["xrCreateReferenceSpace"] = """
    auto info = OverlaysLayerGetHandleInfoFromXrSpace(*space);
    sessionInfo->childSpaces.insert(info);
    info->localHandle = *space;
"""

# left here as breadcrumbs - CreateActionSpace is completely hand-written because of proxying complexity related to XrAction
if False:
    after_downchain_main["xrCreateActionSpace"] = """
    {
        std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrSpaceToHandleInfoMutex);
        gOverlaysLayerXrSpaceToHandleInfo[*space].spaceType = SPACE_REFERENCE;
    }
    auto info = OverlaysLayerGetHandleInfoFromXrSpace(*space);
    sessionInfo->childSpaces.insert(info);
    info->localHandle = localHandle;
"""


# XrPath

after_downchain_main["xrStringToPath"] = """
    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerPathToAtomInfoMutex);
    gOverlaysLayerPathToAtomInfo[*path] = std::make_shared<OverlaysLayerPathAtomInfo>(pathString);
    mlock2.unlock();

"""


# XrSystemId

after_downchain_main["xrGetSystem"] = """
    XrSystemGetInfo* getInfoCopy = reinterpret_cast<XrSystemGetInfo*>(CopyXrStructChainWithMalloc(instance, getInfo));
    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerSystemIdToAtomInfoMutex);
    gOverlaysLayerSystemIdToAtomInfo[*systemId] = std::make_shared<OverlaysLayerSystemIdAtomInfo>(getInfoCopy);
    mlock2.unlock();
"""


# XrSuggestedInteractionProfileBinding

# left here as breadcrumbs - xrSuggestInteractionProfileBindings is completely hand-written
if False:
    after_downchain_main["xrSuggestInteractionProfileBindings"] = """
    auto search = gPathToSuggestedInteractionProfileBinding.find(suggestedBindings->interactionProfile);
    if(search != gPathToSuggestedInteractionProfileBinding.end()) {
        FreeXrStructChainWithFree(instance, search->second);
    }
    // XXX this needs to be by profile, not the whole thing
    gPathToSuggestedInteractionProfileBinding[suggestedBindings->interactionProfile] = reinterpret_cast<XrInteractionProfileSuggestedBinding*>(CopyXrStructChainWithMalloc(instance, suggestedBindings));

"""


# store preambles of generated header and source -----------------------------

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
#include <unordered_map>
#include <map>

#include "overlays.h"

"""


source_text = """

#ifndef NOMINMAX
#define NOMINMAX
#endif  // !NOMINMAX

#include "xr_generated_overlays.hpp"
#include "hex_and_handles.h"

#include <cstring>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <map>
#include <limits>

std::unordered_map<XrPath, XrInteractionProfileSuggestedBinding*> gPathToSuggestedInteractionProfileBinding;

// MUST BE DEFAULT ONLY FOR LEAF OBJECTS (no pointers in them)
template <typename T>
void IPCCopyOut(T* dst, const T* src)
{
    if(!src)
        return;

    *dst = *src;
}

// MUST BE DEFAULT ONLY FOR LEAF OBJECTS (no pointers in them)
template <typename T>
void IPCCopyOut(T* dst, const T* src, size_t count)
{
    if(!src)
        return;

    for(size_t i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

template <typename T>
T* IPCSerialize(XrInstance instance, IPCBuffer& ipcbuf, IPCHeader* header, T* srcbase, CopyType copyType, size_t size)
{
    T* serialized = reinterpret_cast<T*>(ipcbuf.allocate(sizeof(T) * size));

    for(size_t i = 0; i < size; i++) {
        CopyXrStructChain(instance, &srcbase[i], &serialized[i], copyType,
            [&ipcbuf](size_t size){return ipcbuf.allocate(size);},
            [&ipcbuf,&header](void* pointerToPointer){header->addOffsetToPointer(ipcbuf.base, pointerToPointer);});
    }

    return serialized;
}

// CopyOut XR structs -------------------------------------------------------
template <>
void IPCCopyOut(XrBaseOutStructure* dstbase, const XrBaseOutStructure* srcbase)
{
    bool skipped = true;

    do {
        skipped = false;

        if(!srcbase) {
            return;
        }

        switch(dstbase->type) {
            case XR_TYPE_SPACE_LOCATION: {
                auto src = reinterpret_cast<const XrSpaceLocation*>(srcbase);
                auto dst = reinterpret_cast<XrSpaceLocation*>(dstbase);
                dst->locationFlags = src->locationFlags;
                dst->pose = src->pose;
                break;
            }

            case XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR: {
                auto src = reinterpret_cast<const XrGraphicsRequirementsD3D11KHR*>(srcbase);
                auto dst = reinterpret_cast<XrGraphicsRequirementsD3D11KHR*>(dstbase);
                dst->adapterLuid = src->adapterLuid;
                dst->minFeatureLevel = src->minFeatureLevel;
                break;
            }

            case XR_TYPE_FRAME_STATE: {
                auto src = reinterpret_cast<const XrFrameState*>(srcbase);
                auto dst = reinterpret_cast<XrFrameState*>(dstbase);
                dst->predictedDisplayTime = src->predictedDisplayTime;
                dst->predictedDisplayPeriod = src->predictedDisplayPeriod;
                dst->shouldRender = src->shouldRender;
                break;
            }

            case XR_TYPE_INSTANCE_PROPERTIES: {
                auto src = reinterpret_cast<const XrInstanceProperties*>(srcbase);
                auto dst = reinterpret_cast<XrInstanceProperties*>(dstbase);
                dst->runtimeVersion = src->runtimeVersion;
                strncpy_s(dst->runtimeName, src->runtimeName, XR_MAX_RUNTIME_NAME_SIZE);
                break;
            }

            case XR_TYPE_EXTENSION_PROPERTIES: {
                auto src = reinterpret_cast<const XrExtensionProperties*>(srcbase);
                auto dst = reinterpret_cast<XrExtensionProperties*>(dstbase);
                strncpy_s(dst->extensionName, src->extensionName, XR_MAX_EXTENSION_NAME_SIZE);
                dst->extensionVersion = src->extensionVersion;
                break;
            }

            case XR_TYPE_SYSTEM_PROPERTIES: {
                auto src = reinterpret_cast<const XrSystemProperties*>(srcbase);
                auto dst = reinterpret_cast<XrSystemProperties*>(dstbase);
                dst->systemId = src->systemId;
                dst->vendorId = src->vendorId;
                dst->graphicsProperties = src->graphicsProperties;
                dst->trackingProperties = src->trackingProperties;
                strncpy_s(dst->systemName, src->systemName, XR_MAX_SYSTEM_NAME_SIZE);
                break;
            }

            case XR_TYPE_VIEW_CONFIGURATION_PROPERTIES: {
                auto src = reinterpret_cast<const XrViewConfigurationProperties*>(srcbase);
                auto dst = reinterpret_cast<XrViewConfigurationProperties*>(dstbase);
                dst->viewConfigurationType = src->viewConfigurationType;
                dst->fovMutable = src->fovMutable;
                break;
            }

            case XR_TYPE_VIEW_CONFIGURATION_VIEW: {
                auto src = reinterpret_cast<const XrViewConfigurationView*>(srcbase);
                auto dst = reinterpret_cast<XrViewConfigurationView*>(dstbase);
                dst->recommendedImageRectWidth = src->recommendedImageRectWidth;
                dst->maxImageRectWidth = src->maxImageRectWidth;
                dst->recommendedImageRectHeight = src->recommendedImageRectHeight;
                dst->maxImageRectHeight = src->maxImageRectHeight;
                dst->recommendedSwapchainSampleCount = src->recommendedSwapchainSampleCount;
                dst->maxSwapchainSampleCount = src->maxSwapchainSampleCount;
                break;
            }

            case XR_TYPE_VIEW: {
                auto src = reinterpret_cast<const XrView*>(srcbase);
                auto dst = reinterpret_cast<XrView*>(dstbase);
                dst->pose = src->pose;
                dst->fov = src->fov;
                break;
            }

            case XR_TYPE_VIEW_STATE: {
                auto src = reinterpret_cast<const XrViewState*>(srcbase);
                auto dst = reinterpret_cast<XrViewState*>(dstbase);
                dst->viewStateFlags = src->viewStateFlags;
                break;
            }

            default: {
                // I don't know what this is, drop it and keep going
                OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "unknown",
                    OverlaysLayerNoObjectInfo, fmt("IPCCopyOut called to copy out to %p of unknown type %d - skipped.", dstbase, dstbase->type).c_str());

                dstbase = dstbase->next;
                skipped = true;

                // Don't increment srcbase.  Unknown structs were
                // dropped during serialization, so keep going until we
                // see a type we know and then we'll have caught up with
                // what was serialized.
                //
                break;
            }
        }
    } while(skipped);

    IPCCopyOut(dstbase->next, srcbase->next);
}


"""


# types, structs, variables --------------------------------------------------


# XrPath

header_text += f"""
struct {layer_name}PathAtomInfo
{{
    std::string pathString;
    {layer_name}PathAtomInfo(const char *pathString_) :
        pathString(pathString_)
    {{
    }}
    // map of remote XrPath by XrSession
    std::unordered_map<uint64_t, uint64_t> remotePathByLocalSession;
    typedef std::shared_ptr<OverlaysLayerPathAtomInfo> Ptr;
}};

extern std::unordered_map<XrPath, {layer_name}PathAtomInfo::Ptr> g{layer_name}PathToAtomInfo;
extern std::recursive_mutex g{layer_name}PathToAtomInfoMutex;

"""

source_text += f"""

std::unordered_map<XrPath, {layer_name}PathAtomInfo::Ptr> g{layer_name}PathToAtomInfo;
std::recursive_mutex g{layer_name}PathToAtomInfoMutex;
"""


# XrSystemId

header_text += f"""
struct {layer_name}SystemIdAtomInfo
{{
    const XrSystemGetInfo *getInfo;
    {layer_name}SystemIdAtomInfo(const XrSystemGetInfo *copyOfGetInfo)
        : getInfo(copyOfGetInfo)
    {{}}
    // map of remote XrSystemId by XrSession
    typedef std::shared_ptr<OverlaysLayerSystemIdAtomInfo> Ptr;
}};

extern std::unordered_map<XrSystemId, {layer_name}SystemIdAtomInfo::Ptr> g{layer_name}SystemIdToAtomInfo;
extern std::recursive_mutex g{layer_name}SystemIdToAtomInfoMutex;

"""

source_text += f"""

std::unordered_map<XrSystemId, {layer_name}SystemIdAtomInfo::Ptr> g{layer_name}SystemIdToAtomInfo;
std::recursive_mutex g{layer_name}SystemIdToAtomInfoMutex;
"""

# All Handle types

handles_needing_substitution = ['XrSession', 'XrSwapchain', 'XrSpace']

only_child_handles = [h for h in supported_handles if h != "XrInstance"]

for handle_type in supported_handles:

    # If this handle has a parent handle type (e.g. XrSpace has the
    # parent XrSession), then create members, ctor, and dtor text to track
    # and manage those handles
    parent_type = str(handles[handle_type][1] or "")

    if parent_type:
        parent_members = f"""
    {parent_type} parentHandle;
    XrInstance parentInstance;
"""
        parent_dtor = f"""
            parentHandle = XR_NULL_HANDLE;
            parentInstance = XR_NULL_HANDLE;
"""
        parent_ctor_params = f"""{parent_type} parent, XrInstance parentInstance_, """
        parent_ctor_member_init = f"""
    parentHandle(parent),
    parentInstance(parentInstance_),
"""
    else:
        parent_members = ""
        parent_dtor = ""
        parent_ctor_params = ""
        parent_ctor_member_init = ""

    # If this API layer creates local handles to disambiguate handles
    # from this and other processes, create members, ctor, and dtor text
    # to track and manage the local handles and actual handles
    if handle_type in handles_needing_substitution:
        substitution_members = f"""
    {handle_type} actualHandle;
    bool isProxied = false; // The handle is only valid in the Main XrInstance (i.e. in the Main Process)
"""
        substitution_dtor = f"""
            actualHandle = XR_NULL_HANDLE;
"""
        substitution_destroy = f"""
            if(!isProxied) {{
                auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

                downchain->Destroy{handle_type[2:]}(actualHandle);
            }}
"""
        substitution_header_text = f"""
extern std::recursive_mutex gActual{handle_type}ToLocalHandleMutex;
extern std::unordered_map<{handle_type}, {handle_type}> gActual{handle_type}ToLocalHandle;
"""
        substitution_source_text = f"""
std::recursive_mutex gActual{handle_type}ToLocalHandleMutex;
std::unordered_map<{handle_type}, {handle_type}> gActual{handle_type}ToLocalHandle;
"""
    else:
        substitution_members = ""
        substitution_dtor = ""
        substitution_destroy = ""
        substitution_header_text = ""
        substitution_source_text = ""

    handle_header_text = f"""

struct {layer_name}{handle_type}HandleInfo
{{

    std::shared_ptr<XrGeneratedDispatchTable> downchain;
    bool valid = true;
    {parent_members}
    {substitution_members}
    {add_to_handle_struct.get(handle_type, {}).get("members", "")}

    void Destroy() /* For OpenXR's intents.  Not class destructor. */
    {{
        if(valid) {{
            // XXX also Destroy all child handles
            {in_destroy.get(handle_type, "")}
            downchain.reset();
            {substitution_dtor}
            {parent_dtor}
            valid = false;
        }} else {{
            OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "unknown",
                OverlaysLayerNoObjectInfo, "Unexpected Destroy() on already Destroyed {handle_type}HandleInfo.");
        }}
    }}
    typedef std::shared_ptr<{layer_name}{handle_type}HandleInfo> Ptr;

    {layer_name}{handle_type}HandleInfo({parent_ctor_params}std::shared_ptr<XrGeneratedDispatchTable> downchain_) :
        {parent_ctor_member_init}
        downchain(downchain_)
    {{
        {in_constructor.get(handle_type, "")}
    }}

    ~{layer_name}{handle_type}HandleInfo()
    {{
        if(valid) {{
            {in_destructor.get(handle_type, "")}
            {substitution_destroy}
            Destroy();
        }}
    }}

    std::recursive_mutex mutex;
    std::unique_lock<std::recursive_mutex> GetLock()
    {{
        return std::unique_lock<std::recursive_mutex>(mutex);
    }}

    {add_to_handle_struct.get(handle_type, {}).get("methods", "")}
}};

extern std::unordered_map<{handle_type}, {layer_name}{handle_type}HandleInfo::Ptr> g{layer_name}{handle_type}ToHandleInfo;
extern std::recursive_mutex g{layer_name}{handle_type}ToHandleInfoMutex;

void {layer_name}AddHandleInfoFor{handle_type}({handle_type} handle, {layer_name}{handle_type}HandleInfo::Ptr info);
{layer_name}{handle_type}HandleInfo::Ptr {layer_name}GetHandleInfoFrom{handle_type}({handle_type} handle);
void {layer_name}Remove{handle_type}FromHandleInfoMap({handle_type} handle);
{substitution_header_text}
"""

    handle_source_text = f"""

std::unordered_map<{handle_type}, {layer_name}{handle_type}HandleInfo::Ptr> g{layer_name}{handle_type}ToHandleInfo;
std::recursive_mutex g{layer_name}{handle_type}ToHandleInfoMutex;

void {layer_name}AddHandleInfoFor{handle_type}({handle_type} handle, {layer_name}{handle_type}HandleInfo::Ptr info)
{{
    std::unique_lock<std::recursive_mutex> mlock(g{layer_name}{handle_type}ToHandleInfoMutex);
    g{layer_name}{handle_type}ToHandleInfo.insert({{handle, info}});
}}

// could throw if handle not in the map
{layer_name}{handle_type}HandleInfo::Ptr {layer_name}GetHandleInfoFrom{handle_type}({handle_type} handle)
{{
    std::unique_lock<std::recursive_mutex> mlock(g{layer_name}{handle_type}ToHandleInfoMutex);
    auto it = g{layer_name}{handle_type}ToHandleInfo.find(handle);
    if(it == g{layer_name}{handle_type}ToHandleInfo.end()) {{
        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, nullptr,
            OverlaysLayerNoObjectInfo, fmt("Could not look up info from {handle_type} handle %llX", handle).c_str());
        throw OverlaysLayerXrException(XR_ERROR_HANDLE_INVALID);
    }}
    return it->second;
}}

void {layer_name}Remove{handle_type}FromHandleInfoMap({handle_type} handle)
{{
    std::unique_lock<std::recursive_mutex> mlock(g{layer_name}{handle_type}ToHandleInfoMutex);
    auto it = g{layer_name}{handle_type}ToHandleInfo.find(handle);
    if(it == g{layer_name}{handle_type}ToHandleInfo.end()) {{
        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, nullptr,
            OverlaysLayerNoObjectInfo, fmt("Could not look up info from {handle_type} handle %llX", handle).c_str());
        throw OverlaysLayerXrException(XR_ERROR_HANDLE_INVALID);
    }}
    g{layer_name}{handle_type}ToHandleInfo.erase(it);
}}

{substitution_source_text}
"""


    header_text += handle_header_text
    source_text += handle_source_text



# Generate functions for RPC; RPCCallXyz, RPCServeXyz, Serialize, Copyout ----

CreateSessionRPC = {
    "command_name" : "CreateSession",
    "args" : (
        {
            "name" : "formFactor",
            "type" : "POD",
            "pod_type" : "XrFormFactor"
        },
        {
            "name" : "instanceCreateInfo",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrInstanceCreateInfo",
            "is_const" : True
        },
        {
            "name" : "createInfo",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrSessionCreateInfo",
            "is_const" : True
        },
        {
            "name" : "createInfoOverlay",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrSessionCreateInfoOverlayEXTX",
            "is_const" : True
        },
        {
            "name" : "session",
            "type" : "pointer_to_pod",
            "pod_type" : "XrSession",
            "is_const" : False
        },
    ),
    "function" : "OverlaysLayerCreateSessionMainAsOverlay"
}

DestroySessionRPC = {
    "command_name" : "DestroySession",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession"
        },
    ),
    "function" : "OverlaysLayerDestroySessionMainAsOverlay"
}

EnumerateSwapchainFormatsRPC = {
    "command_name" : "EnumerateSwapchainFormats",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "formatCapacityInput",
            "type" : "POD",
            "pod_type" : "uint32_t",
        },
        {
            "name" : "formatCountOutput",
            "type" : "pointer_to_pod",
            "pod_type" : "uint32_t",
            "is_const" : False
        },
        {
            "name" : "formats",
            "type" : "fixed_array",
            "base_type" : "int64_t",
            "input_size" : "formatCapacityInput",
            "output_size" : "formatCountOutput",
            "is_const" : False
        },
    ),
    "function" : "OverlaysLayerEnumerateSwapchainFormatsMainAsOverlay"
}

CreateSwapchainRPC = {
    "command_name" : "CreateSwapchain",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "createInfo",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrSwapchainCreateInfo",
            "is_const" : True
        },
        {
            "name" : "swapchain",
            "type" : "pointer_to_pod",
            "pod_type" : "XrSwapchain",
            "is_const" : False
        },
        {
            "name" : "swapchainCount",
            "type" : "pointer_to_pod",
            "pod_type" : "uint32_t",
            "is_const" : False,
        },
    ),
    "function" : "OverlaysLayerCreateSwapchainMainAsOverlay"
}

CreateReferenceSpaceRPC = {
    "command_name" : "CreateReferenceSpace",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "createInfo",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrReferenceSpaceCreateInfo",
            "is_const" : True
        },
        {
            "name" : "space",
            "type" : "pointer_to_pod",
            "pod_type" : "XrSpace",
            "is_const" : False
        },
    ),
    "function" : "OverlaysLayerCreateReferenceSpaceMainAsOverlay"
}

PollEventRPC = {
    "command_name" : "PollEvent",
    "args" : (
        {
            "name" : "eventData",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrEventDataBuffer",
            "is_const" : False,
        },
    ),
    "function" : "OverlaysLayerPollEventMainAsOverlay"
}

BeginSessionRPC = {
    "command_name" : "BeginSession",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "beginInfo",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrSessionBeginInfo",
            "is_const" : True
        },
    ),
    "function" : "OverlaysLayerBeginSessionMainAsOverlay"
}

RequestExitSessionRPC = {
    "command_name" : "RequestExitSession",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
    ),
    "function" : "OverlaysLayerRequestExitSessionMainAsOverlay"
}

EndSessionRPC = {
    "command_name" : "EndSession",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
    ),
    "function" : "OverlaysLayerEndSessionMainAsOverlay"
}

WaitFrameRPC = {
    "command_name" : "WaitFrame",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "frameWaitInfo",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrFrameWaitInfo",
            "is_const" : True
        },
        {
            "name" : "frameState",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrFrameState",
            "is_const" : False
        },
    ),
    "function" : "OverlaysLayerWaitFrameMainAsOverlay"
}

BeginFrameRPC = {
    "command_name" : "BeginFrame",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "frameBeginInfo",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrFrameBeginInfo",
            "is_const" : True
        },
    ),
    "function" : "OverlaysLayerBeginFrameMainAsOverlay"
}

EndFrameRPC = {
    "command_name" : "EndFrame",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "frameEndInfo",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrFrameEndInfo",
            "is_const" : True
        },
    ),
    "function" : "OverlaysLayerEndFrameMainAsOverlay"
}

AcquireSwapchainImageRPC = {
    "command_name" : "AcquireSwapchainImage",
    "args" : (
        {
            "name" : "swapchain",
            "type" : "POD",
            "pod_type" : "XrSwapchain",
        },
        {
            "name" : "acquireInfo",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrSwapchainImageAcquireInfo",
            "is_const" : True
        },
        {
            "name" : "index",
            "type" : "pointer_to_pod",
            "pod_type" : "uint32_t",
            "is_const" : False
        },
    ),
    "function" : "OverlaysLayerAcquireSwapchainImageMainAsOverlay"
}

WaitSwapchainImageRPC = {
    "command_name" : "WaitSwapchainImage",
    "args" : (
        {
            "name" : "swapchain",
            "type" : "POD",
            "pod_type" : "XrSwapchain",
        },
        {
            "name" : "acquireInfo",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrSwapchainImageWaitInfo",
            "is_const" : True
        },
        {
            "name" : "sharedResourceHandle",
            "type" : "POD",
            "pod_type" : "HANDLE",
        },
    ),
    "function" : "OverlaysLayerWaitSwapchainImageMainAsOverlay"
}

ReleaseSwapchainImageRPC = {
    "command_name" : "ReleaseSwapchainImage",
    "args" : (
        {
            "name" : "swapchain",
            "type" : "POD",
            "pod_type" : "XrSwapchain",
        },
        {
            "name" : "acquireInfo",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrSwapchainImageReleaseInfo",
            "is_const" : True
        },
        {
            "name" : "sharedResourceHandle",
            "type" : "POD",
            "pod_type" : "HANDLE",
        },
    ),
    "function" : "OverlaysLayerReleaseSwapchainImageMainAsOverlay"
}

EnumerateReferenceSpacesRPC = {
    "command_name" : "EnumerateReferenceSpaces",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "spaceCapacityInput",
            "type" : "POD",
            "pod_type" : "uint32_t",
        },
        {
            "name" : "spaceCountOutput",
            "type" : "pointer_to_pod",
            "pod_type" : "uint32_t",
            "is_const" : False
        },
        {
            "name" : "spaces",
            "type" : "fixed_array",
            "base_type" : "XrReferenceSpaceType",
            "input_size" : "spaceCapacityInput",
            "output_size" : "spaceCountOutput",
            "is_const" : False
        },
    ),
    "function" : "OverlaysLayerEnumerateReferenceSpacesMainAsOverlay"
}

GetReferenceSpaceBoundsRectRPC = {
    "command_name" : "GetReferenceSpaceBoundsRect",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "referenceSpaceType",
            "type" : "POD",
            "pod_type" : "XrReferenceSpaceType",
        },
        {
            "name" : "bounds",
            "type" : "pointer_to_pod",
            "pod_type" : "XrExtent2Df",
            "is_const" : False
        },
    ),
    "function" : "OverlaysLayerGetReferenceSpaceBoundsRectMainAsOverlay"
}

LocateViewsRPC = {
    "command_name" : "LocateViews",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "viewLocateInfo",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrViewLocateInfo",
            "is_const" : True
        },
        {
            "name" : "viewState",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrViewState",
            "is_const" : False
        },
        {
            "name" : "viewCapacityInput",
            "type" : "POD",
            "pod_type" : "uint32_t",
        },
        {
            "name" : "viewCountOutput",
            "type" : "pointer_to_pod",
            "pod_type" : "uint32_t",
            "is_const" : False
        },
        {
            "name" : "views",
            "type" : "fixed_xrstruct_array",
            "struct_type" : "XrView",
            "input_size" : "viewCapacityInput",
            "output_size" : "viewCountOutput",
            "is_const" : False
        },

    ),
    "function" : "OverlaysLayerLocateViewsMainAsOverlay"
}

LocateSpaceRPC = {
    "command_name" : "LocateSpace",
    "args" : (
        {
            "name" : "space",
            "type" : "POD",
            "pod_type" : "XrSpace",
        },
        {
            "name" : "baseSpace",
            "type" : "POD",
            "pod_type" : "XrSpace",
        },
        {
            "name" : "time",
            "type" : "POD",
            "pod_type" : "XrTime",
        },
        {
            "name" : "spaceLocation",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrSpaceLocation",
            "is_const" : False
        },
    ),
    "function" : "OverlaysLayerLocateSpaceMainAsOverlay"
}

DestroySpaceRPC = {
    "command_name" : "DestroySpace",
    "args" : (
        {
            "name" : "space",
            "type" : "POD",
            "pod_type" : "XrSpace",
        },
    ),
    "function" : "OverlaysLayerDestroySpaceMainAsOverlay"
}

DestroySwapchainRPC = {
    "command_name" : "DestroySwapchain",
    "args" : (
        {
            "name" : "space",
            "type" : "POD",
            "pod_type" : "XrSwapchain",
        },
    ),
    "function" : "OverlaysLayerDestroySwapchainMainAsOverlay"
}

SyncActionsAndGetStateRPC = { 
    "command_name" : "SyncActionsAndGetState",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "countProfileAndBindings",
            "type" : "POD",
            "pod_type" : "uint32_t",
        },
        {
            "name" : "profileStrings",
            "type" : "fixed_array",
            "base_type" : "WellKnownStringIndex",
            "input_size" : "countProfileAndBindings",
            "is_const" : True
        },
        {
            "name" : "bindingStrings",
            "type" : "fixed_array",
            "base_type" : "WellKnownStringIndex",
            "input_size" : "countProfileAndBindings",
            "is_const" : True
        },
        {
            "name" : "states",
            "type" : "fixed_array",
            "base_type" : "ActionStateUnion",
            "input_size" : "countProfileAndBindings",
            "is_const" : False
        },
        {
            "name" : "countSubactionStrings",
            "type" : "POD",
            "pod_type" : "uint32_t",
        },
        {
            "name" : "subactionStrings",
            "type" : "fixed_array",
            "base_type" : "WellKnownStringIndex",
            "input_size" : "countSubactionStrings",
            "is_const" : True
        },
        {
            "name" : "interactionProfileStrings",
            "type" : "fixed_array",
            "base_type" : "WellKnownStringIndex",
            "input_size" : "countSubactionStrings",
            "is_const" : False
        },
    ),
    "function" : "OverlaysLayerSyncActionsAndGetStateMainAsOverlay"
}

CreateActionSpaceFromBindingRPC = {
    "command_name" : "CreateActionSpaceFromBinding",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "profileString",
            "type" : "POD",
            "pod_type" : "WellKnownStringIndex",
        },
        {
            "name" : "bindingString",
            "type" : "POD",
            "pod_type" : "WellKnownStringIndex",
        },
        {
            "name" : "poseInActionSpace",
            "type" : "pointer_to_pod",
            "pod_type" : "XrPosef",
            "is_const" : True
        },
        {
            "name" : "space",
            "type" : "pointer_to_pod",
            "pod_type" : "XrSpace",
            "is_const" : False
        },
    ),
    "function" : "OverlaysLayerCreateActionSpaceFromBinding",
}

GetInputSourceLocalizedNameRPC = {
    "command_name" : "GetInputSourceLocalizedName",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "getInfo",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrInputSourceLocalizedNameGetInfo",
            "is_const" : True
        },
        {
            "name" : "sourceString",
            "type" : "POD",
            "pod_type" : "WellKnownStringIndex",
        },
        {
            "name" : "bufferCapacityInput",
            "type" : "POD",
            "pod_type" : "uint32_t",
        },
        {
            "name" : "bufferCountOutput",
            "type" : "pointer_to_pod",
            "pod_type" : "uint32_t",
            "is_const" : False
        },
        {
            "name" : "buffer",
            "type" : "fixed_array",
            "base_type" : "char",
            "input_size" : "bufferCapacityInput",
            "output_size" : "bufferCountOutput",
            "is_const" : False
        },
    ),
    "function" : "OverlaysLayerGetInputSourceLocalizedNameMainAsOverlay"
}

ApplyHapticFeedbackRPC = {
    "command_name" : "ApplyHapticFeedback",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "countProfileAndBindings",
            "type" : "POD",
            "pod_type" : "uint32_t",
        },
        {
            "name" : "profileStrings",
            "type" : "fixed_array",
            "base_type" : "WellKnownStringIndex",
            "input_size" : "countProfileAndBindings",
            "is_const" : True
        },
        {
            "name" : "bindingStrings",
            "type" : "fixed_array",
            "base_type" : "WellKnownStringIndex",
            "input_size" : "countProfileAndBindings",
            "is_const" : True
        },
        {
            "name" : "hapticFeedback",
            "type" : "xr_struct_pointer",
            "struct_type" : "XrHapticBaseHeader",
            "is_const" : True
        },
    ),
    "function" : "OverlaysLayerApplyHapticFeedbackMainAsOverlay"
}

StopHapticFeedbackRPC = {
    "command_name" : "StopHapticFeedback",
    "args" : (
        {
            "name" : "session",
            "type" : "POD",
            "pod_type" : "XrSession",
        },
        {
            "name" : "countProfileAndBindings",
            "type" : "POD",
            "pod_type" : "uint32_t",
        },
        {
            "name" : "profileStrings",
            "type" : "fixed_array",
            "base_type" : "WellKnownStringIndex",
            "input_size" : "countProfileAndBindings",
            "is_const" : True
        },
        {
            "name" : "bindingStrings",
            "type" : "fixed_array",
            "base_type" : "WellKnownStringIndex",
            "input_size" : "countProfileAndBindings",
            "is_const" : True
        },
    ),
    "function" : "OverlaysLayerStopHapticFeedbackMainAsOverlay"
}

rpcs = (
    CreateSessionRPC,
    DestroySessionRPC,
    EnumerateSwapchainFormatsRPC,
    CreateSwapchainRPC,
    DestroySwapchainRPC,
    EnumerateReferenceSpacesRPC,
    GetReferenceSpaceBoundsRectRPC,
    CreateReferenceSpaceRPC,
    LocateViewsRPC,
    LocateSpaceRPC,
    DestroySpaceRPC,
    PollEventRPC,
    BeginSessionRPC,
    RequestExitSessionRPC,
    EndSessionRPC,
    WaitFrameRPC,
    BeginFrameRPC,
    EndFrameRPC,
    AcquireSwapchainImageRPC,
    WaitSwapchainImageRPC,
    ReleaseSwapchainImageRPC,
    SyncActionsAndGetStateRPC,
    CreateActionSpaceFromBindingRPC,
    GetInputSourceLocalizedNameRPC,
    ApplyHapticFeedbackRPC,
    StopHapticFeedbackRPC,
)


def rpc_command_name_to_enum(name):
    return "RPC_XR_" + "_".join([s.upper() for s in re.split("([A-Z][^A-Z]*)", name) if s])

def rpc_arg_to_cdecl(arg) :
    if arg.get("is_const", False):
        const_part = "const "
    else:
        const_part = ""

    if arg["type"] == "POD": 
        return f"{const_part}{arg['pod_type']} {arg['name']}"
    elif arg["type"] == "xr_simple_struct": 
        return f"{const_part}{arg['struct_type']} {arg['name']}"
    elif arg["type"] == "pointer_to_pod":
        return f"{const_part}{arg['pod_type']} *{arg['name']}"
    elif arg["type"] == "fixed_array":
        return f"{const_part}{arg['base_type']} *{arg['name']}"
    elif arg["type"] == "xr_struct_pointer":
        return f"{const_part}{arg['struct_type']} *{arg['name']}"
    elif arg["type"] == "fixed_xrstruct_array":
        return f"{const_part}{arg['struct_type']} *{arg['name']}"
    else:
        return f"XXX unknown type {arg['type']}\n"

def rpc_arg_to_serialize(arg):
    if arg["type"] == "POD": 
        return f"    dst->{arg['name']} = src->{arg['name']};\n"
    elif arg["type"] == "pointer_to_pod":
        if arg.get("is_const", False):
            return f"""
    dst->{arg["name"]} = IPCSerialize(ipcbuf, header, src->{arg["name"]}); // pointer_to_pod
    header->addOffsetToPointer(ipcbuf.base, &dst->{arg["name"]});
"""
        else:
            return f"""
    dst->{arg["name"]} = IPCSerializeNoCopy(ipcbuf, header, src->{arg["name"]}); // pointer_to_pod
    header->addOffsetToPointer(ipcbuf.base, &dst->{arg["name"]});
"""
    elif arg["type"] == "fixed_array":
        if arg.get("is_const", False):
            return f"""
    dst->{arg['name']} = IPCSerialize(ipcbuf, header, src->{arg['name']}, src->{arg['input_size']});
    header->addOffsetToPointer(ipcbuf.base, &dst->{arg['name']});
"""
        else:
            return f"""
    dst->{arg['name']} = IPCSerializeNoCopy(ipcbuf, header, src->{arg['name']}, src->{arg['input_size']});
    header->addOffsetToPointer(ipcbuf.base, &dst->{arg['name']});
"""
    elif arg["type"] == "xr_struct_pointer":
        copy_type = {True: "COPY_EVERYTHING", False: "COPY_ONLY_TYPE_NEXT"}[arg["is_const"]]
        return f"""
    dst->{arg["name"]} = reinterpret_cast<{arg["struct_type"]}*>(IPCSerialize(instance, ipcbuf, header, reinterpret_cast<const XrBaseInStructure*>(src->{arg["name"]}), {copy_type}));
    header->addOffsetToPointer(ipcbuf.base, &dst->{arg["name"]});
"""
    elif arg["type"] == "fixed_xrstruct_array":
        copy_type = {True: "COPY_EVERYTHING", False: "COPY_ONLY_TYPE_NEXT"}[arg["is_const"]]
        return f"""
    if(src->{arg["input_size"]} > 0) {{
        dst->{arg["name"]} = IPCSerialize(instance, ipcbuf, header, src->{arg["name"]}, {copy_type}, src->{arg["input_size"]});
        header->addOffsetToPointer(ipcbuf.base, &dst->{arg["name"]});
    }}
"""
    else:
        return f"#error    XXX unimplemented rpc argument type {arg['type']}\n"

def rpc_arg_to_copyout(arg):
    if arg["type"] == "POD": 
        return "" # input only
    elif arg["type"] == "pointer_to_pod":
        if arg["is_const"]:
            return "" # input only
        else:
            return f"    IPCCopyOut(dst->{arg['name']}, src->{arg['name']});\n"
    elif arg["type"] == "fixed_array":
        if arg["is_const"]:
            return "" # input only
        else:
            if "output_size" in arg:
                return f"""
    if (src->{arg["name"]}) {{
        IPCCopyOut(dst->{arg["name"]}, src->{arg["name"]}, *src->{arg["output_size"]});
    }}
"""
            else:
                return f"""
    if (src->{arg["name"]}) {{
        IPCCopyOut(dst->{arg["name"]}, src->{arg["name"]}, src->{arg["input_size"]});
    }}
"""
    elif arg["type"] == "xr_struct_pointer":
        if arg["is_const"]:
            return "" # input only
        else:
            if arg["struct_type"] == "XrEventDataBuffer":
                return f"""
    CopyEventChainIntoBuffer(XR_NULL_HANDLE, const_cast<const XrEventDataBaseHeader*>(reinterpret_cast<XrEventDataBaseHeader*>(src->{arg["name"]})), dst->{arg["name"]});
"""
            else:
                return f"""
    IPCCopyOut(
        reinterpret_cast<XrBaseOutStructure*>(dst->{arg["name"]}),
        reinterpret_cast<const XrBaseOutStructure*>(src->{arg["name"]})
        ); // {arg["struct_type"]}
"""
    elif arg["type"] == "fixed_xrstruct_array":
        if arg["is_const"]:
            return "" # input only
        else:
            return f"""
    if(*src->{arg["output_size"]} > 0) {{
        for(uint32_t i = 0; i < *src->{arg["output_size"]}; i++) {{
            IPCCopyOut(
                reinterpret_cast<XrBaseOutStructure*>(&dst->{arg["name"]}[i]),
                reinterpret_cast<const XrBaseOutStructure*>(&src->{arg["name"]}[i])
                ); // {arg["struct_type"]}
            }}
    }}
"""
    else:
        return f"#error XXX unknown type {arg['type']}"


for rpc in rpcs:
    rpc["command_enum"] = rpc_command_name_to_enum(rpc["command_name"])

header_text += "enum {\n"
for rpc in rpcs:
    header_text += "    %(command_enum)s,\n" % rpc
header_text += "};\n"

rpc_case_bodies = ""

for rpc in rpcs:

    command_name = rpc["command_name"]
    command_type = 'XrResult'
    layer_command = api_layer_name_for_command("xr" + command_name)
    rpc_arguments_list = ", ".join(["%s" % arg["name"] for arg in rpc["args"]])
    served_args = ", ".join(["args->%s" % arg["name"] for arg in rpc["args"]])
    served_args_cdecls = ", ".join([rpc_arg_to_cdecl(arg) for arg in rpc["args"]])

    rpc_args_struct_members = ""
    rpc_serialize_members = ""
    rpc_copyout_members = ""
    for arg in rpc["args"]:
        rpc_args_struct_members += "    " + rpc_arg_to_cdecl(arg) + ";\n"
        rpc_serialize_members += rpc_arg_to_serialize(arg)
        rpc_copyout_members += rpc_arg_to_copyout(arg)

    rpc_args_struct = f"""
struct RPCXr{command_name}
{{
{rpc_args_struct_members}
}};
"""

    ipc_serialize_function = f"""
RPCXr{command_name}* IPCSerialize(XrInstance instance, IPCBuffer& ipcbuf, IPCHeader* header, const RPCXr{command_name}* src)
{{
    auto dst = new(ipcbuf) RPCXr{command_name};

{rpc_serialize_members}

    return dst;
}}
"""

    if rpc_copyout_members:
        ipc_copyout_function = f"""
template <>
void IPCCopyOut(RPCXr{command_name}* dst, const RPCXr{command_name}* src)
{{
{rpc_copyout_members}
}}
"""
    else: 
        ipc_copyout_function = ""

    rpc_call_function_proto = f"{command_type} RPCCall{command_name}(XrInstance instance, {served_args_cdecls});\n"

    rpc_call_function = f"""
{command_type} RPCCall{command_name}(XrInstance instance, {served_args_cdecls})
{{
    // Create a header for RPC
    IPCBuffer ipcbuf = gConnectionToMain->conn.GetIPCBuffer();
    IPCHeader* header = new(ipcbuf) IPCHeader{{ {rpc["command_enum"]} }};

    RPCXr{command_name} args {{ {rpc_arguments_list} }};
    RPCXr{command_name}* argsSerialized = IPCSerialize(instance, ipcbuf, header, &args);

    // XXX substitute handles in input XR structs 

    // Make pointers relative in anticipation of RPC (who will make them absolute, work on them, then make them relative again)
    header->makePointersRelative(ipcbuf.base);

    // Release Main process to do our work
    gConnectionToMain->conn.FinishOverlayRequest();

    // Wait for Main to report to us it has done the work
    bool success = gConnectionToMain->conn.WaitForMainResponseOrFail();
    if(!success) {{
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, nullptr,
            OverlaysLayerNoObjectInfo, "couldn't RPC {command_name} to main process.");
        return XR_ERROR_INITIALIZATION_FAILED;
    }}

    // Set pointers absolute so they are valid in our process space again
    header->makePointersAbsolute(ipcbuf.base);

    // is this necessary?  Are events the only structs that need handles substituted back to local?
    // for now, yes, only sessions, but eventually space and swapchain will need to be made local
    // XXX restore handles in output XR structs
"""

    if ipc_copyout_function:
        rpc_call_function += f"""
    // Copy anything that were "output" parameters into the command arguments
    if(header->result == XR_SUCCESS) {{ // XXX Some other codes may indicate qualified success, requiring CopyOut
        IPCCopyOut(&args, argsSerialized);
    }}
"""

    if "command_post" in rpc:
        rpc_call_function += f"""
    if(XR_SUCCEEDED(header->result)) {{
        {rpc["command_post"]}
    }}
"""

    rpc_call_function += """
    return header->result;
}
"""

    rpc_service_function = f"""
XrResult RPCServe{command_name}(
    ConnectionToOverlay::Ptr        connection,
    RPCXr{command_name}*              args)
{{
    // potential connection related goop

    XrResult result = {rpc["function"]}(connection, {served_args});

    // potential hand-written completion, checking return value and possibly overwriting result

    return result;
}}
"""

    rpc_case_bodies += f"""
        case {rpc["command_enum"]}: {{
            auto* args = ipcbuf.getAndAdvance<RPCXr{command_name}>();
            hdr->result = RPCServe{command_name}(connection, args);
            break;
        }}
"""

    header_text += rpc_call_function_proto

    source_text += rpc_args_struct
    source_text += ipc_serialize_function
    if ipc_copyout_function:
        source_text += ipc_copyout_function
    source_text += rpc_call_function
    source_text += rpc_service_function


header_text += "bool ProcessOverlayRequestOrReturnConnectionLost(ConnectionToOverlay::Ptr connection, IPCBuffer &ipcbuf, IPCHeader *hdr);\n"
source_text += f"""
bool ProcessOverlayRequestOrReturnConnectionLost(ConnectionToOverlay::Ptr connection, IPCBuffer &ipcbuf, IPCHeader *hdr)
{{
    try{{
        switch(hdr->requestType) {{
"""

source_text += rpc_case_bodies

source_text += f"""

            default: {{
                // XXX use log message func
                OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, nullptr,
                    OverlaysLayerNoObjectInfo, fmt("Unknown request type %08X in RPC", hdr->requestType).c_str());
                break;
            }}
        }}

        return true;

    }} catch (const OverlaysLayerXrException& e) {{

        hdr->result = e.result();
        return true;

    }} catch (const std::bad_alloc& e) {{

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, nullptr, OverlaysLayerNoObjectInfo, e.what());
        hdr->result = XR_ERROR_OUT_OF_MEMORY;
        return true;

    }}
}}
"""

# XXX temporary stubs
stub_em = (

    "SessionBeginDebugUtilsLabelRegionEXT",
    "SessionEndDebugUtilsLabelRegionEXT",
    "SessionInsertDebugUtilsLabelEXT",
)

for stub in stub_em:

    command_name = stub
    command = commands["xr" + command_name]
    parameter_cdecls = ", ".join([parameter_to_cdecl(command_name, parameter) for parameter in command["parameters"]])
    command_type = command["return_type"]
    layer_command = api_layer_name_for_command("xr" + command_name)

    header_text += f"{command_type} {layer_command}Overlay(XrInstance instance, {parameter_cdecls});\n"

    source_text += f"{command_type} {layer_command}Overlay(XrInstance instance, {parameter_cdecls})\n"
    source_text += "{ DebugBreak(); return XR_ERROR_VALIDATION_FAILURE; } // not implemented yet\n"


# deep copy, free, substitute and restore handles functions ------------------

xr_typed_structs = [name for name in supported_structs if structs[name][1]]
xr_simple_structs = [name for name in supported_structs if not structs[name][2]]

# accessor_prefix could be e.g. "p->"
# instance_string could be e.g. "instance" or e.g. "XR_NULL_HANDLE"
def get_code_to_restore_handle(member, instance_string, accessor_prefix):
    if member["type"] == "char_array":
        return ""
    elif member["type"] == "c_string":
        return ""
    elif member["type"] == "string_list":
        return ""
    elif member["type"] == "list_of_struct_pointers":

        return f"""
            // array of pointers to XR structs for {name}
            for(uint32_t i = 0; i < {accessor_prefix}{member["size"]}; i++) {{
                if(!RestoreActualHandles({instance_string}, (XrBaseInStructure *){accessor_prefix}{member["name"]}[i])) {{
                    return false;
                }}
            }}
"""
    elif member["type"] == "pointer_to_struct":
        return ""
    elif member["type"] == "pointer_to_atom_or_handle":
        if member["struct_type"] in handles_needing_substitution:
            return f"""
            // array of {member["struct_type"]} for {name}
            for(uint32_t i = 0; i < {accessor_prefix}{member["size"]}; i++) {{
                auto info = {layer_name}GetHandleInfoFrom{member["struct_type"]}({accessor_prefix}{member["name"]}[i]);
                (({member["struct_type"]}*){accessor_prefix}{member["name"]})[i] = info->actualHandle;
            }}
"""
        else:
            return ""
    elif member["type"] == "pointer_to_struct_array":
        if member["struct_type"].startswith("Xr"):
            return f"""
                // pointer to XR structs for {name}
                for(uint32_t i = 0; i < {accessor_prefix}{member["size"]}; i++) {{
                    if(!RestoreActualHandles({instance_string}, ({member["struct_type"]}*)&{accessor_prefix}{member["name"]}[i])) {{
                        return false;
                    }}
                }}
"""
        else:
            return ""
    elif member["type"] == "pointer_to_opaque":
        return ""
    elif member["type"] == "void_pointer":
        return ""
    elif member["type"] == "fixed_array":
        return ""
    elif member["type"] == "POD":
        if member["pod_type"] in handles_needing_substitution:
            return f"""
                {{
                    auto info = {layer_name}GetHandleInfoFrom{member["pod_type"]}({accessor_prefix}{member["name"]});
                    {accessor_prefix}{member["name"]} = info->actualHandle;
                }}
"""
        else:
            return ""
    elif member["type"] == "xr_simple_struct":
        return f"""
            // Expect this to find function with signature by type
            if(!RestoreActualHandles({instance_string}, &{accessor_prefix}{member["name"]})) {{
                return false;
            }}
"""
    elif member["type"] == "pointer_to_xr_struct_array":
        return f"""
            // pointer to XR structs for {name}
            for(uint32_t i = 0; i < {accessor_prefix}{member["size"]}; i++) {{
                if(!RestoreActualHandles({instance_string}, (XrBaseInStructure *)&{accessor_prefix}{member["name"]}[i])) {{
                    return false;
                }}
            }}
"""

    return "    // XXX XXX \"%s\" %s\n" % (member["type"], member["name"])


# accessor_prefix could be e.g. "p->"
# instance_string could be e.g. "instance" or e.g. "XR_NULL_HANDLE"
def get_code_to_substitute_handle(member, instance_string, accessor_prefix):
    if member["type"] == "char_array":
        return ""
    elif member["type"] == "c_string":
        return ""
    elif member["type"] == "string_list":
        return ""
    elif member["type"] == "list_of_struct_pointers":

        return f"""
            // array of pointers to XR structs for {name}
            for(uint32_t i = 0; i < {accessor_prefix}{member["size"]}; i++) {{
                SubstituteLocalHandles({instance_string}, (XrBaseOutStructure *){accessor_prefix}{member["name"]}[i]);
            }}
"""
    elif member["type"] == "pointer_to_struct":
        return ""
    elif member["type"] == "pointer_to_atom_or_handle":
        if member["struct_type"] in handles_needing_substitution:
            return f"""
            // array of {member["struct_type"]} for {name}
            for(uint32_t i = 0; i < {accessor_prefix}{member["size"]}; i++) {{
                std::unique_lock<std::recursive_mutex> lock(gActual{member["struct_type"]}ToLocalHandleMutex);
                auto it = gActual{member["struct_type"]}ToLocalHandle.find({accessor_prefix}{member["name"]}[i]);
                if(it == gActual{member["struct_type"]}ToLocalHandle.end()) {{
                    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, nullptr,
                        OverlaysLayerNoObjectInfo, fmt("Could not look up local handle for {member["struct_type"]} handle %llX", {accessor_prefix}{member["name"]}).c_str());
                    throw OverlaysLayerXrException(XR_ERROR_HANDLE_INVALID);
                }}
                (({member["struct_type"]}*){accessor_prefix}{member["name"]})[i] = gActual{member["struct_type"]}ToLocalHandle.at({accessor_prefix}{member["name"]}[i]);
            }}
"""
        else:
            return ""

    elif member["type"] == "pointer_to_struct_array":
        if member["struct_type"].startswith("Xr"):
            return f"""
                // pointer to XR structs for {name}
                for(uint32_t i = 0; i < {accessor_prefix}{member["size"]}; i++) {{
                    SubstituteLocalHandles({instance_string}, ({member["struct_type"]}*)&{accessor_prefix}{member["name"]}[i]);
                }}
"""
        else:
            return ""
    elif member["type"] == "pointer_to_opaque":
        return ""
    elif member["type"] == "void_pointer":
        return ""
    elif member["type"] == "fixed_array":
        return ""
    elif member["type"] == "POD":
        if member["pod_type"] in handles_needing_substitution:
            return f"""
                {{
                    std::unique_lock<std::recursive_mutex> lock(gActual{member["pod_type"]}ToLocalHandleMutex);
                    auto it = gActual{member["pod_type"]}ToLocalHandle.find({accessor_prefix}{member["name"]});
                    if(it == gActual{member["pod_type"]}ToLocalHandle.end()) {{
                        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, nullptr,
                            OverlaysLayerNoObjectInfo, fmt("Could not look up local handle for {member["pod_type"]} handle %llX", {accessor_prefix}{member["name"]}).c_str());
                        throw OverlaysLayerXrException(XR_ERROR_HANDLE_INVALID);
                    }}
                    {accessor_prefix}{member["name"]} = gActual{member["pod_type"]}ToLocalHandle.at({accessor_prefix}{member["name"]});
                }}
"""
        else:
            return ""
    elif member["type"] == "xr_simple_struct":
        return f"""
            // Expect this to find function with signature by type
            SubstituteLocalHandles({instance_string}, &{accessor_prefix}{member["name"]});
"""
    elif member["type"] == "pointer_to_xr_struct_array":
        return f"""
            // pointer to XR structs for {name}
            for(uint32_t i = 0; i < {accessor_prefix}{member["size"]}; i++) {{
                SubstituteLocalHandles({instance_string}, (XrBaseOutStructure *)&{accessor_prefix}{member["name"]}[i]);
            }}
"""

    return "    // XXX XXX \"%s\" %s\n" % (member["type"], member["name"])



copy_function_case_bodies = ""

free_function_case_bodies = ""

restore_handles_case_bodies = ""

substitute_handles_case_bodies = ""


for name in xr_simple_structs:
    struct = structs[name]

    restore_handles_function_prototype = f"bool RestoreActualHandles(XrInstance instance, {name} *xrstruct)"
    restore_handles_function_header = f"{restore_handles_function_prototype};\n"
    restore_handles_function_source = f"""
{restore_handles_function_prototype}
{{
"""
    for member in struct[3]:
        restore_handles_function_source += get_code_to_restore_handle(member, "instance", "xrstruct->")
    restore_handles_function_source += f"""
    return true;
}}
"""

    substitute_handles_function_prototype = f"void SubstituteLocalHandles(XrInstance instance, {name} *xrstruct)"
    substitute_handles_function_header = f"{substitute_handles_function_prototype};\n"
    substitute_handles_function_source = f"""
{substitute_handles_function_prototype}
{{
"""
    for member in struct[3]:
        substitute_handles_function_source += get_code_to_substitute_handle(member, "instance", "xrstruct->")
    substitute_handles_function_source += f"""
}}
"""

    source_text += restore_handles_function_source
    header_text += restore_handles_function_header
    source_text += substitute_handles_function_source
    header_text += substitute_handles_function_header


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

    restore_handles_case_bodies += f"""
            case {struct[1]}: {{
                auto p = reinterpret_cast<{name}*>(xrstruct);
"""

    substitute_handles_case_bodies += f"""
            case {struct[1]}: {{
                auto p = reinterpret_cast<{name}*>(xrstruct);
"""

    for member in struct[3]:

        restore_handles_case_bodies += f'/* {member["type"]} {member["name"]} */' + get_code_to_restore_handle(member, "instance", "p->")

        substitute_handles_case_bodies += get_code_to_substitute_handle(member, "instance", "p->");

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
        freefunc(p->%(name)s[i]);
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
            free_function += "    freefunc(p->%(name)s);\n" % member

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
        elif member["type"] == "xr_simple_struct":
            copy_function += "    dst->%(name)s = src->%(name)s;\n" % member
        else:
            copy_function += "    // XXX XXX \"%s\" %s\n" % (member["type"], member["name"])
            free_function += "    // XXX XXX \"%s\" %s\n" % (member["type"], member["name"])

#     if member["is_const"]:
#         copy_function += " is const"


    copy_function += """
    dst->next = reinterpret_cast<XrBaseInStructure*>(CopyXrStructChain(instance, reinterpret_cast<const XrBaseInStructure*>(src->next), copyType, alloc, addOffsetToPointer));
    if(dst->next) {
        addOffsetToPointer(&dst->next);
    }
    return true;
}
"""

    free_function += "    FreeXrStructChain(instance, reinterpret_cast<const XrBaseInStructure*>(p->next), freefunc);\n"
    free_function += "}\n\n"

    restore_handles_case_bodies += f"""
                break;
            }}
"""

    substitute_handles_case_bodies += f"""
                break;
            }}
"""

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
                auto info = OverlaysLayerGetHandleInfoFromXrInstance(instance);
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
            auto info = OverlaysLayerGetHandleInfoFromXrInstance(instance);
            char structTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
            structTypeName[0] = '\\0';
            if(info->downchain->StructureTypeToString(instance, p->type, structTypeName) != XR_SUCCESS) {
                sprintf(structTypeName, "<type %08X>", p->type);
            }
            OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                 nullptr, OverlaysLayerNoObjectInfo, fmt("Warning: Free called on %p of unknown type %d - will descend \\"next\\" but don't know any other pointers.", p, structTypeName).c_str());
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

source_text += """
bool RestoreActualHandles(XrInstance instance, XrBaseInStructure *xrstruct)
{
    while(xrstruct) {
        switch(xrstruct->type)
        {
"""
source_text += restore_handles_case_bodies
source_text += """
            default: {
                // I don't know what this is, skip it and try the next one
                auto info = OverlaysLayerGetHandleInfoFromXrInstance(instance);
                char structTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
                structTypeName[0] = '\\0';
                if(info->downchain->StructureTypeToString(instance, xrstruct->type, structTypeName) != XR_SUCCESS) {
                    sprintf(structTypeName, "<type %08X>", xrstruct->type);
                }
                OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                    nullptr, OverlaysLayerNoObjectInfo, fmt("RestoreActualHandles called on %p of unhandled type %s. Handles will not be substituted. Behavior will be undefined; expect a validation error.", xrstruct, structTypeName).c_str());
                break;
            }
        }
        xrstruct = (XrBaseInStructure*)xrstruct->next; /* We allocated this copy ourselves, so just cast ugly */
    }
    return true;
}

void SubstituteLocalHandles(XrInstance instance, XrBaseOutStructure *xrstruct)
{
    while(xrstruct) {
        switch(xrstruct->type)
        {
"""
source_text += substitute_handles_case_bodies
source_text += """
            default: {
                // I don't know what this is, skip it and try the next one
                auto info = OverlaysLayerGetHandleInfoFromXrInstance(instance);
                char structTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
                structTypeName[0] = '\\0';
                if(info->downchain->StructureTypeToString(instance, xrstruct->type, structTypeName) != XR_SUCCESS) {
                    sprintf(structTypeName, "<type %08X>", xrstruct->type);
                }
                OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                    nullptr, OverlaysLayerNoObjectInfo, fmt("SubstituteHandles called on %p of unhandled type %s. Handles will not be substituted. Behavior will be undefined; expect a validation error.", xrstruct, structTypeName).c_str());
                break;
            }
        }
        xrstruct = (XrBaseOutStructure*)xrstruct->next; /* We allocated this copy ourselves, so just cast ugly */
    }
}
"""


# make layer proc functions ----------------------------------------

for command_name in [c for c in supported_commands if c in manually_implemented_commands]:
    if command_name in after_downchain_main:
        print("after_downchain_main was specified for %s but also is in manually_implemented_commands." % command_name)
        sys.exit(1)

for command_name in [c for c in supported_commands if c not in manually_implemented_commands]:
    command = commands[command_name]

    # cdecls for arguments to function
    parameter_cdecls = ", ".join([parameter_to_cdecl(command_name, parameter) for parameter in command["parameters"]])

    # return type of command (at the moment always XrResult)
    command_type = command["return_type"]

    # prefix name of the function we're generating
    layer_command = api_layer_name_for_command(command_name)

    # name and type of handle passed to function
    handle_type = command["parameters"][0].find("type").text
    handle_name = command["parameters"][0].find("name").text

    # names of parameters joined by comma for use in calling downchain
    parameter_names = ", ".join([parameter_to_name(command_name, parameter) for parameter in command["parameters"]])

    # the name of the dispatch command
    dispatch_command = dispatch_name_for_command(command_name)

    # If this is an Xr...Create... function, find the created argument name and type and parent type and set a flag
    command_is_create = (command_name[2:8] == "Create")
    if command_is_create:
        created_type = command["parameters"][-1].find("type").text
        created_name = command["parameters"][-1].find("name").text
        if created_type in handles:
            created_types_parent_type = handles.get(created_type)[1]
        else:
            created_types_parent_type = ""
        end_of_parameters = -1
    else:
        end_of_parameters = len(command["parameters"])

    # If this is an XrDestroy... function, set a flag
    command_is_destroy = (command_name[2:9] == "Destroy")

    restore_preamble = ""
    undo_restore_postscript = ""
    substitute_postscript = ""

    for param in command["parameters"][1:end_of_parameters]:
        parameter_type = param.find("type").text
        parameter_name = param.find("name").text
        is_pointer = param.find("type").tail.strip()
        is_const = (param.text or "").strip() == "const"
        source_text += f"/* {command_name} PARAM {parameter_type} {parameter_name} {is_pointer} {is_const} ({param.text}) */\n"
# /* xrAcquireSwapchainImage PARAM XrSwapchainImageAcquireInfo acquireInfo * True (const ) */
        if is_an_xr_struct(parameter_type):
            if is_pointer:
                if is_const:
                    restore_preamble += f"""
    auto {parameter_name}Save = {parameter_name};
    auto {parameter_name}Copy = GetSharedCopyHandlesRestored({handle_name}Info->parentInstance, "{command_name}", {parameter_name});
    {parameter_name} = {parameter_name}Copy.get();
"""
                    undo_restore_postscript += f"""
    {parameter_name} = {parameter_name}Save;
"""
                else:
                    if is_an_xr_chained_struct(parameter_type):
                        substitute_postscript += f"""
        SubstituteLocalHandles({handle_name}Info->parentInstance, reinterpret_cast<XrBaseOutStructure*>({parameter_name}));
"""
                    else:
                        substitute_postscript += f"""
        SubstituteLocalHandles({handle_name}Info->parentInstance, {parameter_name});
"""
            else:
                pass # no struct parameters are passed by value at the time of writing
        elif is_an_xr_handle(parameter_type):
            if not is_pointer:
                restore_preamble += f"""
    auto {parameter_name}Save = {parameter_name};
    {parameter_name} = {layer_name}GetHandleInfoFrom{parameter_type}({parameter_name})->actualHandle;
"""
                undo_restore_postscript += f"""
    {parameter_name} = {parameter_name}Save;
"""
            else:
                restore_preamble += f'#error oh no {parameter_name} {parameter_type}'


    if command_is_create:
        # If this command creates a handle, figure out the parent handle to pass to the HandleInfo ctor
        if created_types_parent_type == "XrInstance":
            instance_ctor_parameter = "instance"
        else:
            instance_ctor_parameter = f"{handle_name}Info->parentInstance"

        # if the created handle needs to be translated to a local handle,
        # make code to create a local handle, swap it with the actual
        # handle, and store a map from actual to local handle for
        # substitution when runtime sends us back a handle.
        if created_type in handles_needing_substitution:
            allocate_local_handle_and_substitute = f"""
        {created_type} actualHandle = *{created_name};
        {created_type} localHandle = ({created_type})GetNextLocalHandle();
        *{created_name} = localHandle;

        {{
            std::unique_lock<std::recursive_mutex> lock(gActual{created_type}ToLocalHandleMutex);
            gActual{created_type}ToLocalHandle.insert({{actualHandle, localHandle}});
        }}
"""
            store_actual_handle = f"    {created_name}Info->actualHandle = actualHandle;\n"
        else:
            allocate_local_handle_and_substitute = ""
            store_actual_handle = ""


        make_and_store_new_local_handle = f"""
    {allocate_local_handle_and_substitute}

    {layer_name}{created_type}HandleInfo::Ptr {created_name}Info = std::make_shared<{layer_name}{created_type}HandleInfo>({handle_name}, {instance_ctor_parameter}, {handle_name}Info->downchain);

    {layer_name}AddHandleInfoFor{created_type}(*{created_name}, {created_name}Info);
    {store_actual_handle};
"""
    else:
        make_and_store_new_local_handle = ""

    if handle_type in handles_needing_substitution:
        command_for_main_side = f"""
{command_type} {layer_command}Main(XrInstance parentInstance, {parameter_cdecls})
{{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    XrResult result = XR_SUCCESS;

    auto {handle_name}Info = {layer_name}GetHandleInfoFrom{handle_type}({handle_name});

    // restore the actual handle
    {handle_type} localHandleStore = {handle_name};
    {handle_name} = {handle_name}Info->actualHandle;

    {restore_preamble}

    {before_downchain.get(command_name, "")}

    result = {handle_name}Info->downchain->{dispatch_command}({parameter_names});

    {undo_restore_postscript}

    if(result == XR_SUCCESS) {{
        {substitute_postscript}
    }}

    // put the local handle back
    {handle_name} = localHandleStore;

    {make_and_store_new_local_handle}

    return result;
}}
"""
    else:
        command_for_main_side = ""

    if handle_type in handles_needing_substitution:

        call_actual_command = f"""
    bool isProxied = {handle_name}Info->isProxied;
    XrResult result;
    if(isProxied) {{
        result = OverlaysLayer{dispatch_command}Overlay({handle_name}Info->parentInstance, {parameter_names});
    }} else {{
        result = OverlaysLayer{dispatch_command}Main({handle_name}Info->parentInstance, {parameter_names});
    }}
"""
    else:
        call_actual_command = f"""
    XrResult result;
    {{
        auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

        result = {handle_name}Info->downchain->{dispatch_command}({parameter_names});
    }}
"""

    if command_name in after_downchain_main:
        after_downchain_if_success = f"""
    if(XR_SUCCEEDED(result)) {{
        {after_downchain_main.get(command_name, "")}
    }}
"""
    else:
        after_downchain_if_success = ""

    if command_is_destroy:
        special_case_postscript = f"    {handle_name}Info->Destroy();\n"
    # elif other special cases
        # special_case_postscript = ...
    else:
        special_case_postscript = ""

    # handles are guaranteed not to be destroyed while in another command according to the spec
    api_layer_proc = f"""
{command_type} {layer_command}({parameter_cdecls})
{{
    try {{

        auto {handle_name}Info = {layer_name}GetHandleInfoFrom{handle_type}({handle_name});

        {call_actual_command}

        {special_case_postscript}

        {after_downchain_if_success}

        return result;

    }} catch (const OverlaysLayerXrException exc) {{

        return exc.result();

    }} catch (const std::bad_alloc& e) {{

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "{command_name}", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }}
}}
"""

    if command_for_main_side:
        source_text += command_for_main_side
    source_text += api_layer_proc

# make GetInstanceProcAddr ---------------------------------------------------

header_text += f"XrResult {layer_name}XrGetInstanceProcAddr( XrInstance instance, const char* name, PFN_xrVoidFunction* function);\n"


source_text += f"""
XrResult {layer_name}XrGetInstanceProcAddr( XrInstance instance, const char* name, PFN_xrVoidFunction* function)
{{
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
    // and has special handling for some objects, if we didn't set up the
    // function, we must not offer the downchain function.
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}
"""

if outputFilename == "xr_generated_overlays.cpp":
    open(outputFilename, "w").write(source_text)
else:
    open(outputFilename, "w").write(header_text)

# vi: set filetype=text
