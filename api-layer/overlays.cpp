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
#include <map>
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

constexpr bool PrintDebugInfo = false;



std::unordered_map<WellKnownStringIndex, const char *> OverlaysLayerWellKnownStrings = {
    {USER_HAND_LEFT_INPUT_THUMBSTICK_CLICK, "/user/hand/left/input/thumbstick/click"},
    {INPUT_DPAD_DOWN_CLICK, "/input/dpad_down/click"},
    {USER_HAND_RIGHT_INPUT_SELECT_CLICK, "/user/hand/right/input/select/click"},
    {INPUT_THUMBSTICK_RIGHT_CLICK, "/input/thumbstick_right/click"},
    {USER_GAMEPAD_OUTPUT_HAPTIC_RIGHT, "/user/gamepad/output/haptic_right"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_X, "/user/gamepad/input/thumbstick_left/x"},
    {USER_HAND_LEFT_INPUT_SQUEEZE_VALUE, "/user/hand/left/input/squeeze/value"},
    {USER_GAMEPAD_INPUT_DPAD_UP_CLICK, "/user/gamepad/input/dpad_up/click"},
    {USER_HAND_RIGHT_INPUT_THUMBSTICK_X, "/user/hand/right/input/thumbstick/x"},
    {INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, "/interaction_profiles/microsoft/motion_controller"},
    {USER_HAND_LEFT_INPUT_SQUEEZE_FORCE, "/user/hand/left/input/squeeze/force"},
    {USER_HAND_RIGHT_INPUT_THUMBSTICK, "/user/hand/right/input/thumbstick"},
    {INPUT_THUMBSTICK_LEFT_X, "/input/thumbstick_left/x"},
    {INPUT_SYSTEM_TOUCH, "/input/system/touch"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_Y, "/user/gamepad/input/thumbstick_right/y"},
    {USER_HAND_RIGHT_INPUT_THUMBREST_TOUCH, "/user/hand/right/input/thumbrest/touch"},
    {INPUT_DPAD_LEFT_CLICK, "/input/dpad_left/click"},
    {USER_HAND_LEFT_INPUT_SQUEEZE_CLICK, "/user/hand/left/input/squeeze/click"},
    {USER_HAND_RIGHT_INPUT_TRACKPAD_X, "/user/hand/right/input/trackpad/x"},
    {INPUT_TRACKPAD_FORCE, "/input/trackpad/force"},
    {USER_HAND_RIGHT_INPUT_THUMBSTICK_TOUCH, "/user/hand/right/input/thumbstick/touch"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_LEFT, "/user/gamepad/input/thumbstick_left"},
    {USER_HAND_LEFT, "/user/hand/left"},
    {INPUT_SHOULDER_RIGHT_CLICK, "/input/shoulder_right/click"},
    {USER_HAND_RIGHT_INPUT_SQUEEZE_CLICK, "/user/hand/right/input/squeeze/click"},
    {INPUT_THUMBSTICK, "/input/thumbstick"},
    {USER_HAND_LEFT_OUTPUT_HAPTIC, "/user/hand/left/output/haptic"},
    {USER_HAND_RIGHT_INPUT_TRIGGER_VALUE, "/user/hand/right/input/trigger/value"},
    {INPUT_A_TOUCH, "/input/a/touch"},
    {INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, "/interaction_profiles/htc/vive_controller"},
    {USER_HAND_LEFT_INPUT_THUMBSTICK_TOUCH, "/user/hand/left/input/thumbstick/touch"},
    {INPUT_SQUEEZE_FORCE, "/input/squeeze/force"},
    {USER_HAND_LEFT_INPUT_TRIGGER_TOUCH, "/user/hand/left/input/trigger/touch"},
    {INPUT_DPAD_RIGHT_CLICK, "/input/dpad_right/click"},
    {INPUT_Y_CLICK, "/input/y/click"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_X, "/user/gamepad/input/thumbstick_right/x"},
    {USER_HAND_LEFT_INPUT_TRIGGER_CLICK, "/user/hand/left/input/trigger/click"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_CLICK, "/user/gamepad/input/thumbstick_left/click"},
    {USER_GAMEPAD_INPUT_DPAD_DOWN_CLICK, "/user/gamepad/input/dpad_down/click"},
    {USER_HAND_LEFT_INPUT_THUMBREST_TOUCH, "/user/hand/left/input/thumbrest/touch"},
    {INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, "/interaction_profiles/valve/index_controller"},
    {INPUT_Y_TOUCH, "/input/y/touch"},
    {USER_HAND_LEFT_INPUT_SYSTEM_TOUCH, "/user/hand/left/input/system/touch"},
    {USER_HAND_RIGHT_INPUT_A_TOUCH, "/user/hand/right/input/a/touch"},
    {USER_HAND_RIGHT_INPUT_SQUEEZE_VALUE, "/user/hand/right/input/squeeze/value"},
    {USER_GAMEPAD_INPUT_Y_CLICK, "/user/gamepad/input/y/click"},
    {INPUT_SQUEEZE_CLICK, "/input/squeeze/click"},
    {INPUT_MENU_CLICK, "/input/menu/click"},
    {USER_HAND_LEFT_INPUT_TRACKPAD, "/user/hand/left/input/trackpad"},
    {USER_HAND_RIGHT_INPUT_TRACKPAD, "/user/hand/right/input/trackpad"},
    {USER_HAND_RIGHT_INPUT_TRACKPAD_TOUCH, "/user/hand/right/input/trackpad/touch"},
    {INPUT_THUMBREST_TOUCH, "/input/thumbrest/touch"},
    {USER_HAND_LEFT_INPUT_X_TOUCH, "/user/hand/left/input/x/touch"},
    {USER_HAND_RIGHT_INPUT_THUMBSTICK_CLICK, "/user/hand/right/input/thumbstick/click"},
    {USER_HAND_RIGHT_INPUT_SQUEEZE_FORCE, "/user/hand/right/input/squeeze/force"},
    {USER_GAMEPAD_INPUT_TRIGGER_RIGHT_VALUE, "/user/gamepad/input/trigger_right/value"},
    {INPUT_THUMBSTICK_TOUCH, "/input/thumbstick/touch"},
    {INPUT_THUMBSTICK_Y, "/input/thumbstick/y"},
    {INPUT_TRIGGER_LEFT_VALUE, "/input/trigger_left/value"},
    {USER_GAMEPAD_INPUT_TRIGGER_LEFT_VALUE, "/user/gamepad/input/trigger_left/value"},
    {INPUT_SHOULDER_LEFT_CLICK, "/input/shoulder_left/click"},
    {USER_HAND_LEFT_INPUT_MENU_CLICK, "/user/hand/left/input/menu/click"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_Y, "/user/gamepad/input/thumbstick_left/y"},
    {USER_HAND_RIGHT_INPUT_B_TOUCH, "/user/hand/right/input/b/touch"},
    {USER_HAND_LEFT_INPUT_TRACKPAD_TOUCH, "/user/hand/left/input/trackpad/touch"},
    {USER_GAMEPAD_INPUT_MENU_CLICK, "/user/gamepad/input/menu/click"},
    {USER_HAND_LEFT_INPUT_SYSTEM_CLICK, "/user/hand/left/input/system/click"},
    {INPUT_TRACKPAD_X, "/input/trackpad/x"},
    {USER_HEAD, "/user/head"},
    {INPUT_THUMBSTICK_RIGHT, "/input/thumbstick_right"},
    {USER_HAND_RIGHT_INPUT_TRACKPAD_Y, "/user/hand/right/input/trackpad/y"},
    {INPUT_SYSTEM_CLICK, "/input/system/click"},
    {OUTPUT_HAPTIC, "/output/haptic"},
    {USER_HEAD_INPUT_VOLUME_UP_CLICK, "/user/head/input/volume_up/click"},
    {INPUT_X_CLICK, "/input/x/click"},
    {OUTPUT_HAPTIC_RIGHT, "/output/haptic_right"},
    {INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, "/interaction_profiles/microsoft/xbox_controller"},
    {USER_HAND_LEFT_INPUT_TRACKPAD_X, "/user/hand/left/input/trackpad/x"},
    {INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, "/interaction_profiles/oculus/go_controller"},
    {USER_HAND_RIGHT_INPUT_AIM_POSE, "/user/hand/right/input/aim/pose"},
    {INPUT_A_CLICK, "/input/a/click"},
    {INPUT_VOLUME_UP_CLICK, "/input/volume_up/click"},
    {INPUT_BACK_CLICK, "/input/back/click"},
    {INPUT_TRACKPAD_CLICK, "/input/trackpad/click"},
    {USER_GAMEPAD_INPUT_SHOULDER_LEFT_CLICK, "/user/gamepad/input/shoulder_left/click"},
    {USER_HAND_LEFT_INPUT_TRACKPAD_Y, "/user/hand/left/input/trackpad/y"},
    {INPUT_THUMBSTICK_X, "/input/thumbstick/x"},
    {INPUT_TRACKPAD_Y, "/input/trackpad/y"},
    {USER_GAMEPAD_INPUT_DPAD_LEFT_CLICK, "/user/gamepad/input/dpad_left/click"},
    {USER_HAND_LEFT_INPUT_B_TOUCH, "/user/hand/left/input/b/touch"},
    {USER_HAND_RIGHT_INPUT_GRIP_POSE, "/user/hand/right/input/grip/pose"},
    {INPUT_TRIGGER_VALUE, "/input/trigger/value"},
    {USER_GAMEPAD_INPUT_SHOULDER_RIGHT_CLICK, "/user/gamepad/input/shoulder_right/click"},
    {INPUT_MUTE_MIC_CLICK, "/input/mute_mic/click"},
    {USER_GAMEPAD_INPUT_DPAD_RIGHT_CLICK, "/user/gamepad/input/dpad_right/click"},
    {USER_HAND_RIGHT_INPUT_B_CLICK, "/user/hand/right/input/b/click"},
    {INPUT_SQUEEZE_VALUE, "/input/squeeze/value"},
    {USER_HAND_LEFT_INPUT_BACK_CLICK, "/user/hand/left/input/back/click"},
    {INPUT_SELECT_CLICK, "/input/select/click"},
    {USER_HAND_LEFT_INPUT_B_CLICK, "/user/hand/left/input/b/click"},
    {USER_HAND_RIGHT_INPUT_A_CLICK, "/user/hand/right/input/a/click"},
    {INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, "/interaction_profiles/oculus/touch_controller"},
    {INPUT_THUMBSTICK_RIGHT_X, "/input/thumbstick_right/x"},
    {USER_HAND_LEFT_INPUT_TRACKPAD_FORCE, "/user/hand/left/input/trackpad/force"},
    {USER_GAMEPAD_INPUT_A_CLICK, "/user/gamepad/input/a/click"},
    {USER_HAND_RIGHT_INPUT_SYSTEM_CLICK, "/user/hand/right/input/system/click"},
    {INPUT_THUMBSTICK_CLICK, "/input/thumbstick/click"},
    {USER_GAMEPAD_INPUT_X_CLICK, "/user/gamepad/input/x/click"},
    {INPUT_THUMBSTICK_LEFT, "/input/thumbstick_left"},
    {INPUT_X_TOUCH, "/input/x/touch"},
    {USER_HAND_LEFT_INPUT_THUMBSTICK, "/user/hand/left/input/thumbstick"},
    {USER_HAND_LEFT_INPUT_X_CLICK, "/user/hand/left/input/x/click"},
    {USER_HAND_LEFT_INPUT_Y_TOUCH, "/user/hand/left/input/y/touch"},
    {USER_HAND_RIGHT_OUTPUT_HAPTIC, "/user/hand/right/output/haptic"},
    {INPUT_DPAD_UP_CLICK, "/input/dpad_up/click"},
    {USER_HAND_LEFT_INPUT_TRIGGER_VALUE, "/user/hand/left/input/trigger/value"},
    {USER_HAND_LEFT_INPUT_A_CLICK, "/user/hand/left/input/a/click"},
    {INPUT_TRACKPAD_TOUCH, "/input/trackpad/touch"},
    {USER_GAMEPAD_OUTPUT_HAPTIC_LEFT_TRIGGER, "/user/gamepad/output/haptic_left_trigger"},
    {INPUT_GRIP_POSE, "/input/grip/pose"},
    {INPUT_B_CLICK, "/input/b/click"},
    {OUTPUT_HAPTIC_LEFT, "/output/haptic_left"},
    {USER_HAND_LEFT_INPUT_THUMBSTICK_X, "/user/hand/left/input/thumbstick/x"},
    {USER_GAMEPAD_INPUT_B_CLICK, "/user/gamepad/input/b/click"},
    {USER_HAND_LEFT_INPUT_SELECT_CLICK, "/user/hand/left/input/select/click"},
    {USER_HAND_RIGHT_INPUT_TRIGGER_CLICK, "/user/hand/right/input/trigger/click"},
    {USER_HAND_LEFT_INPUT_GRIP_POSE, "/user/hand/left/input/grip/pose"},
    {INPUT_THUMBSTICK_RIGHT_Y, "/input/thumbstick_right/y"},
    {USER_HAND_RIGHT_INPUT_SYSTEM_TOUCH, "/user/hand/right/input/system/touch"},
    {INPUT_AIM_POSE, "/input/aim/pose"},
    {INTERACTION_PROFILES_HTC_VIVE_PRO, "/interaction_profiles/htc/vive_pro"},
    {USER_GAMEPAD_INPUT_VIEW_CLICK, "/user/gamepad/input/view/click"},
    {INPUT_TRIGGER_CLICK, "/input/trigger/click"},
    {USER_HAND_LEFT_INPUT_Y_CLICK, "/user/hand/left/input/y/click"},
    {INPUT_TRIGGER_RIGHT_VALUE, "/input/trigger_right/value"},
    {INPUT_THUMBSTICK_LEFT_Y, "/input/thumbstick_left/y"},
    {USER_HAND_LEFT_INPUT_THUMBSTICK_Y, "/user/hand/left/input/thumbstick/y"},
    {USER_HEAD_INPUT_VOLUME_DOWN_CLICK, "/user/head/input/volume_down/click"},
    {USER_HAND_RIGHT_INPUT_THUMBSTICK_Y, "/user/hand/right/input/thumbstick/y"},
    {USER_HAND_RIGHT_INPUT_TRACKPAD_CLICK, "/user/hand/right/input/trackpad/click"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_CLICK, "/user/gamepad/input/thumbstick_right/click"},
    {INPUT_TRIGGER_TOUCH, "/input/trigger/touch"},
    {USER_GAMEPAD_OUTPUT_HAPTIC_LEFT, "/user/gamepad/output/haptic_left"},
    {USER_HAND_LEFT_INPUT_TRACKPAD_CLICK, "/user/hand/left/input/trackpad/click"},
    {USER_HAND_RIGHT_INPUT_BACK_CLICK, "/user/hand/right/input/back/click"},
    {USER_GAMEPAD_OUTPUT_HAPTIC_RIGHT_TRIGGER, "/user/gamepad/output/haptic_right_trigger"},
    {INPUT_TRACKPAD, "/input/trackpad"},
    {USER_HEAD_INPUT_MUTE_MIC_CLICK, "/user/head/input/mute_mic/click"},
    {INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, "/interaction_profiles/khr/simple_controller"},
    {USER_HAND_RIGHT, "/user/hand/right"},
    {USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT, "/user/gamepad/input/thumbstick_right"},
    {OUTPUT_HAPTIC_LEFT_TRIGGER, "/output/haptic_left_trigger"},
    {USER_HAND_LEFT_INPUT_AIM_POSE, "/user/hand/left/input/aim/pose"},
    {INPUT_B_TOUCH, "/input/b/touch"},
    {USER_HAND_RIGHT_INPUT_MENU_CLICK, "/user/hand/right/input/menu/click"},
    {INPUT_VOLUME_DOWN_CLICK, "/input/volume_down/click"},
    {INPUT_VIEW_CLICK, "/input/view/click"},
    {USER_HAND_LEFT_INPUT_A_TOUCH, "/user/hand/left/input/a/touch"},
    {USER_HAND_RIGHT_INPUT_TRACKPAD_FORCE, "/user/hand/right/input/trackpad/force"},
    {INPUT_THUMBSTICK_LEFT_CLICK, "/input/thumbstick_left/click"},
    {OUTPUT_HAPTIC_RIGHT_TRIGGER, "/output/haptic_right_trigger"},
    {USER_GAMEPAD, "/user/gamepad"},
    {USER_HEAD_INPUT_SYSTEM_CLICK, "/user/head/input/system/click"},
    {USER_HAND_RIGHT_INPUT_TRIGGER_TOUCH, "/user/hand/right/input/trigger/touch"},
};

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
    {"/interaction_profiles/khr/simple_controller/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_LEFT, INPUT_AIM_POSE, USER_HAND_LEFT_INPUT_AIM_POSE},
    {"/interaction_profiles/khr/simple_controller/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_LEFT, OUTPUT_HAPTIC, USER_HAND_LEFT_OUTPUT_HAPTIC},
    {"/interaction_profiles/khr/simple_controller/user/hand/left/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_LEFT, INPUT_MENU_CLICK, USER_HAND_LEFT_INPUT_MENU_CLICK},
    {"/interaction_profiles/khr/simple_controller/user/hand/left/input/select/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_LEFT, INPUT_SELECT_CLICK, USER_HAND_LEFT_INPUT_SELECT_CLICK},
    {"/interaction_profiles/khr/simple_controller/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_LEFT, INPUT_GRIP_POSE, USER_HAND_LEFT_INPUT_GRIP_POSE},
    {"/interaction_profiles/khr/simple_controller/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_RIGHT, INPUT_AIM_POSE, USER_HAND_RIGHT_INPUT_AIM_POSE},
    {"/interaction_profiles/khr/simple_controller/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_RIGHT, OUTPUT_HAPTIC, USER_HAND_RIGHT_OUTPUT_HAPTIC},
    {"/interaction_profiles/khr/simple_controller/user/hand/right/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_RIGHT, INPUT_MENU_CLICK, USER_HAND_RIGHT_INPUT_MENU_CLICK},
    {"/interaction_profiles/khr/simple_controller/user/hand/right/input/select/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_RIGHT, INPUT_SELECT_CLICK, USER_HAND_RIGHT_INPUT_SELECT_CLICK},
    {"/interaction_profiles/khr/simple_controller/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_KHR_SIMPLE_CONTROLLER, USER_HAND_RIGHT, INPUT_GRIP_POSE, USER_HAND_RIGHT_INPUT_GRIP_POSE},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_CLICK, USER_HAND_LEFT_INPUT_TRACKPAD_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_SYSTEM_CLICK, USER_HAND_LEFT_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_AIM_POSE, USER_HAND_LEFT_INPUT_AIM_POSE},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/squeeze/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_SQUEEZE_CLICK, USER_HAND_LEFT_INPUT_SQUEEZE_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_Y, USER_HAND_LEFT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, OUTPUT_HAPTIC, USER_HAND_LEFT_OUTPUT_HAPTIC},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_MENU_CLICK, USER_HAND_LEFT_INPUT_MENU_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_CLICK, USER_HAND_LEFT_INPUT_TRIGGER_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_VALUE, USER_HAND_LEFT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_TOUCH, USER_HAND_LEFT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_X, USER_HAND_LEFT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD, USER_HAND_LEFT_INPUT_TRACKPAD},
    {"/interaction_profiles/htc/vive_controller/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_LEFT, INPUT_GRIP_POSE, USER_HAND_LEFT_INPUT_GRIP_POSE},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_CLICK, USER_HAND_RIGHT_INPUT_TRACKPAD_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_SYSTEM_CLICK, USER_HAND_RIGHT_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_AIM_POSE, USER_HAND_RIGHT_INPUT_AIM_POSE},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/squeeze/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_SQUEEZE_CLICK, USER_HAND_RIGHT_INPUT_SQUEEZE_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_Y, USER_HAND_RIGHT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, OUTPUT_HAPTIC, USER_HAND_RIGHT_OUTPUT_HAPTIC},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_MENU_CLICK, USER_HAND_RIGHT_INPUT_MENU_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_CLICK, USER_HAND_RIGHT_INPUT_TRIGGER_CLICK},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_VALUE, USER_HAND_RIGHT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_TOUCH, USER_HAND_RIGHT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_X, USER_HAND_RIGHT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD, USER_HAND_RIGHT_INPUT_TRACKPAD},
    {"/interaction_profiles/htc/vive_controller/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_HTC_VIVE_CONTROLLER, USER_HAND_RIGHT, INPUT_GRIP_POSE, USER_HAND_RIGHT_INPUT_GRIP_POSE},
    {"/interaction_profiles/htc/vive_pro/user/head/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_PRO, USER_HEAD, INPUT_SYSTEM_CLICK, USER_HEAD_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/htc/vive_pro/user/head/input/volume_down/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_PRO, USER_HEAD, INPUT_VOLUME_DOWN_CLICK, USER_HEAD_INPUT_VOLUME_DOWN_CLICK},
    {"/interaction_profiles/htc/vive_pro/user/head/input/volume_up/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_PRO, USER_HEAD, INPUT_VOLUME_UP_CLICK, USER_HEAD_INPUT_VOLUME_UP_CLICK},
    {"/interaction_profiles/htc/vive_pro/user/head/input/mute_mic/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_HTC_VIVE_PRO, USER_HEAD, INPUT_MUTE_MIC_CLICK, USER_HEAD_INPUT_MUTE_MIC_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_CLICK, USER_HAND_LEFT_INPUT_TRACKPAD_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_AIM_POSE, USER_HAND_LEFT_INPUT_AIM_POSE},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_X, USER_HAND_LEFT_INPUT_THUMBSTICK_X},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK, USER_HAND_LEFT_INPUT_THUMBSTICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/squeeze/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_SQUEEZE_CLICK, USER_HAND_LEFT_INPUT_SQUEEZE_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_Y, USER_HAND_LEFT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_CLICK, USER_HAND_LEFT_INPUT_THUMBSTICK_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_MENU_CLICK, USER_HAND_LEFT_INPUT_MENU_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, OUTPUT_HAPTIC, USER_HAND_LEFT_OUTPUT_HAPTIC},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_Y, USER_HAND_LEFT_INPUT_THUMBSTICK_Y},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_VALUE, USER_HAND_LEFT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_TOUCH, USER_HAND_LEFT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_X, USER_HAND_LEFT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD, USER_HAND_LEFT_INPUT_TRACKPAD},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_LEFT, INPUT_GRIP_POSE, USER_HAND_LEFT_INPUT_GRIP_POSE},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_CLICK, USER_HAND_RIGHT_INPUT_TRACKPAD_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_AIM_POSE, USER_HAND_RIGHT_INPUT_AIM_POSE},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_X, USER_HAND_RIGHT_INPUT_THUMBSTICK_X},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK, USER_HAND_RIGHT_INPUT_THUMBSTICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/squeeze/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_SQUEEZE_CLICK, USER_HAND_RIGHT_INPUT_SQUEEZE_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_Y, USER_HAND_RIGHT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_CLICK, USER_HAND_RIGHT_INPUT_THUMBSTICK_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_MENU_CLICK, USER_HAND_RIGHT_INPUT_MENU_CLICK},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, OUTPUT_HAPTIC, USER_HAND_RIGHT_OUTPUT_HAPTIC},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_Y, USER_HAND_RIGHT_INPUT_THUMBSTICK_Y},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_VALUE, USER_HAND_RIGHT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_TOUCH, USER_HAND_RIGHT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_X, USER_HAND_RIGHT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD, USER_HAND_RIGHT_INPUT_TRACKPAD},
    {"/interaction_profiles/microsoft/motion_controller/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_MICROSOFT_MOTION_CONTROLLER, USER_HAND_RIGHT, INPUT_GRIP_POSE, USER_HAND_RIGHT_INPUT_GRIP_POSE},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_right/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_RIGHT_Y, USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_Y},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_MENU_CLICK, USER_GAMEPAD_INPUT_MENU_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/dpad_down/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_DPAD_DOWN_CLICK, USER_GAMEPAD_INPUT_DPAD_DOWN_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/trigger_right/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_TRIGGER_RIGHT_VALUE, USER_GAMEPAD_INPUT_TRIGGER_RIGHT_VALUE},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_left/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_LEFT_Y, USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_Y},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_right/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_RIGHT_CLICK, USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_right/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_RIGHT_X, USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT_X},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_right", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_RIGHT, USER_GAMEPAD_INPUT_THUMBSTICK_RIGHT},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/trigger_left/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_TRIGGER_LEFT_VALUE, USER_GAMEPAD_INPUT_TRIGGER_LEFT_VALUE},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_left/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_LEFT_X, USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_X},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_left", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_LEFT, USER_GAMEPAD_INPUT_THUMBSTICK_LEFT},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/shoulder_left/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_SHOULDER_LEFT_CLICK, USER_GAMEPAD_INPUT_SHOULDER_LEFT_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/dpad_left/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_DPAD_LEFT_CLICK, USER_GAMEPAD_INPUT_DPAD_LEFT_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/x/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_X_CLICK, USER_GAMEPAD_INPUT_X_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/dpad_up/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_DPAD_UP_CLICK, USER_GAMEPAD_INPUT_DPAD_UP_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/output/haptic_right", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, OUTPUT_HAPTIC_RIGHT, USER_GAMEPAD_OUTPUT_HAPTIC_RIGHT},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/shoulder_right/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_SHOULDER_RIGHT_CLICK, USER_GAMEPAD_INPUT_SHOULDER_RIGHT_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/output/haptic_left_trigger", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, OUTPUT_HAPTIC_LEFT_TRIGGER, USER_GAMEPAD_OUTPUT_HAPTIC_LEFT_TRIGGER},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/view/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_VIEW_CLICK, USER_GAMEPAD_INPUT_VIEW_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/a/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_A_CLICK, USER_GAMEPAD_INPUT_A_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/b/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_B_CLICK, USER_GAMEPAD_INPUT_B_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/dpad_right/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_DPAD_RIGHT_CLICK, USER_GAMEPAD_INPUT_DPAD_RIGHT_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/y/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_Y_CLICK, USER_GAMEPAD_INPUT_Y_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/input/thumbstick_left/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, INPUT_THUMBSTICK_LEFT_CLICK, USER_GAMEPAD_INPUT_THUMBSTICK_LEFT_CLICK},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/output/haptic_right_trigger", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, OUTPUT_HAPTIC_RIGHT_TRIGGER, USER_GAMEPAD_OUTPUT_HAPTIC_RIGHT_TRIGGER},
    {"/interaction_profiles/microsoft/xbox_controller/user/gamepad/output/haptic_left", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_MICROSOFT_XBOX_CONTROLLER, USER_GAMEPAD, OUTPUT_HAPTIC_LEFT, USER_GAMEPAD_OUTPUT_HAPTIC_LEFT},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_CLICK, USER_HAND_LEFT_INPUT_TRACKPAD_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_SYSTEM_CLICK, USER_HAND_LEFT_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_AIM_POSE, USER_HAND_LEFT_INPUT_AIM_POSE},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_GRIP_POSE, USER_HAND_LEFT_INPUT_GRIP_POSE},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_Y, USER_HAND_LEFT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_CLICK, USER_HAND_LEFT_INPUT_TRIGGER_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_TOUCH, USER_HAND_LEFT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_X, USER_HAND_LEFT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD, USER_HAND_LEFT_INPUT_TRACKPAD},
    {"/interaction_profiles/oculus/go_controller/user/hand/left/input/back/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_LEFT, INPUT_BACK_CLICK, USER_HAND_LEFT_INPUT_BACK_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/trackpad/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_CLICK, USER_HAND_RIGHT_INPUT_TRACKPAD_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_SYSTEM_CLICK, USER_HAND_RIGHT_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_AIM_POSE, USER_HAND_RIGHT_INPUT_AIM_POSE},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_GRIP_POSE, USER_HAND_RIGHT_INPUT_GRIP_POSE},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_Y, USER_HAND_RIGHT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_CLICK, USER_HAND_RIGHT_INPUT_TRIGGER_CLICK},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_TOUCH, USER_HAND_RIGHT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_X, USER_HAND_RIGHT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD, USER_HAND_RIGHT_INPUT_TRACKPAD},
    {"/interaction_profiles/oculus/go_controller/user/hand/right/input/back/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_GO_CONTROLLER, USER_HAND_RIGHT, INPUT_BACK_CLICK, USER_HAND_RIGHT_INPUT_BACK_CLICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/trigger/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_TOUCH, USER_HAND_LEFT_INPUT_TRIGGER_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_AIM_POSE, USER_HAND_LEFT_INPUT_AIM_POSE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_X, USER_HAND_LEFT_INPUT_THUMBSTICK_X},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK, USER_HAND_LEFT_INPUT_THUMBSTICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/y/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_Y_TOUCH, USER_HAND_LEFT_INPUT_Y_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/y/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_Y_CLICK, USER_HAND_LEFT_INPUT_Y_CLICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_SQUEEZE_VALUE, USER_HAND_LEFT_INPUT_SQUEEZE_VALUE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/x/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_X_CLICK, USER_HAND_LEFT_INPUT_X_CLICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_MENU_CLICK, USER_HAND_LEFT_INPUT_MENU_CLICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_CLICK, USER_HAND_LEFT_INPUT_THUMBSTICK_CLICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_Y, USER_HAND_LEFT_INPUT_THUMBSTICK_Y},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/thumbstick/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_TOUCH, USER_HAND_LEFT_INPUT_THUMBSTICK_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, OUTPUT_HAPTIC, USER_HAND_LEFT_OUTPUT_HAPTIC},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_VALUE, USER_HAND_LEFT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/thumbrest/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBREST_TOUCH, USER_HAND_LEFT_INPUT_THUMBREST_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/x/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_X_TOUCH, USER_HAND_LEFT_INPUT_X_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_LEFT, INPUT_GRIP_POSE, USER_HAND_LEFT_INPUT_GRIP_POSE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/b/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_B_CLICK, USER_HAND_RIGHT_INPUT_B_CLICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_SYSTEM_CLICK, USER_HAND_RIGHT_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/trigger/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_TOUCH, USER_HAND_RIGHT_INPUT_TRIGGER_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_X, USER_HAND_RIGHT_INPUT_THUMBSTICK_X},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK, USER_HAND_RIGHT_INPUT_THUMBSTICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/thumbstick/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_TOUCH, USER_HAND_RIGHT_INPUT_THUMBSTICK_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_SQUEEZE_VALUE, USER_HAND_RIGHT_INPUT_SQUEEZE_VALUE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_AIM_POSE, USER_HAND_RIGHT_INPUT_AIM_POSE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_CLICK, USER_HAND_RIGHT_INPUT_THUMBSTICK_CLICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, OUTPUT_HAPTIC, USER_HAND_RIGHT_OUTPUT_HAPTIC},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_Y, USER_HAND_RIGHT_INPUT_THUMBSTICK_Y},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/thumbrest/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBREST_TOUCH, USER_HAND_RIGHT_INPUT_THUMBREST_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/b/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_B_TOUCH, USER_HAND_RIGHT_INPUT_B_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_VALUE, USER_HAND_RIGHT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/a/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_A_CLICK, USER_HAND_RIGHT_INPUT_A_CLICK},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/a/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_A_TOUCH, USER_HAND_RIGHT_INPUT_A_TOUCH},
    {"/interaction_profiles/oculus/touch_controller/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_OCULUS_TOUCH_CONTROLLER, USER_HAND_RIGHT, INPUT_GRIP_POSE, USER_HAND_RIGHT_INPUT_GRIP_POSE},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_AIM_POSE, USER_HAND_LEFT_INPUT_AIM_POSE},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_SQUEEZE_VALUE, USER_HAND_LEFT_INPUT_SQUEEZE_VALUE},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_CLICK, USER_HAND_LEFT_INPUT_TRIGGER_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trigger/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_TOUCH, USER_HAND_LEFT_INPUT_TRIGGER_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/thumbstick/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_TOUCH, USER_HAND_LEFT_INPUT_THUMBSTICK_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_CLICK, USER_HAND_LEFT_INPUT_THUMBSTICK_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_Y, USER_HAND_LEFT_INPUT_THUMBSTICK_Y},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/system/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_SYSTEM_TOUCH, USER_HAND_LEFT_INPUT_SYSTEM_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_X, USER_HAND_LEFT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD, USER_HAND_LEFT_INPUT_TRACKPAD},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_SYSTEM_CLICK, USER_HAND_LEFT_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trackpad/force", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_FORCE, USER_HAND_LEFT_INPUT_TRACKPAD_FORCE},
    {"/interaction_profiles/valve/index_controller/user/hand/left/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, OUTPUT_HAPTIC, USER_HAND_LEFT_OUTPUT_HAPTIC},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/b/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_B_TOUCH, USER_HAND_LEFT_INPUT_B_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/a/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_A_CLICK, USER_HAND_LEFT_INPUT_A_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/a/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_A_TOUCH, USER_HAND_LEFT_INPUT_A_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/squeeze/force", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_SQUEEZE_FORCE, USER_HAND_LEFT_INPUT_SQUEEZE_FORCE},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_TOUCH, USER_HAND_LEFT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_GRIP_POSE, USER_HAND_LEFT_INPUT_GRIP_POSE},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/b/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_B_CLICK, USER_HAND_LEFT_INPUT_B_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK_X, USER_HAND_LEFT_INPUT_THUMBSTICK_X},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_THUMBSTICK, USER_HAND_LEFT_INPUT_THUMBSTICK},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRACKPAD_Y, USER_HAND_LEFT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/valve/index_controller/user/hand/left/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_LEFT, INPUT_TRIGGER_VALUE, USER_HAND_LEFT_INPUT_TRIGGER_VALUE},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/aim/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_AIM_POSE, USER_HAND_RIGHT_INPUT_AIM_POSE},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_SQUEEZE_VALUE, USER_HAND_RIGHT_INPUT_SQUEEZE_VALUE},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trigger/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_CLICK, USER_HAND_RIGHT_INPUT_TRIGGER_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trigger/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_TOUCH, USER_HAND_RIGHT_INPUT_TRIGGER_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/thumbstick/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_TOUCH, USER_HAND_RIGHT_INPUT_THUMBSTICK_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_CLICK, USER_HAND_RIGHT_INPUT_THUMBSTICK_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_Y, USER_HAND_RIGHT_INPUT_THUMBSTICK_Y},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/system/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_SYSTEM_TOUCH, USER_HAND_RIGHT_INPUT_SYSTEM_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trackpad/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_X, USER_HAND_RIGHT_INPUT_TRACKPAD_X},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD, USER_HAND_RIGHT_INPUT_TRACKPAD},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/system/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_SYSTEM_CLICK, USER_HAND_RIGHT_INPUT_SYSTEM_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trackpad/force", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_FORCE, USER_HAND_RIGHT_INPUT_TRACKPAD_FORCE},
    {"/interaction_profiles/valve/index_controller/user/hand/right/output/haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, OUTPUT_HAPTIC, USER_HAND_RIGHT_OUTPUT_HAPTIC},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/b/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_B_TOUCH, USER_HAND_RIGHT_INPUT_B_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/a/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_A_CLICK, USER_HAND_RIGHT_INPUT_A_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/a/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_A_TOUCH, USER_HAND_RIGHT_INPUT_A_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/squeeze/force", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_SQUEEZE_FORCE, USER_HAND_RIGHT_INPUT_SQUEEZE_FORCE},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trackpad/touch", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_TOUCH, USER_HAND_RIGHT_INPUT_TRACKPAD_TOUCH},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_GRIP_POSE, USER_HAND_RIGHT_INPUT_GRIP_POSE},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/b/click", XR_ACTION_TYPE_BOOLEAN_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_B_CLICK, USER_HAND_RIGHT_INPUT_B_CLICK},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK_X, USER_HAND_RIGHT_INPUT_THUMBSTICK_X},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_THUMBSTICK, USER_HAND_RIGHT_INPUT_THUMBSTICK},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trackpad/y", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRACKPAD_Y, USER_HAND_RIGHT_INPUT_TRACKPAD_Y},
    {"/interaction_profiles/valve/index_controller/user/hand/right/input/trigger/value", XR_ACTION_TYPE_FLOAT_INPUT, INTERACTION_PROFILES_VALVE_INDEX_CONTROLLER, USER_HAND_RIGHT, INPUT_TRIGGER_VALUE, USER_HAND_RIGHT_INPUT_TRIGGER_VALUE},

};

