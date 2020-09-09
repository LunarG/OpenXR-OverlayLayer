import sys

well_known_strings = set()

# As of 1.0.11
path_specs = """
Path: pathname:/interaction_profiles/khr/simple_controller
* pathname:/user/hand/left
* pathname:/user/hand/right
* subpathname:/input/select/click
* subpathname:/input/menu/click
* subpathname:/input/grip/pose
* subpathname:/input/aim/pose
* subpathname:/output/haptic
Path: pathname:/interaction_profiles/htc/vive_controller
* pathname:/user/hand/left
* pathname:/user/hand/right
* subpathname:/input/system/click
* subpathname:/input/squeeze/click
* subpathname:/input/menu/click
* subpathname:/input/trigger/click
* subpathname:/input/trigger/value
* subpathname:/input/trackpad/x
* subpathname:/input/trackpad/y
* subpathname:/input/trackpad/click
* subpathname:/input/trackpad/touch
* subpathname:/input/grip/pose
* subpathname:/input/aim/pose
* subpathname:/output/haptic
Path: pathname:/interaction_profiles/htc/vive_pro
* pathname:/user/head
* subpathname:/input/system/click
* subpathname:/input/volume_up/click
* subpathname:/input/volume_down/click
* subpathname:/input/mute_mic/click
Path: pathname:/interaction_profiles/microsoft/motion_controller
* pathname:/user/hand/left
* pathname:/user/hand/right
* subpathname:/input/menu/click
* subpathname:/input/squeeze/click
* subpathname:/input/trigger/value
* subpathname:/input/thumbstick/x
* subpathname:/input/thumbstick/y
* subpathname:/input/thumbstick/click
* subpathname:/input/trackpad/x
* subpathname:/input/trackpad/y
* subpathname:/input/trackpad/click
* subpathname:/input/trackpad/touch
* subpathname:/input/grip/pose
* subpathname:/input/aim/pose
* subpathname:/output/haptic
Path: pathname:/interaction_profiles/microsoft/xbox_controller
* pathname:/user/gamepad
* subpathname:/input/menu/click
* subpathname:/input/view/click
* subpathname:/input/a/click
* subpathname:/input/b/click
* subpathname:/input/x/click
* subpathname:/input/y/click
* subpathname:/input/dpad_down/click
* subpathname:/input/dpad_right/click
* subpathname:/input/dpad_up/click
* subpathname:/input/dpad_left/click
* subpathname:/input/shoulder_left/click
* subpathname:/input/shoulder_right/click
* subpathname:/input/thumbstick_left/click
* subpathname:/input/thumbstick_right/click
* subpathname:/input/trigger_left/value
* subpathname:/input/trigger_right/value
* subpathname:/input/thumbstick_left/x
* subpathname:/input/thumbstick_left/y
* subpathname:/input/thumbstick_right/x
* subpathname:/input/thumbstick_right/y
* subpathname:/output/haptic_left
* subpathname:/output/haptic_right
* subpathname:/output/haptic_left_trigger
* subpathname:/output/haptic_right_trigger
Path: pathname:/interaction_profiles/oculus/go_controller
* pathname:/user/hand/left
* pathname:/user/hand/right
* subpathname:/input/system/click
* subpathname:/input/trigger/click
* subpathname:/input/back/click
* subpathname:/input/trackpad/x
* subpathname:/input/trackpad/y
* subpathname:/input/trackpad/click
* subpathname:/input/trackpad/touch
* subpathname:/input/grip/pose
* subpathname:/input/aim/pose
Path: pathname:/interaction_profiles/oculus/touch_controller
* pathname:/user/hand/left
* pathname:/user/hand/right
* On pathname:/user/hand/left only:
** subpathname:/input/x/click
** subpathname:/input/x/touch
** subpathname:/input/y/click
** subpathname:/input/y/touch
** subpathname:/input/menu/click
* On pathname:/user/hand/right only:
** subpathname:/input/a/click
** subpathname:/input/a/touch
** subpathname:/input/b/click
** subpathname:/input/b/touch
** subpathname:/input/system/click
* subpathname:/input/squeeze/value
* subpathname:/input/trigger/value
* subpathname:/input/trigger/touch
* subpathname:/input/thumbstick/x
* subpathname:/input/thumbstick/y
* subpathname:/input/thumbstick/click
* subpathname:/input/thumbstick/touch
* subpathname:/input/thumbrest/touch
* subpathname:/input/grip/pose
* subpathname:/input/aim/pose
* subpathname:/output/haptic
Path: pathname:/interaction_profiles/valve/index_controller
* pathname:/user/hand/left
* pathname:/user/hand/right
* subpathname:/input/system/click
* subpathname:/input/system/touch
* subpathname:/input/a/click
* subpathname:/input/a/touch
* subpathname:/input/b/click
* subpathname:/input/b/touch
* subpathname:/input/squeeze/value
* subpathname:/input/squeeze/force
* subpathname:/input/trigger/click
* subpathname:/input/trigger/value
* subpathname:/input/trigger/touch
* subpathname:/input/thumbstick/x
* subpathname:/input/thumbstick/y
* subpathname:/input/thumbstick/click
* subpathname:/input/thumbstick/touch
* subpathname:/input/trackpad/x
* subpathname:/input/trackpad/y
* subpathname:/input/trackpad/force
* subpathname:/input/trackpad/touch
* subpathname:/input/grip/pose
* subpathname:/input/aim/pose
* subpathname:/output/haptic
"""

