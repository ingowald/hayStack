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

from bpy.types import NodeTree, Node, NodeSocket, Panel
from bpy.utils import register_class, unregister_class
from nodeitems_utils import NodeCategory, NodeItem, register_node_categories, unregister_node_categories

from mathutils import Matrix

from pathlib import Path
import os
import platform
import re

from . import haystack_pref
from . import haystack_remote
#####################################################################################################################

# Define a custom node tree type
class HayStackNodeTree(NodeTree):
    bl_idname = 'HayStackTreeType'
    bl_label = 'HayStack Node Tree'
    bl_icon = 'NODETREE'

# Define a custom node socket type
class HayStackDataSocket(NodeSocket):
    bl_idname = 'HayStackDataSocketType'
    bl_label = 'HayStack Data Node Socket'
    
    # Socket value
    value: bpy.props.StringProperty(
    ) # type: ignore    

    # Optional: Tooltip for the socket
    def draw(self, context, layout, node, text):
        layout.label(text="Data")  # Draw the socket label
        # layout.prop(self, "value", text="Value")  # Draw a property
        pass

    # Optional: Template drawing (for more complex layouts)
    def draw_color(self, context, node):
        return (1.0, 0.4, 0.216, 1.0)  # Define the color of the socket

# Node
class HayStackBaseNode(Node):
    bl_idname = 'HayStackBaseNodeType'
    bl_label = 'BaseNode'
    bl_description = 'BaseNode'
    
    output_data: None

    def init(self, context):
        self.width = 200 # Optionally adjust the default width of the node
        self.initNode(context)

    def update(self):
        self.updateNode()

        if 'Data' in self.outputs and not self.output_data is None:
            for link in self.outputs['Data'].links:
                link.to_socket.value = self.output_data
                link.to_node.update()            

    def initNode(self, context):
        pass

    def updateNode(self):
        pass

    def get_file_path(self):
        if haystack_pref.preferences().haystack_remote:
            return str(self.file_path_remote)
        else:
            return str(bpy.path.abspath(self.file_path))
        
    def draw_file_path(self, layout):
        row = layout.column(align=True)
        if haystack_pref.preferences().haystack_remote:
            row.prop(self, "file_path_remote")
        else:
            row.prop(self, "file_path")

    def get_dir_path(self):
        if haystack_pref.preferences().haystack_remote:
            return str(self.dir_path_remote)
        else:
            return str(bpy.path.abspath(self.dir_path))
        
    def draw_dir_path(self, layout):
        row = layout.column(align=True)
        if haystack_pref.preferences().haystack_remote:
            row.prop(self, "dir_path_remote")
        else:
            row.prop(self, "dir_path")               

def update_property(self, context):
    self.update()

######################################################PANEL######################################################################
# def haystack_update_remote_files(self, context):
#     print(context.scene.haystack_remote_path)
#     #bpy.ops.haystack.update_remote_files()

class HAYSTACK_OT_update_remote_files(bpy.types.Operator):
    bl_idname = 'haystack.update_remote_files'
    bl_label = 'Update remote files'

    name : bpy.props.StringProperty(        
        default="/"
        ) # type: ignore
    
    is_directory : bpy.props.BoolProperty(
        default=True
        ) # type: ignore

    active_node: None     

    def execute(self, context):
        pref = haystack_pref.preferences()

        if self.is_directory:
            context.scene.haystack_remote_list.clear()
            context.scene.haystack_remote_list_index = -1

            if self.name == "..":
                if context.scene.haystack_remote_path[len(context.scene.haystack_remote_path) - 1] == "/":
                    context.scene.haystack_remote_path = os.path.dirname(context.scene.haystack_remote_path)

                context.scene.haystack_remote_path = os.path.dirname(context.scene.haystack_remote_path)
                context.scene.haystack_remote_path = str(context.scene.haystack_remote_path) + "/"
            else:
                divider = "/"
                if context.scene.haystack_remote_path[len(context.scene.haystack_remote_path) - 1] == "/":
                    divider = ""

                context.scene.haystack_remote_path = str(context.scene.haystack_remote_path) + divider + str(self.name)

            item = context.scene.haystack_remote_list.add()
            item.Name = ".."
            item.is_directory = True                

            #folders
            try:
                remote_file_list = haystack_remote.ssh_command_sync(pref.ssh_server_name, " ls -p " + context.scene.haystack_remote_path + " | grep -e /")
                lines = remote_file_list.split('\n')

                for line in lines:
                    if len(line) > 0:
                        item = context.scene.haystack_remote_list.add()
                        item.Name = line
                        item.is_directory = True
            except:
                pass

            #files
            try:
                remote_file_list = haystack_remote.ssh_command_sync(pref.ssh_server_name, " ls -p " + context.scene.haystack_remote_path + " | grep -v /")
                lines = remote_file_list.split('\n')

                for line in lines:
                    if len(line) > 0:
                        item = context.scene.haystack_remote_list.add()
                        item.Name = line
                        item.is_directory = False

            except:
                pass

            try:
                if context.active_node is not None and isinstance(context.active_node, HayStackBaseNode) and pref.haystack_remote:
                    context.active_node.dir_path_remote = str(context.scene.haystack_remote_path)
            except:
                pass 

        else:
            try:
                if context.active_node is not None and isinstance(context.active_node, HayStackBaseNode) and pref.haystack_remote:
                    context.active_node.file_path_remote = str(context.scene.haystack_remote_path) + str(self.name)
            except:
                pass           

        return {"FINISHED"}
    