// Just in case everything is terrible and every proc has to be synchronized
std::recursive_mutex gSynchronizeEveryProcMutex;
bool gSynchronizeEveryProc = true; // XXX Currently true because of both layer view loss and ReleaseSwapchainImage VALIDATION_FAILURE

// LATER understand which lock isn't doing its job and take this out
// But I'm also using to enforce synchronization between LocateSpace and EndFrame, which seem to conflict
std::recursive_mutex EndFrameMutex;

// On OVR I get regular deadlocks in one thread in runtime ReleaseSwapchainImage and in another thread in ApplyHapticFeedback.
std::recursive_mutex HapticQuirkMutex;


const std::set<HandleTypePair> OverlaysLayerNoObjectInfo = {};

uint64_t GetNextLocalHandle()
{
    static std::atomic_uint64_t nextHandle = 1;
    return nextHandle++;
}


std::unique_lock<std::recursive_mutex> GetSyncActionsLock()
{
    static std::recursive_mutex syncActionsMutex;
    return std::unique_lock<std::recursive_mutex>(syncActionsMutex);
}


std::string PathToString(XrInstance instance, XrPath path)
{
    if(path == XR_NULL_PATH) {
        return "XR_NULL_PATH";
    }

    char buffer[257];
    uint32_t countOutput;
    XrResult result;

    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);

    result = instanceInfo->downchain->PathToString(instance, path, 0, &countOutput, nullptr);
    if(result == XR_SUCCESS) {
        result = instanceInfo->downchain->PathToString(instance, path, countOutput, &countOutput, buffer);
    }

    if(result != XR_SUCCESS) {
        if(instanceInfo->OverlaysLayerPathToWellKnownString.count(path) > 0) {
            auto index = instanceInfo->OverlaysLayerPathToWellKnownString.at(path);
            auto str = OverlaysLayerWellKnownStrings.at(index);
            sprintf(buffer, "<PathToString failed?! %08llX, \"%s\">", path, str);
            return buffer;
        } else {
            sprintf(buffer, "<PathToString failed?! %08llX>", path);
            return buffer;
        }
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
        OverlaysLayerNoObjectInfo, fmt("%s at %s:%d failed with %d (%s)", what, file, line, lastError, messageBuf).c_str());
    LocalFree(messageBuf);
}

