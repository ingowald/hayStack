# ======================================================================== //
# Copyright 2022-2023 Ingo Wald                                            //
# Copyright 2022-2023 IT4Innovations, VSB - Technical University of Ostrava//
#                                                                          //
# Licensed under the Apache License, Version 2.0 (the "License");          //
# you may not use this file except in compliance with the License.         //
# You may obtain a copy of the License at                                  //
#                                                                          //
#     http://www.apache.org/licenses/LICENSE-2.0                           //
#                                                                          //
# Unless required by applicable law or agreed to in writing, software      //
# distributed under the License is distributed on an "AS IS" BASIS,        //
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
# See the License for the specific language governing permissions and      //
# limitations under the License.                                           //
# ======================================================================== //

import bpy

ADDON_NAME = 'hayStack'

class HayStackPreferences(bpy.types.AddonPreferences):
    bl_idname = ADDON_NAME

    def draw(self, context):
        layout = self.layout    

def ctx_preferences():
    try:
        return bpy.context.preferences
    except AttributeError:
        return bpy.context.user_preferences

def preferences() -> HayStackPreferences:
    return ctx_preferences().addons[ADDON_NAME].preferences

def register():
    bpy.utils.register_class(HayStackPreferences)

def unregister():
    bpy.utils.unregister_class(HayStackPreferences)
