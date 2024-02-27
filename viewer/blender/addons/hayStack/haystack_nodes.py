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


##################################################LOADING###################################################################    
# UMesh
class HayStackLoadUMeshNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadUMeshNodeType'
    bl_label = 'UMesh'
    bl_description = 'a umesh file of unstructured mesh data'

    file_path: bpy.props.StringProperty(
        name="File",
        default=""
    ) # type: ignore    
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = str(self.file_path)

    def draw_buttons(self, context, layout):
        row = layout.column(align=True)
        row.prop(self, "file_path")

# OBJ
class HayStackLoadOBJNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadOBJNodeType'
    bl_label = 'OBJ'
    bl_description = 'a OBJ file of mesh data'

    file_path: bpy.props.StringProperty(
        name="File",
        default=""
    ) # type: ignore    
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = str(self.file_path)

    def draw_buttons(self, context, layout):
        row = layout.column(align=True)
        row.prop(self, "file_path")

    
# Mini
class HayStackLoadMiniNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadMiniNodeType'
    bl_label = 'Mini'
    bl_description = 'a mini file of mesh data'

    file_path: bpy.props.StringProperty(
        name="File",
        default=""
    ) # type: ignore    
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = str(self.file_path)

    def draw_buttons(self, context, layout):
        row = layout.column(align=True)
        row.prop(self, "file_path")


#spheres://1@/cluster/priya/105000.p4:format=xyzi:radius=1
# Spheres
class HayStackLoadSpheresNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadSpheresNodeType'
    bl_label = 'Spheres'
    bl_description = 'a file of raw spheres'

    num_parts: bpy.props.IntProperty(
        name="Parts",
        default=1
    ) # type: ignore 

    file_path: bpy.props.StringProperty(
        name="File",
        default=""
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
        default='XYZ',  # Default value
    ) # type: ignore

    radius: bpy.props.FloatProperty(
        name="Radius",
        default=1
    ) # type: ignore     
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')                
    
    def updateNode(self):
        self.output_data = "spheres://" + \
            "" + str(self.num_parts) + "@" + \
            "" + str(self.file_path) + \
            ":format=" + str(self.format.lower()) + \
            ":radius=" + str(self.radius)
        
        # for link in self.outputs['Data'].links:
        #     link.to_socket.value = output       
        
    def draw_buttons(self, context, layout):
        # layout.use_property_split = True
        # layout.use_property_decorate = False  # No animation.

        row = layout.column(align=True)
        row.prop(self, "num_parts")
        row.prop(self, "file_path")
        row.prop(self, "format")
        row.prop(self, "radius")


# TSTri
class HayStackLoadTSTriNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadTSTriNodeType'
    bl_label = 'TSTri'
    bl_description = 'Tim Sandstrom type .tri files'
    
    file_path: bpy.props.StringProperty(
        name="File",
        default=""
    ) # type: ignore    
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = "ts.tri://" + \
            "" + str(self.file_path)

    def draw_buttons(self, context, layout):
        row = layout.column(align=True)
        row.prop(self, "file_path")

#raw://4@/home/wald/models/magnetic-512-volume/magnetic-512-volume.raw:format=float:dims=512,512,512
# RAWVolume
class HayStackLoadRAWVolumeNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadRAWVolumeNodeType'
    bl_label = 'RAWVolume'
    bl_description = 'a file of raw volume'
    
    num_parts: bpy.props.IntProperty(
        name="Parts",
        default=1
    ) # type: ignore 

    file_path: bpy.props.StringProperty(
        name="File",
        default=""
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
        default='FLOAT',  # Default value
    ) # type: ignore

    dims: bpy.props.IntVectorProperty(
        name="Dims",
        size=3,
        subtype='XYZ_LENGTH',
        default=(1, 1, 1)
    ) # type: ignore

    channels: bpy.props.IntProperty(
        name="Channels",
        default=1
    ) # type: ignore

    extractEnable: bpy.props.BoolProperty(
        name="Extract",
        default=False
    ) # type: ignore   

    extract: bpy.props.IntVectorProperty(
        name="Extract",
        size=3,
        subtype='XYZ_LENGTH',
        default=(0, 0, 0)
    ) # type: ignore

    isoValueEnable: bpy.props.BoolProperty(
        name="IsoValue",
        default=False
    ) # type: ignore    

    isoValue: bpy.props.FloatProperty(
        name="IsoValue",
        default=1
    ) # type: ignore    

       
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')
        self.width = 200 # Optionally adjust the default width of the node        
    
    def updateNode(self):
        self.output_data = "raw://" + \
            "" + str(self.num_parts) + "@" + \
            "" + str(self.file_path) + \
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

        row = layout.column(align=True)
        row.prop(self, "num_parts")
        row.prop(self, "file_path")
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
        default=""
    ) # type: ignore    
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = "boxes://" + \
            "" + str(self.file_path)

    def draw_buttons(self, context, layout):
        row = layout.column(align=True)
        row.prop(self, "file_path")