void LogWindowsError(HRESULT result, const char *xrfunc, const char* what, const char *file, int line)
{
    LPVOID messageBuf;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, xrfunc,
        OverlaysLayerNoObjectInfo, fmt("%s at %s:%d failed with %d (%s)", what, file, line, result, messageBuf).c_str());
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
        LogWindowsError(result, nullptr, "QueryInterface", __FILE__, __LINE__);
        return nullptr;
    }

    auto it = handleTextureMap.find(sourceHandle);
    if(it == handleTextureMap.end()) {
        if((result = device1->OpenSharedResource1(sourceHandle, __uuidof(ID3D11Texture2D), (LPVOID*) &sharedTexture)) != S_OK) {
            LogWindowsError(result, nullptr, "OpenSharedResource1", __FILE__, __LINE__);
            return nullptr;
        }
        handleTextureMap.insert({sourceHandle, sharedTexture});
    } else  {
        sharedTexture = it->second;
    }
    device1->Release();

    return sharedTexture;
}


// LATER could generate
void OverlaysLayerRemoveXrSpaceHandleInfo(XrSpace localHandle)
{
    {
        OverlaysLayerXrSpaceHandleInfo::Ptr info = OverlaysLayerGetHandleInfoFromXrSpace(localHandle);
        OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(info->parentHandle);
        sessionInfo->childSpaces.erase(info);
    }

    OverlaysLayerRemoveXrSpaceFromHandleInfoMap(localHandle);
}

// LATER could generate
void OverlaysLayerRemoveXrSwapchainHandleInfo(XrSwapchain localHandle)
{
    {
        OverlaysLayerXrSwapchainHandleInfo::Ptr info = OverlaysLayerGetHandleInfoFromXrSwapchain(localHandle);
        OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(info->parentHandle);
        sessionInfo->childSwapchains.erase(info);
    }

    OverlaysLayerRemoveXrSwapchainFromHandleInfoMap(localHandle);
}

// LATER could generate
void OverlaysLayerRemoveXrActionHandleInfo(XrAction localHandle)
{
    {
        OverlaysLayerXrActionHandleInfo::Ptr info = OverlaysLayerGetHandleInfoFromXrAction(localHandle);
        OverlaysLayerXrActionSetHandleInfo::Ptr actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(info->parentHandle);
        actionSetInfo->childActions.erase(info);
    }

    OverlaysLayerRemoveXrActionFromHandleInfoMap(localHandle);
}

// LATER could generate
void OverlaysLayerRemoveXrActionSetHandleInfo(XrActionSet actionSet)
{
    OverlaysLayerXrActionSetHandleInfo::Ptr info = OverlaysLayerGetHandleInfoFromXrActionSet(actionSet);

    /* remove all XrAction children of this XrActionSet */
    for(auto action: info->childActions) {
        OverlaysLayerRemoveXrActionFromHandleInfoMap(action->handle);
    }

    // remove self from Instance childActionSets
    OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(info->parentHandle);
    instanceInfo->childActionSets.erase(info);


    OverlaysLayerRemoveXrActionSetFromHandleInfoMap(actionSet);
}

// LATER could generate
void OverlaysLayerRemoveXrSessionHandleInfo(XrSession session)
{
    OverlaysLayerXrSessionHandleInfo::Ptr info = OverlaysLayerGetHandleInfoFromXrSession(session);

    /* remove all XrSwapchain children of this XrSession */
    for(auto swapchain: info->childSwapchains) {
        OverlaysLayerRemoveXrSwapchainFromHandleInfoMap(swapchain->localHandle);
    }

    /* remove all XrSpace children of this XrSession */
    for(auto space: info->childSpaces) {
        OverlaysLayerRemoveXrSpaceFromHandleInfoMap(space->localHandle);
    }

    // remove self from Instance childSessions
    OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(info->parentHandle);
    instanceInfo->childSessions.erase(info);

    OverlaysLayerRemoveXrSessionFromHandleInfoMap(session);
}

// XXX need OverlaysLayerRemoveXrSessionHandleInfo

// LATER could generate
void OverlaysLayerRemoveXrInstanceHandleInfo(XrInstance instance)
{
    OverlaysLayerXrInstanceHandleInfo::Ptr info = OverlaysLayerGetHandleInfoFromXrInstance(instance);

    /* remove all XrActionSet children of this XrInstance */
    for(auto actionSet: info->childActionSets) {
        OverlaysLayerRemoveXrActionSetFromHandleInfoMap(actionSet->handle);
    }

    /* remove all XrSession children of this XrInstance */
    for(auto session: info->childSessions) {
        OverlaysLayerRemoveXrSessionFromHandleInfoMap(session->localHandle);
    }

    /* remove all XrSession children of this XrInstance */
    for(auto messenger: info->childDebugUtilsMessengerEXTs) {
        OverlaysLayerRemoveXrDebugUtilsMessengerEXTFromHandleInfoMap(messenger->handle);
    }

    OverlaysLayerRemoveXrInstanceFromHandleInfoMap(instance);
}

void OverlaysLayerLogMessage(XrInstance instance,
                         XrDebugUtilsMessageSeverityFlagsEXT message_severity, const char* command_name,
                         const std::set<HandleTypePair>& objects_info, const char* message)
{
    // If we have instance information, see if we need to log this information out to a debug messenger
    // callback.
    if(instance != XR_NULL_HANDLE) {

        auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);

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

                auto messengerInfo = OverlaysLayerGetHandleInfoFromXrDebugUtilsMessengerEXT(messenger);

                XrDebugUtilsMessengerCreateInfoEXT *messenger_create_info = messengerInfo->createInfo;

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

    const char *sync_everything_env = getenv("OVERLAYS_API_LAYER_SYNCHRONIZE_EVERYTHING");
    if(sync_everything_env) {
        std::string sync_everything = sync_everything_env;
        std::set<std::string> truths {"true", "TRUE", "True", "1", "yes"};
        gSynchronizeEveryProc = (truths.count(sync_everything) > 0);
        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, "xrCreateInstance", 
            OverlaysLayerNoObjectInfo, fmt("gSynchronizeEveryProc set to %s", gSynchronizeEveryProc ? "true" : "false").c_str());
    }

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

    OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = std::make_shared<OverlaysLayerXrInstanceHandleInfo>(next_dispatch);
    instanceInfo->createInfo = reinterpret_cast<XrInstanceCreateInfo*>(CopyXrStructChainWithMalloc(*instance, instanceCreateInfo));

    // Create XrPaths for well-known strings.  We can use the compile-time fixed string enums to pass strings and paths over RPC
    // XXX This should be on CreateInstance in the instance info
    instanceInfo->OverlaysLayerWellKnownStringToPath.insert({WellKnownStringIndex::NULL_PATH, XR_NULL_PATH});
    instanceInfo->OverlaysLayerPathToWellKnownString.insert({XR_NULL_PATH, WellKnownStringIndex::NULL_PATH});
    for(auto& w : OverlaysLayerWellKnownStrings) {
        XrPath path;
        XrResult result2 = instanceInfo->downchain->StringToPath(*instance, w.second, &path);
        if(result2 != XR_SUCCESS) {
            OverlaysLayerLogMessage(*instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateInstance", 
                OverlaysLayerNoObjectInfo, fmt("Could not create path from \"%s\".", w.second).c_str());
            return XR_ERROR_INITIALIZATION_FAILED;
        }
        instanceInfo->OverlaysLayerWellKnownStringToPath.insert({w.first, path});
        instanceInfo->OverlaysLayerPathToWellKnownString.insert({path, w.first});
    }
    for(auto& id : PlaceholderActionIds) {
        XrPath profilePath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(id.interactionProfileString);
        XrPath subactionPath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(id.subActionString);
        XrPath componentPath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(id.componentString);
        XrPath fullBindingPath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(id.fullBindingString);

        instanceInfo->OverlaysLayerBindingToSubaction.insert({fullBindingPath, subactionPath});

        instanceInfo->OverlaysLayerAllSubactionPaths.insert(subactionPath);
    }

    OverlaysLayerAddHandleInfoForXrInstance(*instance, instanceInfo);

    return result;
}

XrResult OverlaysLayerXrDestroyInstance(XrInstance instance)
{
    OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);
    std::shared_ptr<XrGeneratedDispatchTable> next_dispatch = instanceInfo->downchain;
    // instanceInfo->Destroy();
    OverlaysLayerRemoveXrInstanceHandleInfo(instance);

    next_dispatch->DestroyInstance(instance);

    return XR_SUCCESS;
}

NegotiationChannels gNegotiationChannels;

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
            OverlaysLayerNoObjectInfo, fmt("Could not initialize the negotiation mutex: CreateMutex error was %d (%s)", lastError, messageBuf).c_str());
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
            OverlaysLayerNoObjectInfo, fmt("Could not initialize the negotiation shmem: CreateFileMappingA error was %08X (%s)", lastError, messageBuf).c_str());
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
            OverlaysLayerNoObjectInfo, fmt("Could not get the negotiation shmem: MapViewOfFile error was %08X (%s)", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false; 
    }

    ch.overlayWaitSema = CreateSemaphoreA(nullptr, 0, 1, NegotiationChannels::overlayWaitSemaName);
    if(ch.overlayWaitSema == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("Could not create negotiation overlay wait sema: CreateSemaphore error was %08X (%s)", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    ch.mainWaitSema = CreateSemaphoreA(nullptr, 0, 1, NegotiationChannels::mainWaitSemaName);
    if(ch.mainWaitSema == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("Could not create negotiation main wait sema: CreateSemaphore error was %08X (%s)", lastError, messageBuf).c_str());
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
            OverlaysLayerNoObjectInfo, fmt("Could not initialize the RPC mutex: CreateMutex error was %d (%s)", lastError, messageBuf).c_str());
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
            OverlaysLayerNoObjectInfo, fmt("Could not initialize the RPC shmem: CreateFileMappingA error was %08X (%s)", lastError, messageBuf).c_str());
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
            OverlaysLayerNoObjectInfo, fmt("Could not get the RPC shmem: MapViewOfFile error was %08X (%s)", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false; 
    }

    ch.overlayRequestSema = CreateSemaphoreA(nullptr, 0, 1, fmt(RPCChannels::overlayRequestSemaNameTemplate, overlayId).c_str());
    if(ch.overlayRequestSema == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("Could not create RPC overlay request sema: CreateSemaphore error was %08X (%s)", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    ch.mainResponseSema = CreateSemaphoreA(nullptr, 0, 1, fmt(RPCChannels::mainResponseSemaNameTemplate, overlayId).c_str());
    if(ch.mainResponseSema == NULL) {
        DWORD lastError = GetLastError();
        LPVOID messageBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuf, 0, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("Could not create RPC main response sema: CreateSemaphore error was %08X (%s)", lastError, messageBuf).c_str());
        LocalFree(messageBuf);
        return false;
    }

    return true;
}


