// Copyright (c) 2017-2020 The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2020 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Mark Young <marky@lunarg.com>
// Author: Dave Houlton <daveh@lunarg.com>
// Author: Brad Grantham <brad@lunarg.com>


#ifndef NOMINMAX
#define NOMINMAX
#endif  // !NOMINMAX

#include "loader_interfaces.h"
#include "platform_utils.hpp"

#include "overlays.h"

#include "xr_generated_overlays.hpp"
#include "xr_generated_dispatch_table.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <unordered_set>

#include <dxgi1_2.h>
#include <d3d11_1.h>
#include <d3d11_4.h>
//#include <d3d12.h>



#if defined(__GNUC__) && __GNUC__ >= 4
#define LAYER_EXPORT __attribute__((visibility("default")))
#elif defined(_WIN32)
#define LAYER_EXPORT __declspec(dllexport)
#else
#define LAYER_EXPORT
#endif

const char *kOverlayLayerName = "xr_extx_overlay";

constexpr bool PrintDebugInfo = true;


// XXX debug this *could* be done at runtime with hashes instead of indices.
std::unordered_map<WellKnownStringIndex, const char *> OverlaysLayerWellKnownStrings = {
    {USER_HAND_RIGHT_OUTPUT_HAPTIC, "/user/hand/right/output/haptic"},
    {USER_HAND_RIGHT_INPUT_TRACKPAD_CLICK, "/user/hand/right/input/trackpad/click"},
    {INPUT_THUMBSTICK_RIGHT, "/input/thumbstick_right"},
    {USER_GAMEPAD_INPUT_TRIGGER_LEFT_VALUE, "/user/gamepad/input/trigger_left/value"},
    {OUTPUT_HAPTIC, "/output/haptic"},
    {USER_HAND_LEFT_INPUT_TRIGGER_VALUE, "/user/hand/left/input/trigger/value"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_Y, "/user/gamepad/input/thumbstick_left/y"},
    {USER_HAND_RIGHT_INPUT_TRACKPAD_Y, "/user/hand/right/input/trackpad/y"},
    {INPUT_THUMBSTICK_CLICK, "/input/thumbstick/click"},
    {USER_HAND_LEFT_INPUT_TRIGGER_CLICK, "/user/hand/left/input/trigger/click"},
    {USER_HAND_LEFT_INPUT_TRACKPAD_CLICK, "/user/hand/left/input/trackpad/click"},
    {INPUT_TRACKPAD_TOUCH, "/input/trackpad/touch"},
    {USER_HAND_LEFT_INPUT_THUMBSTICK_CLICK, "/user/hand/left/input/thumbstick/click"},
    {USER_HAND_RIGHT_INPUT_SYSTEM_CLICK, "/user/hand/right/input/system/click"},
    {INPUT_THUMBSTICK, "/input/thumbstick"},
    {OUTPUT_HAPTIC_LEFT, "/output/haptic_left"},
    {INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, "/interaction_profiles/htc/vive_controller"},
    {USER_HAND_RIGHT_INPUT_A_TOUCH, "/user/hand/right/input/a/touch"},
    {USER_GAMEPAD_INPUT_DPAD_LEFT_CLICK, "/user/gamepad/input/dpad_left/click"},
    {USER_HAND_RIGHT_INPUT_TRACKPAD_FORCE, "/user/hand/right/input/trackpad/force"},
    {USER_GAMEPAD, "/user/gamepad"},
    {INPUT_THUMBSTICK_LEFT_Y, "/input/thumbstick_left/y"},
    {USER_HAND_LEFT_INPUT_MENU_CLICK, "/user/hand/left/input/menu/click"},
    {INPUT_THUMBSTICK_RIGHT_Y, "/input/thumbstick_right/y"},
    {INPUT_TRACKPAD_CLICK, "/input/trackpad/click"},
    {INPUT_THUMBSTICK_LEFT, "/input/thumbstick_left"},
    {USER_HEAD_INPUT_SYSTEM_CLICK, "/user/head/input/system/click"},
    {USER_HAND_LEFT_INPUT_B_CLICK, "/user/hand/left/input/b/click"},
    {USER_HAND_RIGHT_INPUT_THUMBSTICK_TOUCH, "/user/hand/right/input/thumbstick/touch"},
    {USER_HAND_RIGHT_INPUT_AIM_POSE, "/user/hand/right/input/aim/pose"},
    {USER_HAND_LEFT_INPUT_SELECT_CLICK, "/user/hand/left/input/select/click"},
    {INPUT_BACK_CLICK, "/input/back/click"},
    {USER_HAND_RIGHT_INPUT_TRIGGER_VALUE, "/user/hand/right/input/trigger/value"},
    {USER_HAND_RIGHT_INPUT_TRIGGER_TOUCH, "/user/hand/right/input/trigger/touch"},
    {USER_HAND_RIGHT, "/user/hand/right"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_X, "/user/gamepad/input/thumbstick_left/x"},
    {USER_HAND_LEFT_INPUT_A_TOUCH, "/user/hand/left/input/a/touch"},
    {USER_GAMEPAD_INPUT_SHOULDER_LEFT_CLICK, "/user/gamepad/input/shoulder_left/click"},
    {INPUT_Y_CLICK, "/input/y/click"},
    {USER_GAMEPAD_INPUT_DPAD_UP_CLICK, "/user/gamepad/input/dpad_up/click"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT, "/user/gamepad/input/thumbstick_right"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_Y, "/user/gamepad/input/thumbstick_right/y"},
    {USER_HAND_RIGHT_INPUT_SYSTEM_TOUCH, "/user/hand/right/input/system/touch"},
    {INPUT_THUMBSTICK_TOUCH, "/input/thumbstick/touch"},
    {USER_HAND_LEFT_INPUT_TRACKPAD_FORCE, "/user/hand/left/input/trackpad/force"},
    {INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, "/interaction_profiles/valve/index_controller"},
    {INPUT_SQUEEZE_FORCE, "/input/squeeze/force"},
    {USER_HAND_LEFT_INPUT_TRACKPAD_TOUCH, "/user/hand/left/input/trackpad/touch"},
    {USER_HEAD_INPUT_VOLUME_UP_CLICK, "/user/head/input/volume_up/click"},
    {INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, "/interaction_profiles/oculus/go_controller"},
    {INPUT_MENU_CLICK, "/input/menu/click"},
    {USER_HAND_RIGHT_INPUT_GRIP_POSE, "/user/hand/right/input/grip/pose"},
    {INPUT_SYSTEM_CLICK, "/input/system/click"},
    {USER_HAND_LEFT_INPUT_THUMBSTICK_Y, "/user/hand/left/input/thumbstick/y"},
    {INPUT_THUMBSTICK_RIGHT_CLICK, "/input/thumbstick_right/click"},
    {INPUT_TRIGGER_LEFT_VALUE, "/input/trigger_left/value"},
    {USER_HAND_LEFT_INPUT_BACK_CLICK, "/user/hand/left/input/back/click"},
    {USER_GAMEPAD_INPUT_X_CLICK, "/user/gamepad/input/x/click"},
    {USER_GAMEPAD_INPUT_TRIGGER_RIGHT_VALUE, "/user/gamepad/input/trigger_right/value"},
    {USER_HAND_RIGHT_INPUT_TRACKPAD, "/user/hand/right/input/trackpad"},
    {INPUT_SYSTEM_TOUCH, "/input/system/touch"},
    {USER_HAND_RIGHT_INPUT_TRACKPAD_TOUCH, "/user/hand/right/input/trackpad/touch"},
    {USER_GAMEPAD_INPUT_Y_CLICK, "/user/gamepad/input/y/click"},
    {USER_GAMEPAD_INPUT_A_CLICK, "/user/gamepad/input/a/click"},
    {USER_HAND_RIGHT_INPUT_THUMBSTICK_X, "/user/hand/right/input/thumbstick/x"},
    {INPUT_THUMBSTICK_LEFT_X, "/input/thumbstick_left/x"},
    {USER_HAND_RIGHT_INPUT_TRIGGER_CLICK, "/user/hand/right/input/trigger/click"},
    {INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, "/interaction_profiles/microsoft/xbox_controller"},
    {INPUT_THUMBSTICK_LEFT_CLICK, "/input/thumbstick_left/click"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_CLICK, "/user/gamepad/input/thumbstick_left/click"},
    {USER_HEAD_INPUT_VOLUME_DOWN_CLICK, "/user/head/input/volume_down/click"},
    {USER_HAND_LEFT_INPUT_THUMBSTICK_TOUCH, "/user/hand/left/input/thumbstick/touch"},
    {USER_HAND_RIGHT_INPUT_THUMBSTICK, "/user/hand/right/input/thumbstick"},
    {USER_HAND_RIGHT_INPUT_SQUEEZE_CLICK, "/user/hand/right/input/squeeze/click"},
    {USER_HAND_RIGHT_INPUT_SQUEEZE_FORCE, "/user/hand/right/input/squeeze/force"},
    {USER_HAND_LEFT_INPUT_SQUEEZE_FORCE, "/user/hand/left/input/squeeze/force"},
    {INPUT_TRACKPAD_Y, "/input/trackpad/y"},
    {INPUT_THUMBSTICK_Y, "/input/thumbstick/y"},
    {USER_HEAD, "/user/head"},
    {USER_HAND_LEFT_INPUT_TRACKPAD_Y, "/user/hand/left/input/trackpad/y"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_CLICK, "/user/gamepad/input/thumbstick_right/click"},
    {USER_GAMEPAD_OUTPUT_HAPTIC_LEFT, "/user/gamepad/output/haptic_left"},
    {USER_HAND_LEFT_INPUT_TRIGGER_TOUCH, "/user/hand/left/input/trigger/touch"},
    {USER_HAND_RIGHT_INPUT_THUMBSTICK_CLICK, "/user/hand/right/input/thumbstick/click"},
    {INPUT_A_TOUCH, "/input/a/touch"},
    {INPUT_TRIGGER_TOUCH, "/input/trigger/touch"},
    {OUTPUT_HAPTIC_RIGHT_TRIGGER, "/output/haptic_right_trigger"},
    {INTERACTION_PROFILES_HTC_VIVE_PRO, "/interaction_profiles/htc/vive_pro"},
    {USER_HAND_RIGHT_INPUT_TRACKPAD_X, "/user/hand/right/input/trackpad/x"},
    {USER_HAND_LEFT, "/user/hand/left"},
    {INPUT_AIM_POSE, "/input/aim/pose"},
    {INPUT_SQUEEZE_VALUE, "/input/squeeze/value"},
    {USER_HAND_LEFT_INPUT_THUMBREST_TOUCH, "/user/hand/left/input/thumbrest/touch"},
    {USER_HAND_RIGHT_INPUT_B_TOUCH, "/user/hand/right/input/b/touch"},
    {INPUT_VIEW_CLICK, "/input/view/click"},
    {INPUT_SELECT_CLICK, "/input/select/click"},
    {USER_HAND_LEFT_INPUT_AIM_POSE, "/user/hand/left/input/aim/pose"},
    {USER_HAND_LEFT_INPUT_SQUEEZE_VALUE, "/user/hand/left/input/squeeze/value"},
    {USER_HAND_LEFT_INPUT_SYSTEM_CLICK, "/user/hand/left/input/system/click"},
    {INPUT_TRIGGER_VALUE, "/input/trigger/value"},
    {INPUT_SHOULDER_RIGHT_CLICK, "/input/shoulder_right/click"},
    {INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, "/interaction_profiles/oculus/touch_controller"},
    {USER_HAND_LEFT_INPUT_SYSTEM_TOUCH, "/user/hand/left/input/system/touch"},
    {USER_HEAD_INPUT_MUTE_MIC_CLICK, "/user/head/input/mute_mic/click"},
    {USER_HAND_LEFT_INPUT_SQUEEZE_CLICK, "/user/hand/left/input/squeeze/click"},
    {USER_HAND_RIGHT_INPUT_B_CLICK, "/user/hand/right/input/b/click"},
    {USER_HAND_RIGHT_INPUT_SELECT_CLICK, "/user/hand/right/input/select/click"},
    {USER_HAND_LEFT_INPUT_THUMBSTICK, "/user/hand/left/input/thumbstick"},
    {INPUT_A_CLICK, "/input/a/click"},
    {INPUT_B_CLICK, "/input/b/click"},
    {INPUT_THUMBSTICK_X, "/input/thumbstick/x"},
    {USER_HAND_LEFT_INPUT_TRACKPAD_X, "/user/hand/left/input/trackpad/x"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_X, "/user/gamepad/input/thumbstick_right/x"},
    {INPUT_TRACKPAD_FORCE, "/input/trackpad/force"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_LEFT, "/user/gamepad/input/thumbstick_left"},
    {USER_HAND_LEFT_INPUT_TRACKPAD, "/user/hand/left/input/trackpad"},
    {INPUT_TRACKPAD, "/input/trackpad"},
    {USER_GAMEPAD_INPUT_DPAD_RIGHT_CLICK, "/user/gamepad/input/dpad_right/click"},
    {INPUT_TRIGGER_CLICK, "/input/trigger/click"},
    {INPUT_X_CLICK, "/input/x/click"},
    {USER_GAMEPAD_OUTPUT_HAPTIC_RIGHT, "/user/gamepad/output/haptic_right"},
    {INPUT_TRACKPAD_X, "/input/trackpad/x"},
    {INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, "/interaction_profiles/microsoft/motion_controller"},
    {USER_HAND_RIGHT_INPUT_A_CLICK, "/user/hand/right/input/a/click"},
    {INPUT_VOLUME_DOWN_CLICK, "/input/volume_down/click"},
    {USER_HAND_LEFT_INPUT_B_TOUCH, "/user/hand/left/input/b/touch"},
    {USER_GAMEPAD_INPUT_B_CLICK, "/user/gamepad/input/b/click"},
    {INPUT_DPAD_DOWN_CLICK, "/input/dpad_down/click"},
    {INPUT_B_TOUCH, "/input/b/touch"},
    {INPUT_THUMBREST_TOUCH, "/input/thumbrest/touch"},
    {USER_GAMEPAD_INPUT_MENU_CLICK, "/user/gamepad/input/menu/click"},
    {USER_GAMEPAD_INPUT_SHOULDER_RIGHT_CLICK, "/user/gamepad/input/shoulder_right/click"},
    {USER_HAND_LEFT_INPUT_GRIP_POSE, "/user/hand/left/input/grip/pose"},
    {INPUT_GRIP_POSE, "/input/grip/pose"},
    {USER_HAND_LEFT_INPUT_THUMBSTICK_X, "/user/hand/left/input/thumbstick/x"},
    {INPUT_DPAD_LEFT_CLICK, "/input/dpad_left/click"},
    {OUTPUT_HAPTIC_RIGHT, "/output/haptic_right"},
    {INPUT_VOLUME_UP_CLICK, "/input/volume_up/click"},
    {USER_GAMEPAD_OUTPUT_HAPTIC_LEFT_TRIGGER, "/user/gamepad/output/haptic_left_trigger"},
    {USER_HAND_RIGHT_INPUT_MENU_CLICK, "/user/hand/right/input/menu/click"},
    {USER_HAND_LEFT_OUTPUT_HAPTIC, "/user/hand/left/output/haptic"},
    {USER_HAND_RIGHT_INPUT_BACK_CLICK, "/user/hand/right/input/back/click"},
    {INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, "/interaction_profiles/khr/simple_controller"},
    {USER_GAMEPAD_INPUT_VIEW_CLICK, "/user/gamepad/input/view/click"},
    {INPUT_DPAD_RIGHT_CLICK, "/input/dpad_right/click"},
    {USER_HAND_LEFT_INPUT_A_CLICK, "/user/hand/left/input/a/click"},
    {INPUT_DPAD_UP_CLICK, "/input/dpad_up/click"},
    {INPUT_TRIGGER_RIGHT_VALUE, "/input/trigger_right/value"},
    {INPUT_MUTE_MIC_CLICK, "/input/mute_mic/click"},
    {USER_HAND_RIGHT_INPUT_THUMBSTICK_Y, "/user/hand/right/input/thumbstick/y"},
    {INPUT_THUMBSTICK_RIGHT_X, "/input/thumbstick_right/x"},
    {INPUT_SQUEEZE_CLICK, "/input/squeeze/click"},
    {INPUT_SHOULDER_LEFT_CLICK, "/input/shoulder_left/click"},
    {USER_HAND_RIGHT_INPUT_SQUEEZE_VALUE, "/user/hand/right/input/squeeze/value"},
    {USER_GAMEPAD_OUTPUT_HAPTIC_RIGHT_TRIGGER, "/user/gamepad/output/haptic_right_trigger"},
    {USER_GAMEPAD_INPUT_DPAD_DOWN_CLICK, "/user/gamepad/input/dpad_down/click"},
    {USER_HAND_RIGHT_INPUT_THUMBREST_TOUCH, "/user/hand/right/input/thumbrest/touch"},
    {OUTPUT_HAPTIC_LEFT_TRIGGER, "/output/haptic_left_trigger"},

};

std::unordered_map<WellKnownStringIndex, XrPath> OverlaysLayerWellKnownStringToPath;
std::unordered_map<XrPath, WellKnownStringIndex> OverlaysLayerPathToWellKnownString;
std::unordered_map<XrPath, XrPath> OverlaysLayerBindingToSubaction;




struct PlaceholderActionId
{
    std::string name;
    XrActionType type;
    WellKnownStringIndex interactionProfileString;
    WellKnownStringIndex subActionString;
    WellKnownStringIndex componentString;
    WellKnownStringIndex fullBindingString;
};

