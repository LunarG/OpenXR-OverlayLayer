import sys
import xml.etree.ElementTree as etree
print("hello")

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


def dump(indent, depth, element):
    # if element.tag == "type" and element.attrib.get("name", "") == "XrEventDataBuffer":
        # out.write("%s\n" % dir(element[2]))
    if depth == 0:
        return
    out.write("%s%s : %s\n" % (" " * indent, element.tag, element.attrib))
    if element.text:
        if len(element.text.strip()) > 0:
            out.write("%stext = \"%s\"\n" % (" " * (indent + 4), element.text.strip()))
    for child in element:
        dump(indent + 4, depth - 1, child)
    if element.tail:
        if len(element.tail.strip()) > 0:
            out.write("%stail = \"%s\"\n" % (" " * (indent + 4), element.tail.strip()))

LayerName = "OverlaysLayer"
MutexHandles = False

registry = sys.argv[1]
output_name = sys.argv[2]


tree = etree.parse('xr.xml')
root = tree.getroot()

out = open("output.dump", "w")

dump(0, 100, root)


commands = {}

reg_commands = root.find("commands")

for reg_command in reg_commands:
    print("%s" % (reg_command.find("proto").find("name").text))
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

for reg_type in reg_types:
    if reg_type.attrib.get("category", "") == "handle":
        handle_name = reg_type.find("name").text
        handles[handle_name] = (handle_name, reg_type.attrib.get("parent", ""))

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

if True:
    for handle in supported_handles:
        if not handles[handle][1]:
            print("handle %s" % (handles[handle][0]))
        else:
            print("handle %s has parent type %s" % (handles[handle][0], handles[handle][1]))

"""
std::unordered_map<XrInstance, XrGeneratedDispatchTable*> g_instance_dispatch_map;
std::mutex g_instance_dispatch_mutex;
std::unordered_map<XrSession, XrGeneratedDispatchTable*> g_session_dispatch_map;
std::mutex g_session_dispatch_mutex;
std::unordered_map<XrSpace, XrGeneratedDispatchTable*> g_space_dispatch_map;
std::mutex g_space_dispatch_mutex;
std::unordered_map<XrAction, XrGeneratedDispatchTable*> g_action_dispatch_map;
std::mutex g_action_dispatch_mutex;
std::unordered_map<XrSwapchain, XrGeneratedDispatchTable*> g_swapchain_dispatch_map;
std::mutex g_swapchain_dispatch_mutex;
std::unordered_map<XrActionSet, XrGeneratedDispatchTable*> g_actionset_dispatch_map;
std::mutex g_actionset_dispatch_mutex;
std::unordered_map<XrDebugUtilsMessengerEXT, XrGeneratedDispatchTable*> g_debugutilsmessengerext_dispatch_map;
std::mutex g_debugutilsmessengerext_dispatch_mutex;
"""

only_child_handles = [h for h in supported_handles if h != "XrInstance"]

source_text = ""
header_text = ""

for handle_name in handles.keys():
    header_text += f"struct {LayerName}%sHandleInfo\n" % (handle_name)
    header_text += "{\n"
    header_text += "        std::set<uint64_t> childHandles;\n"
    parent_name = str(handles[handle_name][1] or "")
    if parent_name:
        header_text += "        %s parentHandle;\n" % (parent_name)
    header_text += "        XrGeneratedDispatchTable *downchain;\n"
    if parent_name:
        header_text += f"       {LayerName}%sHandleInfo(%s parent, XrGeneratedDispatchTable *downchain_) : \n" % (handle_name, parent_name)
        header_text += "           parentHandle(parent),\n"
        header_text += "           downchain(downchain_)\n"
        header_text += "         {}\n"
    else:
        header_text += f"       {LayerName}%sHandleInfo(XrGeneratedDispatchTable *downchain) : \n" % (handle_name)
        header_text += "           downchain(downchain_)\n"
        header_text += "         {}\n"
    header_text += "};\n"

    source_text = "" 
    source_text += f"std::unordered_map<%s, {LayerName}%sHandleInfo> g{LayerName}%sToHandleInfo;\n" % (handle_name, handle_name, handle_name)
    if MutexHandles:
        source_text += f"std::mutex g{LayerName}%sToHandleInfoMutex;\n" % (handle_name)
    source_text += ";\n"


for command_name in supported_commands:
    command = commands[command_name]

    parameters = ", ".join([parameter_to_cdecl(command_name, parameter) for parameter in command["parameters"]])
    source_text += "%s %s(%s)\n" % (command["return_type"], api_layer_name_for_command(command_name), parameters)
    source_text += "{\n"

    handle_parameter_type = command["parameters"][0].find("type").text
    handle_parameter_name = command["parameters"][0].find("name").text

    # handles are guaranteed not to be destroyed while in another command according to the spec...
    if MutexHandles:
        source_text += f"    std::unique_lock<std::mutex> mlock(g{LayerName}%sToHandleInfoMutex);\n" % (handle_parameter_type)
    source_text += f"    {LayerName}%sHandleInfo& %sInfo = g{LayerName}%sToHandleInfo[%s];\n\n" % (handle_parameter_type, handle_parameter_name, handle_parameter_type, handle_parameter_name)

    parameter_names = ", ".join([parameter_to_name(command_name, parameter) for parameter in command["parameters"]])
    source_text += "    XrResult result = %sInfo.dispatch->%s(%s);\n\n" % (handle_parameter_name, dispatch_name_for_command(command_name), parameter_names)

    if command_name.find("Create") >= 0:
        created_parameter_type = command["parameters"][-1].find("type").text
        created_parameter_name = command["parameters"][-1].find("name").text
        # just assume we hand-coded Instance creation so these are all child handle types
        source_text += f"    g{LayerName}%sToHandleInfo.emplace_back(*%s, %s, %sInfo.dispatch);\n" % (created_parameter_type, created_parameter_name, handle_parameter_name, handle_parameter_name)
        source_text += f"    %sInfo.childHandles.insert(*%s);\n" % (handle_parameter_name, created_parameter_name)

    source_text += "    return result;\n"

    source_text += "}\n"
    source_text += "\n";

if output_name == "xr_generated_overlays.cpp":
    open(output_name, "w").write(source_text)
else:
    open(output_name, "w").write(header_text)