std::unordered_map<DWORD, ConnectionToOverlay::Ptr> gConnectionsToOverlayByProcessId;
std::vector<ConnectionToOverlay::Ptr> gConnectionsToOverlayInDepthOrder;
std::recursive_mutex gConnectionsToOverlayByProcessIdMutex;

// Assumes exclusive access to parameters, so lock around this if necessary
void SortOverlaysByPriority(const std::unordered_map<DWORD, ConnectionToOverlay::Ptr>& connectionsToOverlayByProcessId, 
    std::vector<ConnectionToOverlay::Ptr>& connectionsToOverlayInDepthOrder)
{
    connectionsToOverlayInDepthOrder.clear();
    for(auto [processId, conn]: connectionsToOverlayByProcessId) {
        auto l = conn->GetLock();
        if(conn->ctx) {
            connectionsToOverlayInDepthOrder.push_back(conn);
        }
    }

    std::sort(connectionsToOverlayInDepthOrder.begin(), connectionsToOverlayInDepthOrder.end(), [](const ConnectionToOverlay::Ptr &a, const ConnectionToOverlay::Ptr &b){ auto la = a->GetLock(); auto la2 = a->ctx->GetLock(); auto lb = b->GetLock(); auto lb2 = b->ctx->GetLock(); return a->ctx->sessionLayersPlacement < b->ctx->sessionLayersPlacement; });
}


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
            OverlaysLayerNoObjectInfo, fmt("xrCreateSession for overlay session had different flags (%08X) than those with which main session was created (%08X). Effect is unknown, proceeding anyway.", createInfo->createFlags, sessionInfo->createInfo->createFlags).c_str());
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
                        OverlaysLayerNoObjectInfo, "xrCreateSession for overlay session specified a XrGraphicsBindingD3D11KHR but main session did not.");
                    return XR_ERROR_INITIALIZATION_FAILED;
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
                    OverlaysLayerNoObjectInfo, fmt("xrCreateSession for an overlay session used a struct (%s) which the Overlay API Layer does not know how to check.", structureTypeName).c_str());
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
                OverlaysLayerNoObjectInfo, fmt("xrCreateSession for the main session used a struct (%s) which the overlay session did not. Effect is unknown, proceeding anyway.", structureTypeName).c_str());
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
            OverlaysLayerNoObjectInfo, fmt("xrCreateSession for the overlay session used an XrFormFactor (%s) which the main session did not.", formFactor).c_str());
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
                    OverlaysLayerNoObjectInfo, fmt("xrCreateInstance for the parent of the overlay session specified an extension (%s) which the main session did not.", instanceCreateInfo->enabledExtensionNames[i]).c_str());
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
        SortOverlaysByPriority(gConnectionsToOverlayByProcessId, gConnectionsToOverlayInDepthOrder);
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
            connectionLost = true;

        } else if(result == RPCChannels::WaitResult::WAIT_ERROR) {

            OutputDebugStringA("**OVERLAY** IPC Wait Error\n");
            // DebugBreak();
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

    } while(!connectionLost && !connection->closed);

    {
        std::unique_lock<std::recursive_mutex> m(gConnectionsToOverlayByProcessIdMutex);
        gConnectionsToOverlayByProcessId.erase(connection->conn.otherProcessId);
        SortOverlaysByPriority(gConnectionsToOverlayByProcessId, gConnectionsToOverlayInDepthOrder);
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
            OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
                OverlaysLayerNoObjectInfo, fmt("Could not wait on negotiation sema sema: WaitForMultipleObjects error was %08X (%s)", lastError, messageBuf).c_str());
            // XXX need way to signal main process that thread errored unexpectedly
            LocalFree(messageBuf);
            return;
        }

        if(gNegotiationChannels.params->status != NegotiationParams::SUCCESS) {

            OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrCreateSession",
                OverlaysLayerNoObjectInfo, fmt("The Overlay API Layer in the overlay app has a different version (%u) than in the main app (%u), connection rejected.", gNegotiationChannels.params->overlayLayerBinaryVersion, gNegotiationChannels.params->mainLayerBinaryVersion).c_str());

        } else {

            DWORD overlayProcessId = gNegotiationChannels.params->overlayProcessId;
            RPCChannels channels;

            if(!OpenRPCChannels(gNegotiationChannels.instance, overlayProcessId, overlayProcessId, channels)) {

                OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrCreateSession",
                    OverlaysLayerNoObjectInfo, fmt("Couldn't open RPC channels to overlay app, connection rejected.").c_str());

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
            OverlaysLayerNoObjectInfo, fmt("Could not create overlays negotiation channels").c_str());
        return false;
    }

    DWORD waitresult = WaitForSingleObject(gMainMutexHandle, NegotiationChannels::mutexWaitMillis);
    if (waitresult == WAIT_TIMEOUT) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, fmt("Could not take main mutex sema; is there another main app running?").c_str());
        return false;
    }

    gNegotiationChannels.params->mainProcessId = GetCurrentProcessId();
    gNegotiationChannels.params->mainLayerBinaryVersion = gLayerBinaryVersion;
    gNegotiationChannels.mainNegotiateThreadStop = CreateEventA(nullptr, false, false, nullptr);
    gNegotiationChannels.mainThread = std::thread(MainNegotiateThreadBody);
    gNegotiationChannels.mainThread.detach();

    return true;
}

XrResult OverlaysLayerCreateSessionMain(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session, ID3D11Device *d3d11Device)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

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
    info->localHandle = *session;
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

    // create placeholder Actions

    XrActionSetCreateInfo createActionSetInfo { XR_TYPE_ACTION_SET_CREATE_INFO, nullptr, "overlaysapilayer", "overlays API layer synthetic actionset", 1 };
    XrResult result2 = instanceInfo->downchain->CreateActionSet(instance, &createActionSetInfo, &info->placeholderActionSet);
    if(result2 != XR_SUCCESS) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("Could not create session placeholder ActionSet.").c_str());
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
        XrPath subactionPath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(id.subActionString);
        createActionInfo.subactionPaths = &subactionPath;

        XrAction action;
        XrResult result2 = instanceInfo->downchain->CreateAction(info->placeholderActionSet, &createActionInfo, &action);
        if(result2 != XR_SUCCESS) {
            OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
                OverlaysLayerNoObjectInfo, fmt("Could not create session placeholder action for %s.", id.name).c_str());
            return XR_ERROR_INITIALIZATION_FAILED;
        }

        XrPath fullBindingPath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(id.fullBindingString);
        XrPath interactionProfilePath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(id.interactionProfileString);

        info->placeholderActions.insert({fullBindingPath, {action, id.type}});
        info->placeholderActionsByProfileAndFullBinding.insert({{interactionProfilePath, fullBindingPath}, {action, id.type}});
        info->placeholderActionNames.insert({action, id.name});

        info->bindingsByProfile[interactionProfilePath].push_back({action, fullBindingPath});
        info->bindingsByAction[action] = fullBindingPath;
    }

    for(XrPath p: instanceInfo->OverlaysLayerAllSubactionPaths) {
        info->currentInteractionProfileBySubactionPath.insert({p, XR_NULL_PATH});
    }

    OverlaysLayerAddHandleInfoForXrSession(localHandle, info);
    instanceInfo->childSessions.insert(info);

    bool result = CreateMainSessionNegotiateThread(instance, localHandle);

    if(!result) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("Could not initialize the Main App listener thread.").c_str());
        return XR_ERROR_INITIALIZATION_FAILED;
    }
    return xrresult;
}

ConnectionToMain::Ptr gConnectionToMain;

bool ConnectToMain(XrInstance instance)
{
    // check to make sure not already main session process
    if(gMainSessionInstance != XR_NULL_HANDLE) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, "attempt to make an overlay session while also having a main session");
        return false;
    }

    gConnectionToMain = std::make_shared<ConnectionToMain>();

    if(!OpenNegotiationChannels(instance, gNegotiationChannels)) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, "could not open negotiation channels");
        return false;
    }

    DWORD result;
    int attempts = 0;
    do {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("Attempt #%d (of %d) to connect to the main app", attempts, NegotiationChannels::maxAttempts).c_str());
        result = WaitForSingleObject(gNegotiationChannels.overlayWaitSema, NegotiationChannels::negotiationWaitMillis);
        attempts++;
    } while(attempts < NegotiationChannels::maxAttempts && result == WAIT_TIMEOUT);

    if(result == WAIT_TIMEOUT) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("the Overlay API Layer in the overlay app could not connect to the main app after %d tries.", attempts).c_str());
        return false;
    }

    OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, "xrCreateSession", 
        OverlaysLayerNoObjectInfo, fmt("connected to the main app after %d %s.", attempts, (attempts < 2) ? "try" : "tries").c_str());

    if(gNegotiationChannels.params->mainLayerBinaryVersion != gLayerBinaryVersion) {
        gNegotiationChannels.params->status = NegotiationParams::DIFFERENT_BINARY_VERSION;
        ReleaseSemaphore(gNegotiationChannels.mainWaitSema, 1, nullptr);
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", 
            OverlaysLayerNoObjectInfo, fmt("The Overlay API Layer in the overlay app has a different version (%u) than in the main app (%u).").c_str());
        return false;
    }

    /* save off negotiation parameters because they may be overwritten at any time after we Release mainWait */
    gMainProcessId = gNegotiationChannels.params->mainProcessId;
    gNegotiationChannels.params->overlayProcessId = GetCurrentProcessId();
    gNegotiationChannels.params->status = NegotiationParams::SUCCESS;

    ReleaseSemaphore(gNegotiationChannels.mainWaitSema, 1, nullptr);

    if(!OpenRPCChannels(gNegotiationChannels.instance, gMainProcessId, GetCurrentProcessId(), gConnectionToMain->conn)) {
        OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, "Couldn't open RPC channels to main app, connection failed.");
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
        OverlaysLayerLogMessage(gNegotiationChannels.instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession",
            OverlaysLayerNoObjectInfo, "Couldn't connect to main app.");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Get our tracked information on this XrInstance 
    OverlaysLayerXrInstanceHandleInfo::Ptr instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);

    XrFormFactor formFactor;

    {
        // XXX REALLY SHOULD RPC TO GET THE REMOTE SYSTEMID HERE AND THEN RESTORE THAT IN THE OVERLAY BEFORE THE RPC.
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
    info->localHandle = *session;
    info->isProxied = true;
    info->d3d11Device = d3d11Device;

    for(XrPath p: instanceInfo->OverlaysLayerAllSubactionPaths) {
        info->currentInteractionProfileBySubactionPath.insert({p, XR_NULL_PATH});
    }

    OverlaysLayerAddHandleInfoForXrSession(localHandle, info);
    instanceInfo->childSessions.insert(info);

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

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSession", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;
    }
}

XrResult OverlaysLayerCreateSwapchainMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain, uint32_t *swapchainCount)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

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
            OverlaysLayerNoObjectInfo, "Couldn't call EnumerateSwapchainImages to get swapchain image count.");
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
            OverlaysLayerNoObjectInfo, "Couldn't call EnumerateSwapchainImages to get swapchain images.");
        return result;
    }

    for(uint32_t i = 0; i < count; i++) {
        swapchainTextures[i] = swapchainImages[i].texture;
    }

    *swapchainCount = count;

    OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = std::make_shared<OverlaysLayerXrSwapchainHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);
    swapchainInfo->mainAsOverlaySwapchain = std::make_shared<SwapchainCachedData>(*swapchain, swapchainTextures);
    swapchainInfo->actualHandle = actualHandle;
    swapchainInfo->localHandle = localHandle;

    OverlaysLayerAddHandleInfoForXrSwapchain(*swapchain, swapchainInfo);

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
    swapchainInfo->localHandle = localHandle;
    swapchainInfo->isProxied = true;

    OverlaySwapchain::Ptr overlaySwapchain = std::make_shared<OverlaySwapchain>(*swapchain, swapchainCount, createInfo);
    swapchainInfo->overlaySwapchain = overlaySwapchain;

    if(!overlaySwapchain->CreateTextures(instance, sessionInfo->d3d11Device, gConnectionToMain->conn.otherProcessId)) {
        OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateSwapchain",
            OverlaysLayerNoObjectInfo, "Couldn't create D3D local resources for swapchain images");
        // XXX This leaks the session in main process if the Session is not closed.
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    OverlaysLayerAddHandleInfoForXrSwapchain(*swapchain, swapchainInfo);

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

    // XXX anything here?

    return result;
}


XrResult OverlaysLayerCreateReferenceSpaceMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

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
    spaceInfo->localHandle = localHandle;
    spaceInfo->spaceType = SPACE_REFERENCE;

    OverlaysLayerAddHandleInfoForXrSpace(*space, spaceInfo);

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
    spaceInfo->localHandle = localHandle;
    spaceInfo->spaceType = SPACE_REFERENCE;
    spaceInfo->isProxied = true;

    OverlaysLayerAddHandleInfoForXrSpace(*space, spaceInfo);

    return result;
}

XrResult OverlaysLayerEnumerateReferenceSpacesMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

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
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

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
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    XrResult result = XR_SUCCESS;

    auto spaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(space);
    auto baseSpaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(baseSpace);
    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(spaceInfo->parentHandle);

    if(spaceInfo->spaceType == SPACE_ACTION) {


        XrActiveActionSet activeActionSet { sessionInfo->placeholderActionSet, XR_NULL_PATH };
        XrActionsSyncInfo syncInfo { XR_TYPE_ACTIONS_SYNC_INFO, nullptr, 1, &activeActionSet };
        auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(spaceInfo->parentHandle);
        {
            auto syncActionsLock = GetSyncActionsLock();

            result = spaceInfo->downchain->SyncActions(sessionInfo->actualHandle, &syncInfo);
            if(result != XR_SUCCESS) {
                OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrLocateSpace",
                    OverlaysLayerNoObjectInfo, "failed to SyncActions on placeHolder ActionSets to locate a space");
                return result;
            }

            result = spaceInfo->downchain->LocateSpace(spaceInfo->actualHandle, baseSpaceInfo->actualHandle, time, location);
        }

    } else /* SPACE_REFERENCE */ {

        result = spaceInfo->downchain->LocateSpace(spaceInfo->actualHandle, baseSpaceInfo->actualHandle, time, location);
    }

    if(result == XR_SUCCESS) {
        SubstituteLocalHandles(spaceInfo->parentInstance, (XrBaseOutStructure *)location);
    }

    return result;
}