std::vector<PlaceholderActionId> PlaceholderActionIds =
{
    {"/interaction_profiles/khr/simple_controller/user/hand/left/input/select/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_LEFT, INPUT_SELECT_CLICK, USER_HAND_LEFT_INPUT_SELECT_CLICK},
    {"/interaction_profiles/khr/simple_controller/user/hand/left/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_LEFT, INPUT_MENU_CLICK, USER_HAND_LEFT_INPUT_MENU_CLICK},
    {"/interaction_profiles/khr/simple_controller/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_LEFT, OUTPUT_HAPTIC, USER_HAND_LEFT_OUTPUT_HAPTIC},
    {"/interaction_profiles/khr/simple_controller/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_LEFT, INPUT_GRIP_POSE, USER_HAND_LEFT_INPUT_GRIP_POSE},
    {"/interaction_profiles/khr/simple_controller/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_LEFT, INPUT_AIM_POSE, USER_HAND_LEFT_INPUT_AIM_POSE},
    {"/interaction_profiles/khr/simple_controller/user/hand/right/input/select/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_RIGHT, INPUT_SELECT_CLICK, USER_HAND_RIGHT_INPUT_SELECT_CLICK},
    {"/interaction_profiles/khr/simple_controller/user/hand/right/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_RIGHT, INPUT_MENU_CLICK, USER_HAND_RIGHT_INPUT_MENU_CLICK},
    {"/interaction_profiles/khr/simple_controller/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_RIGHT, OUTPUT_HAPTIC, USER_HAND_RIGHT_OUTPUT_HAPTIC},
    {"/interaction_profiles/khr/simple_controller/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_RIGHT, INPUT_GRIP_POSE, USER_HAND_RIGHT_INPUT_GRIP_POSE},
    {"/interaction_profiles/khr/simple_controller/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_RIGHT, INPUT_AIM_POSE, USER_HAND_RIGHT_INPUT_AIM_POSE},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_CLICK, USER_HAND_LEFT_INPUT_TRIGGER_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_Y, USER_HAND_LEFT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/squeeze/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_SQUEEZE_CLICK, USER_HAND_LEFT_INPUT_SQUEEZE_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_MENU_CLICK, USER_HAND_LEFT_INPUT_MENU_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_TOUCH, USER_HAND_LEFT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_X, USER_HAND_LEFT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD, USER_HAND_LEFT_INPUT_TRACKPAD},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, OUTPUT_HAPTIC, USER_HAND_LEFT_OUTPUT_HAPTIC},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_GRIP_POSE, USER_HAND_LEFT_INPUT_GRIP_POSE},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_VALUE, USER_HAND_LEFT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_SYSTEM_CLICK, USER_HAND_LEFT_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_CLICK, USER_HAND_LEFT_INPUT_TRACKPAD_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_AIM_POSE, USER_HAND_LEFT_INPUT_AIM_POSE},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_CLICK, USER_HAND_RIGHT_INPUT_TRIGGER_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_Y, USER_HAND_RIGHT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/squeeze/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_SQUEEZE_CLICK, USER_HAND_RIGHT_INPUT_SQUEEZE_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_MENU_CLICK, USER_HAND_RIGHT_INPUT_MENU_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_TOUCH, USER_HAND_RIGHT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_X, USER_HAND_RIGHT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD, USER_HAND_RIGHT_INPUT_TRACKPAD},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, OUTPUT_HAPTIC, USER_HAND_RIGHT_OUTPUT_HAPTIC},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_GRIP_POSE, USER_HAND_RIGHT_INPUT_GRIP_POSE},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_VALUE, USER_HAND_RIGHT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_SYSTEM_CLICK, USER_HAND_RIGHT_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_CLICK, USER_HAND_RIGHT_INPUT_TRACKPAD_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_AIM_POSE, USER_HAND_RIGHT_INPUT_AIM_POSE},
    {"/interaction_profiles/htc/vive_pro/user/head/input/volume_up/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_PRO, USER_HEAD, INPUT_VOLUME_UP_CLICK, USER_HEAD_INPUT_VOLUME_UP_CLICK},
    {"/interaction_profiles/htc/vive_pro/user/head/input/mute_mic/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_PRO, USER_HEAD, INPUT_MUTE_MIC_CLICK, USER_HEAD_INPUT_MUTE_MIC_CLICK},
    {"/interaction_profiles/htc/vive_pro/user/head/input/volume_down/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_PRO, USER_HEAD, INPUT_VOLUME_DOWN_CLICK, USER_HEAD_INPUT_VOLUME_DOWN_CLICK},
    {"/interaction_profiles/htc/vive_pro/user/head/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_PRO, USER_HEAD, INPUT_SYSTEM_CLICK, USER_HEAD_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_CLICK, USER_HAND_LEFT_INPUT_THUMBSTICK_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_Y, USER_HAND_LEFT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_Y, USER_HAND_LEFT_INPUT_THUMBSTICK_Y},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/squeeze/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_SQUEEZE_CLICK, USER_HAND_LEFT_INPUT_SQUEEZE_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_MENU_CLICK, USER_HAND_LEFT_INPUT_MENU_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_TOUCH, USER_HAND_LEFT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_X, USER_HAND_LEFT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD, USER_HAND_LEFT_INPUT_TRACKPAD},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_X, USER_HAND_LEFT_INPUT_THUMBSTICK_X},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK, USER_HAND_LEFT_INPUT_THUMBSTICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_GRIP_POSE, USER_HAND_LEFT_INPUT_GRIP_POSE},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, OUTPUT_HAPTIC, USER_HAND_LEFT_OUTPUT_HAPTIC},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_VALUE, USER_HAND_LEFT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_AIM_POSE, USER_HAND_LEFT_INPUT_AIM_POSE},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_CLICK, USER_HAND_LEFT_INPUT_TRACKPAD_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_CLICK, USER_HAND_RIGHT_INPUT_THUMBSTICK_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_Y, USER_HAND_RIGHT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_Y, USER_HAND_RIGHT_INPUT_THUMBSTICK_Y},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/squeeze/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_SQUEEZE_CLICK, USER_HAND_RIGHT_INPUT_SQUEEZE_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_MENU_CLICK, USER_HAND_RIGHT_INPUT_MENU_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_TOUCH, USER_HAND_RIGHT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_X, USER_HAND_RIGHT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD, USER_HAND_RIGHT_INPUT_TRACKPAD},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_X, USER_HAND_RIGHT_INPUT_THUMBSTICK_X},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK, USER_HAND_RIGHT_INPUT_THUMBSTICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_GRIP_POSE, USER_HAND_RIGHT_INPUT_GRIP_POSE},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, OUTPUT_HAPTIC, USER_HAND_RIGHT_OUTPUT_HAPTIC},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_VALUE, USER_HAND_RIGHT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_AIM_POSE, USER_HAND_RIGHT_INPUT_AIM_POSE},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_CLICK, USER_HAND_RIGHT_INPUT_TRACKPAD_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/x/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_X_CLICK, USER_GAMEPAD_INPUT_X_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/y/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_Y_CLICK, USER_GAMEPAD_INPUT_Y_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/dpad_down/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_DPAD_DOWN_CLICK, USER_GAMEPAD_INPUT_DPAD_DOWN_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/output/haptic_right_trigger", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, OUTPUT_HAPTIC_RIGHT_TRIGGER, USER_GAMEPAD_OUTPUT_HAPTIC_RIGHT_TRIGGER},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_MENU_CLICK, USER_GAMEPAD_INPUT_MENU_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/dpad_left/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_DPAD_LEFT_CLICK, USER_GAMEPAD_INPUT_DPAD_LEFT_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_right/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_RIGHT_CLICK, USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/output/haptic_right", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, OUTPUT_HAPTIC_RIGHT, USER_GAMEPAD_OUTPUT_HAPTIC_RIGHT},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/trigger_left/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_TRIGGER_LEFT_VALUE, USER_GAMEPAD_INPUT_TRIGGER_LEFT_VALUE},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/view/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_VIEW_CLICK, USER_GAMEPAD_INPUT_VIEW_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/dpad_right/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_DPAD_RIGHT_CLICK, USER_GAMEPAD_INPUT_DPAD_RIGHT_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/shoulder_right/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_SHOULDER_RIGHT_CLICK, USER_GAMEPAD_INPUT_SHOULDER_RIGHT_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/output/haptic_left", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, OUTPUT_HAPTIC_LEFT, USER_GAMEPAD_OUTPUT_HAPTIC_LEFT},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/dpad_up/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_DPAD_UP_CLICK, USER_GAMEPAD_INPUT_DPAD_UP_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/trigger_right/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_TRIGGER_RIGHT_VALUE, USER_GAMEPAD_INPUT_TRIGGER_RIGHT_VALUE},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_left/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_LEFT_X, USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_X},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_left", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_LEFT, USER_GAMEPAD_INPUT_THUMBSTICK_LEFT},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_right/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_RIGHT_X, USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_X},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_right", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_RIGHT, USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/shoulder_left/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_SHOULDER_LEFT_CLICK, USER_GAMEPAD_INPUT_SHOULDER_LEFT_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_left/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_LEFT_Y, USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_Y},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/a/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_A_CLICK, USER_GAMEPAD_INPUT_A_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/b/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_B_CLICK, USER_GAMEPAD_INPUT_B_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_left/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_LEFT_CLICK, USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_right/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_RIGHT_Y, USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_Y},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/output/haptic_left_trigger", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, OUTPUT_HAPTIC_LEFT_TRIGGER, USER_GAMEPAD_OUTPUT_HAPTIC_LEFT_TRIGGER},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/back/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_BACK_CLICK, USER_HAND_LEFT_INPUT_BACK_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_CLICK, USER_HAND_LEFT_INPUT_TRIGGER_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_Y, USER_HAND_LEFT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_TOUCH, USER_HAND_LEFT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_X, USER_HAND_LEFT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD, USER_HAND_LEFT_INPUT_TRACKPAD},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_GRIP_POSE, USER_HAND_LEFT_INPUT_GRIP_POSE},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_SYSTEM_CLICK, USER_HAND_LEFT_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_CLICK, USER_HAND_LEFT_INPUT_TRACKPAD_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_AIM_POSE, USER_HAND_LEFT_INPUT_AIM_POSE},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/back/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_BACK_CLICK, USER_HAND_RIGHT_INPUT_BACK_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_CLICK, USER_HAND_RIGHT_INPUT_TRIGGER_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_Y, USER_HAND_RIGHT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_TOUCH, USER_HAND_RIGHT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_X, USER_HAND_RIGHT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD, USER_HAND_RIGHT_INPUT_TRACKPAD},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_GRIP_POSE, USER_HAND_RIGHT_INPUT_GRIP_POSE},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_SYSTEM_CLICK, USER_HAND_RIGHT_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_CLICK, USER_HAND_RIGHT_INPUT_TRACKPAD_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_AIM_POSE, USER_HAND_RIGHT_INPUT_AIM_POSE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_CLICK, USER_HAND_LEFT_INPUT_THUMBSTICK_CLICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/trigger/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_TOUCH, USER_HAND_LEFT_INPUT_TRIGGER_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/thumbrest/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBREST_TOUCH, USER_HAND_LEFT_INPUT_THUMBREST_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_Y, USER_HAND_LEFT_INPUT_THUMBSTICK_Y},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, OUTPUT_HAPTIC, USER_HAND_LEFT_OUTPUT_HAPTIC},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_X, USER_HAND_LEFT_INPUT_THUMBSTICK_X},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK, USER_HAND_LEFT_INPUT_THUMBSTICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_GRIP_POSE, USER_HAND_LEFT_INPUT_GRIP_POSE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_VALUE, USER_HAND_LEFT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_AIM_POSE, USER_HAND_LEFT_INPUT_AIM_POSE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_SQUEEZE_VALUE, USER_HAND_LEFT_INPUT_SQUEEZE_VALUE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/thumbstick/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_TOUCH, USER_HAND_LEFT_INPUT_THUMBSTICK_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_CLICK, USER_HAND_RIGHT_INPUT_THUMBSTICK_CLICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/trigger/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_TOUCH, USER_HAND_RIGHT_INPUT_TRIGGER_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/thumbrest/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBREST_TOUCH, USER_HAND_RIGHT_INPUT_THUMBREST_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_Y, USER_HAND_RIGHT_INPUT_THUMBSTICK_Y},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, OUTPUT_HAPTIC, USER_HAND_RIGHT_OUTPUT_HAPTIC},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_X, USER_HAND_RIGHT_INPUT_THUMBSTICK_X},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK, USER_HAND_RIGHT_INPUT_THUMBSTICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_GRIP_POSE, USER_HAND_RIGHT_INPUT_GRIP_POSE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_VALUE, USER_HAND_RIGHT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_AIM_POSE, USER_HAND_RIGHT_INPUT_AIM_POSE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_SQUEEZE_VALUE, USER_HAND_RIGHT_INPUT_SQUEEZE_VALUE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/thumbstick/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_TOUCH, USER_HAND_RIGHT_INPUT_THUMBSTICK_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_CLICK, USER_HAND_LEFT_INPUT_TRIGGER_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_Y, USER_HAND_LEFT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_Y, USER_HAND_LEFT_INPUT_THUMBSTICK_Y},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_X, USER_HAND_LEFT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD, USER_HAND_LEFT_INPUT_TRACKPAD},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/a/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_A_TOUCH, USER_HAND_LEFT_INPUT_A_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/thumbstick/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_TOUCH, USER_HAND_LEFT_INPUT_THUMBSTICK_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/squeeze/force", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_SQUEEZE_FORCE, USER_HAND_LEFT_INPUT_SQUEEZE_FORCE},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/b/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_B_TOUCH, USER_HAND_LEFT_INPUT_B_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trigger/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_TOUCH, USER_HAND_LEFT_INPUT_TRIGGER_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, OUTPUT_HAPTIC, USER_HAND_LEFT_OUTPUT_HAPTIC},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_GRIP_POSE, USER_HAND_LEFT_INPUT_GRIP_POSE},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_SYSTEM_CLICK, USER_HAND_LEFT_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_SQUEEZE_VALUE, USER_HAND_LEFT_INPUT_SQUEEZE_VALUE},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_AIM_POSE, USER_HAND_LEFT_INPUT_AIM_POSE},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_CLICK, USER_HAND_LEFT_INPUT_THUMBSTICK_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_TOUCH, USER_HAND_LEFT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_VALUE, USER_HAND_LEFT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/system/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_SYSTEM_TOUCH, USER_HAND_LEFT_INPUT_SYSTEM_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/a/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_A_CLICK, USER_HAND_LEFT_INPUT_A_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/b/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_B_CLICK, USER_HAND_LEFT_INPUT_B_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_X, USER_HAND_LEFT_INPUT_THUMBSTICK_X},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK, USER_HAND_LEFT_INPUT_THUMBSTICK},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trackpad/force", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_FORCE, USER_HAND_LEFT_INPUT_TRACKPAD_FORCE},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_CLICK, USER_HAND_RIGHT_INPUT_TRIGGER_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_Y, USER_HAND_RIGHT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_Y, USER_HAND_RIGHT_INPUT_THUMBSTICK_Y},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_X, USER_HAND_RIGHT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD, USER_HAND_RIGHT_INPUT_TRACKPAD},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/a/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_A_TOUCH, USER_HAND_RIGHT_INPUT_A_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/thumbstick/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_TOUCH, USER_HAND_RIGHT_INPUT_THUMBSTICK_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/squeeze/force", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_SQUEEZE_FORCE, USER_HAND_RIGHT_INPUT_SQUEEZE_FORCE},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/b/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_B_TOUCH, USER_HAND_RIGHT_INPUT_B_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trigger/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_TOUCH, USER_HAND_RIGHT_INPUT_TRIGGER_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, OUTPUT_HAPTIC, USER_HAND_RIGHT_OUTPUT_HAPTIC},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_GRIP_POSE, USER_HAND_RIGHT_INPUT_GRIP_POSE},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_SYSTEM_CLICK, USER_HAND_RIGHT_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_SQUEEZE_VALUE, USER_HAND_RIGHT_INPUT_SQUEEZE_VALUE},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_AIM_POSE, USER_HAND_RIGHT_INPUT_AIM_POSE},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_CLICK, USER_HAND_RIGHT_INPUT_THUMBSTICK_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_TOUCH, USER_HAND_RIGHT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_VALUE, USER_HAND_RIGHT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/system/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_SYSTEM_TOUCH, USER_HAND_RIGHT_INPUT_SYSTEM_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/a/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_A_CLICK, USER_HAND_RIGHT_INPUT_A_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/b/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_B_CLICK, USER_HAND_RIGHT_INPUT_B_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_X, USER_HAND_RIGHT_INPUT_THUMBSTICK_X},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK, USER_HAND_RIGHT_INPUT_THUMBSTICK},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trackpad/force", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_FORCE, USER_HAND_RIGHT_INPUT_TRACKPAD_FORCE},

};

const std::set<HandleTypePair> OverlaysLayerNoObjectInfo = {};

uint64_t GetNextLocalHandle()
{
    static std::atomic_uint64_t nextHandle = 1;
    return nextHandle++;
}

std::string PathToString(XrInstance instance, XrPath path)
{
    if(path == XR_NULL_PATH) {
        return "XR_NULL_PATH";
    }

    char buffer[257];
    uint32_t countOutput;
    XrResult result;

    auto instanceInfo = gOverlaysLayerXrInstanceToHandleInfo.at(instance);

    result = instanceInfo->downchain->PathToString(instance, path, 0, &countOutput, nullptr);
    if(result != XR_SUCCESS) {
        auto index = OverlaysLayerPathToWellKnownString.at(path);
        auto str = OverlaysLayerWellKnownStrings.at(index);
        sprintf(buffer, "<PathToString failed?! %08llX, \"%s\">", path, str);
        return buffer;
    }
    result = instanceInfo->downchain->PathToString(instance, path, countOutput, &countOutput, buffer);
    if(result != XR_SUCCESS) {
        auto index = OverlaysLayerPathToWellKnownString.at(path);
        auto str = OverlaysLayerWellKnownStrings.at(index);
        sprintf(buffer, "<PathToString failed?! %08llX, \"%s\">", path, str);
        return buffer;
    }
    return buffer;
}

void ClearActionState(XrActionType actionType, ActionStateUnion* stateUnion)
{
    switch(actionType) {
        case XR_ACTION_TYPE_BOOLEAN_INPUT: {
            auto& state = stateUnion->booleanState;
            state.type = XR_TYPE_ACTION_STATE_BOOLEAN;
            state.next = nullptr;
            state.currentState = XR_FALSE;
            state.changedSinceLastSync = XR_FALSE;
            state.lastChangeTime = 0;
            state.isActive = XR_FALSE;
            break;
        }
        case XR_ACTION_TYPE_FLOAT_INPUT: {
            auto& state = stateUnion->floatState;
            state.type = XR_TYPE_ACTION_STATE_FLOAT;
            state.next = nullptr;
            state.currentState = 0.0f;
            state.changedSinceLastSync = XR_FALSE;
            state.lastChangeTime = 0;
            state.isActive = XR_FALSE;
            break;
        }
        case XR_ACTION_TYPE_VECTOR2F_INPUT: {
            auto& state = stateUnion->vector2fState;
            state.type = XR_TYPE_ACTION_STATE_VECTOR2F;
            state.next = nullptr;
            state.currentState = {0.0f, 0.0f};
            state.changedSinceLastSync = XR_FALSE;
            state.lastChangeTime = 0;
            state.isActive = XR_FALSE;
            break;
        }
        case XR_ACTION_TYPE_POSE_INPUT: {
            auto& state = stateUnion->poseState;
            state.type = XR_TYPE_ACTION_STATE_POSE;
            state.next = nullptr;
            state.isActive = XR_FALSE;
            break;
        }
    }
}

// NB: Does not change lastChangeTime or changedSinceLastSync since those depend on the previously sync'd action state
void MergeActionState(XrActionType actionType, const ActionStateUnion* toMergeUnion, ActionStateUnion *accumulatedUnion)
{
    switch(actionType) {
        case XR_ACTION_TYPE_BOOLEAN_INPUT: {
            const auto& toMerge = toMergeUnion->booleanState;
            auto& accumulated = accumulatedUnion->booleanState;

            if(!accumulated.isActive) {
                accumulated = toMerge;
            } else {
                accumulated.currentState |= toMerge.currentState;
            }
            break;
        }
        case XR_ACTION_TYPE_FLOAT_INPUT: {
            auto& toMerge = toMergeUnion->floatState;
            auto& accumulated = accumulatedUnion->floatState;

            if(!accumulated.isActive) {
                accumulated = toMerge;
            } else {
                accumulated.currentState = std::max(accumulated.currentState, toMerge.currentState);
            }
            break;
        }
        case XR_ACTION_TYPE_VECTOR2F_INPUT: {
            auto& toMerge = toMergeUnion->vector2fState;
            auto& accumulated = accumulatedUnion->vector2fState;
            float mergesq = toMerge.currentState.x * toMerge.currentState.x + toMerge.currentState.y * toMerge.currentState.y;
            float accumsq = accumulated.currentState.x * accumulated.currentState.x + accumulated.currentState.y * accumulated.currentState.y;
            if(!accumulated.isActive) {
                accumulated = toMerge;
            } else {
                if(mergesq > accumsq) {
                    accumulated.currentState = toMerge.currentState;
                }
            }
            break;
        }
        case XR_ACTION_TYPE_POSE_INPUT: {
            auto& toMerge = toMergeUnion->poseState;
            auto& accumulated = accumulatedUnion->poseState;
            accumulated.isActive |= toMerge.isActive;
            break;
        }
    }
}

// Updates currentState changedSinceLastSync and lastChangeTime if
// previous was active and current is active and state has changed
void UpdateActionStateLastChange(XrActionType actionType, const ActionStateUnion *previousState, ActionStateUnion *currentState)
{
    switch(actionType) {
        case XR_ACTION_TYPE_BOOLEAN_INPUT: {
            const auto& previous = previousState->booleanState;
            auto& current = currentState->booleanState;

            if(current.isActive && previous.isActive) {
                if(current.currentState != previous.currentState) {
                    current.changedSinceLastSync = XR_TRUE;
                } else {
                    current.lastChangeTime = previous.lastChangeTime;
                }
            }
            break;
        }
        case XR_ACTION_TYPE_FLOAT_INPUT: {
            auto& previous = previousState->floatState;
            auto& current = currentState->floatState;

            if(current.isActive && previous.isActive) {
                if(current.currentState != previous.currentState) {
                    current.changedSinceLastSync = XR_TRUE;
                } else {
                    current.lastChangeTime = previous.lastChangeTime;
                }
            }
            break;
        }
        case XR_ACTION_TYPE_VECTOR2F_INPUT: {
            auto& previous = previousState->vector2fState;
            auto& current = currentState->vector2fState;

            if(current.isActive && previous.isActive) {
                if((current.currentState.x != previous.currentState.x) || (current.currentState.y != previous.currentState.y)) {
                    current.changedSinceLastSync = XR_TRUE;
                } else {
                    current.lastChangeTime = previous.lastChangeTime;
                }
            }
            break;
        }
        case XR_ACTION_TYPE_POSE_INPUT: {
            break;
        }
    }
}

void LogWindowsLastError(const char *xrfunc, const char* what, const char *file, int line)
{
    DWORD lastError = GetLastError();
    LPVOID messageBuf;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, xrfunc,
        OverlaysLayerNoObjectInfo, fmt("FATAL: %s at %s:%d failed with %d (%s)\n", what, file, line, lastError, messageBuf).c_str());
    LocalFree(messageBuf);
}

void LogWindowsError(HRESULT result, const char *xrfunc, const char* what, const char *file, int line)
{
    LPVOID messageBuf;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, xrfunc,
        OverlaysLayerNoObjectInfo, fmt("FATAL: %s at %s:%d failed with %d (%s)\n", what, file, line, result, messageBuf).c_str());
    LocalFree(messageBuf);
}

