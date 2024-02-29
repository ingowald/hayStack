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

#####################################################################################################################
bl_info = {
    "name": "hayStack",
    "author": "Milan Jaros, Ingo Wald",
    "description": "",
    "blender": (4, 0, 0),
    "version": (0, 0, 2),
    "location": "",
    "warning": "",
    "category": "Render"
}
#####################################################################################################################

def register():
    from . import haystack_pref
    from . import haystack_process_handler    
    from . import haystack_dll
    from . import haystack_render
    from . import haystack_nodes
    from . import haystack_scene

    haystack_pref.register()
    haystack_process_handler.register()
    haystack_dll.register()
    haystack_render.register()
    haystack_nodes.register()
    haystack_scene.register()
    

def unregister():
    from . import haystack_pref
    from . import haystack_process_handler    
    from . import haystack_dll
    from . import haystack_render
    from . import haystack_nodes
    from . import haystack_scene
    
    try:        
        haystack_pref.unregister()
        haystack_process_handler.unregister()
        haystack_dll.unregister()
        haystack_render.unregister()
        haystack_nodes.unregister()
        haystack_scene.unregister()

    except RuntimeError:
        pass 