// XXX PUNT - if space was created with subactionPath NULL_PATH, this will probably fail or crash.
bool SynchronizeActionSpaceWithMain(XrInstance instance, XrSpace space)
{
    auto spaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(space);
    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(spaceInfo->parentHandle);
    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(sessionInfo->parentInstance);

    if(sessionInfo->currentInteractionProfileBySubactionPath.count(spaceInfo->actionSpaceCreateInfo->subactionPath) == 0) {
        OverlaysLayerLogMessage(spaceInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrLocateSpace",
            OverlaysLayerNoObjectInfo, "Don't have a current interaction profile for an XrSpace's subactionPath; XrSpace will not be locatable; is the main app using an interaction profile with no suggestions by overlay app?");
        return false;
    }
    XrPath currentInteractionProfile = sessionInfo->currentInteractionProfileBySubactionPath.at(spaceInfo->actionSpaceCreateInfo->subactionPath);

    if((spaceInfo->actualHandle == XR_NULL_HANDLE) || (spaceInfo->createdWithInteractionProfile != currentInteractionProfile)) {

        if(spaceInfo->actualHandle != XR_NULL_HANDLE) {
            RPCCallDestroySpace(instance, spaceInfo->actualHandle);
        }

        auto actionInfo = spaceInfo->action;

        // XXX what if SuggestProfileBindings was never called?  It's not an error.
        XrPath matchingBinding = XR_NULL_PATH;
        XrPath matchingProfile = currentInteractionProfile;
        bool found = false;
        if (actionInfo->suggestedBindingsByProfile.count(currentInteractionProfile) == 0) {
            OverlaysLayerLogMessage(spaceInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrLocateSpace",
                OverlaysLayerNoObjectInfo, "Couldn't find a suggested binding matching the interaction profile for an Action; XrSpace will not be locatable; are controllers turned on?");
            return false;
        }

        for(XrPath binding : actionInfo->suggestedBindingsByProfile.at(currentInteractionProfile)) {
            XrPath subactionPath = instanceInfo->OverlaysLayerBindingToSubaction.at(binding); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
            if(subactionPath == spaceInfo->actionSpaceCreateInfo->subactionPath) {
                matchingBinding = binding;
                found = true;
            }
        }

        if(!found) {

            OverlaysLayerLogMessage(spaceInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrAttachSessionActionSets",
                OverlaysLayerNoObjectInfo, "Couldn't find a suggested binding matching the subaction with which an XrSpace was created; XrSpace will not be locatable");

            return false;

        }

        WellKnownStringIndex bindingString = instanceInfo->OverlaysLayerPathToWellKnownString.at(matchingBinding); // These two .at()s must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
        WellKnownStringIndex profileString = instanceInfo->OverlaysLayerPathToWellKnownString.at(matchingProfile);

        XrResult result = RPCCallCreateActionSpaceFromBinding(spaceInfo->parentInstance, sessionInfo->actualHandle, profileString, bindingString, &spaceInfo->actionSpaceCreateInfo->poseInActionSpace, &spaceInfo->actualHandle);

        if(result != XR_SUCCESS) {
            OverlaysLayerLogMessage(spaceInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrAttachSessionActionSets",
                OverlaysLayerNoObjectInfo, "Couldn't create XrSpace for Action from main placeholders");
            return false;
        }

        spaceInfo->createdWithInteractionProfile = matchingProfile;
    }

    return true;
}

XrResult OverlaysLayerLocateSpaceOverlay(XrInstance instance, XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
{
    auto spaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(space);
    auto baseSpaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(baseSpace);

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

            if(!SynchronizeActionSpaceWithMain(instance, space)) {
                // We couldn't get a placeholderAction in Main for this ActionSpace, so return not locatable
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


XrResult OverlaysLayerLocateSpaceMain(XrInstance parentInstance, XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    XrResult result = XR_SUCCESS;

    auto spaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(space);
    auto baseSpaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(baseSpace);
    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(spaceInfo->parentHandle);

    if(spaceInfo->spaceType == SPACE_ACTION) {

        // Sync this Space's Action so it's active
        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(spaceInfo->action->parentHandle);
        // XXX may need to keep XrActionsSyncInfo from previous xrSyncActions and play that back
        XrActiveActionSet activeActionSet { actionSetInfo->handle, spaceInfo->actionSpaceCreateInfo->subactionPath };
        XrActionsSyncInfo syncInfo { XR_TYPE_ACTIONS_SYNC_INFO, nullptr, 1, &activeActionSet };
        { 
            auto syncActionsLock = GetSyncActionsLock();

            result = spaceInfo->downchain->SyncActions(sessionInfo->actualHandle, &syncInfo);
            if(result != XR_SUCCESS) {
                OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrLocateSpace",
                        OverlaysLayerNoObjectInfo, "failed to SyncActions before reading placeHolder action to locate a space");
                return result;
            }

            result = spaceInfo->downchain->LocateSpace(spaceInfo->actualHandle, baseSpaceInfo->actualHandle, time, location);
        }

    } else /* SPACE_REFERENCE */ {

        result = spaceInfo->downchain->LocateSpace(spaceInfo->actualHandle, baseSpaceInfo->actualHandle, time, location);
    }
    
    if(result == XR_SUCCESS) {
        SubstituteLocalHandles(spaceInfo->parentInstance, location);
    }

    return result;
}


XrResult OverlaysLayerLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
{
    try {

        auto spaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(space);

        bool isProxied = spaceInfo->isProxied;
        XrResult result;
        if(isProxied) {
            result = OverlaysLayerLocateSpaceOverlay(spaceInfo->parentInstance, space, baseSpace, time, location);
        } else {
            result = OverlaysLayerLocateSpaceMain(spaceInfo->parentInstance, space, baseSpace, time, location);
        }

        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrLocateSpace", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}


XrResult OverlaysLayerDestroySpaceMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSpace space)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(space);

    // XXX This will need to be smart about ActionSpaces?

    OverlaysLayerRemoveXrSpaceHandleInfo(space);

    return XR_SUCCESS;
}

XrResult OverlaysLayerDestroySpaceOverlay(XrInstance instance, XrSpace space)
{
    OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = OverlaysLayerGetHandleInfoFromXrSpace(space);

    // XXX This will need to be smart about ActionSpaces?

    XrResult result = RPCCallDestroySpace(instance, spaceInfo->actualHandle);

    OverlaysLayerRemoveXrSpaceHandleInfo(space);

    return result;
}

XrResult OverlaysLayerDestroyActionSetOverlay(XrInstance instance, XrActionSet actionSet)
{
    OverlaysLayerRemoveXrActionSetHandleInfo(actionSet);

    return XR_SUCCESS;
}

XrResult OverlaysLayerDestroyActionOverlay(XrInstance instance, XrAction action)
{
    OverlaysLayerRemoveXrActionHandleInfo(action);

    return XR_SUCCESS;
}

XrResult OverlaysLayerLocateViewsMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

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

    OverlaysLayerRemoveXrSessionHandleInfo(session);

    return result;
}

XrResult OverlaysLayerEnumerateSwapchainFormatsMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

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
            OverlaysLayerNoObjectInfo, fmt("images structure type is %s and not XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR.", structTypeName).c_str());
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
        auto l2 = connection->ctx->GetLock();
        pendingStateChange = connection->ctx->sessionState.GetAndDoPendingStateChange(&mainSessionContext->sessionState);
    }

    if(pendingStateChange.first) {

        auto* ssc = reinterpret_cast<XrEventDataSessionStateChanged*>(eventData);
        ssc->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
        ssc->next = nullptr;

        {
            MainSessionContext::Ptr mainSessionContext = gMainSessionContext;
            auto l = mainSessionContext->GetLock();
            auto l2 = connection->ctx->GetLock();
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
                OverlaysLayerNoObjectInfo, "Enqueued a lost event.");

        } else {

            overlay->eventsSaved.push(newEvent);
            
        }
    }
}

XrResult OverlaysLayerPollEvent(XrInstance instance, XrEventDataBuffer* eventData)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    try {

        // See if any Session needs to return a synthetic interaction profile changed event
        std::unique_lock<std::recursive_mutex> lock(gOverlaysLayerXrSessionToHandleInfoMutex);
        for(auto [sessionHandle, sessionInfo]: gOverlaysLayerXrSessionToHandleInfo) {
            auto l = sessionInfo->GetLock();
            if(sessionInfo->interactionProfileChangePending) {
                auto* ipc = reinterpret_cast<XrEventDataInteractionProfileChanged*>(eventData);
                ipc->type = XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED;
                ipc->next = nullptr;
                ipc->session = sessionHandle;
                sessionInfo->interactionProfileChangePending = false;
                return XR_SUCCESS;
            }
        }

        auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);

        XrResult result = instanceInfo->downchain->PollEvent(instance, eventData);

        if(result == XR_EVENT_UNAVAILABLE) {

            if(gConnectionToMain) {


                /* Overlay app: See if the main process has any events for us */
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

            if(eventData->type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {

                auto* ipc = reinterpret_cast<XrEventDataInteractionProfileChanged*>(eventData);
                ipc->type = XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED;
                ipc->next = nullptr;
                MainSessionContext::Ptr mainSessionContext = gMainSessionContext;
                auto l = mainSessionContext->GetLock();
                if(ipc->session == mainSessionContext->session) {
                    // Discard on the ground since we enqueued to synthesize an event in SyncActions
                    result = XR_EVENT_UNAVAILABLE;
                }

            } else if(eventData->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {

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
                        OverlaysLayerNoObjectInfo, fmt("xrPollEvent filled a struct (%s) which the Overlay API Layer does not know how to check.", structureTypeName).c_str());
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

                    } /* could receive a XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED but we synthesize those in SyncActions */
                }
            }
        }

        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrPollEvent", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;
    }
}

XrResult OverlaysLayerBeginSessionMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrSessionBeginInfo* beginInfo)
{
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    auto l = connection->GetLock();
    // auto beginInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrCreateSwapchain", beginInfo);

    auto l2 = connection->ctx->GetLock();
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
    auto l2 = connection->ctx->GetLock();
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
    auto l2 = connection->ctx->GetLock();

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
	{
		auto l = connection->GetLock();
		auto l2 = connection->ctx->GetLock();

		if (!connection->ctx->relaxedDisplayTime) {
			// XXX tell main we are waiting by setting a variable in ctx and then wait on a semaphore
		}
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
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

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
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

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
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

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

	XrResult result = XR_SUCCESS;
    {
        std::unique_lock<std::recursive_mutex> HapticQuirkLock(HapticQuirkMutex);
        result = swapchainInfo->downchain->ReleaseSwapchainImage(swapchainInfo->actualHandle, releaseInfoCopy.get());
        if(result != XR_SUCCESS) DebugBreak(); // XXX
    }

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
        DebugBreak(); // XXX
        return result;
    }

    swapchainInfo->overlaySwapchain->waited = false;

    return result;
}

XrResult OverlaysLayerEndFrameMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrFrameEndInfo* frameEndInfo)
{
    std::unique_lock<std::recursive_mutex> EndFrameLock(EndFrameMutex);
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

void AddSwapchainsFromLayers(OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo, std::shared_ptr<const XrCompositionLayerBaseHeader> p, std::set<std::shared_ptr<OverlaysLayerXrSwapchainHandleInfo>> swapchains)
{
    switch(p->type) {
        case XR_TYPE_COMPOSITION_LAYER_QUAD: {
            auto p2 = reinterpret_cast<const XrCompositionLayerQuad*>(p.get());
            OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(p2->subImage.swapchain);
            swapchains.insert(swapchainInfo);
            break;
        }
        case XR_TYPE_COMPOSITION_LAYER_PROJECTION: {
            auto p2 = reinterpret_cast<const XrCompositionLayerProjection*>(p.get());
            for(uint32_t j = 0; j < p2->viewCount; j++) {
                OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(p2->views[j].subImage.swapchain);
                swapchains.insert(swapchainInfo);
            }
            break;
        }
        case XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR: {
            auto p2 = reinterpret_cast<const XrCompositionLayerDepthInfoKHR*>(p.get());
            OverlaysLayerXrSwapchainHandleInfo::Ptr swapchainInfo = OverlaysLayerGetHandleInfoFromXrSwapchain(p2->subImage.swapchain);
            swapchains.insert(swapchainInfo);
            break;
        }
        default: {
            char structureTypeName[XR_MAX_STRUCTURE_NAME_SIZE];
            auto sessLock = sessionInfo->GetLock();
            XrResult r = sessionInfo->downchain->StructureTypeToString(sessionInfo->parentInstance, p->type, structureTypeName);
            if(r != XR_SUCCESS) {
                sprintf(structureTypeName, "(type %08X)", p->type);
            }

            OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrEndFrame",
                OverlaysLayerNoObjectInfo, fmt("a compositiion layer was provided of a type (%s) which the Overlay API Layer does not know how to check; will not be added to swapchains protected while submitted.  A crash may result.", structureTypeName).c_str());
            break;
        }
    }
}

XrResult OverlaysLayerEndFrameMain(XrInstance parentInstance, XrSession session, const XrFrameEndInfo* frameEndInfo)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    std::unique_lock<std::recursive_mutex> EndFrameLock(EndFrameMutex);
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    // combine overlay and main layers

    std::vector<const XrCompositionLayerBaseHeader*> layersMerged;

    for(uint32_t i = 0; i < frameEndInfo->layerCount; i++) {
        layersMerged.push_back(frameEndInfo->layers[i]);
    }

    std::set<std::shared_ptr<OverlaysLayerXrSwapchainHandleInfo>> swapchainsInFlight;

    {
        std::unique_lock<std::recursive_mutex> connectionLock(gConnectionsToOverlayByProcessIdMutex);
        if(!gConnectionsToOverlayInDepthOrder.empty()) {
            for(auto& overlayconn: gConnectionsToOverlayInDepthOrder) {
                connectionLock.unlock();
                auto lock = overlayconn->GetLock();
                if(overlayconn->ctx) {
                    auto lock2 = overlayconn->ctx->GetLock();
                    for(uint32_t i = 0; i < overlayconn->ctx->overlayLayers.size(); i++) {
                        AddSwapchainsFromLayers(sessionInfo, overlayconn->ctx->overlayLayers[i], swapchainsInFlight);
                        layersMerged.push_back(overlayconn->ctx->overlayLayers[i].get());
                    }
                }
                connectionLock.lock();
            }
        }
    }
    
    {
        auto mainSession = gMainSessionContext;
        auto lock2 = mainSession->GetLock();
        mainSession->swapchainsInFlight = swapchainsInFlight;
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
        auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

        auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
        
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

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrEndFrame", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;
    }
}


// We don't know until Attach whether this is proxied to main or not, so have to
// make it now as if it will be in main
XrResult OverlaysLayerCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet)
{
    try {
        auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

        auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);

        XrResult result = instanceInfo->downchain->CreateActionSet(instance, createInfo, actionSet);

        if(result == XR_SUCCESS) {
            OverlaysLayerXrActionSetHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrActionSetHandleInfo>(instance, instance, instanceInfo->downchain);
            info->createInfo = reinterpret_cast<XrActionSetCreateInfo*>(CopyXrStructChainWithMalloc(instance, createInfo));
            info->handle = *actionSet;

            OverlaysLayerAddHandleInfoForXrActionSet(*actionSet, info);

            instanceInfo->childActionSets.insert(info);
        }

        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateActionSet", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

// We don't know until Attach whether this is proxied to main or not, so have to
// make it now as if it will be in main
XrResult OverlaysLayerCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action)
{
    try {
        auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(actionSet);

        XrResult result = actionSetInfo->downchain->CreateAction(actionSet, createInfo, action);

        if(result == XR_SUCCESS) {
            OverlaysLayerXrActionHandleInfo::Ptr info = std::make_shared<OverlaysLayerXrActionHandleInfo>(actionSet, actionSetInfo->parentInstance, actionSetInfo->downchain);
            info->createInfo = reinterpret_cast<XrActionCreateInfo*>(CopyXrStructChainWithMalloc(actionSetInfo->parentInstance, createInfo));
            info->handle = *action;
            if(info->createInfo->countSubactionPaths > 0) {
                // Make placeholders for subactionPaths requested for filtering
                info->subactionPaths.insert(info->createInfo->subactionPaths, info->createInfo->subactionPaths + info->createInfo->countSubactionPaths);
            }
            // Make sure Get on XR_NULL_PATH always succeeds, it will merge all valid subactionPath state
            info->subactionPaths.insert(XR_NULL_PATH);

            actionSetInfo->childActions.insert(info);

            OverlaysLayerAddHandleInfoForXrAction(*action, info);
        }

        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateAction", OverlaysLayerNoObjectInfo, e.what());
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
            auto found = instanceInfo->OverlaysLayerPathToWellKnownString.find(it.binding);
            if(found == instanceInfo->OverlaysLayerPathToWellKnownString.end()) {
                OverlaysLayerLogMessage(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "xrSuggestInteractionProfileBindings",
                    OverlaysLayerNoObjectInfo,
                    fmt("Application suggested binding \"%s\", which this API layer does not know; binding will be ignored", PathToString(instance, it.binding).c_str()).c_str());
            }
        }

        return XR_SUCCESS;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrSuggestInteractionProfileBindings", OverlaysLayerNoObjectInfo, e.what());
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
    spaceInfo->localHandle = *space;
    spaceInfo->action = actionInfo;
    spaceInfo->isProxied = true;

    std::shared_ptr<const XrActionSpaceCreateInfo> createInfoCopy(reinterpret_cast<const XrActionSpaceCreateInfo*>(CopyXrStructChainWithMalloc(sessionInfo->parentInstance, createInfo)), [instance=sessionInfo->parentInstance](const XrActionSpaceCreateInfo* p){ FreeXrStructChainWithFree(instance, p);});
    spaceInfo->actionSpaceCreateInfo = createInfoCopy;

    OverlaysLayerAddHandleInfoForXrSpace(*space, spaceInfo);

    return XR_SUCCESS;
}

