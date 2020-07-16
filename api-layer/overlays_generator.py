#!/usr/bin/python3 -i
#
# Copyright (c) 2017-2020 The Khronos Group Inc.
# Copyright (c) 2017-2019 Valve Corporation
# Copyright (c) 2017-2019 LunarG, Inc.
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
# Author(s):    Mark Young <marky@lunarg.com>
#
# Purpose:      This file utilizes the content formatted in the
#               automatic_source_generator.py class to produce the
#               generated source code for the Overlays layer.

from automatic_source_generator import (AutomaticSourceOutputGenerator,
                                        undecorate)
from generator import write

# The following commands should not be generated for the layer
MANUALLY_DEFINED_IN_LAYER = set((
    'xrCreateInstance',
    'xrDestroyInstance',
))

# OverlaysOutputGenerator - subclass of AutomaticSourceOutputGenerator.


class OverlaysOutputGenerator(AutomaticSourceOutputGenerator):
    """Generate Overlays layer source using XML element attributes from registry"""

    # Override the base class header warning so the comment indicates this file.
    #   self            the AutomaticSourceOutputGenerator object
    def outputGeneratedHeaderWarning(self):
        # File Comment
        generated_warning = '// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********\n'
        generated_warning += '//     See overlays_generator.py for modifications\n'
        generated_warning += '// ************************************************************\n'
        write(generated_warning, file=self.outFile)

    # Call the base class to properly begin the file, and then add
    # the file-specific header information.
    #   self            the OverlaysOutputGenerator object
    #   gen_opts        the OverlaysGeneratorOptions object
    def beginFile(self, genOpts):
        AutomaticSourceOutputGenerator.beginFile(self, genOpts)
        preamble = ''
        if self.genOpts.filename == 'xr_generated_overlays.hpp':
            preamble += '#pragma once\n\n'
            preamble += '#include "api_layer_platform_defines.h"\n'
            preamble += '#include <openxr/openxr.h>\n'
            preamble += '#include <openxr/openxr_platform.h>\n\n'
            preamble += '#include <mutex>\n'
            preamble += '#include <string>\n'
            preamble += '#include <tuple>\n'
            preamble += '#include <unordered_map>\n'
            preamble += '#include <vector>\n\n'
            preamble += 'struct XrGeneratedDispatchTable;\n\n'
        elif self.genOpts.filename == 'xr_generated_overlays.cpp':
            preamble += '#include "xr_generated_overlays.hpp"\n'
            preamble += '#include "xr_generated_dispatch_table.h"\n'
            preamble += '#include "hex_and_handles.h"\n\n'
            preamble += '#include <cstring>\n'
            preamble += '#include <mutex>\n'
            preamble += '#include <sstream>\n'
            preamble += '#include <iomanip>\n'
            preamble += '#include <unordered_map>\n\n'
        write(preamble, file=self.outFile)

    # Write out all the information for the appropriate file,
    # and then call down to the base class to wrap everything up.
    #   self            the OverlaysOutputGenerator object
    def endFile(self):
        file_data = ''
        if self.genOpts.filename == 'xr_generated_overlays.hpp':
            file_data += self.outputLayerHeaderPrototypes()
            file_data += self.outputOverlaysExterns()

        elif self.genOpts.filename == 'xr_generated_overlays.cpp':
            file_data += self.outputOverlaysMapMutexItems()
            file_data += self.outputLayerCommands()

        write(file_data, file=self.outFile)

        # Finish processing in superclass
        AutomaticSourceOutputGenerator.endFile(self)

    # Output the externs required by the manual code to work with the Overlays
    # generated code.
    #   self            the OverlaysOutputGenerator object
    def outputOverlaysExterns(self):
        externs = '\n// Externs for Overlays\n'
        for handle in self.api_handles:
            base_handle_name = undecorate(handle.name)
            if handle.protect_value is not None:
                externs += '#if %s\n' % handle.protect_string
            externs += 'extern std::unordered_map<%s, XrGeneratedDispatchTable*> g_%s_dispatch_map;\n' % (
                handle.name, base_handle_name)
            externs += 'extern std::mutex g_%s_dispatch_mutex;\n' % base_handle_name
            if handle.protect_value is not None:
                externs += '#endif // %s\n' % handle.protect_string
        externs += 'void OverlaysCleanUpMapsForTable(XrGeneratedDispatchTable *table);\n'
        return externs

    # Output the externs manually implemented by the Overlays layer so that the generated code
    # can access them.
    #   self            the OverlaysOutputGenerator object
    def outputLayerHeaderPrototypes(self):
        generated_prototypes = '// Layer\'s xrGetInstanceProcAddr\n'
        generated_prototypes += 'XrResult OverlaysLayerXrGetInstanceProcAddr(XrInstance instance,\n'
        generated_prototypes += '                                          const char* name, PFN_xrVoidFunction* function);\n\n'
        generated_prototypes += '// Overlays Manual Functions\n'
        generated_prototypes += 'XrInstance FindInstanceFromDispatchTable(XrGeneratedDispatchTable* dispatch_table);\n'
        generated_prototypes += 'XrResult OverlaysLayerXrCreateInstance(const XrInstanceCreateInfo *info,\n'
        generated_prototypes += '                                      XrInstance *instance);\n'
        generated_prototypes += 'XrResult OverlaysLayerXrDestroyInstance(XrInstance instance);\n'
        generated_prototypes += '\n//Dump utility functions\n'
        generated_prototypes += 'bool OverlaysDecodeNextChain(XrGeneratedDispatchTable* gen_dispatch_table, const void* value, std::string prefix,\n'
        generated_prototypes += '                            std::vector<std::tuple<std::string, std::string, std::string>> &contents);\n'
        generated_prototypes += '\n// Union/Structure Output Helper function prototypes\n'
        return generated_prototypes

    # Output the unordered_map's required to track all the data we need per handle type.  Also, create
    # a mutex that allows us to access the unordered_maps in a thread-safe manner.  Finally, wrap it
    # all up by creating utility functions for cleaning up a dispatch table when it's instance has
    # been deleted.
    #   self            the OverlaysOutputGenerator object
    def outputOverlaysMapMutexItems(self):
        maps_mutexes = ''
        for handle in self.api_handles:
            base_handle_name = undecorate(handle.name)
            if handle.protect_value:
                maps_mutexes += '#if %s\n' % handle.protect_string
            maps_mutexes += 'std::unordered_map<%s, XrGeneratedDispatchTable*> g_%s_dispatch_map;\n' % (
                handle.name, base_handle_name)
            maps_mutexes += 'std::mutex g_%s_dispatch_mutex;\n' % base_handle_name
            if handle.protect_value:
                maps_mutexes += '#endif // %s\n' % handle.protect_string
        maps_mutexes += '\n'
        maps_mutexes += '// Template function to reduce duplicating the map locking, searching, and deleting.`\n'
        maps_mutexes += 'template <typename MapType>\n'
        maps_mutexes += 'void eraseAllTableMapElements(MapType &search_map, std::mutex &mutex, XrGeneratedDispatchTable *search_value) {\n'
        maps_mutexes += '    std::unique_lock<std::mutex> lock(mutex);\n'
        maps_mutexes += '    for (auto it = search_map.begin(); it != search_map.end();) {\n'
        maps_mutexes += '        if (it->second == search_value) {\n'
        maps_mutexes += '            search_map.erase(it++);\n'
        maps_mutexes += '        } else {\n'
        maps_mutexes += '            ++it;\n'
        maps_mutexes += '        }\n'
        maps_mutexes += '    }\n'
        maps_mutexes += '}\n'
        maps_mutexes += '\n'
        maps_mutexes += '// Function used to clean up any residual map values that point to an instance prior to that\n'
        maps_mutexes += '// instance being deleted.\n'
        maps_mutexes += 'void OverlaysCleanUpMapsForTable(XrGeneratedDispatchTable *table) {\n'
        # Call each handle's erase utility function using the template we defined above.
        for handle in self.api_handles:
            base_handle_name = undecorate(handle.name)
            if handle.protect_value:
                maps_mutexes += '#if %s\n' % handle.protect_string
            maps_mutexes += '    eraseAllTableMapElements<std::unordered_map<%s, XrGeneratedDispatchTable*>>' % handle.name
            maps_mutexes += '(g_%s_dispatch_map, g_%s_dispatch_mutex, table);\n' % (
                base_handle_name, base_handle_name)
            if handle.protect_value:
                maps_mutexes += '#endif // %s\n' % handle.protect_string
        maps_mutexes += '}\n'
        maps_mutexes += '\n'
        return maps_mutexes

    # Write the C++ Overlays function for every command we know about
    #   self            the OverlaysOutputGenerator object
    def outputLayerCommands(self):
        cur_extension_name = ''
        generated_commands = '\n// Automatically generated overlays layer commands\n'
        for x in range(0, 2):
            if x == 0:
                commands = self.core_commands
            else:
                commands = self.ext_commands

            for cur_cmd in commands:
                if cur_cmd.ext_name != cur_extension_name:
                    if self.isCoreExtensionName(cur_cmd.ext_name):
                        generated_commands += '\n// ---- Core %s commands\n' % cur_cmd.ext_name[11:].replace(
                            "_", ".")
                    else:
                        generated_commands += '\n// ---- %s extension commands\n' % cur_cmd.ext_name
                    cur_extension_name = cur_cmd.ext_name

                if cur_cmd.name in self.no_trampoline_or_terminator or cur_cmd.name in MANUALLY_DEFINED_IN_LAYER:
                    continue

                # We fill in the GetInstanceProcAddr manually at the end
                if cur_cmd.name == 'xrGetInstanceProcAddr':
                    continue

                is_create = False
                is_destroy = False
                has_return = False

                if ('xrCreate' in cur_cmd.name or 'xrConnect' in cur_cmd.name) and cur_cmd.params[-1].is_handle:
                    is_create = True
                    has_return = True
                elif ('xrDestroy' in cur_cmd.name or 'xrDisconnect' in cur_cmd.name) and cur_cmd.params[-1].is_handle:
                    is_destroy = True
                    has_return = True
                elif cur_cmd.return_type is not None:
                    has_return = True

                base_name = cur_cmd.name[2:]

                if cur_cmd.protect_value:
                    generated_commands += '#if %s\n' % cur_cmd.protect_string

                prototype = cur_cmd.cdecl.replace(" xr", " OverlaysLayerXr")
                prototype = prototype.replace(self.genOpts.apicall, "").replace(self.genOpts.apientry, "")
                prototype = prototype.replace(";", " {\n")
                generated_commands += prototype

                if has_return:
                    return_prefix = '    '
                    return_prefix += cur_cmd.return_type.text
                    return_prefix += ' result'
                    if cur_cmd.return_type.text == 'XrResult':
                        return_prefix += ' = XR_SUCCESS;\n'
                    else:
                        return_prefix += ';\n'
                    generated_commands += return_prefix

                generated_commands += '    try {\n'

                # Next, we have to call down to the next implementation of this command in the call chain.
                # Before we can do that, we have to figure out what the dispatch table is
                if cur_cmd.params[0].is_handle:
                    handle_param = cur_cmd.params[0]
                    base_handle_name = undecorate(handle_param.type)
                    first_handle_name = self.getFirstHandleName(handle_param)
                    generated_commands += '        std::unique_lock<std::mutex> mlock(g_%s_dispatch_mutex);\n' % base_handle_name
                    generated_commands += '        auto map_iter = g_%s_dispatch_map.find(%s);\n' % (base_handle_name, first_handle_name)
                    generated_commands += '        mlock.unlock();\n\n'
                    generated_commands += '        if (map_iter == g_%s_dispatch_map.end()) return XR_ERROR_VALIDATION_FAILURE;\n' % base_handle_name
                    generated_commands += '        XrGeneratedDispatchTable *gen_dispatch_table = map_iter->second;\n'
                else:
                    generated_commands += self.printCodeGenErrorMessage(
                        'Command %s does not have an OpenXR Object handle as the first parameter.' % cur_cmd.name)

                # Call down, looking for the returned result if required.
                generated_commands += '        '
                if has_return:
                    generated_commands += 'result = '
                generated_commands += 'gen_dispatch_table->%s(' % base_name

                count = 0
                for param in cur_cmd.params:
                    if count > 0:
                        generated_commands += ', '
                    generated_commands += param.name
                    count = count + 1
                generated_commands += ');\n'

                # If this is a create command, we have to create an entry in the appropriate
                # unordered_map pointing to the correct dispatch table for the newly created
                # object.  Likewise, if it's a delete command, we have to remove the entry
                # for the dispatch table from the unordered_map
                second_base_handle_name = ''
                if cur_cmd.params[-1].is_handle and (is_create or is_destroy):
                    second_base_handle_name = undecorate(cur_cmd.params[-1].type)
                    if is_create:
                        generated_commands += '        if (XR_SUCCESS == result && nullptr != %s) {\n' % cur_cmd.params[-1].name
                        generated_commands += '            auto exists = g_%s_dispatch_map.find(*%s);\n' % (
                            second_base_handle_name, cur_cmd.params[-1].name)
                        generated_commands += '            if (exists == g_%s_dispatch_map.end()) {\n' % second_base_handle_name
                        generated_commands += '                std::unique_lock<std::mutex> lock(g_%s_dispatch_mutex);\n' % second_base_handle_name
                        generated_commands += '                g_%s_dispatch_map[*%s] = gen_dispatch_table;\n' % (
                            second_base_handle_name, cur_cmd.params[-1].name)
                        generated_commands += '            }\n'
                        generated_commands += '        }\n'
                    elif is_destroy:
                        generated_commands += '        auto exists = g_%s_dispatch_map.find(%s);\n' % (
                            second_base_handle_name, cur_cmd.params[-1].name)
                        generated_commands += '        if (exists != g_%s_dispatch_map.end()) {\n' % second_base_handle_name
                        generated_commands += '            std::unique_lock<std::mutex> lock(g_%s_dispatch_mutex);\n' % second_base_handle_name
                        generated_commands += '            g_%s_dispatch_map.erase(%s);\n' % (
                            second_base_handle_name, cur_cmd.params[-1].name)
                        generated_commands += '        }\n'

                # Catch any exceptions that may have occurred.  If any occurred between any of the
                # valid mutex lock/unlock statements, perform the unlock now.
                generated_commands += '    } catch (...) {\n'
                if has_return:
                    generated_commands += '        return XR_ERROR_VALIDATION_FAILURE;\n'
                generated_commands += '    }\n'

                if has_return:
                    generated_commands += '    return result;\n'

                generated_commands += '}\n\n'
                if cur_cmd.protect_value:
                    generated_commands += '#endif // %s\n' % cur_cmd.protect_string

        # Output the xrGetInstanceProcAddr command for the Overlays layer.
        generated_commands += '\n// Layer\'s xrGetInstanceProcAddr\n'
        generated_commands += 'XrResult OverlaysLayerXrGetInstanceProcAddr(\n'
        generated_commands += '    XrInstance                                  instance,\n'
        generated_commands += '    const char*                                 name,\n'
        generated_commands += '    PFN_xrVoidFunction*                         function) {\n'
        generated_commands += '    try {\n'
        generated_commands += '        std::string func_name = name;\n\n'
        
        generated_commands += '        // Set the function pointer to NULL so that the fall-through below actually works:\n'
        generated_commands += '        *function = nullptr;\n\n'

        count = 0
        for x in range(0, 2):
            if x == 0:
                commands = self.core_commands
            else:
                commands = self.ext_commands

            for cur_cmd in commands:
                if cur_cmd.ext_name != cur_extension_name:
                    if self.isCoreExtensionName(cur_cmd.ext_name):
                        generated_commands += '\n        // ---- Core %s commands\n' % cur_cmd.ext_name[11:].replace(
                            "_", ".")
                    else:
                        generated_commands += '\n        // ---- %s extension commands\n' % cur_cmd.ext_name
                    cur_extension_name = cur_cmd.ext_name

                if cur_cmd.name in self.no_trampoline_or_terminator:
                    continue

                has_return = False
                if cur_cmd.return_type is not None:
                    has_return = True

                # Replace 'xr' in proto name with an Overlays-specific name to avoid collisions.s
                layer_command_name = cur_cmd.name.replace(
                    "xr", "OverlaysLayerXr")

                if cur_cmd.protect_value:
                    generated_commands += '#if %s\n' % cur_cmd.protect_string

                if count == 0:
                    generated_commands += '        if (func_name == "%s") {\n' % cur_cmd.name
                else:
                    generated_commands += '        } else if (func_name == "%s") {\n' % cur_cmd.name
                count = count + 1

                generated_commands += '            *function = reinterpret_cast<PFN_xrVoidFunction>(%s);\n' % layer_command_name
                if cur_cmd.protect_value:
                    generated_commands += '#endif // %s\n' % cur_cmd.protect_string

        generated_commands += '        }\n'
        generated_commands += '        // If we setup the function, just return\n'
        generated_commands += '        if (*function != nullptr) {\n'
        generated_commands += '            return XR_SUCCESS;\n'
        generated_commands += '        }\n\n'
        generated_commands += '        // We have not found it, so pass it down to the next layer/runtime\n'
        generated_commands += '        std::unique_lock<std::mutex> mlock(g_instance_dispatch_mutex);\n'
        generated_commands += '        auto map_iter = g_instance_dispatch_map.find(instance);\n'
        generated_commands += '        mlock.unlock();\n\n'
        generated_commands += '        if (map_iter == g_instance_dispatch_map.end()) {\n'
        generated_commands += '            return XR_ERROR_HANDLE_INVALID;\n'
        generated_commands += '        }\n\n'
        generated_commands += '        XrGeneratedDispatchTable *gen_dispatch_table = map_iter->second;\n'
        generated_commands += '        if (nullptr == gen_dispatch_table) {\n'
        generated_commands += '            return XR_ERROR_HANDLE_INVALID;\n'
        generated_commands += '        }\n\n'
        generated_commands += '        return gen_dispatch_table->GetInstanceProcAddr(instance, name, function);\n'
        generated_commands += '    } catch (...) {\n'
        generated_commands += '        return XR_ERROR_VALIDATION_FAILURE;\n'
        generated_commands += '    }\n'
        generated_commands += '}\n'
        return generated_commands
