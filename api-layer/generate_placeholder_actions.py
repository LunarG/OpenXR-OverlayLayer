# 
# Copyright (c) 2020-2021 LunarG Inc.
# Copyright (c) 2020-2021 PlutoVR Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Author: Brad Grantham <brad@lunarg.com>
#
import sys

list_join_str = ",\n    "

well_known_strings = set()

path_spec_list = []
# As of 1.0.11, these are the paths for input allowlist, from semantic_paths.adoc
simple_controller_paths = """
Path: pathname:/interaction_profiles/khr/simple_controller
* pathname:/user/hand/left
* pathname:/user/hand/right
* subpathname:/input/select/click
* subpathname:/input/menu/click
* subpathname:/input/grip/pose
* subpathname:/input/aim/pose
* subpathname:/output/haptic
"""
path_spec_list.append(simple_controller_paths)

daydream_paths = """
Path: pathname:/interaction_profiles/google/daydream_controller
* pathname:/user/hand/left
* pathname:/user/hand/right
* subpathname:/input/select/click
* subpathname:/input/trackpad/x
* subpathname:/input/trackpad/y
* subpathname:/input/trackpad/click
* subpathname:/input/trackpad/touch
* subpathname:/input/grip/pose
* subpathname:/input/aim/pose
"""
path_spec_list.append(daydream_paths)

vive_paths = """
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
"""
path_spec_list.append(vive_paths)

microsoft_top_path = """
Path: pathname:/interaction_profiles/microsoft/motion_controller
"""

samsung_top_path = """
Path: pathname:/interaction_profiles/samsung/odyssey_controller
EXTENSION:XR_EXT_SAMSUNG_ODYSSEY_CONTROLLER_EXTENSION_NAME
"""

microsoft_samsung_common_paths = """
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
"""

path_spec_list.extend([microsoft_top_path, microsoft_samsung_common_paths, samsung_top_path, microsoft_samsung_common_paths]);

xbox_paths = """
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
"""
path_spec_list.append(xbox_paths)

occulus_paths = """
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
"""
path_spec_list.append(occulus_paths)

valve_paths = """
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
path_spec_list.append(valve_paths)

# From XR_EXT_hp_mixed_reality_controller (which the MSFT "Demo Scene"
# provides profile bindings for and is chosen for the Samsung Odyssey
# controllers, edited to match what this script expects
hp_mrc_paths = """
Path: pathname:/interaction_profiles/hp/mixed_reality_controller
EXTENSION:XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME
* pathname:/user/hand/left
* pathname:/user/hand/right
* On pathname:/user/hand/left only
** subpathname:/input/x/click
** subpathname:/input/y/click
* On pathname:/user/hand/right only
** subpathname:/input/a/click
** subpathname:/input/b/click
* On both hands
* subpathname:/input/menu/click
* subpathname:/input/squeeze/value
* subpathname:/input/trigger/value
* subpathname:/input/thumbstick/x
* subpathname:/input/thumbstick/y
* subpathname:/input/thumbstick/click
* subpathname:/input/grip/pose
* subpathname:/input/aim/pose
* subpathname:/output/haptic
"""
path_spec_list.append(hp_mrc_paths)

eye_gaze_paths = """
Path: pathname:/interaction_profiles/ext/eye_gaze_interaction
EXTENSION:XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME
* pathname:/user/eyes_ext
* subpathname:/input/gaze_ext/pose
"""
path_spec_list.append(eye_gaze_paths)

vive_cosmos_paths = """
Path: pathname:/interaction_profiles/htc/vive_cosmos_controller
EXTENSION:XR_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_EXTENSION_NAME
* pathname:/user/hand/left
* pathname:/user/hand/right
* On pathname:/user/hand/left only:
** subpathname:/input/x/click
** subpathname:/input/y/click
** subpathname:/input/menu/click
* On pathname:/user/hand/right only:
** subpathname:/input/a/click
** subpathname:/input/b/click
** subpathname:/input/system/click
   use)