bool OverlaySwapchain::CreateTextures(XrInstance instance, ID3D11Device *d3d11, DWORD mainProcessId)
{
    for(int i = 0; i < swapchainTextures.size(); i++) {
        D3D11_TEXTURE2D_DESC desc;
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

        HRESULT result;
        if((result = d3d11->CreateTexture2D(&desc, NULL, &swapchainTextures[i])) != S_OK) {
            LogWindowsError(result, "xrCreateSwapchain", "CreateTexture2D", __FILE__, __LINE__);
            return false;
        }

        {
            IDXGIResource1* sharedResource = NULL;
            if((result = swapchainTextures[i]->QueryInterface(__uuidof(IDXGIResource1), (LPVOID*) &sharedResource)) != S_OK) {
                LogWindowsError(result, "xrCreateSwapchain", "QueryInterface", __FILE__, __LINE__);
                return false;
            }

            HANDLE thisProcessHandle = GetCurrentProcess();
            HANDLE hostProcessHandle;
            hostProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, mainProcessId);
            if(hostProcessHandle == NULL) {
                LogWindowsLastError("xrCreateSwapchain", "OpenProcess", __FILE__, __LINE__);
                return false;
            }

            HANDLE handle;

            // Get the Shared Handle for the texture. This is still local to this process but is an actual HANDLE
            if((result = sharedResource->CreateSharedHandle(NULL,
                DXGI_SHARED_RESOURCE_READ, // GENERIC_ALL | DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                NULL, &handle)) != S_OK) {

                LogWindowsError(result, "xrCreateSwapchain", "CreateSharedHandle", __FILE__, __LINE__);
                return false;
            }
            
            // Duplicate the handle so "Host" RPC service process can use it
            if(!DuplicateHandle(thisProcessHandle, handle, hostProcessHandle, &swapchainHandles[i], 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                LogWindowsLastError("xrCreateSwapchain", "DuplicateHandle", __FILE__, __LINE__);
                return false;
            }
            CloseHandle(handle);
            sharedResource->Release();
        }
    }
    return true;
}

OptionalSessionStateChange SessionStateTracker::GetAndDoPendingStateChange(MainSessionSessionState *mainState)
{
    if((sessionState != XR_SESSION_STATE_LOSS_PENDING) &&
        ((mainState->GetLossState() == LOST) ||
        (mainState->GetLossState() == LOSS_PENDING))) {

        return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_LOSS_PENDING };
    }

    switch(sessionState) {
        case XR_SESSION_STATE_UNKNOWN:
            if(mainState->sessionState != XR_SESSION_STATE_UNKNOWN) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_IDLE };
            }
            break;

        case XR_SESSION_STATE_IDLE:
            if(exitRequested || (mainState->sessionState == XR_SESSION_STATE_EXITING)) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_EXITING };
            } else if(mainState->isRunning && mainState->hasCalledWaitFrame) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_READY };
            }
            break;

        case XR_SESSION_STATE_READY:
            if(isRunning) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_SYNCHRONIZED };
            } 
            break;

        case XR_SESSION_STATE_SYNCHRONIZED:
            if(exitRequested || !mainState->isRunning || (mainState->sessionState == XR_SESSION_STATE_STOPPING)) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_STOPPING };
            } else if((mainState->sessionState == XR_SESSION_STATE_VISIBLE) || (mainState->sessionState == XR_SESSION_STATE_FOCUSED)) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_VISIBLE };
            } 
            break;

        case XR_SESSION_STATE_VISIBLE:
            if(exitRequested || !mainState->isRunning || (mainState->sessionState == XR_SESSION_STATE_STOPPING)) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_SYNCHRONIZED };
            } else if(mainState->sessionState == XR_SESSION_STATE_SYNCHRONIZED) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_SYNCHRONIZED };
            } else if(mainState->sessionState == XR_SESSION_STATE_FOCUSED) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_FOCUSED };
            }
            break;

        case XR_SESSION_STATE_FOCUSED:
            if(exitRequested || !mainState->isRunning || (mainState->sessionState == XR_SESSION_STATE_STOPPING)) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_VISIBLE };
            } else if((mainState->sessionState == XR_SESSION_STATE_VISIBLE) || (mainState->sessionState == XR_SESSION_STATE_SYNCHRONIZED)) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_VISIBLE };
            }
            break;

        case XR_SESSION_STATE_STOPPING:
            if(!isRunning) {
                return OptionalSessionStateChange { true, sessionState = XR_SESSION_STATE_IDLE };
            }
            break;

        default:
            // No other combination of states requires an Overlay SessionStateChange
            break;
    }

    return OptionalSessionStateChange { false, XR_SESSION_STATE_UNKNOWN };
}


SwapchainCachedData::~SwapchainCachedData()
{
    for(HANDLE acquired : remoteImagesAcquired) {
        IDXGIKeyedMutex* keyedMutex;
        ID3D11Texture2D *sharedTexture;
        auto it = handleTextureMap.find(acquired);
        if(it != handleTextureMap.end()) {
            sharedTexture = it->second;
            HRESULT result = sharedTexture->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex);
            if(result == S_OK) {
                keyedMutex->ReleaseSync(KEYED_MUTEX_OVERLAY);
                keyedMutex->Release();
            }
        }
    }
    remoteImagesAcquired.clear();
    for(auto shared : handleTextureMap) {
        shared.second->Release();
        CloseHandle(shared.first);
    }
    for(auto texture : swapchainImages) {
        texture->Release();
    }
    handleTextureMap.clear();
}

ID3D11Texture2D* SwapchainCachedData::getSharedTexture(ID3D11Device *d3d11Device, HANDLE sourceHandle)
{
    ID3D11Texture2D *sharedTexture;

    ID3D11Device1 *device1;

    HRESULT result;

    if((result = d3d11Device->QueryInterface(__uuidof (ID3D11Device1), (void **)&device1)) != S_OK) {
        LogWindowsError(result, "xrCreateSwapchain", "QueryInterface", __FILE__, __LINE__);
        return nullptr;
    }

    auto it = handleTextureMap.find(sourceHandle);
    if(it == handleTextureMap.end()) {
        if((result = device1->OpenSharedResource1(sourceHandle, __uuidof(ID3D11Texture2D), (LPVOID*) &sharedTexture)) != S_OK) {
            LogWindowsError(result, "xrCreateSwapchain", "OpenSharedResource1", __FILE__, __LINE__);
            return nullptr;
        }
        handleTextureMap.insert({sourceHandle, sharedTexture});
    } else  {
        sharedTexture = it->second;
    }
    device1->Release();

    return sharedTexture;
}


// XXX could generate
void OverlaysLayerRemoveXrSpaceHandleInfo(XrSpace localHandle)
{
    {
        OverlaysLayerXrSpaceHandleInfo::Ptr info = OverlaysLayerGetHandleInfoFromXrSpace(localHandle);
        OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(info->parentHandle);
        sessionInfo->childSpaces.erase(info);
    }

    OverlaysLayerRemoveXrSpaceFromHandleInfoMap(localHandle);
}

// XXX could generate
void OverlaysLayerRemoveXrSwapchainHandleInfo(XrSwapchain localHandle)
{
    {
        OverlaysLayerXrSwapchainHandleInfo::Ptr info = OverlaysLayerGetHandleInfoFromXrSwapchain(localHandle);
        OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(info->parentHandle);
        sessionInfo->childSwapchains.erase(info);
    }

    OverlaysLayerRemoveXrSwapchainFromHandleInfoMap(localHandle);
}

void OverlaysLayerLogMessage(XrInstance instance,
                         XrDebugUtilsMessageSeverityFlagsEXT message_severity, const char* command_name,
                         const std::set<HandleTypePair>& objects_info, const char* message)
{
    // If we have instance information, see if we need to log this information out to a debug messenger
    // callback.
    if(instance != XR_NULL_HANDLE) {

        OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = gOverlaysLayerXrInstanceToHandleInfo.at(instance);

        // To be a little more performant, check all messenger's
        // messageSeverities and messageTypes to make sure we will call at
        // least one

        /* XXX TBD !instanceInfo->debug_data.Empty() */

        if (!instanceInfo->debugUtilsMessengers.empty()) {

            // Setup our callback data once
            XrDebugUtilsMessengerCallbackDataEXT callback_data = {};
            callback_data.type = XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
            callback_data.messageId = "Overlays API Layer";
            callback_data.functionName = command_name;
            callback_data.message = message;

#if 0
            // TBD
            NamesAndLabels names_and_labels;
            std::vector<XrSdkLogObjectInfo> objects;
            objects.reserve(objects_info.size());
            std::transform(objects_info.begin(), objects_info.end(), std::back_inserter(objects),
                           [](GenValidUsageXrObjectInfo const &info) {
                               return XrSdkLogObjectInfo{info.handle, info.type};
                           });
            names_and_labels = instance_info->debug_data.PopulateNamesAndLabels(std::move(objects));
            names_and_labels.PopulateCallbackData(callback_data);
#endif

            // Loop through all active messengers and give each a chance to output information
            for (const auto &messenger : instanceInfo->debugUtilsMessengers) {

                std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
                XrDebugUtilsMessengerCreateInfoEXT *messenger_create_info = gOverlaysLayerXrDebugUtilsMessengerEXTToHandleInfo.at(messenger)->createInfo;
                mlock.unlock();

                // If a callback exists, and the message is of a type this callback cares about, call it.
                if (nullptr != messenger_create_info->userCallback &&
                    0 != (messenger_create_info->messageSeverities & message_severity) &&
                    0 != (messenger_create_info->messageTypes & XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)) {

                    XrBool32 ret_val = messenger_create_info->userCallback(message_severity,
                                                                           XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                                                                           &callback_data, messenger_create_info->userData);
                }
            }
        } else {
            if(command_name) {
                OutputDebugStringA(fmt("Overlays API Layer: %s, %s\n", command_name, message).c_str());
            } else {
                OutputDebugStringA(fmt("Overlays API Layer: %s\n", message).c_str());
            }
        }
    } else {
        if(command_name) {
            OutputDebugStringA(fmt("Overlays API Layer: %s, %s\n", command_name, message).c_str());
        } else {
            OutputDebugStringA(fmt("Overlays API Layer: %s\n", message).c_str());
        }
    }
}

void OverlaysLayerLogMessage(XrDebugUtilsMessageSeverityFlagsEXT message_severity, const char* command_name,
                         const std::set<HandleTypePair>& objects_info, const char* message)
{
    OverlaysLayerLogMessage(XR_NULL_HANDLE, message_severity, command_name, objects_info, message);
}



XrResult OverlaysLayerXrCreateInstance(const XrInstanceCreateInfo * /*info*/, XrInstance * /*instance*/)
{
    return XR_SUCCESS;
}


XrResult OverlaysLayerXrCreateApiLayerInstance(const XrInstanceCreateInfo *instanceCreateInfo,
        const struct XrApiLayerCreateInfo *apiLayerInfo, XrInstance *instance)
{
    PFN_xrGetInstanceProcAddr next_get_instance_proc_addr = nullptr;
    PFN_xrCreateApiLayerInstance next_create_api_layer_instance = nullptr;
    XrApiLayerCreateInfo new_api_layer_info = {};

    // Validate the API layer info and next API layer info structures before we try to use them
    if (!apiLayerInfo ||
        XR_LOADER_INTERFACE_STRUCT_API_LAYER_CREATE_INFO != apiLayerInfo->structType ||
        XR_API_LAYER_CREATE_INFO_STRUCT_VERSION > apiLayerInfo->structVersion ||
        sizeof(XrApiLayerCreateInfo) > apiLayerInfo->structSize ||
        !apiLayerInfo->nextInfo ||
        XR_LOADER_INTERFACE_STRUCT_API_LAYER_NEXT_INFO != apiLayerInfo->nextInfo->structType ||
        XR_API_LAYER_NEXT_INFO_STRUCT_VERSION > apiLayerInfo->nextInfo->structVersion ||
        sizeof(XrApiLayerNextInfo) > apiLayerInfo->nextInfo->structSize ||
        0 != strcmp(kOverlayLayerName, apiLayerInfo->nextInfo->layerName) ||
        nullptr == apiLayerInfo->nextInfo->nextGetInstanceProcAddr ||
        nullptr == apiLayerInfo->nextInfo->nextCreateApiLayerInstance) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Copy the contents of the layer info struct, but then move the next info up by
    // one slot so that the next layer gets information.
    memcpy(&new_api_layer_info, apiLayerInfo, sizeof(XrApiLayerCreateInfo));
    new_api_layer_info.nextInfo = apiLayerInfo->nextInfo->next;

    // Remove XR_EXTX_overlay from the extension list if requested
    const char** extensionNamesMinusOverlay = new const char*[instanceCreateInfo->enabledExtensionCount];
    uint32_t extensionCountMinusOverlay = 0;
    for(uint32_t i = 0; i < instanceCreateInfo->enabledExtensionCount; i++) {
        if(strncmp(instanceCreateInfo->enabledExtensionNames[i], XR_EXTX_OVERLAY_EXTENSION_NAME, strlen(XR_EXTX_OVERLAY_EXTENSION_NAME)) != 0) {
            extensionNamesMinusOverlay[extensionCountMinusOverlay++] = instanceCreateInfo->enabledExtensionNames[i];
        }
    }

    XrInstanceCreateInfo createInfoMinusOverlays = *instanceCreateInfo;
    createInfoMinusOverlays.enabledExtensionNames = extensionNamesMinusOverlay;
    createInfoMinusOverlays.enabledExtensionCount = extensionCountMinusOverlay;

    // Get the function pointers we need
    next_get_instance_proc_addr = apiLayerInfo->nextInfo->nextGetInstanceProcAddr;
    next_create_api_layer_instance = apiLayerInfo->nextInfo->nextCreateApiLayerInstance;

    // Create the instance
    XrInstance returned_instance = *instance;
    XrResult result = next_create_api_layer_instance(&createInfoMinusOverlays, &new_api_layer_info, &returned_instance);
    *instance = returned_instance;

    delete[] extensionNamesMinusOverlay;

    // Create the dispatch table to the next levels
    std::shared_ptr<XrGeneratedDispatchTable> next_dispatch = std::make_shared<XrGeneratedDispatchTable>();
    GeneratedXrPopulateDispatchTable(next_dispatch.get(), returned_instance, next_get_instance_proc_addr);

    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
    OverlaysLayerXrInstanceHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrInstanceHandleInfo>(next_dispatch);
    info->createInfo = reinterpret_cast<XrInstanceCreateInfo*>(CopyXrStructChainWithMalloc(*instance, instanceCreateInfo));
    gOverlaysLayerXrInstanceToHandleInfo.insert({*instance, info});

    return result;
}

XrResult OverlaysLayerXrDestroyInstance(XrInstance instance)
{
    OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);
    std::shared_ptr<XrGeneratedDispatchTable> next_dispatch = instanceInfo->downchain;
    instanceInfo->Destroy();

    next_dispatch->DestroyInstance(instance);

    return XR_SUCCESS;
}

NegotiationChannels gNegotiationChannels;

bool gAppHasAMainSession = false;
XrInstance gMainSessionInstance;
MainSessionContext::Ptr gMainSessionContext;
DWORD gMainProcessId;   // Set by Overlay to check for main process unexpected exit
HANDLE gMainMutexHandle; // Held by Main for duration of operation as Main Session