# Cylinders
class HayStackLoadCylindersNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadCylindersNodeType'
    bl_label = 'Cylinders'
    bl_description = 'a file of raw cylinders'
    
    file_path: bpy.props.StringProperty(
        name="File",
        default=""
    ) # type: ignore    
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = "cylinders://" + \
            "" + str(self.file_path)

    def draw_buttons(self, context, layout):
        row = layout.column(align=True)
        row.prop(self, "file_path")

# SpatiallyPartitionedUMesh
class HayStackLoadSpatiallyPartitionedUMeshNode(HayStackBaseNode):
    bl_idname = 'HayStackLoadSpatiallyPartitionedUMeshNodeType'
    bl_label = 'SpatiallyPartitionedUMesh'
    bl_description = 'spatially partitioned umeshes'
    
    file_path: bpy.props.StringProperty(
        name="File",
        default=""
    ) # type: ignore    
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')        
    
    def updateNode(self):
        self.output_data = "spumesh://" + \
            "" + str(self.file_path)

    def draw_buttons(self, context, layout):
        row = layout.column(align=True)
        row.prop(self, "file_path")

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
    ) # type: ignore

    vp: bpy.props.FloatVectorProperty(
        name="vp",
        size=3,
        subtype='XYZ',
        default=(0.0, 0.0, 0.0)
    ) # type: ignore

    vi: bpy.props.FloatVectorProperty(
        name="vi",
        size=3,
        subtype='XYZ',
        default=(0.0, 0.0, 0.0)
    ) # type: ignore    

    vu: bpy.props.FloatVectorProperty(
        name="vu",
        size=3,
        subtype='XYZ',
        default=(0.0, 1.0, 0.0)
    ) # type: ignore   

    fovy: bpy.props.FloatProperty(
        name="fovy",
        default=60.0
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
            " -fovy " + str(self.fovy)    

##################################################Utility###################################################################
class HayStackMergeNode(HayStackBaseNode):
    bl_idname = 'HayStackMergeNodeType'
    bl_label = 'Merge'
    bl_description = 'Merge'

    # Integer property to specify the number of inputs
    num_inputs: bpy.props.IntProperty(
        name="Inputs",
        default=2,
        min=2,  # Minimum value to ensure there's at least one input
        max=10, # Maximum value for practicality, adjust as needed
        update=lambda self, context: self.adjust_inputs(),
    ) # type: ignore   
    
    # def init(self, context):
    #     self.inputs.new('HayStackDataSocketType', 'Data 1')
    #     self.inputs.new('HayStackDataSocketType', 'Data 2')
    #     self.outputs.new('HayStackDataSocketType', 'Data')
    
    def initNode(self, context):
        self.adjust_inputs()
        self.outputs.new('HayStackDataSocketType', 'Data')

    def adjust_inputs(self):
        # Adjust the number of inputs to match num_inputs
        current_inputs = len(self.inputs)
        
        while current_inputs < self.num_inputs:
            current_inputs += 1
            self.inputs.new('HayStackDataSocketType', f"Data {current_inputs}")

        while current_inputs > self.num_inputs:
            self.inputs.remove(self.inputs[-1])
            current_inputs -= 1

    def draw_buttons(self, context, layout):
        col = layout.column()
        col.prop(self, "num_inputs")       
    
    def updateNode(self):
        output = ""
        for current_inputs in range(self.num_inputs):
            output = output + self.inputs[current_inputs].value
            
            if current_inputs < self.num_inputs - 1:
                output = output + " "

        self.output_data = output

        # for link in self.outputs['Data'].links:
        #     link.to_socket.value = output
        #     link.to_node.update()
##################################################Render###################################################################    
class HayStackRenderInteractiveNode(HayStackBaseNode):
    bl_idname = 'HayStackRenderInteractiveNodeType'
    bl_label = 'Interactive'
    bl_description = 'HayStack Render Interactive'
    
    def initNode(self, context):
        self.inputs.new('HayStackDataSocketType', 'Data')
    
    def updateNode(self):
        command_arg = str(self.inputs['Data'].value)
        print('Interactive', command_arg)

class HayStackRenderOfflineNode(HayStackBaseNode):
    bl_idname = 'HayStackRenderOfflineNodeType'
    bl_label = 'Offline'
    bl_description = 'HayStack Render Offline'

    image_path: bpy.props.StringProperty(
        name="Image",
        default=""
    ) # type: ignore

    resolution: bpy.props.IntVectorProperty(
        name="Resolution",
        size=2,
        subtype='XYZ',
        default=(800, 600)
    ) # type: ignore            
    
    def initNode(self, context):
        self.inputs.new('HayStackDataSocketType', 'Data')
        #self.width = 300
    
    def updateNode(self):
        command_arg = str(self.inputs['Data'].value) + \
            " -o " + str(self.image_path) + \
            " -res " + str(self.resolution[0]) + " " + str(self.resolution[1])

        print('Offline', command_arg)

    def draw_buttons(self, context, layout):
        col = layout.column()
        col.prop(self, "image_path")
        col.prop(self, "resolution")
##################################################Property###################################################################    
class HayStackPropertiesNode(HayStackBaseNode):
    bl_idname = 'HayStackPropertiesNodeType'
    bl_label = 'Properties'
    bl_description = 'HayStack Properties'


    # } else if (arg == "--num-frames") {
    #   fromCL.numFramesAccum = std::stoi(av[++i]);
    num_frames: bpy.props.IntProperty(
        name="Num. frames",
        default=1024
    ) # type: ignore  

    # } else if (arg == "-spp" || arg == "-ppp" || arg == "--paths-per-pixel") {
    #   fromCL.spp = std::stoi(av[++i]);
    paths_per_pixel: bpy.props.IntProperty(
        name="Paths per pixel",
        default=1
    ) # type: ignore     
    # } else if (arg == "-mum" || arg == "--merge-unstructured-meshes" || arg == "--merge-umeshes") {
    #   fromCL.mergeUnstructuredMeshes = true; 
    # } else if (arg == "--no-mum") {
    #   fromCL.mergeUnstructuredMeshes = false;
    merge_umeshes: bpy.props.BoolProperty(
        name="Merge umeshes",
        default=False
    ) # type: ignore       
    # } else if (arg == "--default-radius") {
    #   loader.defaultRadius = std::stof(av[++i]);
    default_radius: bpy.props.FloatProperty(
        name="Default Radius",
        default=0.1
    ) # type: ignore      
    # } else if (arg == "--measure") {
    #   fromCL.measure = true;
    measure: bpy.props.BoolProperty(
        name="Measure",
        default=False
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
        default=1
    ) # type: ignore     
    # } else if (arg == "-dpr") {
    #   fromCL.dpr = std::stoi(av[++i]);
    dpr: bpy.props.IntProperty(
        name="dpr",
        description="Data groups per rank",
        default=0
    ) # type: ignore
    # } else if (arg == "-nhn" || arg == "--no-head-node") {
    #   fromCL.createHeadNode = false; 
    # } else if (arg == "-hn" || arg == "-chn" ||
    #            arg == "--head-node" || arg == "--create-head-node") {
    #   fromCL.createHeadNode = true; 
    create_head_node: bpy.props.BoolProperty(
        name="Head node",
        default=False
    ) # type: ignore           
    
    def initNode(self, context):
        self.outputs.new('HayStackDataSocketType', 'Data')
    
    def updateNode(self):
        self.output_data = "" + \
            " --num-frames " + str(self.num_frames) + \
            " --paths-per-pixel " + str(self.paths_per_pixel) + \
            " --default-radius " + str(self.default_radius) + \
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
        NodeItem("HayStackLoadRAWVolumeNodeType"),
        NodeItem("HayStackLoadBoxesNodeType"),
        NodeItem("HayStackLoadCylindersNodeType"),
        NodeItem("HayStackLoadSpatiallyPartitionedUMeshNodeType"),
    ]),

    HayStackSceneCategory("HAYSTACK_SCENE_NODES", "Scene", items=[
        NodeItem("HayStackCameraNodeType"),
    ]),

    HayStackUtilityCategory("HAYSTACK_UTILITY_NODES", "Utility", items=[
        NodeItem("HayStackMergeNodeType"),
    ]),
    
    HayStackPropertyCategory("HAYSTACK_PROPERTY_NODES", "Property", items=[
        NodeItem("HayStackPropertiesNodeType"),
    ]),

    HayStackRenderCategory("HAYSTACK_RENDER_NODES", "Render", items=[
        NodeItem("HayStackRenderInteractiveNodeType"),
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
    HayStackLoadRAWVolumeNode,
    HayStackLoadBoxesNode,
    HayStackLoadCylindersNode,
    HayStackLoadSpatiallyPartitionedUMeshNode,

    #Render
    HayStackRenderInteractiveNode,
    HayStackRenderOfflineNode,

    #Property
    HayStackPropertiesNode,

    #Utility
    HayStackMergeNode,

    #Scene
    HayStackCameraNode,    
    ]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.haystack_tree = bpy.props.PointerProperty(type=HayStackNodeTree)

    # Register the node categories
    register_node_categories("HAYSTACK_CATEGORIES", haystack_node_categories)    

def unregister():
    # Unregister the node categories first
    unregister_node_categories("HAYSTACK_CATEGORIES")

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
    del bpy.types.Scene.haystack_tree