// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "viewer/render/RenderEngineInterface.h"
#include "viewer/Viewer.h"
#include "viewer/ClientServer.h"

using namespace hm;

struct {
  Camera camera;
  hs::VolumeScatterSettings scatterSettings;
} fromCL;
  
int main(int ac, char **av)
{
  std::string server = av[1];
  int port = std::stoi(av[2]);

  HSClient remote(server,port);

  auto &worldBounds = remote.worldBounds;
  PRINT(worldBounds.spatial);
  
  if (fromCL.camera.vp == fromCL.camera.vi) {
    fromCL.camera.vp
      = worldBounds.spatial.center()
      + mini::common::vec3f(-.3f, .7f, +1.f) * worldBounds.spatial.span();
    fromCL.camera.vi = worldBounds.spatial.center();
  }
  
  Viewer viewer(worldBounds,
                fromCL.scatterSettings,
                &remote,//&world,
                fromCL.camera);

  // viewer.show();
  viewer.enableFlyMode();
  viewer.enableInspectMode();
  viewer.setWorldScale(length(worldBounds.spatial.span()));
//   viewer.setCameraOrientation
//     (/*origin   */(const owl::common::vec3f &)fromCL.camera.vp,
//      /*lookat   */(const owl::common::vec3f &)fromCL.camera.vi,
//      /*up-vector*/(const owl::common::vec3f &)fromCL.camera.vu,
// /*fovy(deg)*//*ignore now ,we handle that differently*/fromCL.camera.perspective.fovy);

//   if (!fromCL.xfFileName.empty())
//     viewer.loadXF(fromCL.xfFileName);

  viewer.showAndRun();
  
  return 0;
}