// Both main and overlay processes call this function, which creates/opens
// the negotiation mutex, shmem, and semaphores.
bool OpenNegotiationChannels(XrInstance instance, NegotiationChannels &ch)
{
    ch.instance = instance;
    ch.mutexHandle = CreateMutexA(NULL, TRUE, NegotiationChannels::mutexName);
    if (ch.mutexHandle == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not initialize the negotiation mutex: CreateMutex error was %d (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    ch.shmemHandle = CreateFileMappingA( 
        INVALID_HANDLE_VALUE,   // use sys paging file instead of an existing file
        NULL,                   // default security attributes
        PAGE_READWRITE,         // read/write access
        0,                      // size: high 32-bits
        NegotiationChannels::shmemSize,         // size: low 32-bits
        NegotiationChannels::shmemName);        // name of map object

    if (ch.shmemHandle == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not initialize the negotiation shmem: CreateFileMappingA error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false; 
    }

    // Get a pointer to the file-mapped shared memory, read/write
    ch.params = reinterpret_cast<NegotiationParams*>(MapViewOfFile(ch.shmemHandle, FILE_MAP_WRITE, 0, 0, 0));
    if (!ch.params) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not get the negotiation shmem: MapViewOfFile error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false; 
    }

    ch.overlayWaitSema = CreateSemaphoreA(nullptr, 0, 1, NegotiationChannels::overlayWaitSemaName);
    if(ch.overlayWaitSema == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create negotiation overlay wait sema: CreateSemaphore error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    ch.mainWaitSema = CreateSemaphoreA(nullptr, 0, 1, NegotiationChannels::mainWaitSemaName);
    if(ch.mainWaitSema == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create negotiation main wait sema: CreateSemaphore error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    return true;
}

bool OpenRPCChannels(XrInstance instance, DWORD otherProcessId, DWORD overlayId, RPCChannels& ch)
{
    ch.instance = instance;

    ch.otherProcessId = otherProcessId;
    ch.otherProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, ch.otherProcessId);

    ch.mutexHandle = CreateMutexA(NULL, TRUE, fmt(RPCChannels::mutexNameTemplate, overlayId).c_str());
    if (ch.mutexHandle == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "no function", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not initialize the RPC mutex: CreateMutex error was %d (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    ch.shmemHandle = CreateFileMappingA( 
        INVALID_HANDLE_VALUE,   // use sys paging file instead of an existing file
        NULL,                   // default security attributes
        PAGE_READWRITE,         // read/write access
        0,                      // size: high 32-bits
        RPCChannels::shmemSize,         // size: low 32-bits
        fmt(RPCChannels::shmemNameTemplate, overlayId).c_str());        // name of map object

    if (ch.shmemHandle == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "no function", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not initialize the RPC shmem: CreateFileMappingA error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false; 
    }

    // Get a pointer to the file-mapped shared memory, read/write
    ch.shmem = MapViewOfFile(ch.shmemHandle, FILE_MAP_WRITE, 0, 0, 0);
    if (ch.shmem == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not get the RPC shmem: MapViewOfFile error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false; 
    }

    ch.overlayRequestSema = CreateSemaphoreA(nullptr, 0, 1, fmt(RPCChannels::overlayRequestSemaNameTemplate, overlayId).c_str());
    if(ch.overlayRequestSema == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create RPC overlay request sema: CreateSemaphore error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    ch.mainResponseSema = CreateSemaphoreA(nullptr, 0, 1, fmt(RPCChannels::mainResponseSemaNameTemplate, overlayId).c_str());
    if(ch.mainResponseSema == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create RPC main response sema: CreateSemaphore error was %08X (%s)\n", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    return true;
}


std::unordered_map<DWORD, std::shared_ptr<ConnectionToOverlay>> gConnectionsToOverlayByProcessId;
std::recursive_mutex gConnectionsToOverlayByProcessIdMutex;


XrBaseInStructure* IPCSerialize(XrInstance instance, IPCBuffer& ipcbuf, IPCHeader* header, const XrBaseInStructure* srcbase, CopyType copyType)
{
    return CopyXrStructChain(instance, srcbase, copyType,
            [&ipcbuf](size_t size){return ipcbuf.allocate(size);},
            [&ipcbuf,&header](void* pointerToPointer){header->addOffsetToPointer(ipcbuf.base, pointerToPointer);});
}


template <class T>
const T* FindStructInChain(const void *head, XrStructureType type)
{
    const XrBaseInStructure* p = reinterpret_cast<const XrBaseInStructure*>(head);
    while(p) {
        if(p->type == type) {
            return reinterpret_cast<const T*>(p);
        }
		p = p->next;
    }
    return nullptr;
}

bool FindExtensionInList(const char* extension, uint32_t extensionsCount, const char * const* extensions)
{
    for(uint32_t i = 0; i < extensionsCount; i++) {
        if(strcmp(extension, extensions[i]) == 0) {
            return true;
        }
    }
    return false;
}

XrResult OverlaysLayerCreateSessionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrFormFactor formFactor, const XrInstanceCreateInfo *instanceCreateInfo, const XrSessionCreateInfo *createInfo, const XrSessionCreateInfoOverlayEXTX* createInfoOverlay, XrSession *session)
{
    XrSession mainSession;
    {
        auto mainSessionContext = gMainSessionContext;
        auto l = mainSessionContext->GetLock();
        mainSession = mainSessionContext->session;
    }
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(mainSession);

    if(createInfo->createFlags != sessionInfo->createInfo->createFlags) {
        OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("WARNING: xrCreateSession for overlay session had different flags (%08X) than those with which main session was created (%08X). Effect is unknown, proceeding anyway.\n", createInfo->createFlags, sessionInfo->createInfo->createFlags).c_str());
    }

    // Verify that any structures match that are chained off createInfo
    for(const XrBaseInStructure* p = reinterpret_cast<const XrBaseInStructure*>(createInfo->next); p; p = reinterpret_cast<const XrBaseInStructure*>(p->next)) {
        switch(p->type) {

            case XR_TYPE_GRAPHICS_BINDING_D3D11_KHR: {
                const XrGraphicsBindingD3D11KHR* d3dbinding = reinterpret_cast<const XrGraphicsBindingD3D11KHR*>(p);
                const XrGraphicsBindingD3D11KHR* other = FindStructInChain< XrGraphicsBindingD3D11KHR>(sessionInfo->createInfo->next, p->type);

                if(!other) {
                    // XXX send directly to channels somehow
                    OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
                        OverlaysLayerNoObjectInfo, "FATAL: xrCreateSession for overlay session specified a XrGraphicsBindingD3D11KHR but main session did not.\n");
                    return XR_ERROR_INITIALIZATION_FAILED;
                }

                if(other->device != d3dbinding->device) {
                    OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrCreateSession",
                        OverlaysLayerNoObjectInfo, fmt("WARNING: xrCreateSession for overlay session used different D3D11 Device (%08X) than that with which main session was created (%08X). Effect is unknown, proceeding anyway.\n", d3dbinding->device, other->device).c_str());
                }
                break;
            }

            // XXX Check out all other GAPI structs as support for them is added

            case XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX:
                // This is fine, ignore.  We could probably remove it on the Overlay side and not pass it through...
                break;

            default: {
                char structureTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
                XrResult r = sessionInfo->downchain->StructureTypeToString(gMainSessionInstance, p->type, structureTypeName);
                if(r != XR_SUCCESS) {
                    sprintf(structureTypeName, "(type %08X)", p->type);
                }

                OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
                    OverlaysLayerNoObjectInfo, fmt("FATAL: xrCreateSession for an overlay session used a struct (%s) which the Overlay API Layer does not know how to check.\n", structureTypeName).c_str());
                return XR_ERROR_INITIALIZATION_FAILED;
                break;
            }
        }
    }

    // Verify that main session didn't have any unexpected structures that the overlay didn't have
    for(const XrBaseInStructure* p = reinterpret_cast<const XrBaseInStructure*>(createInfo->next); p; p = reinterpret_cast<const XrBaseInStructure*>(p->next)) {
        // XXX this should probably just reject any structure that isn't known.
        const XrBaseInStructure* other = FindStructInChain<XrBaseInStructure>(sessionInfo->createInfo->next, p->type);
        if(!other) {
            char structureTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
            XrResult r = sessionInfo->downchain->StructureTypeToString(gMainSessionInstance, p->type, structureTypeName);
            if(r != XR_SUCCESS) {
                sprintf(structureTypeName, "(type %08X)", p->type);
            }

            OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrCreateSession",
                OverlaysLayerNoObjectInfo, fmt("WARNING: xrCreateSession for the main session used a struct (%s) which the overlay session did not. Effect is unknown, proceeding anyway.\n", structureTypeName).c_str());
        }
    }
    
    XrFormFactor mainSessionFormFactor;
    {
        OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(mainSession);
        XrSystemId systemId = sessionInfo->createInfo->systemId;

        std::unique_lock<std::recursive_mutex> m2(gOverlaysLayerSystemIdToAtomInfoMutex);
        const XrSystemGetInfo* systemGetInfo = gOverlaysLayerSystemIdToAtomInfo[systemId]->getInfo;
        mainSessionFormFactor = systemGetInfo->formFactor;
    }
    if(mainSessionFormFactor != formFactor) {
        OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: xrCreateSession for the overlay session used an XrFormFactor (%s) which the main session did not.\n", formFactor).c_str());
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    bool didntFindExtension = false;
    {
        OverlaysLayerXrInstanceHandleInfo::Ptr mainInstanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(gMainSessionInstance);
        const XrInstanceCreateInfo* mainInstanceCreateInfo = mainInstanceInfo->createInfo;
        for(uint32_t i = 0; i < instanceCreateInfo->enabledExtensionCount; i++) {
            bool alsoInMain = FindExtensionInList(instanceCreateInfo->enabledExtensionNames[i], mainInstanceCreateInfo->enabledExtensionCount, mainInstanceCreateInfo->enabledExtensionNames);
            if(!alsoInMain) {
                OverlaysLayerLogMessage(gMainSessionInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrCreateSession",
                    OverlaysLayerNoObjectInfo, fmt("FATAL: xrCreateInstance for the parent of the overlay session specified an extension (%s) which the main session did not.\n", instanceCreateInfo->enabledExtensionNames[i]).c_str());
                didntFindExtension = true;
            }
        }
    }
    if(didntFindExtension) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    {
        auto l = connection->GetLock();
        connection->ctx = std::make_shared<MainAsOverlaySessionContext>(createInfoOverlay);
    }

    *session = mainSession;

    return XR_SUCCESS;
}


void MainRPCThreadBody(ConnectionToOverlay::Ptr connection, DWORD overlayProcessId)
{
    auto l = connection->GetLock();
    RPCChannels rpc = connection->conn;
    l.unlock();

    bool connectionLost = false;

    do {
        RPCChannels::WaitResult result = rpc.WaitForOverlayRequestOrFail();

        if(result == RPCChannels::WaitResult::OVERLAY_PROCESS_TERMINATED_UNEXPECTEDLY) {

            OutputDebugStringA("**OVERLAY** other process terminated\n");
#if 0 // XXX this should happen through graceful dtor on connection
            if(gMainSession && gMainSession->overlaySession) { // Might have LOST SESSION
                ScopedMutex scopedMutex(gOverlayCallMutex, INFINITE, "overlay layer command mutex", __FILE__, __LINE__);
                gMainSession->overlaySession->swapchainMap.clear();
                gMainSession->ClearOverlayLayers();
            }
#endif
            connectionLost = true;

        } else if(result == RPCChannels::WaitResult::WAIT_ERROR) {

            OutputDebugStringA("**OVERLAY** IPC Wait Error\n");
            DebugBreak();
#if 0 // XXX
            if(gMainSession && gMainSession->overlaySession) { // Might have LOST SESSION
                ScopedMutex scopedMutex(gOverlayCallMutex, INFINITE, "overlay layer command mutex", __FILE__, __LINE__);
                gMainSession->overlaySession->swapchainMap.clear();
                gMainSession->ClearOverlayLayers();
            }
#endif
            connectionLost = true;

        } else {

            IPCBuffer ipcbuf = rpc.GetIPCBuffer();
            IPCHeader *hdr = ipcbuf.getAndAdvance<IPCHeader>();

            hdr->makePointersAbsolute(ipcbuf.base);

            bool success = ProcessOverlayRequestOrReturnConnectionLost(connection, ipcbuf, hdr);

            if(success) {
                hdr->makePointersRelative(ipcbuf.base);
                rpc.FinishMainResponse();
            } else {
                connectionLost = true;
            }
        }

#if 0 // XXX
        if(connectionLost && gMainSession && gMainSession->overlaySession) {
            gMainSession->DestroyOverlaySession();
        }
#endif

    } while(!connectionLost && !connection->closed);

    {
        std::unique_lock<std::recursive_mutex> m(gConnectionsToOverlayByProcessIdMutex);
        gConnectionsToOverlayByProcessId.erase(connection->conn.otherProcessId);
    }
}

void MainNegotiateThreadBody()
{
    DWORD result;
    HANDLE handles[2];
    handles[0] = gNegotiationChannels.mainNegotiateThreadStop;
    handles[1] = gNegotiationChannels.mainWaitSema;

    while(1) {
        // Signal that one overlay app may attempt to connect
        ReleaseSemaphore(gNegotiationChannels.overlayWaitSema, 1, nullptr);

        do {
            result = WaitForMultipleObjects(2, handles, FALSE, NegotiationChannels::negotiationWaitMillis);
        } while(result == WAIT_TIMEOUT);

        if(result == WAIT_OBJECT_0 + 0) {

            // Main process has signaled us to stop, probably Session was destroyed.
            return;

        } else if(result != WAIT_OBJECT_0 + 1) {

            // WAIT_FAILED
            DWORD lastError = GetLastError();
            LPVOID messageBuf;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
            OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "no function", 
                OverlaysLayerNoObjectInfo, fmt("FATAL: Could not wait on negotiation sema sema: WaitForMultipleObjects error was %08X (%s)\n", lastError, messageBuf).c_str());
            // XXX need way to signal main process that thread errored unexpectedly
            LocalFree(messageBuf);
            return;
        }

        if(gNegotiationChannels.params->status != NegotiationParams::SUCCESS) {

            OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "no function",
                OverlaysLayerNoObjectInfo, fmt("WARNING: the Overlay API Layer in the overlay app has a different version (%u) than in the main app (%u), connection rejected.\n", gNegotiationChannels.params->overlayLayerBinaryVersion, gNegotiationChannels.params->mainLayerBinaryVersion).c_str());

        } else {

            DWORD overlayProcessId = gNegotiationChannels.params->overlayProcessId;
            RPCChannels channels;

            if(!OpenRPCChannels(gNegotiationChannels.instance, overlayProcessId, overlayProcessId, channels)) {

                OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "no function",
                    OverlaysLayerNoObjectInfo, fmt("WARNING: couldn't open RPC channels to overlay app, connection rejected.\n").c_str());

            } else {
                ConnectionToOverlay::Ptr connection = std::make_shared<ConnectionToOverlay>(channels);

                {
                    std::unique_lock<std::recursive_mutex> m(gConnectionsToOverlayByProcessIdMutex);
                    gConnectionsToOverlayByProcessId[overlayProcessId] = connection;
                }

                std::thread receiverThread(MainRPCThreadBody, connection, overlayProcessId);

                {
                    auto l = connection->GetLock();
                    connection->thread = std::move(receiverThread);
					connection->thread.detach();
                }
            }
        }
    }
}

bool CreateMainSessionNegotiateThread(XrInstance instance, XrSession hostingSession)
{
    gMainSessionInstance = instance;
    gMainSessionContext = std::make_shared<MainSessionContext>(hostingSession);
    if(!OpenNegotiationChannels(instance, gNegotiationChannels)) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create overlays negotiation channels\n").c_str());
        return false;
    }

    DWORD waitresult = WaitForSingleObject(gMainMutexHandle, NegotiationChannels::mutexWaitMillis);
    if (waitresult == WAIT_TIMEOUT) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not take main mutex sema; is there another main app running?\n").c_str());
        return false;
    }

    gNegotiationChannels.params->mainProcessId = GetCurrentProcessId();
    gNegotiationChannels.params->mainLayerBinaryVersion = gLayerBinaryVersion;
    gNegotiationChannels.mainNegotiateThreadStop = CreateEventA(nullptr, false, false, nullptr);
    gNegotiationChannels.mainThread = std::thread(MainNegotiateThreadBody);
    gNegotiationChannels.mainThread.detach();

    return true;
}

ConnectionToMain::Ptr gConnectionToMain;

XrResult OverlaysLayerCreateSessionMain(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session, ID3D11Device *d3d11Device)
{
    OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);

    XrResult xrresult = instanceInfo->downchain->CreateSession(instance, createInfo, session);

    // XXX create unique local id, place as that instead of created handle
    XrSession actualHandle = *session;
    XrSession localHandle = (XrSession)GetNextLocalHandle();
    *session = localHandle;

    {
        std::unique_lock<std::recursive_mutex> lock(gActualXrSessionToLocalHandleMutex);
        gActualXrSessionToLocalHandle[actualHandle] = localHandle;
    }

    OverlaysLayerXrSessionHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrSessionHandleInfo>(instance, instance, instanceInfo->downchain);
    info->createInfo = reinterpret_cast<XrSessionCreateInfo*>(CopyXrStructChainWithMalloc(instance, createInfo));
    info->actualHandle = actualHandle;
    info->isProxied = false;
    info->d3d11Device = d3d11Device;

    ID3D11Multithread* d3dMultithread;
    HRESULT hr = d3d11Device->QueryInterface(__uuidof(ID3D11Multithread), reinterpret_cast<void**>(&d3dMultithread));
    if(hr != S_OK) {
        LogWindowsError(hr, "xrCreateSession", "QueryInterface", __FILE__, __LINE__);
        return XR_ERROR_RUNTIME_FAILURE;
    }
    d3dMultithread->SetMultithreadProtected(TRUE);
    d3dMultithread->Release();

    // Create XrPaths for well-known strings.  We can use the compile-time fixed string enums to pass strings and paths over RPC
    // XXX This should be on CreateInstance in the instance info
    if(OverlaysLayerWellKnownStringToPath.size() == 0) { 
        for(auto& w : OverlaysLayerWellKnownStrings) {
            XrPath path;
            XrResult result2 = instanceInfo->downchain->StringToPath(instance, w.second, &path);
            if(result2 != XR_SUCCESS) {
                OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
                    OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create path from.\n", w.second).c_str());
                return XR_ERROR_INITIALIZATION_FAILED;
            }
            OverlaysLayerWellKnownStringToPath.insert({w.first, path});
            OverlaysLayerPathToWellKnownString.insert({path, w.first});
        }
    }

    // create placeholder Actions

    XrActionSetCreateInfo createActionSetInfo { XR_TYPE_ACTION_SET_CREATE_INFO, nullptr, "overlaysapilayer", "overlays API layer synthetic actionset", 1 };
    XrResult result2 = instanceInfo->downchain->CreateActionSet(instance, &createActionSetInfo, &info->placeholderActionSet);
    if(result2 != XR_SUCCESS) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create session placeholder ActionSet.\n").c_str());
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    int i=1;
    for(auto& id : PlaceholderActionIds) {
        char placeholderNameString[64];
        sprintf(placeholderNameString, "overlays%d", i++);

        XrActionCreateInfo createActionInfo { XR_TYPE_ACTION_CREATE_INFO };
        strcpy(createActionInfo.actionName, placeholderNameString);
        strcpy(createActionInfo.localizedActionName, placeholderNameString);
        createActionInfo.actionType = id.type;
        createActionInfo.countSubactionPaths = 1;
        XrPath subactionPath = OverlaysLayerWellKnownStringToPath.at(id.subActionString);
        createActionInfo.subactionPaths = &subactionPath;

        XrAction action;
        XrResult result2 = instanceInfo->downchain->CreateAction(info->placeholderActionSet, &createActionInfo, &action);
        if(result2 != XR_SUCCESS) {
            OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
                OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create session placeholder action for %s.\n", id.name).c_str());
            return XR_ERROR_INITIALIZATION_FAILED;
        }

        XrPath fullBindingPath = OverlaysLayerWellKnownStringToPath.at(id.fullBindingString);
        info->placeholderActions.insert({fullBindingPath, {action, id.type}});

        XrPath interactionProfilePath = OverlaysLayerWellKnownStringToPath.at(id.interactionProfileString);
        info->bindingsByProfile[interactionProfilePath].push_back({action, fullBindingPath});
        info->bindingsByAction[action] = fullBindingPath;

        // XXX This should be on CreateInstance in the instance info
        OverlaysLayerBindingToSubaction.insert({fullBindingPath, subactionPath});
    }

    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrSessionToHandleInfoMutex);
    gOverlaysLayerXrSessionToHandleInfo.insert({localHandle, info});
    mlock2.unlock();

    gAppHasAMainSession = true;

    bool result = CreateMainSessionNegotiateThread(instance, localHandle);

    if(!result) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not initialize the Main App listener thread.\n").c_str());
        return XR_ERROR_INITIALIZATION_FAILED;
    }
    return xrresult;
}

bool ConnectToMain(XrInstance instance)
{
    // check to make sure not already main session process
    if(gMainSessionInstance != XR_NULL_HANDLE) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, "FATAL: attempt to make an overlay session while also having a main session\n");
        return false;
    }

    gConnectionToMain = std::make_shared<ConnectionToMain>();

    if(!OpenNegotiationChannels(instance, gNegotiationChannels)) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, "FATAL: could not open negotiation channels\n");
        return false;
    }

    DWORD result;
    int attempts = 0;
    do {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("Attempt #%d (of %d) to connect to the main app\n", attempts, NegotiationChannels::maxAttempts).c_str());
        result = WaitForSingleObject(gNegotiationChannels.overlayWaitSema, NegotiationChannels::negotiationWaitMillis);
        attempts++;
    } while(attempts < NegotiationChannels::maxAttempts && result == WAIT_TIMEOUT);

    if(result == WAIT_TIMEOUT) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: the Overlay API Layer in the overlay app could not connect to the main app after %d tries.\n", attempts).c_str());
        return false;
    }

    OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, "xrCreateSession", 
        OverlaysLayerNoObjectInfo, fmt("connected to the main app after %d %s.\n", attempts, (attempts < 2) ? "try" : "tries").c_str());

    if(gNegotiationChannels.params->mainLayerBinaryVersion != gLayerBinaryVersion) {
        gNegotiationChannels.params->status = NegotiationParams::DIFFERENT_BINARY_VERSION;
        ReleaseSemaphore(gNegotiationChannels.mainWaitSema, 1, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: the Overlay API Layer in the overlay app has a different version (%u) than in the main app (%u).\n").c_str());
        return false;
    }

    /* save off negotiation parameters because they may be overwritten at any time after we Release mainWait */
    gMainProcessId = gNegotiationChannels.params->mainProcessId;
    gNegotiationChannels.params->overlayProcessId = GetCurrentProcessId();
    gNegotiationChannels.params->status = NegotiationParams::SUCCESS;

    ReleaseSemaphore(gNegotiationChannels.mainWaitSema, 1, nullptr);

    if(!OpenRPCChannels(gNegotiationChannels.instance, gMainProcessId, GetCurrentProcessId(), gConnectionToMain->conn)) {
        OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "no function",
            OverlaysLayerNoObjectInfo, "WARNING: couldn't open RPC channels to main app, connection failed.\n");
        return false;
    }

    return true;
}


XrResult OverlaysLayerCreateSessionOverlay(
    XrInstance                                  instance,
    const XrSessionCreateInfo*                  createInfo,
    XrSession*                                  session,
    const XrSessionCreateInfoOverlayEXTX*       createInfoOverlay,
    ID3D11Device*   d3d11Device)
{
    XrResult result = XR_SUCCESS;

    // Only on Overlay XrSession Creation, connect to the main app.
    if(!ConnectToMain(instance)) {
        OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "XrCreateSession",
            OverlaysLayerNoObjectInfo, "WARNING: couldn't connect to main app.\n");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Get our tracked information on this XrInstance 
    OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);

    XrFormFactor formFactor;

    {
        // XXX REALLY SHOULD RPC TO GET THE REMOTE SYSTEMID HERE AND THEN SUBSTITUTE THAT IN THE OVERLAY BEFORE THE RPC.
        std::unique_lock<std::recursive_mutex> m(gOverlaysLayerSystemIdToAtomInfoMutex);
        const XrSystemGetInfo* systemGetInfo = gOverlaysLayerSystemIdToAtomInfo[createInfo->systemId]->getInfo;
        formFactor = systemGetInfo->formFactor;
        // XXX should check here that systemGetInfo->next == nullptr
        // but current API Layer is not specified to support any
        // XrSystemId-related extensions
    }

    auto instanceCreateInfo = instanceInfo->createInfo;

    const char** extensionNamesMinusOverlay = new const char*[instanceCreateInfo->enabledExtensionCount];
    uint32_t extensionCountMinusOverlay = 0;
    for(uint32_t i = 0; i < instanceCreateInfo->enabledExtensionCount; i++) {
        if(strncmp(instanceCreateInfo->enabledExtensionNames[i], XR_EXTX_OVERLAY_EXTENSION_NAME, strlen(XR_EXTX_OVERLAY_EXTENSION_NAME)) != 0) {
            extensionNamesMinusOverlay[extensionCountMinusOverlay++] = instanceCreateInfo->enabledExtensionNames[i];
        }
    }

    XrInstanceCreateInfo createInfoMinusOverlays = *instanceCreateInfo;
    createInfoMinusOverlays.enabledExtensionNames = extensionNamesMinusOverlay;
    createInfoMinusOverlays.enabledExtensionCount = extensionCountMinusOverlay;

    result = RPCCallCreateSession(instance, formFactor, &createInfoMinusOverlays, createInfo, createInfoOverlay, session);

    delete[] extensionNamesMinusOverlay;

    if (!XR_SUCCEEDED(result)) {
        return result;
    }

    // Since Overlays are the parent object of a hierarchy of objects that the Main hosts on behalf of the Overlay,
    // make a unique local XrSession that notes that this is actually an overlay session and any command on this handle has to be proxied.
    // Non-Overlay XrSessions are also replaced locally with a unique local handle in case an overlay app has one.
    XrSession actualHandle = *session;
    XrSession localHandle = (XrSession)GetNextLocalHandle();
	*session = localHandle;

    {
        std::unique_lock<std::recursive_mutex> lock(gActualXrSessionToLocalHandleMutex);
        gActualXrSessionToLocalHandle.insert({actualHandle, localHandle});
    }
 
    OverlaysLayerXrSessionHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrSessionHandleInfo>(instance, instance, instanceInfo->downchain);
    info->actualHandle = actualHandle;
    info->isProxied = true;
    info->d3d11Device = d3d11Device;
    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrSessionToHandleInfoMutex);
    gOverlaysLayerXrSessionToHandleInfo.insert({localHandle, info});
    mlock2.unlock();

    // Create XrPaths for well-known strings.  We can use the compile-time fixed string enums to pass strings and paths over RPC
    // XXX This should be on CreateInstance in the instance info
    if(OverlaysLayerWellKnownStringToPath.size() == 0) { 
        for(auto& w : OverlaysLayerWellKnownStrings) {
            XrPath path;
            XrResult result2 = instanceInfo->downchain->StringToPath(instance, w.second, &path);
            if(result2 != XR_SUCCESS) {
                OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
                    OverlaysLayerNoObjectInfo, fmt("FATAL: Could not create path from.\n", w.second).c_str());
                return XR_ERROR_INITIALIZATION_FAILED;
            }
            OverlaysLayerWellKnownStringToPath.insert({w.first, path});
            OverlaysLayerPathToWellKnownString.insert({path, w.first});
        }
    }

    // XXX This should be on CreateInstance in the instance info
    int i=1;
    for(auto& id : PlaceholderActionIds) {
        XrPath fullBindingPath = OverlaysLayerWellKnownStringToPath.at(id.fullBindingString);
        XrPath subactionPath = OverlaysLayerWellKnownStringToPath.at(id.subActionString);
        OverlaysLayerBindingToSubaction.insert({fullBindingPath, subactionPath});
    }

    return result;
}