XrResult OverlaysLayerCreateActionSpaceMain(XrInstance parentInstance, XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    XrResult result = XR_SUCCESS;

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

    // restore the actual handle
    XrSession localHandleStore = session;
    session = sessionInfo->actualHandle;

    auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(createInfo->action);
    XrActionSpaceCreateInfo createInfo2 = *createInfo;
    createInfo2.action = actionInfo->handle;

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

        std::shared_ptr<const XrActionSpaceCreateInfo> createInfoCopy(reinterpret_cast<const XrActionSpaceCreateInfo*>(CopyXrStructChainWithMalloc(sessionInfo->parentInstance, createInfo)), [instance=sessionInfo->parentInstance](const XrActionSpaceCreateInfo* p){ FreeXrStructChainWithFree(instance, p);});
        OverlaysLayerXrSpaceHandleInfo::Ptr spaceInfo = std::make_shared<OverlaysLayerXrSpaceHandleInfo>(session, sessionInfo->parentInstance, sessionInfo->downchain);
        spaceInfo->spaceType = SPACE_ACTION;
        spaceInfo->actualHandle = actualHandle;
        spaceInfo->localHandle = *space;
        spaceInfo->action = actionInfo;
        spaceInfo->isProxied = false;
        spaceInfo->actionSpaceCreateInfo = createInfoCopy;

        OverlaysLayerAddHandleInfoForXrSpace(*space, spaceInfo);
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
            auto l = sessionInfo->GetLock();
            sessionInfo->childSpaces.insert(OverlaysLayerGetHandleInfoFromXrSpace(*space));
        }

        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrCreateActionSpace", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

XrResult OverlaysLayerCreateActionSpaceFromBinding(ConnectionToOverlay::Ptr connection, XrSession session, WellKnownStringIndex profileString, WellKnownStringIndex bindingString, const XrPosef* poseInActionSpace, XrSpace *space)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session); 
    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(sessionInfo->parentInstance);

    XrPath bindingPath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(bindingString); // These two .at()s must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
    XrPath profilePath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(profileString);

    XrAction actualActionHandle = sessionInfo->placeholderActionsByProfileAndFullBinding.at({profilePath, bindingPath}).first; // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support

    XrActionSpaceCreateInfo createInfo { XR_TYPE_ACTION_SPACE_CREATE_INFO };
    createInfo.action = actualActionHandle;
    createInfo.subactionPath = instanceInfo->OverlaysLayerBindingToSubaction.at(bindingPath); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
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
        spaceInfo->placeholderAction = actualActionHandle;
        // spaceInfo->isProxied; // Should never be accessed from MainAsOverlay

        OverlaysLayerAddHandleInfoForXrSpace(*space, spaceInfo);
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
            actionInfo->suggestedBindingsByProfile[interactionProfile].insert(binding.binding);
        }

        if(bindings.size() > 0) {
            sessionInfo->interactionProfiles.insert(interactionProfile);
        }
    }

    sessionInfo->actionSetsWereAttached = true;
    return XR_SUCCESS;
}

XrResult OverlaysLayerAttachSessionActionSetsMain(XrInstance parentInstance, XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

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

        if(bindings.size() > 0) {
            sessionInfo->interactionProfiles.insert(interactionProfile);
        }

        for(auto binding: bindings) {
            auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(binding.action);
            actionInfo->suggestedBindingsByProfile[interactionProfile].insert(binding.binding);
        }

        XrInteractionProfileSuggestedBinding suggestedBindings { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
        suggestedBindings.interactionProfile = interactionProfile;
        suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
        suggestedBindings.suggestedBindings = bindings.data();

        auto suggestedBindingsCopy = GetSharedCopyHandlesRestored(parentInstance, "xrAttachSessionActionSets", &suggestedBindings);

        // merge main app's suggested bindings and our suggested bindings - our Actions are actual runtime Handles, so add after Restoring
        std::vector<XrActionSuggestedBinding> newBindings(suggestedBindingsCopy->suggestedBindings, suggestedBindingsCopy->suggestedBindings + suggestedBindingsCopy->countSuggestedBindings);

        {
            for (const auto& actionBinding : sessionInfo->bindingsByProfile.at(interactionProfile)) { // This must succeed; interactionProfile is from suggested bindings and adding new binding paths would require enabling an extension which API Layer doesn't support
                newBindings.push_back(actionBinding);
                XrPath binding = actionBinding.binding;
                XrPath recorded = sessionInfo->bindingsByAction[actionBinding.action];
                OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrAttachSessionActionSets",
                    OverlaysLayerNoObjectInfo,
                    fmt("Suggested \"%s\" for placeholder action \"%s\"", PathToString(sessionInfo->parentInstance, binding).c_str(), PathToString(sessionInfo->parentInstance, recorded).c_str()).c_str());
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
                fmt("Couldn't apply deferred suggested interaction profile bindings, SuggestInteractionProfileBindings returned %d", result).c_str());
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

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrAttachSessionActionSets", OverlaysLayerNoObjectInfo, e.what());
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

XrResult GetActionStates(XrSession session, const ActionGetInfoList& actionsToGet, ActionStateUnion *states)
{
    XrResult result = XR_SUCCESS;

    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

	auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

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
                    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrSyncActions", OverlaysLayerNoObjectInfo, "Couldn't get state in bulk update");
                    return result;
                }
                break;
            }
            case XR_ACTION_TYPE_FLOAT_INPUT: {
                auto *state = reinterpret_cast<XrActionStateFloat*>(&states[index]);
                state->type = XR_TYPE_ACTION_STATE_FLOAT;
                state->next = nullptr;
                result = sessionInfo->downchain->GetActionStateFloat(sessionInfo->actualHandle, &get, state);
                if(result != XR_SUCCESS) {
                    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrSyncActions", OverlaysLayerNoObjectInfo, "Couldn't get state in bulk update");
                    return result;
                }
                break;
            }
            case XR_ACTION_TYPE_VECTOR2F_INPUT: {
                auto *state = reinterpret_cast<XrActionStateVector2f*>(&states[index]);
                state->type = XR_TYPE_ACTION_STATE_VECTOR2F;
                state->next = nullptr;
                result = sessionInfo->downchain->GetActionStateVector2f(sessionInfo->actualHandle, &get, state);
                if(result != XR_SUCCESS) {
                    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrSyncActions", OverlaysLayerNoObjectInfo, "Couldn't get state in bulk update");
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
                    OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrSyncActions", OverlaysLayerNoObjectInfo, "Couldn't get state in bulk update");
                    return result;
                }
                break;
            }
        }
        index ++;
    }

    return result;
}

XrResult OverlaysLayerGetCurrentInteractionProfile(XrSession session, XrPath topLevelUserPath, XrInteractionProfileState* interactionProfile)
{
    try {

        auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

        auto it = sessionInfo->currentInteractionProfileBySubactionPath.find(topLevelUserPath);
        if(it == sessionInfo->currentInteractionProfileBySubactionPath.end()) {
            return XR_ERROR_PATH_INVALID;
        }
            
        if(sessionInfo->interactionProfiles.count(it->second) > 0) {
            interactionProfile->interactionProfile = it->second;
        } else {
            interactionProfile->interactionProfile = XR_NULL_PATH;
        }
        
        return XR_SUCCESS;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrGetCurrentInteractionProfile", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

XrResult OverlaysLayerSyncActionsAndGetStateMainAsOverlay(
    ConnectionToOverlay::Ptr connection, XrSession session,
    uint32_t countProfileAndBindings, const WellKnownStringIndex *profileStrings, const WellKnownStringIndex *bindingStrings,   /* input is profiles and bindings for which to Get */
    ActionStateUnion *states,                                                                                                   /* output is result of Get */
    uint32_t countSubactionStrings, const WellKnownStringIndex *subactionStrings,                                               /* input is subactionPaths for which to get current interaction Profile */
    WellKnownStringIndex *interactionProfileStrings)                                                                            /* output is current interaction profiles */
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    XrResult result = XR_SUCCESS;

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(sessionInfo->parentInstance);

    XrActiveActionSet activeActionSet { sessionInfo->placeholderActionSet, XR_NULL_PATH };
    XrActionsSyncInfo syncInfo { XR_TYPE_ACTIONS_SYNC_INFO, nullptr, 1, &activeActionSet };

    result = sessionInfo->downchain->SyncActions(sessionInfo->actualHandle, &syncInfo);

    if(result == XR_SESSION_NOT_FOCUSED) {
        return XR_SESSION_NOT_FOCUSED;
    }

    if(result != XR_SUCCESS) {
        return result;
    }

    ActionGetInfoList actionsToGet;

    for(uint32_t i = 0; i < countProfileAndBindings; i++) {
        XrPath profilePath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(profileStrings[i]); // These two at()s must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
        XrPath bindingPath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(bindingStrings[i]);
        auto it = sessionInfo->placeholderActionsByProfileAndFullBinding.find({profilePath, bindingPath});
        if(it == sessionInfo->placeholderActionsByProfileAndFullBinding.end()) {
            DebugBreak();
        }
			
        XrAction action = it->second.first;
        XrActionType type = it->second.second;
        XrPath subactionPath = instanceInfo->OverlaysLayerBindingToSubaction.at(bindingPath); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
        actionsToGet.push_back({ action, type, subactionPath });

        if(false) printf("for %s%s, I think I'm getting action %s\n",
            PathToString(sessionInfo->parentInstance, profilePath).c_str(),
            PathToString(sessionInfo->parentInstance, bindingPath).c_str(),
            sessionInfo->placeholderActionNames.at(action).c_str());
    }
    result = GetActionStates(session, actionsToGet, states);

    if(result != XR_SUCCESS) {
        return result;
    }

    // XXX debug
    if(false) for(uint32_t i = 0; i < countProfileAndBindings; i++) {
        auto got = actionsToGet[i];
        XrPath profilePath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(profileStrings[i]); // This .at() must succeed; it was translated by the overlay side to a well-known string
        XrPath bindingPath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(bindingStrings[i]); // This .at() must succeed; it was translated by the overlay side to a well-known string
        if(got.actionType == XR_ACTION_TYPE_BOOLEAN_INPUT) {
            XrActionStateBoolean *boolean = (XrActionStateBoolean*)&states[i];
            printf("for %s%s, I got for action %s {state = %s, active = %s}\n",
                PathToString(sessionInfo->parentInstance, profilePath).c_str(),
                PathToString(sessionInfo->parentInstance, bindingPath).c_str(),
                sessionInfo->placeholderActionNames.at(got.action).c_str(),
                boolean->currentState ? "true" : "false",
                boolean->isActive ? "true" : "false");
        }
    }

    if(result != XR_SUCCESS) {
        return result;
    }

    for(uint32_t i = 0; i < countSubactionStrings; i++) {
        XrPath p = instanceInfo->OverlaysLayerWellKnownStringToPath.at(subactionStrings[i]); // This .at() must succeed; it was translated by the overlay side to a well-known string
        XrInteractionProfileState interactionProfile { XR_TYPE_INTERACTION_PROFILE_STATE };

        XrResult result2 = sessionInfo->downchain->GetCurrentInteractionProfile(sessionInfo->actualHandle, p, &interactionProfile);

        if(result2 != XR_SUCCESS) {
            OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrSyncActions",
                OverlaysLayerNoObjectInfo,
                fmt("Couldn't get current interaction profile for top-level path \"%s\" in OverlaysLayerSyncActionsAndGetStateMainAsOverlay", PathToString(sessionInfo->parentInstance, p).c_str()).c_str());
            interactionProfileStrings[i] = WellKnownStringIndex::NULL_PATH;
        } else {
            interactionProfileStrings[i] = instanceInfo->OverlaysLayerPathToWellKnownString.at(interactionProfile.interactionProfile); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
        }
    }

    return result;
}

void ClearSessionLastSyncedActiveActionSets(OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo, const XrActionsSyncInfo* syncInfo)
{
    for(auto activeActionSet: sessionInfo->lastSyncedActiveActionSets) {
        XrActionSet actionSet = activeActionSet.actionSet;
        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(actionSet);
        XrPath subactionPath = activeActionSet.subactionPath;
        for(auto actionInfo: actionSetInfo->childActions) {
            actionInfo->stateBySubactionPath.clear();
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
            actionInfo->stateBySubactionPath.clear();
        }
    }
}

void GetPreviousActionStates(XrInstance parentInstance, XrSession session, const XrActionsSyncInfo* syncInfo, std::unordered_map <OverlaysLayerXrActionHandleInfo::Ptr, std::unordered_map<XrPath, ActionStateUnion>> &previousActionStates)
{
    for(uint32_t i = 0; i < syncInfo->countActiveActionSets; i++) {
        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(syncInfo->activeActionSets[i].actionSet);
        for(auto actionInfo: actionSetInfo->childActions) {
            previousActionStates.insert({actionInfo, actionInfo->stateBySubactionPath});
        }
    }
}