placeholder_profiles = {}
for l in path_specs.splitlines():
    if l.startswith("Path: pathname:"):
        profile = l.split(":")[2]
        # print("profile %s" % profile)
        well_known_strings.add(profile)
        placeholder_profiles[profile] = {}
        pathnames = []
    elif l.startswith("* pathname:"):
        top_level = l.split(":")[1]
        # print("    top level path %s" % top_level)
        well_known_strings.add(top_level)
        placeholder_profiles[profile][top_level] = set()
        pathnames.append(top_level)
    elif l.startswith("* subpathname:"):
        component = l.split(":")[1]
        # print("        component %s" % component)
        well_known_strings.add(component)
        for top_level in pathnames:
            well_known_strings.add(top_level + component)
            placeholder_profiles[profile][top_level].add(component)
        if component.endswith("x") or component.endswith("y"):
            well_known_strings.add(component[:-2])
            for top_level in pathnames:
                well_known_strings.add(top_level + component[:-2])
            # don't add upper component for vector2f here
            # that's added below, once, for just .x

def to_upper_snake(str) :
    return "_".join([s.upper() for s in str[1:].split("/")])

num = 1
well_known_enums = ""
well_known_mappings = ""
for str in well_known_strings:
    well_known_enums += "    " + to_upper_snake(str) + " = %d,\n" % num
    well_known_mappings += "    {" + to_upper_snake(str) + ', "' + str + '"},\n'
    num += 1

placeholder_ids = ""

for (profile, top_levels) in placeholder_profiles.items():
    for (top_level, components) in top_levels.items():
        for component in components:
            if component.endswith("value") or component.endswith("x") or component.endswith("y") or component.endswith("force") or component.endswith("touch"):
                type = "XR_ACTION_TYPE_FLOAT_INPUT"
            elif component.endswith("click"):
                type = "XR_ACTION_TYPE_BOOLEAN_INPUT"
            elif component.endswith("pose"):
                type = "XR_ACTION_TYPE_POSE_INPUT"
            elif component.endswith("haptic") or component.endswith("haptic_left") or component.endswith("haptic_right") or component.endswith("haptic_left_trigger") or component.endswith("haptic_right_trigger"):
                type = "XR_ACTION_TYPE_VIBRATION_OUTPUT"
            else:
                print("oh, crap: %s" % component)
                sys.exit(1);
            placeholder_ids += '    {"' + profile + top_level + component + '", ' + type + ', ' + to_upper_snake(profile) + ", " + to_upper_snake(top_level) + ", " + to_upper_snake(component) + ", " + to_upper_snake(top_level + component) + "},\n"
            if component.endswith("x"):
                placeholder_ids += '    {"' + profile + top_level + component[:-2] + '", XR_ACTION_TYPE_VECTOR2F_INPUT, ' + to_upper_snake(profile) + ", " + to_upper_snake(top_level) + ", " + to_upper_snake(component[:-2]) + ", " + to_upper_snake(top_level + component[:-2]) + "},\n"


well_known = f"""
enum WellKnownStringIndex {{
{well_known_enums}
}}; // These will need to not change for subsequent versions for backward compatibility

std::unordered_map<WellKnownStringIndex, const char *> OverlaysLayerWellKnownStrings = {{
{well_known_mappings}
}};

std::unordered_map<WellKnownStringIndex, XrPath> OverlaysLayerWellKnownStringToPath;
std::unordered_map<XrPath, WellKnownStringIndex> OverlaysLayerPathToWellKnownString;
std::unordered_map<XrPath, XrPath> OverlaysLayerBindingToSubAction;

"""

placeholders = f"""

struct PlaceholderActionId
{{
    std::string name;
    XrActionType type;
    WellKnownStringIndex interactionProfileString;
    WellKnownStringIndex subActionString;
    WellKnownStringIndex componentString;
    WellKnownStringIndex fullBindingString;
}};

std::vector<PlaceholderActionId> PlaceholderActionIds =
{{
{placeholder_ids}
}};
"""

print(well_known,)
print(placeholders)