XrResult OverlaysLayerCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session)
{
    try{
        XrResult result;

        const XrBaseInStructure* p = reinterpret_cast<const XrBaseInStructure*>(createInfo->next);
        const XrSessionCreateInfoOverlayEXTX* cio = nullptr;
        const XrGraphicsBindingD3D11KHR* d3dbinding = nullptr;
        while(p) {
            if(p->type == XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX) {
                cio = reinterpret_cast<const XrSessionCreateInfoOverlayEXTX*>(p);
            }
            // XXX save off requested API in Overlay, match against Main API
            // XXX save off requested API in Main, match against Overlay API
            if( (p->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR) ||
                (p->type == XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR) ||
                (p->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR) ||
                (p->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR) ||
                (p->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR) ||
                (p->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR) ||
                (p->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR) ||
                (p->type == XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR)) {
                return XR_ERROR_GRAPHICS_DEVICE_INVALID;
            }
            if(p->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR) {
                d3dbinding = reinterpret_cast<const XrGraphicsBindingD3D11KHR*>(p);
            }
            p = reinterpret_cast<const XrBaseInStructure*>(p->next);
        }

        if(!cio) {
            result = OverlaysLayerCreateSessionMain(instance, createInfo, session, d3dbinding->device);
        } else {
            result = OverlaysLayerCreateSessionOverlay(instance, createInfo, session, cio, d3dbinding->device);
        }

        return result;
    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;
    }
}

XrResult OverlaysLayerCreateSwapchainMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain, uint32_t *swapchainCount)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto createInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrCreateSwapchain", createInfo);

    XrResult result = sessionInfo->downchain->CreateSwapchain(sessionInfo->actualHandle, createInfoCopy.get(), swapchain);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    XrSwapchain actualHandle = *swapchain;
    XrSwapchain localHandle = (XrSwapchain)GetNextLocalHandle();
    *swapchain = localHandle;

    uint32_t count;
    result = sessionInfo->downchain->EnumerateSwapchainImages(actualHandle, 0, &count, nullptr);
    if(!XR_SUCCEEDED(result)) {
        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSwapchain",
            OverlaysLayerNoObjectInfo, "FATAL: Couldn't call EnumerateSwapchainImages to get swapchain image count.\n");
        return result;
    }

    std::vector<XrSwapchainImageD3D11KHR> swapchainImages(count);
    std::vector<ID3D11Texture2D*> swapchainTextures(count);
    for(uint32_t i = 0; i < count; i++) {
        swapchainImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
        swapchainImages[i].next = nullptr;
    }
    result = sessionInfo->downchain->EnumerateSwapchainImages(actualHandle, count, &count, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages.data()));
    if(!XR_SUCCEEDED(result)) {
        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSwapchain",
            OverlaysLayerNoObjectInfo, "FATAL: Couldn't call EnumerateSwapchainImages to get swapchain images.\n");
        return result;
    }

    for(uint32_t i = 0; i < count; i++) {
        swapchainTextures[i] = swapchainImages[i].texture;
    }

    *swapchainCount = count;

    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = std::make_shared<OverlaysLayerXrSwapchainHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);
    swapchainInfo->mainAsOverlaySwapchain = std::make_shared<SwapchainCachedData>(*swapchain, swapchainTextures);
    swapchainInfo->actualHandle = actualHandle;
    { 
        std::unique_lock<std::recursive_mutex> lock(gOverlaysLayerXrSwapchainToHandleInfoMutex);
        gOverlaysLayerXrSwapchainToHandleInfo.insert({*swapchain, swapchainInfo});
    }

    return result;
}

XrResult OverlaysLayerCreateSwapchainOverlay(XrInstance instance, XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    uint32_t swapchainCount;

    auto createInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrCreateSwapchain", createInfo);

    XrResult result = RPCCallCreateSwapchain(instance, sessionInfo->actualHandle, createInfoCopy.get(), swapchain, &swapchainCount);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    XrSwapchain actualHandle = *swapchain;
    XrSwapchain localHandle = (XrSwapchain)GetNextLocalHandle();
    *swapchain = localHandle;

    {
        std::unique_lock<std::recursive_mutex> lock(gActualXrSwapchainToLocalHandleMutex);
        gActualXrSwapchainToLocalHandle.insert({actualHandle, localHandle});
    }

    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = std::make_shared<OverlaysLayerXrSwapchainHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);

    swapchainInfo->actualHandle = actualHandle;
    swapchainInfo->isProxied = true;

    OverlaySwapchain::Ptr overlaySwapchain = std::make_shared<OverlaySwapchain>(*swapchain, swapchainCount, createInfo);
    swapchainInfo->overlaySwapchain = overlaySwapchain;

    if(!overlaySwapchain->CreateTextures(instance, sessionInfo->d3d11Device, gConnectionToMain->conn.otherProcessId)) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSwapchain",
            OverlaysLayerNoObjectInfo, "FATAL: couldn't create D3D local resources for swapchain images\\n");
        // XXX This leaks the session in main process if the Session is not closed.
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    { 
        std::unique_lock<std::recursive_mutex> lock(gOverlaysLayerXrSwapchainToHandleInfoMutex);
        gOverlaysLayerXrSwapchainToHandleInfo.insert({*swapchain, swapchainInfo});
    }

    return result;
}

XrResult OverlaysLayerDestroySwapchainMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSwapchain swapchain)
{
    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(swapchain);

    OverlaysLayerRemoveXrSwapchainHandleInfo(swapchain);

    // XXX anything here?  Need to manage error returns as if this was a runtime?  invalid handle will be caught by GetHandleInfo...

    return XR_SUCCESS;
}

XrResult OverlaysLayerDestroySwapchainOverlay(XrInstance instance, XrSwapchain swapchain)
{
    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(swapchain);

    XrResult result = RPCCallDestroySwapchain(swapchainInfo->parentInstance, swapchainInfo->actualHandle);

    OverlaysLayerRemoveXrSwapchainHandleInfo(swapchain);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

	// XXX anything here?

    return result;
}


XrResult OverlaysLayerCreateReferenceSpaceMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto createInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrCreateSwapchain", createInfo);

    XrResult result = sessionInfo->downchain->CreateReferenceSpace(sessionInfo->actualHandle, createInfoCopy.get(), space);

    XrSpace actualHandle = *space;
    XrSpace localHandle = (XrSpace)GetNextLocalHandle();
    *space = localHandle;

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = std::make_shared<OverlaysLayerXrSpaceHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);
    spaceInfo->actualHandle = actualHandle;
    spaceInfo->spaceType = SPACE_REFERENCE;

    { 
        std::unique_lock<std::recursive_mutex> lock(gOverlaysLayerXrSpaceToHandleInfoMutex);
        gOverlaysLayerXrSpaceToHandleInfo.insert({*space, spaceInfo});
    }

    return result;
}

XrResult OverlaysLayerCreateReferenceSpaceOverlay(XrInstance instance, XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto createInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrCreateSwapchain", createInfo);

    XrResult result = RPCCallCreateReferenceSpace(instance, sessionInfo->actualHandle, createInfoCopy.get(), space);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    XrSpace actualHandle = *space;
    XrSpace localHandle = (XrSpace)GetNextLocalHandle();
    *space = localHandle;

    {
        std::unique_lock<std::recursive_mutex> lock(gActualXrSpaceToLocalHandleMutex);
        gActualXrSpaceToLocalHandle.insert({actualHandle, localHandle});
    }

    OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = std::make_shared<OverlaysLayerXrSpaceHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);

    spaceInfo->actualHandle = actualHandle;
    spaceInfo->spaceType = SPACE_REFERENCE;
    spaceInfo->isProxied = true;

    { 
        std::unique_lock<std::recursive_mutex> lock(gOverlaysLayerXrSpaceToHandleInfoMutex);
        gOverlaysLayerXrSpaceToHandleInfo.insert({*space, spaceInfo});
    }

    return result;
}

XrResult OverlaysLayerEnumerateReferenceSpacesMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    return sessionInfo->downchain->EnumerateReferenceSpaces(sessionInfo->actualHandle, spaceCapacityInput, spaceCountOutput, spaces);
}

XrResult OverlaysLayerEnumerateReferenceSpacesOverlay(XrInstance instance, XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    return RPCCallEnumerateReferenceSpaces(instance, sessionInfo->actualHandle, spaceCapacityInput, spaceCountOutput, spaces);
}

XrResult OverlaysLayerGetReferenceSpaceBoundsRectMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, XrReferenceSpaceType referenceSpaceType, XrExtent2Df* bounds)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    return sessionInfo->downchain->GetReferenceSpaceBoundsRect(sessionInfo->actualHandle, referenceSpaceType, bounds);
}

XrResult OverlaysLayerGetReferenceSpaceBoundsRectOverlay(XrInstance instance, XrSession session, XrReferenceSpaceType referenceSpaceType, XrExtent2Df* bounds)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    return RPCCallGetReferenceSpaceBoundsRect(instance, sessionInfo->actualHandle, referenceSpaceType, bounds);
}

XrResult OverlaysLayerLocateSpaceMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
{
    OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(space);
    OverlaysLayerXrSpaceHandleInfo::Ptr baseSpaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(baseSpace);

    XrResult result = spaceInfo->downchain->LocateSpace(spaceInfo->actualHandle, baseSpaceInfo->actualHandle, time, location);

    if(result == XR_SUCCESS) {
        SubstituteLocalHandles(spaceInfo->parentInstance, (XrBaseOutStructure *)location);
    }

    return result;
}


XrResult OverlaysLayerLocateSpaceOverlay(XrInstance instance, XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
{
    OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(space);
    OverlaysLayerXrSpaceHandleInfo::Ptr baseSpaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(baseSpace);

    XrResult result;

    switch(spaceInfo->spaceType) {
        case SPACE_REFERENCE:
            result = RPCCallLocateSpace(instance, spaceInfo->actualHandle, baseSpaceInfo->actualHandle, time, location);
            break;
        case SPACE_ACTION:
            if(spaceInfo->action->bindLocation == BIND_PENDING) {
                // This isn't bound anywhere yet, so we return not locatable
                location->locationFlags = 0;
                return XR_SUCCESS;
            }
            result = RPCCallLocateSpace(instance, spaceInfo->actualHandle, baseSpaceInfo->actualHandle, time, location);
            break;
    }

    if(result == XR_SUCCESS) {
        SubstituteLocalHandles(spaceInfo->parentInstance, (XrBaseOutStructure *)location);
    }

    return result;
}

XrResult OverlaysLayerDestroySpaceMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSpace space)
{
    OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(space);

    // XXX This will need to be smart about ActionSpaces?

    XrResult result = spaceInfo->downchain->DestroySpace(spaceInfo->actualHandle);
    OverlaysLayerRemoveXrSpaceHandleInfo(space);

    return result;
}

XrResult OverlaysLayerDestroySpaceOverlay(XrInstance instance, XrSpace space)
{
    OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(space);

    // XXX This will need to be smart about ActionSpaces?

    XrResult result = RPCCallDestroySpace(instance, spaceInfo->actualHandle);

    OverlaysLayerRemoveXrSpaceHandleInfo(space);

    return result;
}

XrResult OverlaysLayerLocateViewsMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto viewLocateInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrLocateViews", viewLocateInfo);

    XrResult result = sessionInfo->downchain->LocateViews(sessionInfo->actualHandle, viewLocateInfoCopy.get(), viewState, viewCapacityInput, viewCountOutput, views);

    if(result == XR_SUCCESS) {
        SubstituteLocalHandles(sessionInfo->parentInstance, (XrBaseOutStructure *)viewState);
        if(views != nullptr) {
            for(uint32_t i = 0; i < *viewCountOutput; i++) {
                SubstituteLocalHandles(sessionInfo->parentInstance, (XrBaseOutStructure *)&views[i]);
            }
        }
    }

    return result;
}

XrResult OverlaysLayerLocateViewsOverlay(XrInstance instance, XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto viewLocateInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrLocateViews", viewLocateInfo);

    XrResult result = RPCCallLocateViews(instance, sessionInfo->actualHandle, viewLocateInfoCopy.get(), viewState, viewCapacityInput, viewCountOutput, views);

    if(result == XR_SUCCESS) {
        SubstituteLocalHandles(instance, (XrBaseOutStructure *)viewState);
        if(views != nullptr) {
            for(uint32_t i = 0; i < *viewCountOutput; i++) {
                SubstituteLocalHandles(instance, (XrBaseOutStructure *)&views[i]);
            }
        }
    }
    return result;
}

XrResult OverlaysLayerDestroySessionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    connection->closed = true;

    return XR_SUCCESS;
}

XrResult OverlaysLayerDestroySessionOverlay(XrInstance instance, XrSession session)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    XrResult result = RPCCallDestroySession(instance, sessionInfo->actualHandle);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    return result;
}

XrResult OverlaysLayerEnumerateSwapchainFormatsMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    // Already have our tracked information on this XrSession from generated code in sessionInfo
    XrResult result = sessionInfo->downchain->EnumerateSwapchainFormats(sessionInfo->actualHandle, formatCapacityInput, formatCountOutput, formats);

    return result;
}

XrResult OverlaysLayerEnumerateSwapchainFormatsOverlay(XrInstance instance, XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    XrResult result = RPCCallEnumerateSwapchainFormats(instance, sessionInfo->actualHandle, formatCapacityInput, formatCountOutput, formats);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    return result;
}

// EnumerateSwapchainImages is handled entirely on Overlay side because we
// already Enumerated the main SwapchainImages when we created the Swapchain.
XrResult OverlaysLayerEnumerateSwapchainImagesOverlay(
        XrInstance instance,
        XrSwapchain swapchain,
        uint32_t imageCapacityInput,
        uint32_t* imageCountOutput,
        XrSwapchainImageBaseHeader* images)
{
    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(swapchain);

    auto& overlaySwapchain = swapchainInfo->overlaySwapchain;

    if(imageCapacityInput == 0) {
        *imageCountOutput = (uint32_t)overlaySwapchain->swapchainTextures.size();
        return XR_SUCCESS;
    }

    if(images[0].type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
        OverlaysLayerXrInstanceHandleInfo::Ptr info = OverlaysLayerGetHandleInfoFromXrInstance(instance);

        char structTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
        structTypeName[0] = '\0';
        if(info->downchain->StructureTypeToString(instance, images[0].type, structTypeName) != XR_SUCCESS) {
            sprintf(structTypeName, "<type %08X>", images[0].type);
        }
        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrEnumerateSwapchainImages",
            OverlaysLayerNoObjectInfo, fmt("FATAL: images structure type is %s and not XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR.\n", structTypeName).c_str());
        return XR_ERROR_VALIDATION_FAILURE;
    }

    // (If storage is provided) Give back the "local" swapchainimages (rendertarget) for rendering
    auto sci = reinterpret_cast<XrSwapchainImageD3D11KHR*>(images);
    uint32_t toWrite = std::min(imageCapacityInput, (uint32_t)overlaySwapchain->swapchainTextures.size());
    for(uint32_t i = 0; i < toWrite; i++) {
        sci[i].texture = overlaySwapchain->swapchainTextures[i];
    }

    *imageCountOutput = toWrite;

    return XR_SUCCESS;
}

XrResult OverlaysLayerPollEventMainAsOverlay(ConnectionToOverlay::Ptr connection, XrEventDataBuffer *eventData)
{
    XrResult result;

    OptionalSessionStateChange pendingStateChange;

	{
		MainSessionContext::Ptr mainSessionContext = gMainSessionContext;
		auto l = mainSessionContext->GetLock();
		pendingStateChange = connection->ctx->sessionState.GetAndDoPendingStateChange(&mainSessionContext->sessionState);
	}

    if(pendingStateChange.first) {

        auto* ssc = reinterpret_cast<XrEventDataSessionStateChanged*>(eventData);
        ssc->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
        ssc->next = nullptr;

		{
			MainSessionContext::Ptr mainSessionContext = gMainSessionContext;
			auto l = mainSessionContext->GetLock();
			ssc->session = mainSessionContext->session;
		}
        
        ssc->state = pendingStateChange.second;
        XrTime calculatedTime = 1; // XXX 
        ssc->time = calculatedTime;
        result = XR_SUCCESS;

    } else {

        auto lock = connection->ctx->GetLock();

        if(connection->ctx->eventsSaved.size() == 0) {

            result = XR_EVENT_UNAVAILABLE;

        } else {

            EventDataBufferPtr event = connection->ctx->eventsSaved.front();
            connection->ctx->eventsSaved.pop();

            // XXX does this correctly fill the EventDataBuffer with a chain?
            CopyEventChainIntoBuffer(gMainSessionInstance, reinterpret_cast<XrEventDataBaseHeader*>(event.get()), eventData);

            result = XR_SUCCESS;
        }
    }

    return result;
}

void EnqueueEventToOverlay(XrInstance instance, XrEventDataBuffer *eventData, MainAsOverlaySessionContext::Ptr overlay)
{
    auto lock = overlay->GetLock();

    bool queueFull = (overlay->eventsSaved.size() == MainAsOverlaySessionContext::maxEventsSavedForOverlay);
    bool queueOneShortOfFull = (overlay->eventsSaved.size() == MainAsOverlaySessionContext::maxEventsSavedForOverlay - 1);
    bool backIsEventsLostEvent = (overlay->eventsSaved.size() > 0) && (overlay->eventsSaved.back()->type == XR_TYPE_EVENT_DATA_EVENTS_LOST);

    bool alreadyLostSomeEvents = queueFull || (queueOneShortOfFull && backIsEventsLostEvent);

    EventDataBufferPtr newEvent(new XrEventDataBuffer{XR_TYPE_EVENT_DATA_BUFFER});
    CopyEventChainIntoBuffer(instance, const_cast<const XrEventDataBaseHeader*>(reinterpret_cast<XrEventDataBaseHeader*>(eventData)), newEvent.get());

    if(newEvent.get()->type != XR_TYPE_EVENT_DATA_BUFFER) {
        // We were able to find some known events in the event pointer chain

        if(alreadyLostSomeEvents) {

            auto* lost = reinterpret_cast<XrEventDataEventsLost*>(overlay->eventsSaved.back().get());
            lost->lostEventCount ++;

        } else if(queueOneShortOfFull) {

            EventDataBufferPtr newEvent(new XrEventDataBuffer);
            XrEventDataEventsLost* lost = reinterpret_cast<XrEventDataEventsLost*>(newEvent.get());
            lost->type = XR_TYPE_EVENT_DATA_EVENTS_LOST;
            lost->next = nullptr;
            lost->lostEventCount = 1;
            overlay->eventsSaved.push(newEvent);
            OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrPollEvent",
                OverlaysLayerNoObjectInfo, "WARNING: Enqueued a lost event.\n");

        } else {

            overlay->eventsSaved.push(newEvent);
            
        }
    }
}