XrResult OverlaysLayerSyncActionsOverlay(XrInstance parentInstance, XrSession session, const XrActionsSyncInfo* syncInfo)
{
    XrResult result = XR_SUCCESS;

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(parentInstance);

    // Make queryable data structures for data spread across activeActionSets
    std::set<OverlaysLayerXrActionSetHandleInfo::Ptr> actionSetInfos;
    std::unordered_map<OverlaysLayerXrActionSetHandleInfo::Ptr, std::set<XrPath>> actionSetInfoSubactionPaths;
    std::set<OverlaysLayerXrActionHandleInfo::Ptr> actionInfos;
    std::unordered_map<OverlaysLayerXrActionHandleInfo::Ptr, std::set<XrPath>> actionInfoSubactionPaths;

    for(uint32_t i = 0; i < syncInfo->countActiveActionSets; i++) {
        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(syncInfo->activeActionSets[i].actionSet);
        XrPath subactionPath = syncInfo->activeActionSets[i].subactionPath;
        actionSetInfos.insert(actionSetInfo);
        actionSetInfoSubactionPaths[actionSetInfo].insert(subactionPath);
    }

    for(const auto& [actionSetInfo, subactionPaths] : actionSetInfoSubactionPaths) {
        for(auto actionInfo: actionSetInfo->childActions) {
            actionInfos.insert(actionInfo);
            for(auto subactionPath: subactionPaths) {
                if((subactionPath != XR_NULL_PATH) && (actionInfo->subactionPaths.count(subactionPath) == 0)) {
                    return XR_ERROR_PATH_UNSUPPORTED;
                }
                actionInfoSubactionPaths[actionInfo].insert(subactionPath);
            }
        }
    }

    // Figure out which placeholder actions (interaction profile and binding) to query on Main side
    std::vector<WellKnownStringIndex> profileStrings;
    std::vector<WellKnownStringIndex> fullBindingStrings;
    std::vector<OverlaysLayerXrActionHandleInfo::Ptr> bindingAppliesToActionInfo;
    std::vector<XrPath> bindingMergesToSubactionPath;

    for(const auto& [actionInfo, subactionPaths] : actionInfoSubactionPaths) {

        for(auto [profilePath, fullBindingPaths]: actionInfo->suggestedBindingsByProfile) {

            for(auto fullBindingPath: fullBindingPaths) {

                // XXX really should find() this - could be path from an extension
                XrPath bindingSubactionPath = instanceInfo->OverlaysLayerBindingToSubaction.at(fullBindingPath); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support

                for(auto subactionPath: subactionPaths) {

                    if((subactionPath == XR_NULL_PATH) || (subactionPath == bindingSubactionPath)) {

                        WellKnownStringIndex fullBindingString = instanceInfo->OverlaysLayerPathToWellKnownString.at(fullBindingPath); // These two .at()s must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
                        WellKnownStringIndex profileString = instanceInfo->OverlaysLayerPathToWellKnownString.at(profilePath);

                        // get profile and full path which the main process side of the API layer maps to a placeholder action

                        profileStrings.push_back(profileString);
                        fullBindingStrings.push_back(fullBindingString);
                        bindingAppliesToActionInfo.push_back(actionInfo);
                        bindingMergesToSubactionPath.push_back(bindingSubactionPath);

                        if(PrintDebugInfo) {
                            OverlaysLayerLogMessage(parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrSyncActions",
                                OverlaysLayerNoObjectInfo,
                                fmt("I think I'm probing placeholder \"%s%s\" for an action", OverlaysLayerWellKnownStrings.at(profileString), OverlaysLayerWellKnownStrings.at(fullBindingString)).c_str());
                        }
                    }
                }
            }
        }
    }

    std::vector<ActionStateUnion> states(fullBindingStrings.size());

    std::vector<WellKnownStringIndex> topLevelStrings;
    for(XrPath subactionPath: instanceInfo->OverlaysLayerAllSubactionPaths) {
        topLevelStrings.push_back(instanceInfo->OverlaysLayerPathToWellKnownString.at(subactionPath)); // This .at() must succeed, it was constructed by a table of known strings.
    }

    std::vector<WellKnownStringIndex> currentInteractionProfileStrings(topLevelStrings.size());

    result = RPCCallSyncActionsAndGetState(parentInstance, session, (uint32_t)fullBindingStrings.size(), profileStrings.data(), fullBindingStrings.data(), states.data(), (uint32_t)topLevelStrings.size(), topLevelStrings.data(), currentInteractionProfileStrings.data());

    if(result == XR_SUCCESS) {

        // Save off previous action's states
        std::unordered_map <OverlaysLayerXrActionHandleInfo::Ptr, std::unordered_map<XrPath, ActionStateUnion>> previousActionStates;
        GetPreviousActionStates(sessionInfo->parentInstance, session, syncInfo, previousActionStates);

        // On all actions in previous ActionSet and in this ActionSet, clear state
        ClearSessionLastSyncedActiveActionSets(sessionInfo, syncInfo);
        ClearSyncedActiveActionSets(sessionInfo->parentInstance, session, syncInfo);

        // Merge all fetched state
        for(size_t i = 0; i < fullBindingStrings.size(); i++) {
            OverlaysLayerXrActionHandleInfo::Ptr actionInfo = bindingAppliesToActionInfo.at(i); // at() succeeds; vector lookup
            XrPath subactionPath = bindingMergesToSubactionPath.at(i); // at() succeeds; vector lookup

            if(false)if(actionInfo->createInfo->actionType == XR_ACTION_TYPE_BOOLEAN_INPUT) { // XXX debug
                const std::string& profile = OverlaysLayerWellKnownStrings.at(profileStrings.at(i));
                const std::string& fullBinding = OverlaysLayerWellKnownStrings.at(fullBindingStrings.at(i));
                XrActionStateBoolean* boolean = (XrActionStateBoolean*)&states[i];
                printf("for action \"%s\", subactionPath \"%s\" I fetched %s%s; currentState is %s, active is %s, changedSinceLastSync is %d\n",
                    actionInfo->createInfo->actionName,
                    PathToString(sessionInfo->parentInstance, subactionPath).c_str(),
                    profile.c_str(), fullBinding.c_str(),
                    boolean->currentState ? "true" : "false", boolean->isActive ? "true" : "false",
                    boolean->changedSinceLastSync);
            } 

            /* merge all states that are represented under this subactionPath */
            if(actionInfo->stateBySubactionPath.count(subactionPath) == 0) {
                ActionStateUnion actionStateUnion;
                ClearActionState(actionInfo->createInfo->actionType, &actionStateUnion);
                actionInfo->stateBySubactionPath[subactionPath] = actionStateUnion;
            }
            MergeActionState(actionInfo->createInfo->actionType, &states[i], &actionInfo->stateBySubactionPath.at(subactionPath)); // This at() will succeed because previous if-clause populates it if empty

            /* merge all states */
            if(actionInfo->stateBySubactionPath.count(XR_NULL_PATH) == 0) {
                ActionStateUnion actionStateUnion;
                ClearActionState(actionInfo->createInfo->actionType, &actionStateUnion);
                actionInfo->stateBySubactionPath[XR_NULL_PATH] = actionStateUnion;
            }
            MergeActionState(actionInfo->createInfo->actionType, &states[i], &actionInfo->stateBySubactionPath.at(XR_NULL_PATH)); // This at() will succeed because previous if-clause populates it if empty

            if(false) if(actionInfo->createInfo->actionType == XR_ACTION_TYPE_BOOLEAN_INPUT) { // XXX debug
                XrActionStateBoolean* boolean = (XrActionStateBoolean*)&actionInfo->stateBySubactionPath.at(subactionPath);
                printf("for action \"%s\", subactionPath \"%s\", merged state is now currentState is %s, active is %s, changedSinceLastSync is %d\n",
                    actionInfo->createInfo->actionName,
                    PathToString(sessionInfo->parentInstance, subactionPath).c_str(),
                    boolean->currentState ? "true" : "false", boolean->isActive ? "true" : "false",
                    boolean->changedSinceLastSync);
            } 
        }

        // On all actions in current state and all subactionPaths, set lastSyncTime and changedSinceLastSync 
        for(const auto& [actionInfo, subactionPaths] : actionInfoSubactionPaths) {

            std::set<XrPath> subactionPathsToUpdate;

            if(subactionPaths.count(XR_NULL_PATH) != 0) {
                subactionPathsToUpdate = actionInfo->subactionPaths;
                subactionPathsToUpdate.insert(XR_NULL_PATH);
            } else {
                subactionPathsToUpdate = subactionPaths;
            }

            for(auto subactionPath: subactionPathsToUpdate) {
                if(previousActionStates.count(actionInfo) != 0) {
                    auto previousStates = previousActionStates.at(actionInfo);
                    if(previousStates.count(subactionPath) != 0) {
                        auto previousState = previousStates.at(subactionPath);
                        UpdateActionStateLastChange(actionInfo->createInfo->actionType, &previousState, &actionInfo->stateBySubactionPath.at(subactionPath)); // This .at() will succeed because previous loop populated all updated subactionPaths and XR_NULL_PATH
                        if(false) if(actionInfo->createInfo->actionType == XR_ACTION_TYPE_BOOLEAN_INPUT) { // XXX debug
                            XrActionStateBoolean* booleanState = &actionInfo->stateBySubactionPath.at(subactionPath).booleanState;
                            printf("for action \"%s\", subactionPath \"%s\"; changedSinceLastSync is %d, last time is %lld\n",
                                actionInfo->createInfo->actionName,
                                PathToString(sessionInfo->parentInstance, subactionPath).c_str(),
                                booleanState->changedSinceLastSync, booleanState->lastChangeTime);
                        } 
                    }
                }
            }
        }

        // Store the interaction profiles current for allowlisted top-level paths
        for(uint32_t i = 0; i < topLevelStrings.size(); i++) {
            XrPath topLevelPath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(topLevelStrings[i]); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
            XrPath interactionProfile = instanceInfo->OverlaysLayerWellKnownStringToPath.at(currentInteractionProfileStrings[i]); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
            XrPath previousProfile = sessionInfo->currentInteractionProfileBySubactionPath.at(topLevelPath); // This .at() must succeed because currentInteractionProfileBySubactionPath.at was filled with all possible topLevelPaths in CreateSessionMain()
            if(previousProfile != interactionProfile) {
                auto l = sessionInfo->GetLock();
                sessionInfo->interactionProfileChangePending = true;
            }

            sessionInfo->currentInteractionProfileBySubactionPath[topLevelPath] = interactionProfile;
        }

    }

    return result;
}

XrResult OverlaysLayerSyncActionsMain(XrInstance parentInstance, XrSession session, const XrActionsSyncInfo* syncInfo)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    XrResult result = XR_SUCCESS;

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(parentInstance);

    // Sync all the actions requested by the Main app
    {
        auto syncInfoSave = syncInfo;
        auto syncInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrSyncActions", syncInfo);
        syncInfo = syncInfoCopy.get();

        result = sessionInfo->downchain->SyncActions(sessionInfo->actualHandle, syncInfo);

        syncInfo = syncInfoSave;
    }

    if(result == XR_SESSION_NOT_FOCUSED) {
        return XR_SESSION_NOT_FOCUSED;
    }

    if(result != XR_SUCCESS) {
        return result;
    }

    // Make queryable data structures for data spread across activeActionSets
    std::set<OverlaysLayerXrActionSetHandleInfo::Ptr> actionSetInfos;
    std::unordered_map<OverlaysLayerXrActionSetHandleInfo::Ptr, std::set<XrPath>> actionSetInfoSubactionPaths;
    std::set<OverlaysLayerXrActionHandleInfo::Ptr> actionInfos;
    std::unordered_map<OverlaysLayerXrActionHandleInfo::Ptr, std::set<XrPath>> actionInfoSubactionPaths;

    for(uint32_t i = 0; i < syncInfo->countActiveActionSets; i++) {
        auto actionSetInfo = OverlaysLayerGetHandleInfoFromXrActionSet(syncInfo->activeActionSets[i].actionSet);
        XrPath subactionPath = syncInfo->activeActionSets[i].subactionPath;
        actionSetInfos.insert(actionSetInfo);
        actionSetInfoSubactionPaths[actionSetInfo].insert(subactionPath);
    }

    for(const auto& [actionSetInfo, subactionPaths] : actionSetInfoSubactionPaths) {
        for(auto actionInfo: actionSetInfo->childActions) {
            actionInfos.insert(actionInfo);
            for(auto subactionPath: subactionPaths) {
                actionInfoSubactionPaths[actionInfo].insert(subactionPath);
            }
            if(subactionPaths.count(XR_NULL_PATH) > 0) {
                actionInfoSubactionPaths[actionInfo].insert(actionInfo->subactionPaths.begin(), actionInfo->subactionPaths.end());
            }
            actionInfoSubactionPaths[actionInfo].insert(XR_NULL_PATH);
        }
    }

    ActionGetInfoList actionsToGet;
    for(const auto& [actionInfo, subactionPaths] : actionInfoSubactionPaths) {
        for(auto subactionPath: subactionPaths) {
            actionsToGet.push_back({ actionInfo->handle, actionInfo->createInfo->actionType, subactionPath });
        }
    }

    std::vector<ActionStateUnion> states(actionsToGet.size());

    result = GetActionStates(session, actionsToGet, states.data());

    if(result != XR_SUCCESS) {
        return result;
    }

    if(result == XR_SUCCESS) {

        std::unordered_map <OverlaysLayerXrActionHandleInfo::Ptr, std::unordered_map<XrPath, ActionStateUnion>> previousActionStates;

        // Save off previous actions' states
        GetPreviousActionStates(sessionInfo->parentInstance, session, syncInfo, previousActionStates);

        // On all actions in previous ActionSet and in this ActionSet, clear state
        ClearSessionLastSyncedActiveActionSets(sessionInfo, syncInfo);
        ClearSyncedActiveActionSets(sessionInfo->parentInstance, session, syncInfo);

        uint32_t index = 0;
        for(const auto& actionGetInfo: actionsToGet) {

            auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(actionGetInfo.action);
            auto subactionPath = actionGetInfo.subactionPath;
            actionInfo->stateBySubactionPath.insert({subactionPath, states[index]});

            if(false) if(actionInfo->createInfo->actionType == XR_ACTION_TYPE_BOOLEAN_INPUT) { // XXX debug
                XrActionStateBoolean* boolean = (XrActionStateBoolean*)&states[index];
                printf("for action \"%s\", subactionPath \"%s\"; currentState is %s, active is %s\n",
                    actionInfo->createInfo->actionName,
                    PathToString(sessionInfo->parentInstance, subactionPath).c_str(),
                    boolean->currentState ? "true" : "false", boolean->isActive ? "true" : "false");
            } 
            if(false) if(actionInfo->createInfo->actionType == XR_ACTION_TYPE_FLOAT_INPUT) { // XXX debug
                XrActionStateFloat* floatState = (XrActionStateFloat*)&states[index];
                printf("for action \"%s\", subactionPath \"%s\"; currentState is %f, active is %s\n",
                    actionInfo->createInfo->actionName,
                    PathToString(sessionInfo->parentInstance, subactionPath).c_str(),
                    floatState->currentState, floatState->isActive ? "true" : "false");
            } 
            index ++;
        }

        // On all actions in current state and all subactionPaths, set lastSyncTime and changedSinceLastSync 
        for(const auto& [actionInfo, subactionPaths] : actionInfoSubactionPaths) {
            for(auto subactionPath: subactionPaths) {
                if(previousActionStates.count(actionInfo) != 0) {
                    auto previousStates = previousActionStates.at(actionInfo);
                    if(previousStates.count(subactionPath) != 0) {
                        auto previousState = previousStates.at(subactionPath);
                        UpdateActionStateLastChange(actionInfo->createInfo->actionType, &previousState, &actionInfo->stateBySubactionPath.at(subactionPath)); // This .at() will succeed because previous loop populated all updated subactionPaths and XR_NULL_PATH
                        if(false) if(actionInfo->createInfo->actionType == XR_ACTION_TYPE_FLOAT_INPUT) { // XXX debug
                            XrActionStateFloat* floatState = &actionInfo->stateBySubactionPath.at(subactionPath).floatState;
                            printf("for action \"%s\", subactionPath \"%s\"; changedSinceLastSync is %d, last time is %lld\n",
                                actionInfo->createInfo->actionName,
                                PathToString(sessionInfo->parentInstance, subactionPath).c_str(),
                                floatState->changedSinceLastSync, floatState->lastChangeTime);
                        } 
                    }
                }
            }
        }

        // update interaction profiles and mark whether we need to synthesize an EVENT_DATA_INTERACTION_PROFILE_CHANGE
        for(XrPath p: instanceInfo->OverlaysLayerAllSubactionPaths) {

            XrInteractionProfileState interactionProfile { XR_TYPE_INTERACTION_PROFILE_STATE };
            result = sessionInfo->downchain->GetCurrentInteractionProfile(sessionInfo->actualHandle, p, &interactionProfile);

            if(result != XR_SUCCESS) {
                OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrSyncActions",
                    OverlaysLayerNoObjectInfo,
                    fmt("Couldn't get current interaction profile for top-level path \"%s\" in order to possibly synthesize a profile change event", PathToString(sessionInfo->parentInstance, p).c_str()).c_str());
                return XR_ERROR_RUNTIME_FAILURE;
            }

            XrPath previousProfile = sessionInfo->currentInteractionProfileBySubactionPath.at(p); // This .at() must succeed because currentInteractionProfileBySubactionPath.at was filled with all possible topLevelPaths in CreateSessionMain()
            if(previousProfile != interactionProfile.interactionProfile) {
                auto l = sessionInfo->GetLock();
                sessionInfo->interactionProfileChangePending = true;
            }

            sessionInfo->currentInteractionProfileBySubactionPath[p] = interactionProfile.interactionProfile;
        }
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

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrSyncActions", OverlaysLayerNoObjectInfo, e.what());
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

        if((getInfo->subactionPath != XR_NULL_PATH) && (actionInfo->subactionPaths.count(getInfo->subactionPath) == 0)) {
            return XR_ERROR_PATH_UNSUPPORTED; 
        }

        if(actionInfo->stateBySubactionPath.count(getInfo->subactionPath) == 0) {

            ActionStateUnion actionStateUnion;
            ClearActionState(actionInfo->createInfo->actionType, &actionStateUnion);
            *state = actionStateUnion.booleanState;

        } else {

            *state = actionInfo->stateBySubactionPath.at(getInfo->subactionPath).booleanState;
        }
        
        return XR_SUCCESS;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrGetActionStateBoolean", OverlaysLayerNoObjectInfo, e.what());
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

        if((getInfo->subactionPath != XR_NULL_PATH) && (actionInfo->subactionPaths.count(getInfo->subactionPath) == 0)) {
            return XR_ERROR_PATH_UNSUPPORTED; 
        }

        if(actionInfo->stateBySubactionPath.count(getInfo->subactionPath) == 0) {

            ActionStateUnion actionStateUnion;
            ClearActionState(actionInfo->createInfo->actionType, &actionStateUnion);
            *state = actionStateUnion.floatState;

            // XXX debug
            if(false) printf("for action \"%s\", subactionPath \"%s\"; GetActionState stored inActive.\n",
                actionInfo->createInfo->actionName,
                PathToString(actionInfo->parentInstance, getInfo->subactionPath).c_str());

        } else {

            *state = actionInfo->stateBySubactionPath.at(getInfo->subactionPath).floatState;

            // XXX debug
            if(false) printf("for action \"%s\", subactionPath \"%s\"; GetActionState yielded %f, active %d, changed %d, last time is %lld\n",
                actionInfo->createInfo->actionName,
                PathToString(actionInfo->parentInstance, getInfo->subactionPath).c_str(),
                state->currentState, state->isActive,
                state->changedSinceLastSync, state->lastChangeTime);
        }
        
        return XR_SUCCESS;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrGetActionStateFloat", OverlaysLayerNoObjectInfo, e.what());
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

        if((getInfo->subactionPath != XR_NULL_PATH) && (actionInfo->subactionPaths.count(getInfo->subactionPath) == 0)) {
            return XR_ERROR_PATH_UNSUPPORTED; 
        }

        if(actionInfo->stateBySubactionPath.count(getInfo->subactionPath) == 0) {

            ActionStateUnion actionStateUnion;
            ClearActionState(actionInfo->createInfo->actionType, &actionStateUnion);
            *state = actionStateUnion.vector2fState;

        } else {

            *state = actionInfo->stateBySubactionPath.at(getInfo->subactionPath).vector2fState;
        }
        
        return XR_SUCCESS;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrGetActionStateVector2f", OverlaysLayerNoObjectInfo, e.what());
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

        if((getInfo->subactionPath != XR_NULL_PATH) && (actionInfo->subactionPaths.count(getInfo->subactionPath) == 0)) {
            return XR_ERROR_PATH_UNSUPPORTED; 
        }

        if(actionInfo->stateBySubactionPath.count(getInfo->subactionPath) == 0) {

            ActionStateUnion actionStateUnion;
            ClearActionState(actionInfo->createInfo->actionType, &actionStateUnion);
            *state = actionStateUnion.poseState;

        } else {

            *state = actionInfo->stateBySubactionPath.at(getInfo->subactionPath).poseState;
        }
        
        
        return XR_SUCCESS;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrGetActionStatePose", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

