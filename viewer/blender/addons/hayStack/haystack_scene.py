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

import os
import bpy
import numpy as np

from . import haystack_pref
from . import haystack_dll

class HayStackCreateBBoxOperator(bpy.types.Operator):
    bl_idname = "haystack.create_bbox"
    bl_label = "Create BBox"

    def execute(self, context):
        context.scene.haystack_scene.create_bbox(context)
        return {'FINISHED'}

#struct HsDataInit {
#	float world_bounds_spatial_lower[3];
#	float world_bounds_spatial_upper[3];
#	float scalars_range[2];
#};
class HsDataInit:
    def __init__(self):
        self.world_bounds_spatial_lower = np.zeros((3), dtype=np.float32)
        self.world_bounds_spatial_upper = np.zeros((3), dtype=np.float32)
        self.scalars_range = np.zeros((2), dtype=np.float32)

class HayStackScene:
    def __init__(self):
        self.haystack_bbox = None

    def create_bbox(self, context):

        # set env
        pref = haystack_pref.preferences()
        # os.environ['SOCKET_SERVER_PORT_CAM'] = str(pref.haystack_port_cam)
        # os.environ['SOCKET_SERVER_PORT_DATA'] = str(pref.haystack_port_data)
        # os.environ['SOCKET_SERVER_NAME_CAM'] = str(pref.haystack_server_name)
        # os.environ['SOCKET_SERVER_NAME_DATA'] = str(pref.haystack_server_name) 
                
        hsDataInit = HsDataInit()
        haystack_dll._renderengine_dll.get_haystack_range(#str(pref.haystack_server_name).encode(),
                                                           #   pref.haystack_port_cam,
                                                           #   pref.haystack_port_data,
                                                              hsDataInit.world_bounds_spatial_lower.ctypes.data, 
                                                              hsDataInit.world_bounds_spatial_upper.ctypes.data, 
                                                              hsDataInit.scalars_range.ctypes.data)

        # Lower and upper vertex coordinates
        lower_vertex = (hsDataInit.world_bounds_spatial_lower[0], hsDataInit.world_bounds_spatial_lower[1], hsDataInit.world_bounds_spatial_lower[2])
        upper_vertex = (hsDataInit.world_bounds_spatial_upper[0], hsDataInit.world_bounds_spatial_upper[1], hsDataInit.world_bounds_spatial_upper[2])

        # Calculate size and center position
        size = tuple(upper - lower for upper, lower in zip(upper_vertex, lower_vertex))
        center = tuple(lower + (size_dim / 2.0) for lower, size_dim in zip(lower_vertex, size))

        # Create a cube mesh
        if "HayStack BBOX" in bpy.data.objects:
            self.haystack_bbox = bpy.data.objects["HayStack BBOX"]
        else:
            bpy.ops.mesh.primitive_cube_add(size=1) # (size=1, location=center)
            self.haystack_bbox = bpy.context.view_layer.objects.active
            self.haystack_bbox.name = "HayStack BBOX"

        bpy.context.view_layer.objects.active = self.haystack_bbox

        self.haystack_bbox.location=center

        # Scale the cube to the desired size
        self.haystack_bbox.scale = (size[0], size[1], size[2])

        # Set display type to 'BOUNDS'
        self.haystack_bbox.display_type = 'BOUNDS'

        # set material
        try:
            mat = bpy.context.scene.haystack.server_settings.mat_volume
            mat.node_tree.nodes['DomainX'].outputs[0].default_value = hsDataInit.scalars_range[0]
            mat.node_tree.nodes['DomainY'].outputs[0].default_value = hsDataInit.scalars_range[1]
        except:
            pass

        # Ensure the scale is applied correctly
        #bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)        

def register():
    bpy.utils.register_class(HayStackCreateBBoxOperator)

    bpy.types.Scene.haystack_scene = HayStackScene()
    

def unregister():
    bpy.utils.unregister_class(HayStackCreateBBoxOperator)

    del bpy.types.Scene.haystack_scene