XrResult OverlaysLayerPollEvent(XrInstance instance, XrEventDataBuffer* eventData)
{
    try {
        std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrInstanceToHandleInfoMutex);
        auto it = gOverlaysLayerXrInstanceToHandleInfo.find(instance);
        if(it == gOverlaysLayerXrInstanceToHandleInfo.end()) {
            OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrPollEvent",
                OverlaysLayerNoObjectInfo, "FATAL: invalid handle couldn't be found in tracking map.\n");
            return XR_ERROR_VALIDATION_FAILURE;
        }
        OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = it->second;

        XrResult result = instanceInfo->downchain->PollEvent(instance, eventData);

        if(result == XR_EVENT_UNAVAILABLE) {

            if(gConnectionToMain) {

                /* See if the main process has any events for us */
                result = RPCCallPollEvent(instance, eventData);
                if(eventData->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                    const auto* ssc = reinterpret_cast<const XrEventDataSessionStateChanged*>(eventData);
                }
                if(result == XR_SUCCESS) {
                    SubstituteLocalHandles(instance, (XrBaseOutStructure *)eventData);
                }
            }

        } else if(result == XR_SUCCESS) {

            SubstituteLocalHandles(instance, (XrBaseOutStructure *)eventData);

            if(eventData->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {

                // XXX ignores any chained event data
                const auto* ssc = reinterpret_cast<const XrEventDataSessionStateChanged*>(eventData);
                MainSessionContext::Ptr mainSessionContext = gMainSessionContext;
                auto l = mainSessionContext->GetLock();
                mainSessionContext->sessionState.DoStateChange(ssc->state, ssc->time);

                if(ssc->next) {
                    char structureTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
                                        auto* p = reinterpret_cast<const XrBaseOutStructure*>(ssc->next);
                    XrResult r = instanceInfo->downchain->StructureTypeToString(gMainSessionInstance, p->type, structureTypeName);
                    if(r != XR_SUCCESS) {
                        sprintf(structureTypeName, "(type %08X)", p->type);
                    }

                    OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrPollEvent",
                        OverlaysLayerNoObjectInfo, fmt("WARNING: xrPollEvent filled a struct (%s) which the Overlay API Layer does not know how to check.\n", structureTypeName).c_str());
                }

            } else {

                std::unique_lock<std::recursive_mutex> lock(gConnectionsToOverlayByProcessIdMutex);
                if(!gConnectionsToOverlayByProcessId.empty()) {

                    if(eventData->type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {

                        for(auto& overlayconn: gConnectionsToOverlayByProcessId) {
                            auto conn = overlayconn.second;
                            auto lock = conn->GetLock();
                            if(conn->ctx) {
                                EnqueueEventToOverlay(instance, eventData, conn->ctx);
                            }
                        }

                    } else if(eventData->type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {

                        // XXX further processing
                        // If occurred earlier than an Overlay connect, will need to synthesize this.
                        for(auto& overlayconn: gConnectionsToOverlayByProcessId) {
                            auto conn = overlayconn.second;
                            auto lock = conn->GetLock();
                            if(conn->ctx) {
                                EnqueueEventToOverlay(instance, eventData, conn->ctx);
                            }
                        }
                    }

                }
            }
        }

        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;
    }
}

XrResult OverlaysLayerBeginSessionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrSessionBeginInfo* beginInfo)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto l = connection->GetLock();
    // auto beginInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrCreateSwapchain", beginInfo);

    connection->ctx->sessionState.DoCommand(OpenXRCommand::BEGIN_SESSION);

    return XR_SUCCESS;
}

XrResult OverlaysLayerBeginSessionOverlay(XrInstance instance, XrSession session, const XrSessionBeginInfo* beginInfo)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto beginInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrBeginSession", beginInfo);

    XrResult result = RPCCallBeginSession(instance, sessionInfo->actualHandle, beginInfoCopy.get());

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    return result;
}

XrResult OverlaysLayerRequestExitSessionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto l = connection->GetLock();
    if(!connection->ctx->sessionState.isRunning) {
        return XR_ERROR_SESSION_NOT_RUNNING;
    }
    connection->ctx->sessionState.DoCommand(OpenXRCommand::REQUEST_EXIT_SESSION);

    return XR_SUCCESS;
}

XrResult OverlaysLayerRequestExitSessionOverlay(XrInstance instance, XrSession session)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    XrResult result = RPCCallRequestExitSession(instance, sessionInfo->actualHandle);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    return result;
}

XrResult OverlaysLayerEndSessionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto l = connection->GetLock();
    if(connection->ctx->sessionState.sessionState != XR_SESSION_STATE_STOPPING) {
        return XR_ERROR_SESSION_NOT_STOPPING;
    }
    connection->ctx->sessionState.DoCommand(OpenXRCommand::END_SESSION);
    connection->ctx->overlayLayers.clear();

    return XR_SUCCESS;
}

XrResult OverlaysLayerEndSessionOverlay(XrInstance instance, XrSession session)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    XrResult result = RPCCallEndSession(instance, sessionInfo->actualHandle);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    return result;
}

XrResult OverlaysLayerWaitFrameMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState)
{
    auto l = connection->GetLock();

    if(!connection->ctx->relaxedDisplayTime) {
        // XXX tell main we are waiting by setting a variable in ctx and then wait on a semaphore
    }
    auto mainSession = gMainSessionContext;
    auto lock2 = mainSession->GetLock();
    // XXX this is incomplete; need to descend next chain and copy as possible from saved requirements.
    frameState->predictedDisplayTime = mainSession->sessionState.savedFrameState->predictedDisplayTime;
    frameState->predictedDisplayPeriod = mainSession->sessionState.savedFrameState->predictedDisplayPeriod;
    frameState->shouldRender = mainSession->sessionState.savedFrameState->shouldRender;

    mainSession->sessionState.IncrementPredictedDisplayTime();

    return XR_SUCCESS;
}


XrResult OverlaysLayerWaitFrameOverlay(XrInstance instance, XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto frameWaitInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrWaitFrame", frameWaitInfo);

    XrResult result = RPCCallWaitFrame(instance, sessionInfo->actualHandle, frameWaitInfoCopy.get(), frameState);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    return result;
}

XrResult OverlaysLayerBeginFrameMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrFrameBeginInfo* frameBeginInfo)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto l = connection->GetLock();

    // At this time xrBeginFrame has no inputs and returns nothing.
    //auto beginInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrBeginFrame", beginInfo);

    return XR_SUCCESS;
}

XrResult OverlaysLayerBeginFrameOverlay(XrInstance instance, XrSession session, const XrFrameBeginInfo* frameBeginInfo)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto frameBeginInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrBeginFrame", frameBeginInfo);

    XrResult result = RPCCallBeginFrame(instance, sessionInfo->actualHandle, frameBeginInfoCopy.get());

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    return result;
}

XrResult OverlaysLayerAcquireSwapchainImageMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t *index)
{
    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(swapchain);

    auto acquireInfoCopy = GetSharedCopyHandlesRestored(swapchainInfo->parentInstance, "xrAcquireSwapchainImage", acquireInfo);

    XrResult result = swapchainInfo->downchain->AcquireSwapchainImage(swapchainInfo->actualHandle, acquireInfoCopy.get(), index);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    swapchainInfo->mainAsOverlaySwapchain->acquired.push_back(*index);

    return result;
}

XrResult OverlaysLayerAcquireSwapchainImageOverlay(XrInstance instance, XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t *index)
{
    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(swapchain);

    auto acquireInfoCopy = GetSharedCopyHandlesRestored(swapchainInfo->parentInstance, "xrAcquireSwapchainImage", acquireInfo);

    XrResult result = RPCCallAcquireSwapchainImage(instance, swapchainInfo->actualHandle, acquireInfoCopy.get(), index);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    swapchainInfo->overlaySwapchain->acquired.push_back(*index);

    return result;
}

XrResult OverlaysLayerWaitSwapchainImageMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo, HANDLE sourceImage)
{
    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(swapchain);

    auto waitInfoCopy = GetSharedCopyHandlesRestored(swapchainInfo->parentInstance, "xrWaitSwapchainImage", waitInfo);

    XrResult result = swapchainInfo->downchain->WaitSwapchainImage(swapchainInfo->actualHandle, waitInfoCopy.get());

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    auto& mainAsOverlaySwapchain = swapchainInfo->mainAsOverlaySwapchain;
    if(mainAsOverlaySwapchain->remoteImagesAcquired.find(sourceImage) != mainAsOverlaySwapchain->remoteImagesAcquired.end()) {
        IDXGIKeyedMutex* keyedMutex;
        {
            ID3D11Device* d3d11Device;
            {
                OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(swapchainInfo->parentHandle);
                d3d11Device = sessionInfo->d3d11Device;
            }

            ID3D11Texture2D *sharedTexture = mainAsOverlaySwapchain->getSharedTexture(d3d11Device, sourceImage);
            HRESULT result = sharedTexture->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex);
            if(result != S_OK) {
                LogWindowsError(result, "xrWaitSwapchainImage", "QueryInterface", __FILE__, __LINE__);
                return XR_ERROR_RUNTIME_FAILURE;
            }
        }
        mainAsOverlaySwapchain->remoteImagesAcquired.erase(sourceImage);
        HRESULT result = keyedMutex->ReleaseSync(SwapchainCachedData::KEYED_MUTEX_OVERLAY);
        keyedMutex->Release();
        if(result != S_OK) {
            LogWindowsError(result, "xrWaitSwapchainImage", "ReleaseSync", __FILE__, __LINE__);
            return XR_ERROR_RUNTIME_FAILURE;
        }
    }

    return result;
}

XrResult OverlaysLayerWaitSwapchainImageOverlay(XrInstance instance, XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo)
{
    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(swapchain);

    if(swapchainInfo->overlaySwapchain->waited) {
        return XR_ERROR_CALL_ORDER_INVALID;
    }

    auto& overlaySwapchain = swapchainInfo->overlaySwapchain;

    uint32_t wasWaited = overlaySwapchain->acquired[0];
    HANDLE sourceImage = overlaySwapchain->swapchainHandles[wasWaited];

    auto waitInfoCopy = GetSharedCopyHandlesRestored(swapchainInfo->parentInstance, "xrWaitSwapchainImage", waitInfo);

    XrResult result = RPCCallWaitSwapchainImage(instance, swapchainInfo->actualHandle, waitInfoCopy.get(), sourceImage);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    overlaySwapchain->waited = true;
    IDXGIKeyedMutex* keyedMutex;
    HRESULT hresult;

    hresult = overlaySwapchain->swapchainTextures[wasWaited]->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex);
    if(hresult != S_OK) {
        LogWindowsError(result, "xrWaitSwapchainImage", "QueryInterface", __FILE__, __LINE__);
        return XR_ERROR_RUNTIME_FAILURE;
    }
    hresult = keyedMutex->AcquireSync(SwapchainCachedData::KEYED_MUTEX_OVERLAY, INFINITE); // XXX INFINITE timeout
    keyedMutex->Release();
    if(hresult != S_OK) {
        LogWindowsError(result, "xrWaitSwapchainImage", "AcquireSync", __FILE__, __LINE__);
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return result;
}

XrResult OverlaysLayerReleaseSwapchainImageMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo, HANDLE sourceImage)
{
    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(swapchain);

    auto& mainAsOverlaySwapchain = swapchainInfo->mainAsOverlaySwapchain;

    ID3D11Device* d3d11Device;
    {
        OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(swapchainInfo->parentHandle);
        d3d11Device = sessionInfo->d3d11Device;
    }

    ID3D11Texture2D *sharedTexture = mainAsOverlaySwapchain->getSharedTexture(d3d11Device, sourceImage);

    {
        IDXGIKeyedMutex* keyedMutex;
        HRESULT result;
        result = sharedTexture->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex);
        if(result != S_OK) {
            LogWindowsError(result, "xrReleaseSwapchainImage", "QueryInterface", __FILE__, __LINE__);
            return XR_ERROR_RUNTIME_FAILURE;
        }
        result = keyedMutex->AcquireSync(SwapchainCachedData::KEYED_MUTEX_MAIN, INFINITE); // XXX INFINITE timeout
        keyedMutex->Release();
        if(result != S_OK) {
            LogWindowsError(result, "xrReleaseSwapchainImage", "AcquireSync", __FILE__, __LINE__);
            return XR_ERROR_RUNTIME_FAILURE;
        }
    }

    mainAsOverlaySwapchain->remoteImagesAcquired.insert(sourceImage);
    int which = mainAsOverlaySwapchain->acquired[0];
    mainAsOverlaySwapchain->acquired.erase(mainAsOverlaySwapchain->acquired.begin());

    ID3D11Device* d3dDevice;
    sharedTexture->GetDevice(&d3dDevice);
    ID3D11DeviceContext* d3dContext;
    d3dDevice->GetImmediateContext(&d3dContext);
    d3dContext->CopyResource(mainAsOverlaySwapchain->swapchainImages[which], sharedTexture);

    auto releaseInfoCopy = GetSharedCopyHandlesRestored(swapchainInfo->parentInstance, "xrReleaseSwapchainImage", releaseInfo);

    XrResult result = swapchainInfo->downchain->ReleaseSwapchainImage(swapchainInfo->actualHandle, releaseInfoCopy.get());

    return result;
}

XrResult OverlaysLayerReleaseSwapchainImageOverlay(XrInstance instance, XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo)
{
    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(swapchain);

    if(!swapchainInfo->overlaySwapchain->waited) {
        return XR_ERROR_CALL_ORDER_INVALID;
    }

    auto& overlaySwapchain = swapchainInfo->overlaySwapchain;

    uint32_t beingReleased = overlaySwapchain->acquired[0];

    overlaySwapchain->acquired.erase(overlaySwapchain->acquired.begin());

    IDXGIKeyedMutex* keyedMutex;
    HRESULT hresult = overlaySwapchain->swapchainTextures[beingReleased]->QueryInterface( __uuidof(IDXGIKeyedMutex), (LPVOID*)&keyedMutex);
    if(hresult != S_OK) {
        LogWindowsError(hresult, "xrReleaseSwapchainImage", "QueryInterface", __FILE__, __LINE__);
        return XR_ERROR_RUNTIME_FAILURE;
    }
    hresult = keyedMutex->ReleaseSync(SwapchainCachedData::KEYED_MUTEX_MAIN);
    keyedMutex->Release();
    if(hresult != S_OK) {
        LogWindowsError(hresult, "xrReleaseSwapchainImage", "ReleaseSync", __FILE__, __LINE__);
        return XR_ERROR_RUNTIME_FAILURE;
    }

    HANDLE sourceImage = overlaySwapchain->swapchainHandles[beingReleased];

    auto releaseInfoCopy = GetSharedCopyHandlesRestored(instance, "xrReleaseSwapchainImage", releaseInfo);
    XrResult result = RPCCallReleaseSwapchainImage(instance, swapchainInfo->actualHandle, releaseInfoCopy.get(), sourceImage);

    if(!XR_SUCCEEDED(result)) {
        return result;
    }

    swapchainInfo->overlaySwapchain->waited = false;

    return result;
}

XrResult OverlaysLayerEndFrameMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrFrameEndInfo* frameEndInfo)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    XrResult result = XR_SUCCESS;

    // TODO: validate blend mode matches main session
    //
    {
        auto lock = connection->ctx->GetLock();
        connection->ctx->overlayLayers.clear();
    }

    if(frameEndInfo->layerCount > MainAsOverlaySessionContext::maxOverlayCompositionLayers) {

        result = XR_ERROR_LAYER_LIMIT_EXCEEDED;

    } else {

        for(uint32_t i = 0; (result == XR_SUCCESS) && (i < frameEndInfo->layerCount); i++) {

            std::shared_ptr<const XrCompositionLayerBaseHeader> copy(reinterpret_cast<const XrCompositionLayerBaseHeader*>(CopyXrStructChainWithMalloc(sessionInfo->parentInstance, frameEndInfo->layers[i])), [instance=sessionInfo->parentInstance](const XrCompositionLayerBaseHeader*p){ FreeXrStructChainWithFree(instance, p);});

            if(!copy) {

                auto lock = connection->ctx->GetLock();
                connection->ctx->overlayLayers.clear();
                result = XR_ERROR_OUT_OF_MEMORY;

            } else {

                auto lock = connection->ctx->GetLock();
                connection->ctx->overlayLayers.push_back(copy);

            }
        }
    }

    return result;
}

XrResult OverlaysLayerEndFrameOverlay(XrInstance instance, XrSession session, const XrFrameEndInfo* frameEndInfo)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto frameEndInfoCopy = GetSharedCopyHandlesRestored(instance, "xrEndFrame", frameEndInfo);

    XrResult result = RPCCallEndFrame(instance, sessionInfo->actualHandle, frameEndInfoCopy.get());

    return result;
}

void AddSwapchainInFlight(OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo)
{
    auto mainSession = gMainSessionContext;
    auto lock2 = mainSession->GetLock();
    mainSession->swapchainsInFlight.insert(swapchainInfo);
}

void AddSwapchainsInFlightFromLayers(OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo, std::shared_ptr<const XrCompositionLayerBaseHeader> p)
{
    switch(p->type) {
        case XR_TYPE_COMPOSITION_LAYER_QUAD: {
            auto p2 = reinterpret_cast<const XrCompositionLayerQuad*>(p.get());
            OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(p2->subImage.swapchain);
            AddSwapchainInFlight(swapchainInfo);
            break;
        }
        case XR_TYPE_COMPOSITION_LAYER_PROJECTION: {
            auto p2 = reinterpret_cast<const XrCompositionLayerProjection*>(p.get());
            for(uint32_t j = 0; j < p2->viewCount; j++) {
                OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(p2->views[j].subImage.swapchain);
                AddSwapchainInFlight(swapchainInfo);
            }
            break;
        }
        case XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR: {
            auto p2 = reinterpret_cast<const XrCompositionLayerDepthInfoKHR*>(p.get());
            OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(p2->subImage.swapchain);
            AddSwapchainInFlight(swapchainInfo);
            break;
        }
        default: {
            char structureTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
            XrResult r = sessionInfo->downchain->StructureTypeToString(sessionInfo->parentInstance, p->type, structureTypeName);
            if(r != XR_SUCCESS) {
                sprintf(structureTypeName, "(type %08X)", p->type);
            }

            OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "XrEndFrame",
                OverlaysLayerNoObjectInfo, fmt("a compositiion layer was provided of a type (%s) which the Overlay API Layer does not know how to check; will not be added to swapchains protected while submitted.  A crash may result.\n", structureTypeName).c_str());
            break;
        }
    }
}

XrResult OverlaysLayerEndFrameMain(XrInstance parentInstance, XrSession session, const XrFrameEndInfo* frameEndInfo)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    {
        auto mainSession = gMainSessionContext;
        auto lock2 = mainSession->GetLock();
        mainSession->swapchainsInFlight.clear();
    }

    // combine overlay and main layers

    std::vector<const XrCompositionLayerBaseHeader*> layersMerged;

    for(uint32_t i = 0; i < frameEndInfo->layerCount; i++) {
        layersMerged.push_back(frameEndInfo->layers[i]);
    }

    std::unique_lock<std::recursive_mutex> lock(gConnectionsToOverlayByProcessIdMutex);
    if(!gConnectionsToOverlayByProcessId.empty()) {
        for(auto& overlayconn: gConnectionsToOverlayByProcessId) {
            auto conn = overlayconn.second;
            auto lock = conn->GetLock();
            if(conn->ctx) {
                auto lock2 = conn->ctx->GetLock();
                for(uint32_t i = 0; i < conn->ctx->overlayLayers.size(); i++) {
                    auto sessLock = sessionInfo->GetLock();
                    AddSwapchainsInFlightFromLayers(sessionInfo, conn->ctx->overlayLayers[i]);
                    layersMerged.push_back(conn->ctx->overlayLayers[i].get());
                }
            }
        }
    }

    // Malloc this struct and the layer pointers array and then deep copy "next" and the layer pointers.
    // unique_ptr will free the whole deep copy later with custom deleter.
    std::shared_ptr<XrFrameEndInfo> frameEndInfoMerged(reinterpret_cast<XrFrameEndInfo*>(malloc(sizeof(XrFrameEndInfo))), [instance=sessionInfo->parentInstance](const XrFrameEndInfo* p){ FreeXrStructChainWithFree(instance, p);});

    frameEndInfoMerged->type = XR_TYPE_FRAME_END_INFO;
    frameEndInfoMerged->next = CopyXrStructChainWithMalloc(parentInstance, frameEndInfo->next);
    frameEndInfoMerged->displayTime = frameEndInfo->displayTime;
    frameEndInfoMerged->environmentBlendMode = frameEndInfo->environmentBlendMode;
    frameEndInfoMerged->layerCount = (uint32_t)layersMerged.size();

    if(frameEndInfoMerged->layerCount > 0) {

        auto layerPointers = reinterpret_cast<XrCompositionLayerBaseHeader**>(malloc(sizeof(XrCompositionLayerBaseHeader*) * layersMerged.size()));
        frameEndInfoMerged->layers = layerPointers;
        for(size_t i = 0; i < layersMerged.size(); i++) {
            layerPointers[i] = reinterpret_cast<XrCompositionLayerBaseHeader*>(CopyXrStructChainWithMalloc(parentInstance, layersMerged[i]));
        }

    } else {

        frameEndInfoMerged->layers = nullptr;

    }

    auto frameEndInfoMergedCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrEndFrame", frameEndInfoMerged.get());

    auto sessLock = sessionInfo->GetLock();
    XrResult result = sessionInfo->downchain->EndFrame(sessionInfo->actualHandle, frameEndInfoMergedCopy.get());

    return result;
}