void GetBindingPathsForActionAndSubactionPath(XrSession session, XrAction action, XrPath requestedSubactionPath, std::vector<std::pair<XrPath, XrPath>>& profileAndBindingPaths)
{
    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(sessionInfo->parentInstance);
    auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(action);

    std::set<XrPath> subactionPaths;

    if(requestedSubactionPath == XR_NULL_PATH) {
        subactionPaths = actionInfo->subactionPaths;
        subactionPaths.erase(XR_NULL_PATH); // in case it was in there
    } else {
        subactionPaths.insert(requestedSubactionPath);
    }

    for(auto subactionPath: subactionPaths) {
        XrPath currentInteractionProfile = sessionInfo->currentInteractionProfileBySubactionPath.at(subactionPath); // This .at() must succeed because currentInteractionProfileBySubactionPath.at was filled with all possible topLevelPaths in CreateSessionMain()

        if(actionInfo->suggestedBindingsByProfile.count(currentInteractionProfile) > 0) {
            for(auto fullBindingPath: actionInfo->suggestedBindingsByProfile.at(currentInteractionProfile)) {

                XrPath bindingSubactionPath = instanceInfo->OverlaysLayerBindingToSubaction.at(fullBindingPath);

                if(subactionPath == bindingSubactionPath) {
                    // get profile and full path which the main process side of the API layer maps to a placeholder action
                    profileAndBindingPaths.push_back({currentInteractionProfile, fullBindingPath});
                }
            }
        }
    }
}

XrResult OverlaysLayerApplyHapticFeedbackMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, uint32_t profileStringCount, const WellKnownStringIndex *profileStrings, const WellKnownStringIndex *bindingStrings, const XrHapticBaseHeader* hapticFeedback)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(sessionInfo->parentInstance);

    auto hapticFeedbackCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrStopHapticFeedback", hapticFeedback);

    for(uint32_t i = 0; i < profileStringCount; i++) {
        XrPath bindingPath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(bindingStrings[i]); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
        XrPath profilePath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(profileStrings[i]); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support

        XrAction actualActionHandle = sessionInfo->placeholderActionsByProfileAndFullBinding.at({profilePath, bindingPath}).first; // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support

        XrHapticActionInfo hapticActionInfo { XR_TYPE_HAPTIC_ACTION_INFO, nullptr, actualActionHandle, XR_NULL_PATH };

        XrResult result = sessionInfo->downchain->ApplyHapticFeedback(sessionInfo->actualHandle, &hapticActionInfo, hapticFeedbackCopy.get());

        if(result != XR_SUCCESS) {
            return result;
        }
    }

    return XR_SUCCESS;
}

XrResult OverlaysLayerStopHapticFeedbackMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, uint32_t profileStringCount, const WellKnownStringIndex *profileStrings, const WellKnownStringIndex *bindingStrings)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(sessionInfo->parentInstance);

    for(uint32_t i = 0; i < profileStringCount; i++) {
        XrPath bindingPath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(bindingStrings[i]); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
        XrPath profilePath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(profileStrings[i]); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support

        XrAction actualActionHandle = sessionInfo->placeholderActionsByProfileAndFullBinding.at({profilePath, bindingPath}).first; // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support

        XrHapticActionInfo hapticActionInfo { XR_TYPE_HAPTIC_ACTION_INFO, nullptr, actualActionHandle, XR_NULL_PATH };

        XrResult result = sessionInfo->downchain->StopHapticFeedback(sessionInfo->actualHandle, &hapticActionInfo);
    }

    return XR_SUCCESS;
}

XrResult OverlaysLayerApplyHapticFeedbackOverlay(XrInstance instance, XrSession session, const XrHapticActionInfo* hapticActionInfo, const XrHapticBaseHeader* hapticFeedback)
{
    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);
    auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(hapticActionInfo->action);

    if(actionInfo->createInfo->actionType != XR_ACTION_TYPE_VIBRATION_OUTPUT) {
        return XR_ERROR_ACTION_TYPE_MISMATCH; 
    }

    if((hapticActionInfo->subactionPath != XR_NULL_PATH) && (actionInfo->subactionPaths.count(hapticActionInfo->subactionPath) == 0)) {
        return XR_ERROR_PATH_UNSUPPORTED;
    }

    std::vector<std::pair<XrPath, XrPath>> profileAndBindingPaths;
    GetBindingPathsForActionAndSubactionPath(session, hapticActionInfo->action, hapticActionInfo->subactionPath, profileAndBindingPaths);

    if(profileAndBindingPaths.size() == 0) {
        return XR_SUCCESS; // Spec says "If an appropriate device is unavailable the runtime may ignore this request for haptic feedback."
    }

    auto hapticFeedbackCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrStopHapticFeedback", hapticFeedback);

    std::vector<WellKnownStringIndex> profileStrings;
    std::vector<WellKnownStringIndex> bindingStrings;
    for(auto profileAndBindingPath: profileAndBindingPaths) {
        profileStrings.push_back(instanceInfo->OverlaysLayerPathToWellKnownString.at(profileAndBindingPath.first)); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
        bindingStrings.push_back(instanceInfo->OverlaysLayerPathToWellKnownString.at(profileAndBindingPath.second)); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
    }

    XrResult result = RPCCallApplyHapticFeedback(sessionInfo->parentInstance, sessionInfo->actualHandle, (uint32_t)profileStrings.size(), profileStrings.data(), bindingStrings.data(), hapticFeedbackCopy.get());

    return result;
}

XrResult OverlaysLayerApplyHapticFeedbackMain(XrInstance parentInstance, XrSession session, const XrHapticActionInfo* hapticActionInfo, const XrHapticBaseHeader* hapticFeedback)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    XrResult result = XR_SUCCESS;

    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrSessionToHandleInfoMutex);
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = gOverlaysLayerXrSessionToHandleInfo.at(session);
    // restore the actual handle
    XrSession localHandleStore = session;
    session = sessionInfo->actualHandle;
    
    auto hapticActionInfoSave = hapticActionInfo;
    auto hapticActionInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrApplyHapticFeedback", hapticActionInfo);
    hapticActionInfo = hapticActionInfoCopy.get();

    auto hapticFeedbackSave = hapticFeedback;
    auto hapticFeedbackCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrApplyHapticFeedback", hapticFeedback);
    hapticFeedback = hapticFeedbackCopy.get();
    
    {
        std::unique_lock<std::recursive_mutex> HapticQuirkLock(HapticQuirkMutex);
        result = sessionInfo->downchain->ApplyHapticFeedback(session, hapticActionInfo, hapticFeedback);
    }

    return result;
}

XrResult OverlaysLayerApplyHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo, const XrHapticBaseHeader* hapticFeedback)
{
    try {

        auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
        
        bool isProxied = sessionInfo->isProxied;
        XrResult result;
        if(isProxied) {
            result = OverlaysLayerApplyHapticFeedbackOverlay(sessionInfo->parentInstance, session, hapticActionInfo, hapticFeedback);
        } else {
            result = OverlaysLayerApplyHapticFeedbackMain(sessionInfo->parentInstance, session, hapticActionInfo, hapticFeedback);
        }

        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrApplyHapticFeedback", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

XrResult OverlaysLayerStopHapticFeedbackOverlay(XrInstance instance, XrSession session, const XrHapticActionInfo* hapticActionInfo)
{
    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);
    auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(hapticActionInfo->action);

    if(actionInfo->createInfo->actionType != XR_ACTION_TYPE_VIBRATION_OUTPUT) {
        return XR_ERROR_ACTION_TYPE_MISMATCH; 
    }

    if((hapticActionInfo->subactionPath != XR_NULL_PATH) && (actionInfo->subactionPaths.count(hapticActionInfo->subactionPath) == 0)) {
        return XR_ERROR_PATH_UNSUPPORTED;
    }

    std::vector<std::pair<XrPath, XrPath>> profileAndBindingPaths;
    GetBindingPathsForActionAndSubactionPath(session, hapticActionInfo->action, hapticActionInfo->subactionPath, profileAndBindingPaths);

    if(profileAndBindingPaths.size() == 0) {
        return XR_SUCCESS; // Spec says "If an appropriate device is unavailable the runtime may ignore this request for haptic feedback."
    }

    std::vector<WellKnownStringIndex> profileStrings;
    std::vector<WellKnownStringIndex> bindingStrings;
    for(auto profileAndBindingPath: profileAndBindingPaths) {
        profileStrings.push_back(instanceInfo->OverlaysLayerPathToWellKnownString.at(profileAndBindingPath.first)); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
        bindingStrings.push_back(instanceInfo->OverlaysLayerPathToWellKnownString.at(profileAndBindingPath.second)); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support
    }

    XrResult result = RPCCallStopHapticFeedback(sessionInfo->parentInstance, sessionInfo->actualHandle, (uint32_t)profileStrings.size(), profileStrings.data(), bindingStrings.data());

    return result;
}

XrResult OverlaysLayerStopHapticFeedbackMain(XrInstance parentInstance, XrSession session, const XrHapticActionInfo* hapticActionInfo)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();

    XrResult result = XR_SUCCESS;

    std::unique_lock<std::recursive_mutex> mlock(gOverlaysLayerXrSessionToHandleInfoMutex);
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = gOverlaysLayerXrSessionToHandleInfo.at(session);
    // restore the actual handle
    XrSession localHandleStore = session;
    session = sessionInfo->actualHandle;

    auto hapticActionInfoSave = hapticActionInfo;
    auto hapticActionInfoCopy = GetSharedCopyHandlesRestored(sessionInfo->parentInstance, "xrStopHapticFeedback", hapticActionInfo);
    hapticActionInfo = hapticActionInfoCopy.get();

    result = sessionInfo->downchain->StopHapticFeedback(session, hapticActionInfo);

    hapticActionInfo = hapticActionInfoSave;

    return result;
}

XrResult OverlaysLayerStopHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo)
{
    try {

        auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);

        bool isProxied = sessionInfo->isProxied;
        XrResult result;
        if(isProxied) {
            result = XR_SUCCESS; // OverlaysLayerStopHapticFeedbackOverlay(sessionInfo->parentInstance, session, hapticActionInfo);
        } else {
            result = OverlaysLayerStopHapticFeedbackMain(sessionInfo->parentInstance, session, hapticActionInfo);
        }

        return result;

    } catch (const OverlaysLayerXrException exc) {

        return exc.result();

    } catch (const std::bad_alloc& e) {

        OverlaysLayerLogMessage(XR_NULL_HANDLE, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "xrStopHapticFeedback", OverlaysLayerNoObjectInfo, e.what());
        return XR_ERROR_OUT_OF_MEMORY;

    }
}

XrResult OverlaysLayerGetInputSourceLocalizedNameMainAsOverlay(ConnectionToOverlay::Ptr connection, XrSession session, const XrInputSourceLocalizedNameGetInfo* getInfo /* sourcePath ignored */, WellKnownStringIndex sourceString, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer)
{
    auto synchronizeEveryProcLock = gSynchronizeEveryProc ? std::unique_lock<std::recursive_mutex>(gSynchronizeEveryProcMutex) : std::unique_lock<std::recursive_mutex>();
    OverlaysLayerXrSessionHandleInfo::Ptr sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(sessionInfo->parentInstance);

    XrInputSourceLocalizedNameGetInfo getInfoCopy = *getInfo;

    getInfoCopy.sourcePath = instanceInfo->OverlaysLayerWellKnownStringToPath.at(sourceString); // This .at() must succeed; it was translated by the overlay side to a well-known string

	std::unique_lock<std::recursive_mutex> HapticQuirkLock(HapticQuirkMutex);
    return sessionInfo->downchain->GetInputSourceLocalizedName(sessionInfo->actualHandle, &getInfoCopy, bufferCapacityInput, bufferCountOutput, buffer);
}

XrResult OverlaysLayerGetInputSourceLocalizedNameOverlay( XrInstance instance, XrSession session, const XrInputSourceLocalizedNameGetInfo* getInfo, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer)
{
    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);

    if(instanceInfo->OverlaysLayerPathToWellKnownString.count(getInfo->sourcePath) == 0) {
        return XR_ERROR_PATH_UNSUPPORTED;
    }

    WellKnownStringIndex sourceString = instanceInfo->OverlaysLayerPathToWellKnownString.at(getInfo->sourcePath); // This .at() must succeed; adding new binding paths would require enabling an extension which API Layer doesn't support

    return RPCCallGetInputSourceLocalizedName(sessionInfo->parentInstance, sessionInfo->actualHandle, getInfo, sourceString, bufferCapacityInput, bufferCountOutput, buffer);
}

XrResult OverlaysLayerEnumerateBoundSourcesForActionOverlay(XrInstance instance, XrSession session, const XrBoundSourcesForActionEnumerateInfo* enumerateInfo, uint32_t sourceCapacityInput, uint32_t* sourceCountOutput, XrPath* sources)
{
    auto sessionInfo = OverlaysLayerGetHandleInfoFromXrSession(session);
    auto instanceInfo = OverlaysLayerGetHandleInfoFromXrInstance(instance);
    auto actionInfo = OverlaysLayerGetHandleInfoFromXrAction(enumerateInfo->action);

    std::set<XrPath> boundSourcesForAction;

    for(auto subactionPath: actionInfo->subactionPaths) {

        if(subactionPath != XR_NULL_PATH) {
            XrPath currentInteractionProfile = sessionInfo->currentInteractionProfileBySubactionPath.at(subactionPath); // This .at() must succeed because currentInteractionProfileBySubactionPath.at was filled with all possible topLevelPaths in CreateSessionMain()

            if(actionInfo->suggestedBindingsByProfile.count(currentInteractionProfile) == 0) {
                OverlaysLayerLogMessage(sessionInfo->parentInstance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "xrEnumerateBoundSourcesForAction",
                    OverlaysLayerNoObjectInfo,
                    fmt("Couldn't get current interaction profile for action \"%s\" for top-level path \"%s\". It's possible Main App has suggested bindings for a profile for which this Overlay App didn't.  EnumerateBoundSourcesForAction may incorrectly return 0 sources.", actionInfo->createInfo->actionName, PathToString(sessionInfo->parentInstance, subactionPath).c_str()).c_str());

            } else {

                if(actionInfo->suggestedBindingsByProfile.count(currentInteractionProfile) > 0) {
                    for(auto fullBindingPath: actionInfo->suggestedBindingsByProfile.at(currentInteractionProfile)) {

                        // XXX really should find() this - could be path from an extension
                        XrPath bindingSubactionPath = instanceInfo->OverlaysLayerBindingToSubaction.at(fullBindingPath);

                        if(subactionPath == bindingSubactionPath) {

                            boundSourcesForAction.insert(fullBindingPath);
                        }
                    }
                }
            }
        }
    }

    if(sourceCapacityInput == 0) {

        if(sourceCountOutput) {
            *sourceCountOutput = (uint32_t)boundSourcesForAction.size();
        }

    } else {

        if(!sources) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        if(sourceCapacityInput < boundSourcesForAction.size()) {
            return XR_ERROR_SIZE_INSUFFICIENT;
        }
            
        std::copy(boundSourcesForAction.begin(), boundSourcesForAction.end(), sources);
    }

    return XR_SUCCESS;
}


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
