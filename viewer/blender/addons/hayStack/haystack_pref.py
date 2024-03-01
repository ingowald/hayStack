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
import subprocess
import importlib
import sys
import os

ADDON_NAME = 'hayStack'
from collections import namedtuple

####################Dependency##############################

Dependency = namedtuple("Dependency", ["module", "package", "name"])
python_dependencies = (Dependency(module="paramiko", package="paramiko", name=None),
                       Dependency(module="scp", package="scp", name=None),
                       )

internal_dependencies = []

def import_module(module_name, global_name=None, reload=True):
    if global_name is None:
        global_name = module_name

    if global_name in globals():
        importlib.reload(globals()[global_name])
    else:
        globals()[global_name] = importlib.import_module(module_name)


def install_pip():
    try:
        if bpy.app.version < (2, 90, 0):
            python_exe = bpy.app.binary_path_python
        else:
            python_exe = sys.executable

        subprocess.run([python_exe, "-m", "pip", "--version"], check=True)

        # Upgrade
        subprocess.run([python_exe, "-m", "pip", "install",
                       "--upgrade", "pip"], check=True)

    except subprocess.CalledProcessError:
        import ensurepip

        ensurepip.bootstrap()
        os.environ.pop("PIP_REQ_TRACKER", None)


def install_and_import_module(module_name, package_name=None, global_name=None):
    if package_name is None:
        package_name = module_name

    if global_name is None:
        global_name = module_name

    environ_copy = dict(os.environ)
    environ_copy["PYTHONNOUSERSITE"] = "1"

    if bpy.app.version < (2, 90, 0):
        python_exe = bpy.app.binary_path_python
    else:
        python_exe = sys.executable

    subprocess.run([python_exe, "-m", "pip", "install",
                   package_name], check=True, env=environ_copy)

    import_module(module_name, global_name)

########################HayStack_OT##########################################

class HayStack_OT_install_dependencies(bpy.types.Operator):
    bl_idname = 'haystack.install_dependencies'
    bl_label = 'Install dependencies'

    def execute(self, context):
        try:
            install_pip()
            for dependency in python_dependencies:
                install_and_import_module(module_name=dependency.module,
                                          package_name=dependency.package,
                                          global_name=dependency.name)

        except (subprocess.CalledProcessError, ImportError) as err:
            self.report({"ERROR"}, str(err))
            return {"CANCELLED"}

        preferences().dependencies_installed = True

        self.report({'INFO'}, "'%s' finished" % (self.bl_label))
        return {"FINISHED"}


class HayStack_OT_update_dependencies(bpy.types.Operator):
    bl_idname = 'haystack.update_dependencies'
    bl_label = 'Update dependencies'

    def execute(self, context):
        try:
            install_pip()
            for dependency in python_dependencies:
                install_and_import_module(module_name=dependency.module,
                                          package_name=dependency.package,
                                          global_name=dependency.name)

        except (subprocess.CalledProcessError, ImportError) as err:
            self.report({"ERROR"}, str(err))
            return {"CANCELLED"}

        preferences().dependencies_installed = True

        self.report({'INFO'}, "'%s' finished" % (self.bl_label))
        return {"FINISHED"}


#######################HayStackPreferences#########################################

class HayStackPreferences(bpy.types.AddonPreferences):
    bl_idname = ADDON_NAME

    dependencies_installed: bpy.props.BoolProperty(
        default=False
    ) # type: ignore

    haystack_remote: bpy.props.BoolProperty(
        default=False
    ) # type: ignore

    # haystack_dir: bpy.props.StringProperty(
    #     name='HayStack Dir',
    #     description='Path to HayStack folder',
    #     subtype='DIR_PATH',
    #     default=""
    # ) # type: ignore

    # haystack_dir_remote: bpy.props.StringProperty(
    #     name='HayStack Remote Dir',
    #     description='Path to HayStack folder on the remote server',
    #     default=""
    # ) # type: ignore

    haystack_port_cam: bpy.props.IntProperty(
        name="Port Cam",
        min=0,
        max=65565,
        default=6004
    ) # type: ignore

    haystack_port_data: bpy.props.IntProperty(
        name="Port Data",
        min=0,
        max=65565,
        default=6005
    ) # type: ignore

    haystack_server_name: bpy.props.StringProperty(
        name="Server",
        default="localhost"
    ) # type: ignore

    ssh_server_name: bpy.props.StringProperty(
        name="Server for SSH",
        default="karolina"
    ) # type: ignore

    ssh_server_node_name: bpy.props.StringProperty(
        name="Server Node for SSH",
        default="localhost"
    ) # type: ignore

    def draw(self, context):
        layout = self.layout

        box = layout.box()
        box.label(text='Remote/Local:')
        col = box.column()
        col.prop(self, 'haystack_remote')        
        if self.haystack_remote:
            #col.prop(self, 'haystack_dir_remote')
            col.prop(self, 'ssh_server_name')
            col.prop(self, 'ssh_server_node_name')
        #else:
        #    col.prop(self, 'haystack_dir')

        box = layout.box()
        box.label(text='Haystack TCP Server:')
        col = box.column()
        col.prop(self, "haystack_server_name", text="Server")
        col.prop(self, "haystack_port_cam", text="Port Cam")
        col.prop(self, "haystack_port_data", text="Port Data")            

        # boxD = layout.box()
        # boxD.label(text='Dependencies:')

        # dependencies_installed = preferences().dependencies_installed
        # if not dependencies_installed:
        #     boxD.label(text='Dependencies are not installed', icon='ERROR')

        # if not dependencies_installed:
        #     boxD.operator(HayStack_OT_install_dependencies.bl_idname,
        #                   icon="CONSOLE")
        # else:
        #     boxD.operator(HayStack_OT_update_dependencies.bl_idname,
        #                   icon="CONSOLE")            

def ctx_preferences():
    try:
        return bpy.context.preferences
    except AttributeError:
        return bpy.context.user_preferences

def preferences() -> HayStackPreferences:
    return ctx_preferences().addons[ADDON_NAME].preferences

def register():
    bpy.utils.register_class(HayStackPreferences)
    bpy.utils.register_class(HayStack_OT_install_dependencies)
    bpy.utils.register_class(HayStack_OT_update_dependencies)

def unregister():
    bpy.utils.unregister_class(HayStackPreferences)
    bpy.utils.unregister_class(HayStack_OT_install_dependencies)
    bpy.utils.unregister_class(HayStack_OT_update_dependencies)