XrResult OverlaysLayerEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo)
{
    try { 
        std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrSessionToHandleInfoMutex);
        auto it = gOverlaysLayerXrSessionToHandleInfo.find(session);
        if(it == gOverlaysLayerXrSessionToHandleInfo.end()) {
            OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrEndFrame",
                OverlaysLayerNoObjectInfo, "FATAL: invalid handle couldn't be found in tracking map.\n");
            return XR_ERROR_VALIDATION_FAILURE;
        }
        OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = it->second;
        
        bool isProxied = sessionInfo->isProxied;
        XrResult result;
        if(isProxied) {
            result = OverlaysLayerEndFrameOverlay(sessionInfo->parentInstance, session, frameEndInfo);
        } else {
            result = OverlaysLayerEndFrameMain(sessionInfo->parentInstance, session, frameEndInfo);
        }

        return result;
    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;
    }
}


// We don't know until Attach whether this is proxied to main or not, so have to
// make it now as if it will be in main
XrResult OverlaysLayerCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet)
{
    try {

        auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);

        XrResult result = instanceInfo->downchain->CreateActionSet(instance, createInfo, actionSet);

        if(result == XR_SUCCESS) {
            OverlaysLayerXrActionSetHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrActionSetHandleInfo>(instance, instance, instanceInfo->downchain);
            info->createInfo = reinterpret_cast<XrActionSetCreateInfo*>(CopyXrStructChainWithMalloc(instance, createInfo));
            info->actualHandle = *actionSet;
            *actionSet = (XrActionSet)GetNextLocalHandle();

            {
                std::unique_lock<std::recursive_mutex> lock(gActualXrActionSetToLocalHandleMutex);
                gActualXrActionSetToLocalHandle.insert({info->actualHandle, *actionSet});
            }

            {
                std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrActionSetToHandleInfoMutex);
                gOverlaysLayerXrActionSetToHandleInfo.insert({*actionSet, info});
            }
        }

        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

// We don't know until Attach whether this is proxied to main or not, so have to
// make it now as if it will be in main
XrResult OverlaysLayerCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action)
{
    try {

        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(actionSet);

        XrResult result = actionSetInfo->downchain->CreateAction(actionSetInfo->actualHandle, createInfo, action);

        if(result == XR_SUCCESS) {
            OverlaysLayerXrActionHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrActionHandleInfo>(actionSet, actionSetInfo->parentInstance, actionSetInfo->downchain);
            info->createInfo = reinterpret_cast<XrActionCreateInfo*>(CopyXrStructChainWithMalloc(actionSetInfo->parentInstance, createInfo));
            info->actualHandle = *action;
            info->subactionPaths.insert(info->createInfo->subactionPaths, info->createInfo->subactionPaths + info->createInfo->countSubactionPaths);
            for(auto subactionPath: info->subactionPaths) {
                ActionStateUnion state;
                ClearActionState(createInfo->actionType, &state);
                info->stateBySubactionPath.insert({subactionPath, state});
            }

            actionSetInfo->childActions.insert(info);
            *action = (XrAction)GetNextLocalHandle();
            info->localHandle = *action;

            {
                std::unique_lock<std::recursive_mutex> lock(gActualXrActionToLocalHandleMutex);
                gActualXrActionToLocalHandle.insert({info->actualHandle, *action});
            }

            {
                std::unique_lock<std::recursive_mutex> lock(gOverlaysLayerXrActionToHandleInfoMutex);
                gOverlaysLayerXrActionToHandleInfo.insert({*action, info});
            }
        }

        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}


XrResult OverlaysLayerSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings)
{
    try {

        auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);

        // XXX This does not take into account any extension structs like the one Valve suggested
        instanceInfo->profilesToBindings[suggestedBindings->interactionProfile] = 
            std::vector<XrActionSuggestedBinding>(suggestedBindings->suggestedBindings, suggestedBindings->suggestedBindings + suggestedBindings->countSuggestedBindings);

        for(auto it: instanceInfo->profilesToBindings[suggestedBindings->interactionProfile]) {
            auto found = OverlaysLayerPathToWellKnownString.find(it.binding);
            if(found == OverlaysLayerPathToWellKnownString.end()) {
                OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrAttachSessionActionSets",
                    OverlaysLayerNoObjectInfo,
                    fmt("Application suggested binding \"%s\", which this API layer does not know", PathToString(instance, it.binding)).c_str());
                DebugBreak();
            }
        }

        return XR_SUCCESS;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

XrResult OverlaysLayerCreateActionSpaceOverlay(XrInstance parentInstance, XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space)
{
    XrResult result = XR_SUCCESS;

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(createInfo->action);

    *space = (XrSpace)GetNextLocalHandle();

    OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = std::make_shared<OverlaysLayerXrSpaceHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);
    spaceInfo->spaceType = SPACE_ACTION;
    spaceInfo->actualHandle = XR_NULL_HANDLE; // There is no remote XrSpace created yet
    spaceInfo->action = actionInfo;
    spaceInfo->isProxied = true;

    std::shared_ptr<const XrActionSpaceCreateInfo> createInfoCopy(reinterpret_cast<const XrActionSpaceCreateInfo*>(CopyXrStructChainWithMalloc(sessionInfo->parentInstance, createInfo)), [instance=sessionInfo->parentInstance](const XrActionSpaceCreateInfo* p){ FreeXrStructChainWithFree(instance, p);});
    spaceInfo->actionSpaceCreateInfo = createInfoCopy;

    std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrSpaceToHandleInfoMutex);
    gOverlaysLayerXrSpaceToHandleInfo.insert({*space, spaceInfo});

    return XR_SUCCESS;
}

XrResult OverlaysLayerCreateActionSpaceMain(XrInstance parentInstance, XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space)
{
    XrResult result = XR_SUCCESS;

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    // restore the actual handle
    XrSession localHandleStore = session;
    session = sessionInfo->actualHandle;

    auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(createInfo->action);
    XrActionSpaceCreateInfo createInfo2 = *createInfo;
    createInfo2.action = actionInfo->actualHandle;

    result = sessionInfo->downchain->CreateActionSpace(session, &createInfo2, space);

    // put the local handle back
    session = localHandleStore;
    
    if(result == XR_SUCCESS) {

        XrSpace actualHandle = *space;
        XrSpace localHandle = (XrSpace)GetNextLocalHandle();
        *space = localHandle;

        {
            std::unique_lock<std::recursive_mutex> lock(gActualXrSpaceToLocalHandleMutex);
            gActualXrSpaceToLocalHandle.insert({actualHandle, localHandle});
        }

        OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = std::make_shared<OverlaysLayerXrSpaceHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);
        spaceInfo->spaceType = SPACE_ACTION;
        spaceInfo->actualHandle = actualHandle;
        spaceInfo->action = actionInfo;
        spaceInfo->isProxied = false;

        std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrSpaceToHandleInfoMutex);
        gOverlaysLayerXrSpaceToHandleInfo.insert({*space, spaceInfo});
    }

    return result;
}

XrResult OverlaysLayerCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space)
{
    try {

        auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
        
        bool isProxied = sessionInfo->isProxied;
        XrResult result;
        if(isProxied) {
            result = OverlaysLayerCreateActionSpaceOverlay(sessionInfo->parentInstance, session, createInfo, space);
        } else {
            result = OverlaysLayerCreateActionSpaceMain(sessionInfo->parentInstance, session, createInfo, space);
        }

        if(XR_SUCCEEDED(result)) {
            std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrSpaceToHandleInfoMutex);
            sessionInfo->childSpaces.insert(gOverlaysLayerXrSpaceToHandleInfo.at(*space));
        }

        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

XrResult OverlaysLayerCreateActionSpaceFromBinding(ConnectionToOverlay::Ptr connection, XrSession session, WellKnownStringIndex bindingString, const XrPosef* poseInActionSpace, XrSpace *space)
{
    XrPath path = OverlaysLayerWellKnownStringToPath.at(bindingString);
    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session); 
    XrAction actualActionHandle = sessionInfo->placeholderActions.at(path).first;

    XrActionSpaceCreateInfo createInfo { XR_TYPE_ACTION_SPACE_CREATE_INFO };
    createInfo.action = actualActionHandle, 
    createInfo.subactionPath = OverlaysLayerBindingToSubaction.at(path); 
    createInfo.poseInActionSpace = *poseInActionSpace; 
    XrResult result = sessionInfo->downchain->CreateActionSpace(sessionInfo->actualHandle, &createInfo, space);

    if(result == XR_SUCCESS) {

        XrSpace actualHandle = *space;
        XrSpace localHandle = (XrSpace)GetNextLocalHandle();
        *space = localHandle;

        {
            std::unique_lock<std::recursive_mutex> lock(gActualXrSpaceToLocalHandleMutex);
            gActualXrSpaceToLocalHandle.insert({actualHandle, localHandle});
        }

        OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = std::make_shared<OverlaysLayerXrSpaceHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);
        spaceInfo->spaceType = SPACE_ACTION;
        spaceInfo->actualHandle = actualHandle;
        // spaceInfo->action.reset(); // Should never be accessed from MainAsOverlay
        // spaceInfo->isProxied; // Should never be accessed from MainAsOverlay

        std::unique_lock<std::recursive_mutex> mlock2(gOverlaysLayerXrSpaceToHandleInfoMutex);
        gOverlaysLayerXrSpaceToHandleInfo.insert({*space, spaceInfo});
    }

    return result;
}

XrResult OverlaysLayerAttachSessionActionSetsOverlay(XrInstance parentInstance, XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
{
    XrResult result = XR_SUCCESS;

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session); 
    if(sessionInfo->actionSetsWereAttached) {
        return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
    }

    for(uint32_t i = 0; i < attachInfo->countActionSets; i++) {
        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(attachInfo->actionSets[i]);
        actionSetInfo->bindLocation = BOUND_OVERLAY;
        for(auto actionInfo: actionSetInfo->childActions) {
            actionInfo->bindLocation = BOUND_OVERLAY;
        }
    }

    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(parentInstance);
    for(auto profileAndBindings : instanceInfo->profilesToBindings) {
        XrPath interactionProfile = profileAndBindings.first;
        auto bindings = profileAndBindings.second;

        for(auto binding: bindings) {
            auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(binding.action);
            actionInfo->suggestedBindings.insert(binding.binding);
        }
    }

    for(auto space: sessionInfo->childSpaces) {

        // Determine placeholder action to use as pose
        
        if(space->spaceType == SPACE_ACTION) {

            auto actionInfo = space->action;
			
            // XXX what if SuggestProfileBindings was never called?  It's not an error.
            XrPath matchingBinding = XR_NULL_PATH;
            bool found = false;
            for(auto binding: actionInfo->suggestedBindings) {
                XrPath subactionPath = OverlaysLayerBindingToSubaction.at(binding);
                if(subactionPath == space->actionSpaceCreateInfo->subactionPath) {
                    matchingBinding = binding;
                    found = true;
                }
            }

            if(!found) {

                OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrAttachSessionActionSets",
                    OverlaysLayerNoObjectInfo, "Couldn't find a suggested binding matching the subaction with which an XrSpace was created");

            } else {

                WellKnownStringIndex bindingString = OverlaysLayerPathToWellKnownString.at(matchingBinding);

                result = RPCCallCreateActionSpaceFromBinding(parentInstance, sessionInfo->actualHandle, bindingString, &space->actionSpaceCreateInfo->poseInActionSpace, &space->actualHandle);
                if(result != XR_SUCCESS) {
                    OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrAttachSessionActionSets",
                        OverlaysLayerNoObjectInfo, "Couldn't create XrSpace for Action from main placeholders");
                }
            }
        }
    }

    sessionInfo->actionSetsWereAttached = true;
    return XR_SUCCESS;
}

XrResult OverlaysLayerAttachSessionActionSetsMain(XrInstance parentInstance, XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
{
    XrResult result = XR_SUCCESS;

    // Submit main app's suggestions and our own placeholder suggestions

    // XXX check and return ALREADY_ATTACHED, don't call Suggest

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session); 
    if(sessionInfo->actionSetsWereAttached) {
        return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
    }

    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(parentInstance);
    for(auto profileAndBindings : instanceInfo->profilesToBindings) {
        XrPath interactionProfile = profileAndBindings.first;
        auto bindings = profileAndBindings.second;

        for(auto binding: bindings) {
            auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(binding.action);
            actionInfo->suggestedBindings.insert(binding.binding);
        }

        XrInteractionProfileSuggestedBinding suggestedBindings { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
        suggestedBindings.interactionProfile = interactionProfile;
        suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
        suggestedBindings.suggestedBindings = bindings.data();

        auto suggestedBindingsCopy = GetSharedCopyHandlesRestored(parentInstance, "xrAttachSessionActionSets", &suggestedBindings);

        // merge main app's suggested bindings and our suggested bindings - our Actions are actual runtime Handles, so add after Restoring
        std::vector<XrActionSuggestedBinding> newBindings(suggestedBindingsCopy->suggestedBindings, suggestedBindingsCopy->suggestedBindings + suggestedBindingsCopy->countSuggestedBindings);

        {
            for (const auto& actionBinding : sessionInfo->bindingsByProfile.at(interactionProfile)) {
                newBindings.push_back(actionBinding);
                if(PrintDebugInfo) {
                    XrPath binding = actionBinding.binding;
                    XrPath recorded = sessionInfo->bindingsByAction[actionBinding.action];
                    OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
                        OverlaysLayerNoObjectInfo,
                        fmt("Suggested \"%s\" for placeholder action \"%s\"", PathToString(sessionInfo->parentInstance, binding).c_str(), PathToString(sessionInfo->parentInstance, recorded).c_str()).c_str());
                }
            }
        }
        
        suggestedBindingsCopy->countSuggestedBindings = (uint32_t)newBindings.size();
        auto suggestedBindingsSave = suggestedBindingsCopy->suggestedBindings;
        suggestedBindingsCopy->suggestedBindings = newBindings.data();

        result = instanceInfo->downchain->SuggestInteractionProfileBindings(parentInstance, suggestedBindingsCopy.get());

        if(result == XR_ERROR_PATH_UNSUPPORTED) {
            std::vector<XrActionSuggestedBinding> newBindings2;
            suggestedBindingsCopy->countSuggestedBindings = 1;
            for(int i = 0; i < newBindings.size(); i++) {
                suggestedBindingsCopy->suggestedBindings = &newBindings[i];
                result = instanceInfo->downchain->SuggestInteractionProfileBindings(parentInstance, suggestedBindingsCopy.get());
                if(result != XR_SUCCESS) {
                    OverlaysLayerLogMessage(parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrAttachSessionActionSets",
                        OverlaysLayerNoObjectInfo,
                        fmt("Applying deferred SuggestedInteractionProfileBindings, discovered %s is unsupported for %s, omitting...", PathToString(parentInstance, newBindings[i].binding).c_str(), PathToString(parentInstance, interactionProfile).c_str()).c_str());
                } else {
                    newBindings2.push_back(newBindings[i]);
                }
            }

            suggestedBindingsCopy->countSuggestedBindings = (uint32_t)newBindings2.size();
            suggestedBindingsCopy->suggestedBindings = newBindings2.data();

            result = instanceInfo->downchain->SuggestInteractionProfileBindings(parentInstance, suggestedBindingsCopy.get());
        }

        suggestedBindingsCopy->suggestedBindings = suggestedBindingsSave;

        if(result != XR_SUCCESS) {
            OverlaysLayerLogMessage(parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrAttachSessionActionSets",
                OverlaysLayerNoObjectInfo,
                fmt("Couldn't apply deferred suggested interaction profile bindings, SuggestInteractionProfileBindings returned %08llX", result).c_str());
            return XR_ERROR_HANDLE_INVALID;
        }
    }

	
    auto attachInfoCopy = GetSharedCopyHandlesRestored(parentInstance, "xrAttachSessionActionSets", attachInfo);

    // Now we add our placeholder ActionSet because it is already the runtime's ("restored") handle
    auto actionSetsSave = attachInfoCopy->actionSets;

    std::vector<XrActionSet> actionSetsPlusPlaceholder(actionSetsSave, actionSetsSave + attachInfoCopy->countActionSets);
    attachInfoCopy->countActionSets = attachInfoCopy->countActionSets + 1;
    actionSetsPlusPlaceholder.push_back(sessionInfo->placeholderActionSet);
    attachInfoCopy->actionSets = actionSetsPlusPlaceholder.data();

    result = instanceInfo->downchain->AttachSessionActionSets(sessionInfo->actualHandle, attachInfoCopy.get());

    // Put pointer back so it will be free'd correctly when it goes out of scope
    attachInfoCopy->countActionSets = attachInfoCopy->countActionSets - 1;
    attachInfoCopy->actionSets = actionSetsSave;

    if(result == XR_SUCCESS) {
        for(uint32_t i = 0; i < attachInfo->countActionSets; i++) {
            auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(attachInfo->actionSets[i]);
            actionSetInfo->bindLocation = BOUND_MAIN;
            for(auto actionInfo: actionSetInfo->childActions) {
                actionInfo->bindLocation = BOUND_MAIN;
            }
        }

        sessionInfo->actionSetsWereAttached = true;
    }

    return result;
}