class HAYSTACK_PG_remote_files(bpy.types.PropertyGroup):
    Name : bpy.props.StringProperty(
        name="Name"
        ) # type: ignore
    
    is_directory : bpy.props.BoolProperty(
        default=False
        ) # type: ignore    
    
class HAYSTACK_UL_remote_files(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname):
        #row = layout.row()
        #row.label(text=item.Name)
        op = layout.operator("haystack.update_remote_files", text=item.Name, icon='FILE_FOLDER' if item.is_directory else 'FILE_BLEND')
        op.name = item.Name
        op.is_directory = item.is_directory

class HAYSTACK_PT_remote_file_path_node(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Node"
    bl_label = "Remote"   

    @classmethod
    def poll(cls, context):
        pref = haystack_pref.preferences()        
        return context.active_node is not None and isinstance(context.active_node, HayStackBaseNode) and pref.haystack_remote

    def draw(self, context):
        layout = self.layout
        #node = context.active_node    

        col = layout.column()
        col.prop(context.scene, "haystack_remote_path")
        col.operator("haystack.update_remote_files")
        col.template_list("HAYSTACK_UL_remote_files", "", context.scene, "haystack_remote_list", context.scene, "haystack_remote_list_index")        

##################################################LOADING###################################################################    
# UMesh
class HayStackLoadUMeshNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadUMeshNodeType'
    bl_label = 'UMesh'
    bl_description = 'a umesh file of unstructured mesh data'

    file_path: bpy.props.StringProperty(
        name="File",
        default="",
        subtype="FILE_PATH",        
        update = update_property
    ) # type: ignore

    file_path_remote: bpy.props.StringProperty(
        name="File",
        default="",
        update = update_property
    ) # type: ignore          
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = self.get_file_path()

    def draw_buttons(self, context, layout):        
         self.draw_file_path(layout)

# OBJ
class HayStackLoadOBJNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadOBJNodeType'
    bl_label = 'OBJ'
    bl_description = 'a OBJ file of mesh data'

    file_path: bpy.props.StringProperty(
        name="File",
        subtype="FILE_PATH",        
        default="",
        update = update_property
    ) # type: ignore

    file_path_remote: bpy.props.StringProperty(
        name="File",
        default="",
        update = update_property
    ) # type: ignore          
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = self.get_file_path()

    def draw_buttons(self, context, layout):        
         self.draw_file_path(layout)

    
# Mini
class HayStackLoadMiniNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadMiniNodeType'
    bl_label = 'Mini'
    bl_description = 'a mini file of mesh data'

    file_path: bpy.props.StringProperty(
        name="File",
        default="",
        subtype="FILE_PATH",        
        update = update_property
    ) # type: ignore

    file_path_remote: bpy.props.StringProperty(
        name="File",
        default="",
        update = update_property
    ) # type: ignore      
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = self.get_file_path()

    def draw_buttons(self, context, layout):        
         self.draw_file_path(layout)


#spheres://1@/cluster/priya/105000.p4:format=xyzi:radius=1
# Spheres
class HayStackLoadSpheresNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadSpheresNodeType'
    bl_label = 'Spheres'
    bl_description = 'a file of raw spheres'

    num_parts: bpy.props.IntProperty(
        name="Parts",
        default=1,
        update = update_property
    ) # type: ignore 

    file_path: bpy.props.StringProperty(
        name="File",
        default="",
        subtype="FILE_PATH",        
        update = update_property
    ) # type: ignore

    file_path_remote: bpy.props.StringProperty(
        name="File",
        default="",
        update = update_property
    ) # type: ignore    

    format_items = [
        ('XYZ', "xyz", "Format without type specifier"),
        ('XYZF', "xyzf", "Format with floating point"),
        ('XYZI', "xyzi", "Format with integer"),        
    ]

    format: bpy.props.EnumProperty(
        name="Format",
        description="Choose the format",
        items=format_items,
        default='XYZ',
        update = update_property
    ) # type: ignore

    radius: bpy.props.FloatProperty(
        name="Radius",
        default=1,
        update = update_property
    ) # type: ignore     
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')                
    
    def updateNode(self):
        self.output_data = "spheres://" + \
            "" + str(self.num_parts) + "@" + \
            "" + self.get_file_path() + \
            ":format=" + str(self.format.lower()) + \
            ":radius=" + str(self.radius)
        
        # for link in self.outputs['Data'].links:
        #     link.to_socket.value = output       
        
    def draw_buttons(self, context, layout):
        # layout.use_property_split = True
        # layout.use_property_decorate = False  # No animation.

        self.draw_file_path(layout)
        
        row = layout.column(align=True)
        row.prop(self, "num_parts")
        row.prop(self, "format")
        row.prop(self, "radius")


# TSTri
class HayStackLoadTSTriNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadTSTriNodeType'
    bl_label = 'TSTri'
    bl_description = 'Tim Sandstrom type .tri files'
    
    file_path: bpy.props.StringProperty(
        name="File",
        default="",
        subtype="FILE_PATH",        
        update = update_property
    ) # type: ignore

    file_path_remote: bpy.props.StringProperty(
        name="File",
        default="",
        update = update_property
    ) # type: ignore          
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = "ts.tri://" + \
            "" + self.get_file_path()

    def draw_buttons(self, context, layout):
        self.draw_file_path(layout)

# NanoVDB
class HayStackLoadNanoVDBNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadNanoVDBNodeType'
    bl_label = 'NanoVDB'
    bl_description = 'NanoVDB files'
    
    file_path: bpy.props.StringProperty(
        name="File",
        default="",
        subtype="FILE_PATH",        
        update = update_property
    ) # type: ignore

    file_path_remote: bpy.props.StringProperty(
        name="File",
        default="",
        update = update_property
    ) # type: ignore          
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = "nvdb://" + \
            "" + self.get_file_path()

    def draw_buttons(self, context, layout):
        self.draw_file_path(layout)        

#raw://4@/home/wald/models/magnetic-512-volume/magnetic-512-volume.raw:format=float:dims=512,512,512
# RAWVolume
class HayStackLoadRAWVolumeNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadRAWVolumeNodeType'
    bl_label = 'RAWVolume'
    bl_description = 'a file of raw volume'
    
    num_parts: bpy.props.IntProperty(
        name="Parts",
        default=1,
        update = update_property
    ) # type: ignore 

    file_path: bpy.props.StringProperty(
        name="File",
        default="",
        subtype="FILE_PATH",        
        update = update_property
    ) # type: ignore

    file_path_remote: bpy.props.StringProperty(
        name="File",
        default="",
        update = update_property
    ) # type: ignore           

    format_items = [
        ('UINT8', "uint8", "Format uint8"),
        ('BYTE', "byte", "Format byte"),
        ('FLOAT', "float", "Format float"),
        ('F', "f", "Format float"),
        ('UINT16', "uint16", "Format uint16"),
    ]

    format: bpy.props.EnumProperty(
        name="Format",
        description="Choose the format",
        items=format_items,
        default='FLOAT',
        update = update_property
    ) # type: ignore

    dims: bpy.props.IntVectorProperty(
        name="Dims",
        size=3,
        subtype='XYZ_LENGTH',
        default=(1, 1, 1),
        update = update_property
    ) # type: ignore

    channels: bpy.props.IntProperty(
        name="Channels",
        default=1,
        update = update_property
    ) # type: ignore

    extractEnable: bpy.props.BoolProperty(
        name="Extract",
        default=False,
        update = update_property
    ) # type: ignore   

    extract: bpy.props.IntVectorProperty(
        name="Extract",
        size=3,
        subtype='XYZ_LENGTH',
        default=(0, 0, 0),
        update = update_property
    ) # type: ignore

    isoValueEnable: bpy.props.BoolProperty(
        name="IsoValue",
        default=False,
        update = update_property
    ) # type: ignore    

    isoValue: bpy.props.FloatProperty(
        name="IsoValue",
        default=1,
        update = update_property
    ) # type: ignore    

       
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')
        self.width = 200 # Optionally adjust the default width of the node        
    
    def updateNode(self):
        self.output_data = "raw://" + \
            "" + str(self.num_parts) + "@" + \
            "" + self.get_file_path() + \
            ":format=" + str(self.format.lower()) + \
            ":dims=" + str(self.dims[0]) + "," +str(self.dims[1]) + "," +str(self.dims[2]) + \
            ":channels=" + str(self.channels)
        
        if self.extractEnable:
            self.output_data = self.output_data + \
                ":extract=" + str(self.extract[0]) + "," +str(self.extract[1]) + "," +str(self.extract[2])
            
        if self.isoValueEnable:
            self.output_data = self.output_data + \
                ":isoValue=" + str(self.isoValue)
        
        # for link in self.outputs['Data'].links:
        #     link.to_socket.value = output       
        
    def draw_buttons(self, context, layout):
        # layout.use_property_split = True
        # layout.use_property_decorate = False  # No animation.

        self.draw_file_path(layout)

        row = layout.column(align=True)
        row.prop(self, "num_parts")        
        row.prop(self, "format")
        row.prop(self, "dims")
        row.prop(self, "channels") 
   
        row.prop(self, "extractEnable")
        if self.extractEnable:
            row.prop(self, "extract")

        row.prop(self, "isoValueEnable")
        if self.isoValueEnable:
            row.prop(self, "isoValue")

# Boxes
class HayStackLoadBoxesNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadBoxesNodeType'
    bl_label = 'Boxes'
    bl_description = 'a file of raw boxes'
    
    file_path: bpy.props.StringProperty(
        name="File",
        default="",
        subtype="FILE_PATH",        
        update = update_property
    ) # type: ignore    

    file_path_remote: bpy.props.StringProperty(
        name="File",
        default="",
        update = update_property
    ) # type: ignore      
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = "boxes://" + \
            "" + self.get_file_path()

    def draw_buttons(self, context, layout):
        self.draw_file_path(layout)

# Cylinders
class HayStackLoadCylindersNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadCylindersNodeType'
    bl_label = 'Cylinders'
    bl_description = 'a file of raw cylinders'
    
    file_path: bpy.props.StringProperty(
        name="File",
        default="",
        subtype="FILE_PATH",        
        update = update_property
    ) # type: ignore    

    file_path_remote: bpy.props.StringProperty(
        name="File",
        default="",
        update = update_property
    ) # type: ignore      
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = "cylinders://" + \
            "" + self.get_file_path()

    def draw_buttons(self, context, layout):
        self.draw_file_path(layout)

# SpatiallyPartitionedUMesh
class HayStackLoadSpatiallyPartitionedUMeshNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadSpatiallyPartitionedUMeshNodeType'
    bl_label = 'SpatiallyPartitionedUMesh'
    bl_description = 'spatially partitioned umeshes'
    
    file_path: bpy.props.StringProperty(
        name="File",
        default="",
        subtype="FILE_PATH",        
        update = update_property
    ) # type: ignore    

    file_path_remote: bpy.props.StringProperty(
        name="File",
        default="",
        update = update_property
    ) # type: ignore      
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = "spumesh://" + \
            "" + self.get_file_path()

    def draw_buttons(self, context, layout):
        self.draw_file_path(layout)

##################################################Scene###################################################################
def camera_poll(self, object):
    return object.type == 'CAMERA'
    
#--camera 33.7268 519.912 545.901 499.61 166.807 -72.1014 0 1 0 -fovy 60
#   fromCL.camera.vp.x = std::stof(av[++i]);
#   fromCL.camera.vp.y = std::stof(av[++i]);
#   fromCL.camera.vp.z = std::stof(av[++i]);
#   fromCL.camera.vi.x = std::stof(av[++i]);
#   fromCL.camera.vi.y = std::stof(av[++i]);
#   fromCL.camera.vi.z = std::stof(av[++i]);
#   fromCL.camera.vu.x = std::stof(av[++i]);
#   fromCL.camera.vu.y = std::stof(av[++i]);
#   fromCL.camera.vu.z = std::stof(av[++i]);

#   fromCL.camera.fovy = std::stof(av[++i]);
# Camera
class HayStackCameraNode(HayStackBaseNode):
    bl_idname = 'HayStackCameraNodeType'
    bl_label = 'Camera'
    bl_description = 'Camera'

    # Define the PointerProperty for selecting camera objects
    camera_object: bpy.props.PointerProperty(
        name="Camera",
        type=bpy.types.Object,
        poll=camera_poll,
        update = update_property
    ) # type: ignore

    vp: bpy.props.FloatVectorProperty(
        name="vp",
        size=3,
        subtype='XYZ',
        default=(0.0, 0.0, 0.0),
        update = update_property
    ) # type: ignore

    vi: bpy.props.FloatVectorProperty(
        name="vi",
        size=3,
        subtype='XYZ',
        default=(0.0, 0.0, 0.0),
        update = update_property
    ) # type: ignore    

    vu: bpy.props.FloatVectorProperty(
        name="vu",
        size=3,
        subtype='XYZ',
        default=(0.0, 1.0, 0.0),
        update = update_property
    ) # type: ignore   

    fovy: bpy.props.FloatProperty(
        name="fovy",
        default=60.0,
        update = update_property
    ) # type: ignore
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
        self.width = 200 # Optionally adjust the default width of the node
            
    def draw_buttons(self, context, layout):
        col = layout.column()
        col.prop(self, "camera_object")

        box = layout.box()
        col = box.column()
        col.prop(self, "vp", text="vp")
        col.prop(self, "vi", text="vi")
        col.prop(self, "vu", text="vu")
        col = layout.column()
        col.prop(self, "fovy", text="fovy")

    def updateNode(self):
        self.output_data = "--camera" + \
            " " +  str(self.vp[0]) + " " + str(self.vp[1]) + " " +  str(self.vp[2]) + \
            " " +  str(self.vi[0]) + " " + str(self.vi[1]) + " " +  str(self.vi[2]) + \
            " " +  str(self.vu[0]) + " " + str(self.vu[1]) + " " +  str(self.vu[2]) + \
            " -fovy " + str(round(self.fovy, 3))


#TransferFunction
class HayStackTransferFunctionNode(HayStackBaseNode):
    bl_idname = 'HayStackTransferFunctionNodeType'
    bl_label = 'TransferFunction'
    bl_description = 'TransferFunction'

    # Define the PointerProperty for selecting camera objects
    material: bpy.props.PointerProperty(
        name="Material",
        type=bpy.types.Material,
        update = update_property
    ) # type: ignore

    file_path: bpy.props.StringProperty(
        name="XF",
        default="",
        subtype="FILE_PATH",        
        update = update_property
    ) # type: ignore

    file_path_remote: bpy.props.StringProperty(
        name="File",
        default="",
        update = update_property
    ) # type: ignore      
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')
        self.width = 200
            
    def draw_buttons(self, context, layout):
        self.draw_file_path(layout)

        col = layout.column()
        col.prop(self, "material")        

    def updateNode(self):
        self.output_data = "-xf " + self.get_file_path()

##################################################Utility###################################################################
class HayStackMerge2Node(HayStackBaseNode):
    bl_idname = 'HayStackMerge2NodeType'
    bl_label = 'Merge2'
    bl_description = 'Merge2'
   
    def init(self, context):
        self.inputs.new('HayStackDataSocketType', 'Data 1')
        self.inputs.new('HayStackDataSocketType', 'Data 2')
        self.outputs.new('HayStackDataSocketType', 'Data')
        
    def updateNode(self):
        self.output_data = str(self.inputs['Data 1'].value) + " " + str(self.inputs['Data 2'].value)

class HayStackMerge4Node(HayStackBaseNode):
    bl_idname = 'HayStackMerge4NodeType'
    bl_label = 'Merge4'
    bl_description = 'Merge4'
   
    def init(self, context):
        self.inputs.new('HayStackDataSocketType', 'Data 1')
        self.inputs.new('HayStackDataSocketType', 'Data 2')
        self.inputs.new('HayStackDataSocketType', 'Data 3')
        self.inputs.new('HayStackDataSocketType', 'Data 4')
        self.outputs.new('HayStackDataSocketType', 'Data')
        
    def updateNode(self):
        self.output_data = str(self.inputs['Data 1'].value) + " " + str(self.inputs['Data 2'].value)  + " " + \
            str(self.inputs['Data 3'].value) + " " + str(self.inputs['Data 4'].value)
        
##########################################Output#################################################
class HayStackOutputImageNode(HayStackBaseNode):
    bl_idname = 'HayStackOutputImageNodeType'
    bl_label = 'Output Image'
    bl_description = 'HayStack Output Image'

    image_file_name: bpy.props.StringProperty(
        name="Name",
        default="output.png",
        subtype="FILE_NAME",
        update = update_property
    ) # type: ignore

    dir_path: bpy.props.StringProperty(
        name="Path",
        default="",
        subtype="DIR_PATH",        
        update = update_property
    ) # type: ignore

    dir_path_remote: bpy.props.StringProperty(
        name="Path",
        default="",
        update = update_property
    ) # type: ignore        

    resolution: bpy.props.IntVectorProperty(
        name="Resolution",
        size=2,
        subtype='XYZ',
        default=(800, 600),
        update = update_property
    ) # type: ignore            
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')  
    
    def updateNode(self):
        self.output_data = "" + \
            " -o " + self.get_dir_path() + "/" + str(self.image_file_name)  + \
            " -res " + str(self.resolution[0]) + " " + str(self.resolution[1])        

    def draw_buttons(self, context, layout):
        col = layout.column()
        col.prop(self, "image_file_name")

        self.draw_dir_path(layout)

        col = layout.column()
        col.prop(self, "resolution")                
##################################################Render###################################################################
def replace_drive_substrings(input_string):
    if platform.system() == 'Windows':
        ## pattern @
        pattern1 = r"@([a-zA-Z]):"
        def repl_func1(match):
            return f"@{match.group(1)}$"        
        result_string = re.sub(pattern1, repl_func1, input_string)

        ## pattern /
        pattern2 = r"/([a-zA-Z]):"
        def repl_func2(match):
            return f"/{match.group(1)}$"        
        result_string = re.sub(pattern2, repl_func2, result_string)
        
        return result_string
    else:
        return input_string
        
class HayStackRenderBaseNode(HayStackBaseNode):
    bl_idname = 'HayStackRenderBaseNodeType'
    bl_label = 'RenderBase'
    bl_description = 'HayStack Render'
    
    file_path: bpy.props.StringProperty(
        name="Path",
        default="",
        subtype="FILE_PATH",        
        update = update_property
    ) # type: ignore

    file_path_remote: bpy.props.StringProperty(
        name="Path",
        default="",
        update = update_property
    ) # type: ignore        
    
    def initNode(self, context):
        self.inputs.new('HayStackDataSocketType', 'Data')
    
    def updateNode(self):
        input_value = replace_drive_substrings(self.inputs['Data'].value)
        haystack_path = self.get_file_path() + " "
        command_arg = haystack_path + str(input_value)

        print(command_arg)
        self.output_data = command_arg

        text_block_name = self.bl_label + ".txt"

        # Check if the text block already exists
        if text_block_name in bpy.data.texts:
            # If it exists, just update its content
            text_block = bpy.data.texts[text_block_name]
            text_block.clear()  # Clear existing content
        else:
            # If it does not exist, create a new text block
            text_block = bpy.data.texts.new(name=text_block_name)

        # Write the content to the text block
        text_block.write(command_arg)        

    def draw_buttons(self, context, layout):
        self.draw_file_path(layout)  

class HayStackRenderBlenderNode(HayStackRenderBaseNode):
    bl_idname = 'HayStackRenderBlenderNodeType'
    bl_label = 'hsBlender'
    bl_description = 'HayStack Render Blender'

class HayStackRenderViewerNode(HayStackRenderBaseNode):
    bl_idname = 'HayStackRenderViewerNodeType'
    bl_label = 'hsViewer'
    bl_description = 'HayStack Render hsViewer'
    
class HayStackRenderViewerQTNode(HayStackRenderBaseNode):
    bl_idname = 'HayStackRenderViewerQTNodeType'
    bl_label = 'hsViewerQT'
    bl_description = 'HayStack Render hsViewerQT'

class HayStackRenderOfflineNode(HayStackRenderBaseNode):
    bl_idname = 'HayStackRenderOfflineNodeType'
    bl_label = 'hsOffline'
    bl_description = 'HayStack Render hsOffline'

##################################################Property###################################################################    
class HayStackPropertiesNode(HayStackBaseNode):
    bl_idname = 'HayStackPropertiesNodeType'
    bl_label = 'Properties'
    bl_description = 'HayStack Properties'


    # } else if (arg == "--num-frames") {
    #   fromCL.numFramesAccum = std::stoi(av[++i]);
    num_frames: bpy.props.IntProperty(
        name="Num. frames",
        default=1024,
        update = update_property
    ) # type: ignore  

    # } else if (arg == "-spp" || arg == "-ppp" || arg == "--paths-per-pixel") {
    #   fromCL.spp = std::stoi(av[++i]);
    paths_per_pixel: bpy.props.IntProperty(
        name="Paths per pixel",
        default=1,
        update = update_property
    ) # type: ignore     
    # } else if (arg == "-mum" || arg == "--merge-unstructured-meshes" || arg == "--merge-umeshes") {
    #   fromCL.mergeUnstructuredMeshes = true; 
    # } else if (arg == "--no-mum") {
    #   fromCL.mergeUnstructuredMeshes = false;
    merge_umeshes: bpy.props.BoolProperty(
        name="Merge umeshes",
        default=False,
        update = update_property
    ) # type: ignore       
    # } else if (arg == "--default-radius") {
    #   loader.defaultRadius = std::stof(av[++i]);
    default_radius: bpy.props.FloatProperty(
        name="Default Radius",
        default=0.1,
        update = update_property,
    ) # type: ignore      
    # } else if (arg == "--measure") {
    #   fromCL.measure = true;
    measure: bpy.props.BoolProperty(
        name="Measure",
        default=False,
        update = update_property
    ) # type: ignore      
    # } else if (arg == "-o") {
    #   fromCL.outFileName = av[++i];
    # } else if (arg == "--camera") {
    #   fromCL.camera.vp.x = std::stof(av[++i]);
    #   fromCL.camera.vp.y = std::stof(av[++i]);
    #   fromCL.camera.vp.z = std::stof(av[++i]);
    #   fromCL.camera.vi.x = std::stof(av[++i]);
    #   fromCL.camera.vi.y = std::stof(av[++i]);
    #   fromCL.camera.vi.z = std::stof(av[++i]);
    #   fromCL.camera.vu.x = std::stof(av[++i]);
    #   fromCL.camera.vu.y = std::stof(av[++i]);
    #   fromCL.camera.vu.z = std::stof(av[++i]);
    # } else if (arg == "-fovy") {
    #   fromCL.camera.fovy = std::stof(av[++i]);
    # } else if (arg == "-xf") {
    #   fromCL.xfFileName = av[++i];
    # } else if (arg == "-res") {
    #   fromCL.fbSize.x = std::stoi(av[++i]);
    #   fromCL.fbSize.y = std::stoi(av[++i]);
    # } else if (arg == "-ndg") {
    #   fromCL.ndg = std::stoi(av[++i]);
    ndg: bpy.props.IntProperty(
        name="ndg",
        description="Num data groups",
        default=1,
        update = update_property
    ) # type: ignore     
    # } else if (arg == "-dpr") {
    #   fromCL.dpr = std::stoi(av[++i]);
    dpr: bpy.props.IntProperty(
        name="dpr",
        description="Data groups per rank",
        default=0,
        update = update_property
    ) # type: ignore
    # } else if (arg == "-nhn" || arg == "--no-head-node") {
    #   fromCL.createHeadNode = false; 
    # } else if (arg == "-hn" || arg == "-chn" ||
    #            arg == "--head-node" || arg == "--create-head-node") {
    #   fromCL.createHeadNode = true; 
    create_head_node: bpy.props.BoolProperty(
        name="Head node",
        default=False,
        update = update_property
    ) # type: ignore           
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')
    
    def updateNode(self):
        self.output_data = "" + \
            " --num-frames " + str(self.num_frames) + \
            " --paths-per-pixel " + str(self.paths_per_pixel) + \
            " --default-radius " + str(round(self.default_radius, 7)) + \
            " -ndg " + str(self.ndg) + \
            " -dpr " + str(self.dpr)
        
        if self.merge_umeshes:
            self.output_data = self.output_data + " --merge-umeshes "
        else:
            self.output_data = self.output_data + " --no-mum "

        if self.measure:
            self.output_data = self.output_data + " --measure "

        if self.create_head_node:
            self.output_data = self.output_data + " --create-head-node "                

    def draw_buttons(self, context, layout):
        col = layout.column()
        col.prop(self, "num_frames")
        col.prop(self, "paths_per_pixel")
        col.prop(self, "merge_umeshes")
        col.prop(self, "default_radius")
        col.prop(self, "measure")
        col.prop(self, "ndg")
        col.prop(self, "dpr")
        col.prop(self, "create_head_node")
##################################################CATEGORY###################################################################    
# Define a new node category
class HayStackDataLoaderCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'HayStackTreeType'

class HayStackSceneCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'HayStackTreeType'
    
class HayStackUtilityCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'HayStackTreeType'

class HayStackPropertyCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'HayStackTreeType'
    
class HayStackOutputCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'HayStackTreeType'    
    
class HayStackRenderCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'HayStackTreeType'    

# List of node categories in a tuple (identifier, name, description, items)
haystack_node_categories = [
    HayStackDataLoaderCategory("HAYSTACK_DATALOADER_NODES", "DataLoader", items=[
        NodeItem("HayStackLoadUMeshNodeType"),
        NodeItem("HayStackLoadOBJNodeType"),
        NodeItem("HayStackLoadMiniNodeType"),
        NodeItem("HayStackLoadSpheresNodeType"),
        NodeItem("HayStackLoadTSTriNodeType"),
        NodeItem("HayStackLoadNanoVDBNodeType"),
        NodeItem("HayStackLoadRAWVolumeNodeType"),        
        NodeItem("HayStackLoadBoxesNodeType"),
        NodeItem("HayStackLoadCylindersNodeType"),
        NodeItem("HayStackLoadSpatiallyPartitionedUMeshNodeType"),
    ]),

    HayStackSceneCategory("HAYSTACK_SCENE_NODES", "Scene", items=[
        NodeItem("HayStackCameraNodeType"),
        NodeItem("HayStackTransferFunctionNodeType"),        
    ]),

    HayStackUtilityCategory("HAYSTACK_UTILITY_NODES", "Utility", items=[
        NodeItem("HayStackMerge2NodeType"),
        NodeItem("HayStackMerge4NodeType"),
    ]),
    
    HayStackPropertyCategory("HAYSTACK_PROPERTY_NODES", "Property", items=[
        NodeItem("HayStackPropertiesNodeType"),
    ]),

    HayStackOutputCategory("HAYSTACK_OUTPUT_NODES", "Output", items=[
        NodeItem("HayStackOutputImageNodeType"),
    ]),    

    HayStackRenderCategory("HAYSTACK_RENDER_NODES", "Render", items=[
        NodeItem("HayStackRenderBlenderNodeType"),
        NodeItem("HayStackRenderViewerNodeType"),
        NodeItem("HayStackRenderViewerQTNodeType"),
        NodeItem("HayStackRenderOfflineNodeType"),
    ]),    
]

#####################################################################################################################    

# Registering
classes = [
    HayStackNodeTree, 
    HayStackDataSocket,
    HayStackBaseNode,

    #Loading
    HayStackLoadUMeshNode,
    HayStackLoadOBJNode,
    HayStackLoadMiniNode,
    HayStackLoadSpheresNode,
    HayStackLoadTSTriNode,
    HayStackLoadNanoVDBNode,
    HayStackLoadRAWVolumeNode,
    HayStackLoadBoxesNode,
    HayStackLoadCylindersNode,
    HayStackLoadSpatiallyPartitionedUMeshNode,

    #Render
    HayStackRenderBlenderNode,
    HayStackRenderViewerNode,
    HayStackRenderViewerQTNode,
    HayStackRenderOfflineNode,

    #Property
    HayStackPropertiesNode,

    #Utility
    HayStackMerge2Node,
    HayStackMerge4Node,

    #Scene
    HayStackCameraNode,
    HayStackTransferFunctionNode,

    #Output
    HayStackOutputImageNode,

    #Other
    HAYSTACK_OT_update_remote_files,
    HAYSTACK_PG_remote_files,
    HAYSTACK_UL_remote_files,
    HAYSTACK_PT_remote_file_path_node,
    ]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.haystack_tree = bpy.props.PointerProperty(type=HayStackNodeTree)

    # Register the node categories
    register_node_categories("HAYSTACK_CATEGORIES", haystack_node_categories)

    bpy.types.Scene.haystack_remote_list = bpy.props.CollectionProperty(type=HAYSTACK_PG_remote_files)
    bpy.types.Scene.haystack_remote_list_index = bpy.props.IntProperty(default=-1)
    bpy.types.Scene.haystack_remote_path = bpy.props.StringProperty(name="Remote path", default="/")

def unregister():
    # Unregister the node categories first
    unregister_node_categories("HAYSTACK_CATEGORIES")

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
    del bpy.types.Scene.haystack_tree

    del bpy.types.Scene.haystack_remote_list
    del bpy.types.Scene.haystack_remote_list_index    
    del bpy.types.Scene.haystack_remote_path