* subpathname:/input/shoulder/click
* subpathname:/input/squeeze/click
* subpathname:/input/trigger/click
* subpathname:/input/trigger/value
* subpathname:/input/thumbstick/x
* subpathname:/input/thumbstick/y
* subpathname:/input/thumbstick/click
* subpathname:/input/thumbstick/touch
* subpathname:/input/grip/pose
* subpathname:/input/aim/pose
* subpathname:/output/haptic
"""
path_spec_list.append(vive_cosmos_paths)

# Microsoft Hand Controller  -- Hand interaction profile
msft_hand_controller = """
Path: pathname:/interaction_profiles/microsoft/hand_interaction
EXTENSION:XR_MSFT_HAND_INTERACTION_EXTENSION_NAME
* pathname:/user/hand/left
* pathname:/user/hand/right
* subpathname:/input/select/value
* subpathname:/input/squeeze/value
* subpathname:/input/aim/pose
* subpathname/input/grip/pose
"""
path_spec_list.append(msft_hand_controller)

placeholder_profiles = {}
profile_extension = {}
extensions = set()
for l in "\n".join(path_spec_list).splitlines():
    if l.startswith("Path: pathname:"):
        profile = l.split(":")[2]
        # print("profile %s" % profile)
        well_known_strings.add(profile)
        placeholder_profiles[profile] = {}
        pathnames = []
        profile_extension[profile] = []
    elif l.startswith("EXTENSION:"):
        extension_name = l.split(":")[1]
        profile_extension[profile].append(extension_name)
        extensions.add(extension_name)
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
    elif l.startswith("* On pathname:"):
        # This restricts following subpathnames to only this top_level path
        top_level_restriction = l.split(":")[1].split(" ")[0]
        # print("    top level path restriction %s" % top_level_restriction)
    elif l.startswith("** subpathname:"):
        # This subpath is only in the previously-given restricted top_level path
        component = l.split(":")[1]
        # print("        component %s" % component)
        well_known_strings.add(component)
        well_known_strings.add(top_level_restriction + component)
        placeholder_profiles[profile][top_level_restriction].add(component)
        if component.endswith("x") or component.endswith("y"):
            well_known_strings.add(component[:-2])
            well_known_strings.add(top_level_restriction + component[:-2])
            # don't add upper component for vector2f here
            # that's added below, once, for just .x


def to_upper_snake(str) :
    return "_".join([s.upper() for s in str[1:].split("/")])

# Construct the well known string enum and map
num = 1
well_known_enums = ""
well_known_mappings = ""
well_known_dict = {} # The extension enum will find this useful
for str in sorted(well_known_strings):
    enum_name = to_upper_snake(str) 
    well_known_dict[str] = enum_name
    well_known_enums += "    " + enum_name + " = %d,\n" % num
    well_known_mappings += "    {" + enum_name + ', "' + str + '"},\n'
    num += 1

# Construct the extension mask enum and map
ext_none_bit = "REQ_EXTENSION_NONE_MASK"
ext_masks = list()
ext_masks.append(f"{ext_none_bit} = 0")
ext_bit = 0;
ext_bits = {}
ext_bit_list = []
for ext in sorted(extensions):
    bit_name = "REQ_" + ext[3:-15] + "_BIT"
    ext_bits[ext] = bit_name
    ext_masks.append(f"{bit_name} = (1 << {ext_bit})")
    ext_bit_list.append(f'{{{ext}, {bit_name}}}') # No quotes because the ext is the name of a #define string literal
    ext_bit += 1

def profile_extension_mask(profile):
    return "REQ_" + well_known_dict[profile] + "_MASK"

for profile in sorted(profile_extension.keys()):
    ext_list = profile_extension[profile]
    enum_name = profile_extension_mask(profile)
    if (ext_list):
        ext_mask = " | ".join([ext_bits[e] for e in ext_list]) 
        if len(ext_list) > 1 :
            ext_mask = "(" + ext_mask + ")"
    else:
        ext_mask = ext_none_bit
    ext_masks.append(f"{enum_name} = {ext_mask}")



# Construct the placeholder entries
placeholder_ids = ""

extension_map_list = []
for profile in sorted(placeholder_profiles.keys()):
    top_levels = placeholder_profiles[profile]
    ext_list = ", ".join(profile_extension[profile])
    extension_map_list.append(f"{{{well_known_dict[profile]}, {profile_extension_mask(profile)}}}")
    for top_level in sorted(top_levels.keys()):
        components = top_levels[top_level]
        for component in sorted(components):
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


well_known_decl = f"""
// All applications within an overlay session must use the same version of the overlay layer to ensure compatibility, or the some
// RPC to share the main overlay's enum -> string map must be added.
enum WellKnownStringIndex {{
    NULL_PATH = 0,
{well_known_enums}
}};

"""

ext_req_type = "ProfileExtensionReqMask"
ext_enum_decl = f"""
enum {ext_req_type} {{
    {list_join_str.join(ext_masks)}
}};

"""

well_known_def = f"""
std::unordered_map<WellKnownStringIndex, const char *> OverlaysLayerWellKnownStrings = {{
{well_known_mappings}
}};

"""

extension_map_def = f"""
std::unordered_map<std::string, {ext_req_type}> ExtensionsBits = {{
    {list_join_str.join(ext_bit_list)}
}};

std::unordered_map<WellKnownStringIndex, {ext_req_type}> ProfileExtensions = {{
    {list_join_str.join(extension_map_list)}
}};

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

print(well_known_decl)
print(ext_enum_decl)
print(extension_map_def)
print(well_known_def)
print(placeholders)