XrResult OverlaysLayerAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
{
    try {

        auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

        bool isProxied = sessionInfo->isProxied;
        XrResult result;
        if(isProxied) {
            result = OverlaysLayerAttachSessionActionSetsOverlay(sessionInfo->parentInstance, session, attachInfo);
        } else {
            result = OverlaysLayerAttachSessionActionSetsMain(sessionInfo->parentInstance, session, attachInfo);
        }

        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

struct ActionGetInfo
{
    XrAction action;
    XrActionType actionType;
    XrPath subactionPath;
};

typedef std::vector<ActionGetInfo> ActionGetInfoList;

// syncInfo will be RestoreHandles()d but actionsToGet will not
XrResult SyncActionsAndGetState(XrSession session, const XrActionsSyncInfo* syncInfo, const ActionGetInfoList& actionsToGet, ActionStateUnion *states)
{
    static std::recursive_mutex mutex;
    auto syncAndGetLock = std::unique_lock<std::recursive_mutex>(mutex);

    XrResult result;

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    result = sessionInfo->downchain->SyncActions(sessionInfo->actualHandle, syncInfo);
    if(result == XR_SESSION_NOT_FOCUSED) {
        return XR_SESSION_NOT_FOCUSED;
    }

    if(result != XR_SUCCESS) {
        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, "Couldn't sync actions in bulk update");
        return result;
    }

    uint32_t index = 0;
    for(const auto& whatToGet: actionsToGet) {
        XrActionStateGetInfo get { XR_TYPE_ACTION_STATE_GET_INFO, nullptr, whatToGet.action, whatToGet.subactionPath};

        switch(whatToGet.actionType) {
            case XR_ACTION_TYPE_BOOLEAN_INPUT: {
                auto *state = reinterpret_cast<XrActionStateBoolean*>(&states[index]);
                state->type = XR_TYPE_ACTION_STATE_BOOLEAN;
                state->next = nullptr;
                result = sessionInfo->downchain->GetActionStateBoolean(sessionInfo->actualHandle, &get, state);
                if(result != XR_SUCCESS) {
                    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, "Couldn't get state in bulk update");
                    return result;
                }
                if(PrintDebugInfo) {
                    std::unique_lock<std::recursive_mutex> lock(gActualXrSessionToLocalHandleMutex);
                    auto it = gActualXrActionToLocalHandle.find(whatToGet.action);
                    if(it != gActualXrActionToLocalHandle.end()) {
                        // application XrAction
                        auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(it->second);
                        if(false) OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
                            OverlaysLayerNoObjectInfo,
                            fmt("Got Boolean from \"%s\" for application action \"%s\": %s", PathToString(sessionInfo->parentInstance, whatToGet.subactionPath).c_str(), actionInfo->createInfo->actionName, state->currentState ? "on" : " off").c_str());
                    } else {
                        // placeholder XrAction
                        XrPath binding = sessionInfo->bindingsByAction[whatToGet.action];
                        OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
                            OverlaysLayerNoObjectInfo,
                            fmt("Got Boolean from \"%s\" for placeholder action \"%s\": %s", PathToString(sessionInfo->parentInstance, whatToGet.subactionPath).c_str(), PathToString(sessionInfo->parentInstance, binding).c_str(), state->currentState ? "on" : " off").c_str());
                        XrBoundSourcesForActionEnumerateInfo enumerateInfo = {XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO, nullptr, whatToGet.action};
                        uint32_t capacity;
						result = sessionInfo->downchain->EnumerateBoundSourcesForAction(sessionInfo->actualHandle, &enumerateInfo, 0, &capacity, nullptr);
						if(result != XR_SUCCESS) DebugBreak();
						
                        std::vector<XrPath> sources(capacity);
						sessionInfo->downchain->EnumerateBoundSourcesForAction(sessionInfo->actualHandle, &enumerateInfo, capacity, &capacity, sources.data());
                        if(capacity == 0) {
                            OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
                                OverlaysLayerNoObjectInfo,
                                fmt("No bound sources for that action...").c_str());
                        } else {
                            for(uint32_t i = 0; i< capacity; i++) {
                                OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
                                    OverlaysLayerNoObjectInfo,
                                    fmt("That action bound to \"%s\"", PathToString(sessionInfo->parentInstance, sources[i]).c_str()).c_str());
                            }
                        }
                    }
                }
                break;
            }
            case XR_ACTION_TYPE_FLOAT_INPUT: {
                auto *state = reinterpret_cast<XrActionStateFloat*>(&states[index]);
                state->type = XR_TYPE_ACTION_STATE_FLOAT;
                state->next = nullptr;
                result = sessionInfo->downchain->GetActionStateFloat(sessionInfo->actualHandle, &get, state);
                if(result != XR_SUCCESS) {
                    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, "Couldn't get state in bulk update");
                    return result;
                }
                if(PrintDebugInfo) {
                    std::unique_lock<std::recursive_mutex> lock(gActualXrSessionToLocalHandleMutex);
                    auto it = gActualXrActionToLocalHandle.find(whatToGet.action);
                    if(it != gActualXrActionToLocalHandle.end()) {
                        // application XrAction
                        auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(it->second);
                        if(false) OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
                            OverlaysLayerNoObjectInfo,
                            fmt("Got Float from \"%s\" for application action \"%s\": %d", PathToString(sessionInfo->parentInstance, whatToGet.subactionPath).c_str(), actionInfo->createInfo->actionName, state->currentState).c_str());
                    } else {
                        // placeholder XrAction
                        XrPath binding = sessionInfo->bindingsByAction[whatToGet.action];
                        OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
                            OverlaysLayerNoObjectInfo,
                            fmt("Got Float from \"%s\" for placeholder action \"%s\": %f", PathToString(sessionInfo->parentInstance, whatToGet.subactionPath).c_str(), PathToString(sessionInfo->parentInstance, binding).c_str(), state->currentState).c_str());
                    }
                }
                break;
            }
            case XR_ACTION_TYPE_VECTOR2F_INPUT: {
                auto *state = reinterpret_cast<XrActionStateVector2f*>(&states[index]);
                state->type = XR_TYPE_ACTION_STATE_VECTOR2F;
                state->next = nullptr;
                result = sessionInfo->downchain->GetActionStateVector2f(sessionInfo->actualHandle, &get, state);
                if(result != XR_SUCCESS) {
                    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, "Couldn't get state in bulk update");
                    return result;
                }
                break;
            }
            case XR_ACTION_TYPE_POSE_INPUT: {
                auto *state = reinterpret_cast<XrActionStatePose*>(&states[index]);
                state->type = XR_TYPE_ACTION_STATE_POSE;
                state->next = nullptr;
                result = sessionInfo->downchain->GetActionStatePose(sessionInfo->actualHandle, &get, state);
                if(result != XR_SUCCESS) {
                    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, "Couldn't get state in bulk update");
                    return result;
                }
                if(PrintDebugInfo) {
                    std::unique_lock<std::recursive_mutex> lock(gActualXrSessionToLocalHandleMutex);
                    auto it = gActualXrActionToLocalHandle.find(whatToGet.action);
                    if(it != gActualXrActionToLocalHandle.end()) {
                        // application XrAction
                        auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(it->second);
                        if(false) OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
                            OverlaysLayerNoObjectInfo,
                            fmt("Got Pose from \"%s\" for application action \"%s\": %s", PathToString(sessionInfo->parentInstance, whatToGet.subactionPath).c_str(), actionInfo->createInfo->actionName, state->isActive ? "active" : "inactive").c_str());
                    } else {
                        // placeholder XrAction
                        XrPath binding = sessionInfo->bindingsByAction[whatToGet.action];
                        OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
                            OverlaysLayerNoObjectInfo,
                            fmt("Got Pose from \"%s\" for placeholder action \"%s\": %s", PathToString(sessionInfo->parentInstance, whatToGet.subactionPath).c_str(), PathToString(sessionInfo->parentInstance, binding).c_str(), state->isActive ? "active" : "inactive").c_str());
                        XrBoundSourcesForActionEnumerateInfo enumerateInfo = {XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO, nullptr, whatToGet.action};
                        uint32_t capacity;
						result = sessionInfo->downchain->EnumerateBoundSourcesForAction(sessionInfo->actualHandle, &enumerateInfo, 0, &capacity, nullptr);
						if(result != XR_SUCCESS) DebugBreak();
						
                        std::vector<XrPath> sources(capacity);
						sessionInfo->downchain->EnumerateBoundSourcesForAction(sessionInfo->actualHandle, &enumerateInfo, capacity, &capacity, sources.data());
                        if(capacity == 0) {
                            OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
                                OverlaysLayerNoObjectInfo,
                                fmt("No bound sources for that action...").c_str());
                        } else {
                            for(uint32_t i = 0; i< capacity; i++) {
                                OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
                                    OverlaysLayerNoObjectInfo,
                                    fmt("That action bound to \"%s\"", PathToString(sessionInfo->parentInstance, sources[i]).c_str()).c_str());
                            }
                        }
                    }
                }
                break;
            }
        }
        index ++;
    }
    return result;
}

XrResult OverlaysLayerSyncActionsAndGetStateMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, uint32_t countBindings, const WellKnownStringIndex *bindingStrings, ActionStateUnion *states)
{
    XrResult result = XR_SUCCESS;

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    XrActiveActionSet activeActionSet { sessionInfo->placeholderActionSet, XR_NULL_PATH };
    XrActionsSyncInfo syncInfo { XR_TYPE_ACTIONS_SYNC_INFO, nullptr, 1, &activeActionSet };
    ActionGetInfoList actionsToGet;

    for(uint32_t i = 0; i < countBindings; i++) {
        XrPath bindingPath = OverlaysLayerWellKnownStringToPath.at(bindingStrings[i]);
        auto it = sessionInfo->placeholderActions.find(bindingPath);
        if(it == sessionInfo->placeholderActions.end()) {
            DebugBreak();
        }
			
        XrAction action = it->second.first;
        XrActionType type = it->second.second;
        actionsToGet.push_back({ action, type, XR_NULL_PATH });
    }
    if(PrintDebugInfo) {
        OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
            OverlaysLayerNoObjectInfo,
            fmt("I think I'm getting %d placeholder Action states", actionsToGet.size()).c_str());
    }

    result = SyncActionsAndGetState(session, &syncInfo, actionsToGet, states);

    return result;
}

void ClearSessionLastSyncedActiveActionSets(OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo, const XrActionsSyncInfo* syncInfo)
{
    for(auto activeActionSet: sessionInfo->lastSyncedActiveActionSets) {
        XrActionSet actionSet = activeActionSet.actionSet;
        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(actionSet);
        XrPath subactionPath = activeActionSet.subactionPath;
        for(auto actionInfo: actionSetInfo->childActions) {
            if(subactionPath == XR_NULL_PATH) {
                for(XrPath subactionPath: actionInfo->subactionPaths) {
                    ClearActionState(actionInfo->createInfo->actionType, &actionInfo->stateBySubactionPath.at(subactionPath));
                }
            } else {
                if(actionInfo->stateBySubactionPath.count(subactionPath) > 0) {
                    ClearActionState(actionInfo->createInfo->actionType, &actionInfo->stateBySubactionPath.at(subactionPath));
                }
            }
        }
    }
}

void ClearSyncedActiveActionSets(XrInstance parentInstance, XrSession session, const XrActionsSyncInfo* syncInfo)
{
    for(uint32_t i = 0; i < syncInfo->countActiveActionSets; i++) {
        XrActionSet actionSet = syncInfo->activeActionSets[i].actionSet;
        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(actionSet);
        XrPath subactionPath = syncInfo->activeActionSets[i].subactionPath;
        for(auto actionInfo: actionSetInfo->childActions) {
            if(subactionPath == XR_NULL_PATH) {
                for(XrPath subactionPath: actionInfo->subactionPaths) {
                    ClearActionState(actionInfo->createInfo->actionType, &actionInfo->stateBySubactionPath.at(subactionPath));
                }
            } else {
                if(actionInfo->stateBySubactionPath.count(subactionPath) > 0) {
                    ClearActionState(actionInfo->createInfo->actionType, &actionInfo->stateBySubactionPath.at(subactionPath));
                }
            }
        }
    }
}

void GetPreviousActionStates(XrInstance parentInstance, XrSession session, const XrActionsSyncInfo* syncInfo, std::unordered_map <XrAction, std::unordered_map<XrPath, ActionStateUnion>> &previousActionStates)
{
    for(uint32_t i = 0; i < syncInfo->countActiveActionSets; i++) {
        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(syncInfo->activeActionSets[i].actionSet);
        for(auto actionInfo: actionSetInfo->childActions) {
            previousActionStates.insert({actionInfo->localHandle, actionInfo->stateBySubactionPath});
        }
    }
}

void UpdateSyncActionStateLastChange(XrInstance parentInstance, XrSession session, const XrActionsSyncInfo* syncInfo, const std::unordered_map <XrAction, std::unordered_map<XrPath, ActionStateUnion>> &previousActionStates)
{
    for(uint32_t i = 0; i < syncInfo->countActiveActionSets; i++) {
        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(syncInfo->activeActionSets[i].actionSet);
        XrPath subactionPath = syncInfo->activeActionSets[i].subactionPath;
        for(auto actionInfo: actionSetInfo->childActions) {
            if(subactionPath == XR_NULL_PATH) {
                for(XrPath subactionPath: actionInfo->subactionPaths) {
                    UpdateActionStateLastChange(actionInfo->createInfo->actionType, &previousActionStates.at(actionInfo->localHandle).at(subactionPath), &actionInfo->stateBySubactionPath.at(subactionPath));
                }
            } else {
                if(actionInfo->stateBySubactionPath.count(subactionPath) > 0) {
                    UpdateActionStateLastChange(actionInfo->createInfo->actionType, &previousActionStates.at(actionInfo->localHandle).at(subactionPath), &actionInfo->stateBySubactionPath.at(subactionPath));
                }
            }
        }
    }
}

XrResult OverlaysLayerSyncActionsOverlay(XrInstance parentInstance, XrSession session, const XrActionsSyncInfo* syncInfo)
{
    XrResult result = XR_SUCCESS;

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    //DebugBreak();

    std::vector<WellKnownStringIndex> bindingStrings;

    for(uint32_t i = 0; i < syncInfo->countActiveActionSets; i++) {
        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(syncInfo->activeActionSets[i].actionSet);
        for(auto actionInfo: actionSetInfo->childActions) {
            for(auto path: actionInfo->suggestedBindings) {
                auto found = OverlaysLayerPathToWellKnownString.find(path);
                if(found != OverlaysLayerPathToWellKnownString.end()) {
                    WellKnownStringIndex index = found->second;
                    bindingStrings.push_back(index);
                    if(PrintDebugInfo) {
                        OverlaysLayerLogMessage(parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
                            OverlaysLayerNoObjectInfo,
                            fmt("I think I'm probing placeholder \"%s\" for an action", OverlaysLayerWellKnownStrings.at(index)).c_str());
                    }
                }
            }
        }
    }

    std::vector<ActionStateUnion> states(bindingStrings.size());
    RPCCallSyncActionsAndGetState(parentInstance, session, (uint32_t)bindingStrings.size(), bindingStrings.data(), states.data());

    if(result == XR_SUCCESS) {

        // Save off previous action's states
        std::unordered_map <XrAction, std::unordered_map<XrPath, ActionStateUnion>> previousActionStates;
        GetPreviousActionStates(sessionInfo->parentInstance, session, syncInfo, previousActionStates);

        // On all actions in previous ActionSet and in this ActionSet, clear state
        ClearSessionLastSyncedActiveActionSets(sessionInfo, syncInfo);
        ClearSyncedActiveActionSets(sessionInfo->parentInstance, session, syncInfo);

        // Merge all fetched state
        uint32_t index = 0;
        for(uint32_t i = 0; i < syncInfo->countActiveActionSets; i++) {
            auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(syncInfo->activeActionSets[i].actionSet);
            for(auto actionInfo: actionSetInfo->childActions) {
                for(auto path: actionInfo->suggestedBindings) {
                    XrPath subactionPath = OverlaysLayerBindingToSubaction.at(path);
                    MergeActionState(actionInfo->createInfo->actionType, &states[index], &actionInfo->stateBySubactionPath[subactionPath]);
                    index ++;
                }
            }
        }

        // On all actions in current state and all subactionPaths, set lastSyncTime and changedSinceLastSync 
        UpdateSyncActionStateLastChange(sessionInfo->parentInstance, session, syncInfo, previousActionStates);
    }

    return result;
}

XrResult OverlaysLayerSyncActionsMain(XrInstance parentInstance, XrSession session, const XrActionsSyncInfo* syncInfo)
{
    XrResult result = XR_SUCCESS;

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    
    std::unordered_map<XrActionSet, std::set<XrPath>> actionSetSubactionPaths;

    for(uint32_t i = 0; i < syncInfo->countActiveActionSets; i++) {
        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(syncInfo->activeActionSets[i].actionSet);
        actionSetSubactionPaths[syncInfo->activeActionSets[i].actionSet].insert(syncInfo->activeActionSets[i].subactionPath);
    }

    ActionGetInfoList actionsToGet;

    for(auto& as: actionSetSubactionPaths) {
        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(as.first);
        auto subactionPaths = as.second;

        /* subactionPaths is either XR_NULL_PATH (get them all) or a set of valid subactionPaths */

        for(auto& actionInfo: actionSetInfo->childActions) {

            if(subactionPaths.size() == 1 && *(subactionPaths.begin()) == XR_NULL_PATH) {

                if(actionInfo->createInfo->countSubactionPaths == 0) { // action created with no subaction paths

                    // GetActionState can only be called with XR_NULL_PATH
                    // Get action state with XR_NULL_PATH
                    actionsToGet.push_back({ actionInfo->actualHandle, actionInfo->createInfo->actionType, XR_NULL_PATH });

                } else { // action created with subaction paths

                    // GetActionState be called with any created path or with XR_NULL_PATH (combine all paths)
                    // Get action state for all created paths
                    for(const auto& subactionPath : actionInfo->subactionPaths) {
                        actionsToGet.push_back({ actionInfo->actualHandle, actionInfo->createInfo->actionType, subactionPath });
                    }

                }

            } else {

                if(actionInfo->createInfo->countSubactionPaths == 0) { // action created with no subaction paths

                    // GetActionState can only be called with XR_NULL_PATH
                    // do nothing -> Get would return XR_ERROR_PATH_UNSUPPORTED, so this is not valid

                } else { // action created with subaction paths

                    // GetActionState be called with any created path or with XR_NULL_PATH (combine all paths)
                    // Get action state for all passed subactionPaths that are in actionInfo->subactionPaths
                    for(const auto& subactionPath : subactionPaths) {
                        if(actionInfo->subactionPaths.count(subactionPath) > 0) {
                            actionsToGet.push_back({actionInfo->actualHandle, actionInfo->createInfo->actionType, subactionPath});
                        }
                    }
                }
            }
        }

    }

    std::vector<ActionStateUnion> states(actionsToGet.size());

    auto syncInfoSave = syncInfo;
    auto syncInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrSyncActions", syncInfo);
    syncInfo = syncInfoCopy.get();

    result = SyncActionsAndGetState(session, syncInfo, actionsToGet, states.data());

    syncInfo = syncInfoSave;

    if(result == XR_SUCCESS) {

        // Save off previous action's states
        std::unordered_map <XrAction, std::unordered_map<XrPath, ActionStateUnion>> previousActionStates;
        GetPreviousActionStates(sessionInfo->parentInstance, session, syncInfo, previousActionStates);

        // On all actions in previous ActionSet and in this ActionSet, clear state
        ClearSessionLastSyncedActiveActionSets(sessionInfo, syncInfo);
        ClearSyncedActiveActionSets(sessionInfo->parentInstance, session, syncInfo);

        uint32_t index = 0;
        for(const auto& actionGetInfo: actionsToGet) {
            XrAction localHandle; 
            {
                std::unique_lock<std::recursive_mutex> lock(gActualXrSessionToLocalHandleMutex);
                localHandle = gActualXrActionToLocalHandle[actionGetInfo.action];
            }
            auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(localHandle);
            MergeActionState(actionInfo->createInfo->actionType, &states[index], &actionInfo->stateBySubactionPath[actionGetInfo.subactionPath]);
            index ++;
        }

        // On all actions in current state and all subactionPaths, set lastSyncTime and changedSinceLastSync 
        UpdateSyncActionStateLastChange(sessionInfo->parentInstance, session, syncInfo, previousActionStates);
    }

    return result;
}

XrResult OverlaysLayerSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo)
{
    try {

        auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
        
        bool isProxied = sessionInfo->isProxied;
        XrResult result;
        if(isProxied) {
            result = OverlaysLayerSyncActionsOverlay(sessionInfo->parentInstance, session, syncInfo);
        } else {
            result = OverlaysLayerSyncActionsMain(sessionInfo->parentInstance, session, syncInfo);
        }
        sessionInfo->lastSyncedActiveActionSets.assign(syncInfo->activeActionSets, syncInfo->activeActionSets + syncInfo->countActiveActionSets);

        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

XrResult OverlaysLayerGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state)
{
    try {
        auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(getInfo->action);

        if(actionInfo->createInfo->actionType != XR_ACTION_TYPE_BOOLEAN_INPUT) {
            return XR_ERROR_ACTION_TYPE_MISMATCH; 
        }

        XrResult result = actionInfo->getBoolean(getInfo->subactionPath, state); 
        
        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

XrResult OverlaysLayerGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state)
{
    try {
        auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(getInfo->action);

        if(actionInfo->createInfo->actionType != XR_ACTION_TYPE_FLOAT_INPUT) {
            return XR_ERROR_ACTION_TYPE_MISMATCH; 
        }

        XrResult result = actionInfo->getFloat(getInfo->subactionPath, state); 
        
        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

XrResult OverlaysLayerGetActionStateVector2f(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateVector2f* state)
{
    try {
        auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(getInfo->action);

        if(actionInfo->createInfo->actionType != XR_ACTION_TYPE_VECTOR2F_INPUT) {
            return XR_ERROR_ACTION_TYPE_MISMATCH; 
        }

        XrResult result = actionInfo->getVector2f(getInfo->subactionPath, state); 
        
        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

XrResult OverlaysLayerGetActionStatePose(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* state)
{
    try {
        auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(getInfo->action);

        if(actionInfo->createInfo->actionType != XR_ACTION_TYPE_POSE_INPUT) {
            return XR_ERROR_ACTION_TYPE_MISMATCH; 
        }

        XrResult result = actionInfo->getPose(getInfo->subactionPath, state); 
        
        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}


// CreateActionSpace
// SyncActions
// AttachSessionActionSets
// GetActionState*

#if 0 // Do on AttachSessionActionSets
    XrSessionActionSetsAttachInfo attachInfo { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &info->placeholderActionSet;
    result2 = instanceInfo->downchain->AttachSessionActionSets(actualHandle, &attachInfo);
    if(result2 != XR_SUCCESS) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("FATAL: Could not attach placeholder ActionSet to XrSession.\n").c_str());
        return XR_ERROR_INITIALIZATION_FAILED;
    }
#endif

extern "C" {

// Function used to negotiate an interface betewen the loader and an API layer.  Each library exposing one or
// more API layers needs to expose at least this function.
XrResult LAYER_EXPORT XRAPI_CALL Overlays_xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo *loaderInfo,
                                                                    const char* apiLayerName,
                                                                    XrNegotiateApiLayerRequest *apiLayerRequest)
{
    if (apiLayerName)
    {
        if (0 != strncmp(kOverlayLayerName, apiLayerName, strnlen_s(kOverlayLayerName, XR_MAX_API_LAYER_NAME_SIZE)))
        {
            return XR_ERROR_INITIALIZATION_FAILED;
        }
    }

    if (!loaderInfo ||
        !apiLayerRequest ||
        loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION ||
        loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo) ||
        apiLayerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        apiLayerRequest->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION ||
        apiLayerRequest->structSize != sizeof(XrNegotiateApiLayerRequest) ||
        loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxApiVersion < XR_CURRENT_API_VERSION ||
        loaderInfo->minApiVersion > XR_CURRENT_API_VERSION) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    apiLayerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
    apiLayerRequest->layerApiVersion = XR_CURRENT_API_VERSION;
    apiLayerRequest->getInstanceProcAddr = reinterpret_cast<PFN_xrGetInstanceProcAddr>(OverlaysLayerXrGetInstanceProcAddr);
    apiLayerRequest->createApiLayerInstance = reinterpret_cast<PFN_xrCreateApiLayerInstance>(OverlaysLayerXrCreateApiLayerInstance);

    return XR_SUCCESS;
}

}  // extern "C"
