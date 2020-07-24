import sys
import xml.etree.ElementTree as etree
print("hello")

tree = etree.parse('xr.xml')
root = tree.getroot()

out = open("output.dump", "w")

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

dump(0, 100, root)

commands = {}

reg_commands = root.find("commands")

CONST = "const"
NAME = "name"
POINTER = "pointer"
ARRAY = "array"

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

# XXX development placeholder
for command_name in supported_commands:
    command = commands[command_name]
    parameters = ", ".join([parameter_to_cdecl(command_name, parameter) for parameter in command["parameters"]])
    print("%s %s(%s)" % (command["return_type"], command["name"], parameters))

"""
    commands : {}
        command : {'successcodes': 'XR_SUCCESS', 'errorcodes': 'XR_ERROR_HANDLE_INVALID,XR_ERROR_INSTANCE_LOST,XR_ERROR_RUNTIME_FAILURE,XR_ERROR_VALIDATION_FAILURE'}
            proto : {}
                type : {}
                    text = "XrResult"
                name : {}
                    text = "xrResultToString"
            param : {}
                type : {}
                    text = "XrInstance"
                name : {}
                    text = "instance"
            param : {}
                type : {}
                    text = "XrResult"
                name : {}
                    text = "value"
            param : {}
                type : {}
                    text = "char"
                name : {}
                    text = "buffer"
                    tail = "["
                enum : {}
                    text = "XR_MAX_RESULT_STRING_SIZE"
                    tail = "]"
"""
