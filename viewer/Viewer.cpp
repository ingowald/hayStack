// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "viewer/Viewer.h"

// # define STB_IMAGE_WRITE_IMPLEMENTATION 1
// # define STB_IMAGE_IMPLEMENTATION 1
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

namespace hm {

  using owl3f = owl::common::vec3f;
  
  Viewer::Viewer(BoundsData worldBounds,
                 VolumeScatterSettings scatterSettings,
                 RenderEngineInterface *const renderer,
                 Camera defaultCamera)
    : renderer(renderer),
      scatterSettings(scatterSettings),
      worldBounds(worldBounds),
      currentCamera(defaultCamera)
  {
    xf.domain = worldBounds.scalars;
    uiIsOn = !worldBounds.scalars.empty();
    emissionXF.domain = worldBounds.scalars;
  }


  owl2i Viewer::viewportMousePosition(const owl2i &position) const
  {
    return { position.x - panelWidthFor(windowSize.x), position.y };
  }

  void Viewer::mouseMotion(const owl::common::vec2i &position) 
  {
    owl2i translated = viewportMousePosition(position);

    if (leftButton.isPressed && leftButton.shiftWhenPressed)
      return;
    
    OWLViewer::mouseMotion(translated);
  }

  void Viewer::mouseButtonLeft(const owl::common::vec2i &position,
                               bool pressed) 
  {
    owl2i translated = viewportMousePosition(position);
    
    if (leftButton.shiftWhenPressed) {
      // check if this is a click, or a release
      if (pressed) {
        owl2f relativePositionInViewport
          = (owl2f(translated)+owl2f(.5f)) / owl2f(fbSize);
        float aspect = fbSize.x / (float)fbSize.y;
        int iy = fbSize.y-1-translated.y;
        int pixelID = translated.x + fbSize.x*iy;
        relativePositionInViewport.y = 1.f-relativePositionInViewport.y;
        float depth = fbDepth[pixelID];
        if (depth > 1e20f) {
          std::cout << "pick-click on pixel, but nothing pickable there..." << std::endl;
        }
        else {
          vec3f pickedPos = currentCamera
            .reproject((const vec2f&)relativePositionInViewport,
                       depth,aspect);
           
          if (leftButton.ctrlWhenPressed) {
            std::cout << "pick pos: " << pickedPos << std::endl;
          } else {
            owl::viewer::Camera &fc = getCamera();
            fc.setOrientation((const owl3f&)currentCamera.vp,
                              (const owl3f&)pickedPos,
                              (const owl3f&)currentCamera.vu,
                    /*ignore*/1.f);
            updateCamera();
          }
        } 
      }
    }
    else
      OWLViewer::mouseButtonLeft(translated, pressed);
  }

  void Viewer::mouseButtonCenter(const owl::common::vec2i &position,
                                 bool pressed) 
  {
    OWLViewer::mouseButtonCenter(viewportMousePosition(position), pressed);
  }

  void Viewer::mouseButtonRight(const owl::common::vec2i &position,
                                bool pressed) 
  {
    OWLViewer::mouseButtonRight(viewportMousePosition(position), pressed);
  }

  int Viewer::panelWidthFor(int width) const
  {
    if (!uiIsOn)
      return 0;
    int pw = std::min(controlPanelWidth,
                      std::max(0, width - /* minimum render canvas size */320));
    return pw;
  }
  
  void Viewer::toggleUI()
  {
    uiIsOn = !uiIsOn;
    resize(windowSize);
  }
  
  void Viewer::runImgui() 
  {
    // this is in 'imgui pixels', not physical pixels!
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const float panelWidth = float(panelWidthFor(int(displaySize.x)));
    const float maxPanelWidth = std::max(300.f, displaySize.x - 320.f);

    ImGui::GetIO().ConfigWindowsResizeFromEdges = true;
      ImGui::GetBackgroundDrawList()
      ->AddImage(reinterpret_cast<void *>(fbTexture),
        /*lower*/ImVec2(panelWidth, 0.f),
        /*upper*/displaySize,
                 ImVec2(0.f, 1.f),
                 ImVec2(1.f, 0.f));
    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, displaySize.y),
                             ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(300.f, displaySize.y),
                                        ImVec2(maxPanelWidth,
                                               displaySize.y));
    ImGui::SetNextWindowBgAlpha(1.f);
    if (uiIsOn) {

      ImGui::Begin("HayStack/Imgui", nullptr,
                   ImGuiWindowFlags_NoMove
                   | ImGuiWindowFlags_NoCollapse
                   | ImGuiWindowFlags_NoSavedSettings);

      const int resizedPanelWidth = int(ImGui::GetWindowWidth() + .5f);
      if (resizedPanelWidth != controlPanelWidth) {
        controlPanelWidth = resizedPanelWidth;
        if (windowSize.x > 0 && windowSize.y > 0)
          resize(windowSize);
      }
      
      if (worldBounds.scalars.empty()) {
        ImGui::Text("(no volume data)");
      } else {
        ImGui::Text("scalar range [%f .. %f]",
                    worldBounds.scalars.lower,
                    worldBounds.scalars.upper);
        ImGui::PushID("opacityTransferFunction");
        const bool opacityChanged = xfEditor.runUI();
        ImGui::PopID();
        if (opacityChanged)
          xfDirty = true;
#if HS_USE_MULTI_SCATTERING
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Volume rendering",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          if (ImGui::Checkbox("Multi-scatter", &scatterSettings.enabled))
            scatterDirty = true;
          if (ImGui::SliderInt("Volume bounces",
                               &scatterSettings.maxVolumeBounces, 1, 64))
            scatterDirty = true;
          if (ImGui::SliderFloat("Anisotropy",
                                 &scatterSettings.medium.anisotropy,
                                 -.99f, .99f))
            scatterDirty = true;
        }
        if (ImGui::CollapsingHeader("Emission",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          if (ImGui::DragFloat("Emission strength",
                               &scatterSettings.medium.emissionStrength,
                               .001f, 0.f, 3.f, "%.4f"))
            scatterDirty = true;
          ImGui::PushID("emissionTransferFunction");
          const bool emissionChanged = emissionXfEditor.runUI(false);
          ImGui::PopID();
          if (emissionChanged)
            emissionXfDirty = true;
        }
#endif
        
        if (ImGui::SliderFloat("xfDomain.x", &xf.domain.lower,
                               worldBounds.scalars.lower,
                               worldBounds.scalars.upper))
          xfDirty = true;
        if (ImGui::SliderFloat("xfDomain.y", &xf.domain.upper, 
                               worldBounds.scalars.lower,
                               worldBounds.scalars.upper))
          xfDirty = true;
        if (ImGui::SliderFloat("baseDensity", &xf.baseDensity, 
                               0.f,200.f))
          xfDirty = true;
      }
      ImGui::End();
    }
  }
  
  void Viewer::screenShot()
  {
    std::string fileName = "hayThere.png";
    std::cout << "#ht: saving screen shot in " << fileName << std::endl;
    stbi_flip_vertically_on_write(true);
    stbi_write_png(fileName.c_str(),fbSize.x,fbSize.y,4,
                   fbPointer,fbSize.x*sizeof(uint32_t));
  }

  void Viewer::loadXF(const std::string &fileName)
  {
    auto set = hs::VolumeTransferFunctionSet::load(fileName);
    xf              = set.opacity;
    scatterSettings = set.scatterSettings;
    emissionXF      = set.emission;
    emissionXfEditor.setColorAndAlpha((const owl::common::vec4f *)emissionXF.colorMap.data(),
                                      emissionXF.colorMap.size());
    xfEditor.setColorAndAlpha((const owl::common::vec4f*)xf.colorMap.data(),
                              xf.colorMap.size());
    scatterDirty = true;
    emissionXfDirty = true;
    xfDirty = true;
  }
  
  void Viewer::key(char key, const owl::common::vec2i &where) 
  {
    switch(key) {
    case 'C': {
      hs::Camera &camera = currentCamera;
      vec3f vp = camera.vp;
      vec3f vi = camera.vi;
      vec3f vu = camera.vu;
        
      std::cout << "(C)urrent camera:" << std::endl;
      std::cout << "- from  :" << vp << std::endl;
      std::cout << "- poi   :" << vi << std::endl;
      std::cout << "- upVec :" << vu << std::endl;
      std::cout << "- dir   :" << normalize(vi-vp) << std::endl;

      if (camera.type == Camera::PERSPECTIVE) {
        std::cout << "- fovy  :" << camera.perspective.fovy << std::endl;
        std::cout << "(suggested cmdline format, for apps that support this:) "
                  << " --camera"
                  << " " << vp.x << " " << vp.y << " " << vp.z
                  << " " << vi.x << " " << vi.y << " " << vi.z
                  << " " << vu.x << " " << vu.y << " " << vu.z
                  << " -fovy " << camera.perspective.fovy
                  << std::endl;
        // printf("-dof %f %f\n",
        //        currentCamera.lensRadius,
        //        currentCamera.focalLength);
      } else {
        std::cout << "- height:" << camera.ortho.height << std::endl;
        std::cout << "(suggested cmdline format, for apps that support this:) "
                  << " --camera"
                  << " " << vp.x << " " << vp.y << " " << vp.z
                  << " " << vi.x << " " << vi.y << " " << vi.z
                  << " " << vu.x << " " << vu.y << " " << vu.z
                  << " -ortho " << camera.ortho.height
                  << std::endl;
      }
    } break;
    case 'u':
      toggleUI();
      break;
    case '!':
      screenShot();
      break;
    case '9': case '0':
    case '(': case ')': {
      char* flc = getenv("BARNEY_FOCAL_LENGTH");
      float ratio = (key == '(' || key == '9') ? (0.9f) : (1.1f);
      if (key == '9' || key == '0')
        ratio = (ratio - 1.f) * 0.1f + 1.f;
      currentCamera.focalLength *= ratio;
      renderer->setCamera(currentCamera);
      accumDirty = true;
      break;
    }
    case '{': case '}':
    case '[': case ']': {
      char* lrc = getenv("BARNEY_LENS_RADIUS");
      float ratio = (key == '[' || key == '{') ? (0.9f) : (1.1f);
      if (key == '[' || key == ']')
        ratio = (ratio - 1.f) * 0.1f + 1.f;
      if (currentCamera.lensRadius == 0.f)
        currentCamera.lensRadius
          = .001f*length(currentCamera.vi-currentCamera.vp);
      else
        currentCamera.lensRadius *= ratio;
      renderer->setCamera(currentCamera);
      accumDirty = true;
      break;
    }
    case 'T': {
      std::cout << "(T) : dumping transfer function" << std::endl;
      hs::VolumeTransferFunctionSet set;
      set.opacity = xf;
      set.emission = emissionXF;
      set.scatterSettings = scatterSettings;
      set.save("hayThere.xf");
    }; break;

    default:
      OWLViewer::key(key,where);
    };
  }

  
  void Viewer::cameraChanged() 
  {  
    hs::Camera &camera = currentCamera;
    float ignore;
    OWLViewer::getCameraOrientation
      ((owl::common::vec3f&)camera.vp,
       (owl::common::vec3f&)camera.vi,
       (owl::common::vec3f&)camera.vu,
       ignore);
    renderer->setCamera(camera);
    accumDirty = true;
  }

  void Viewer::resize(const owl::common::vec2i &_newSize) 
  {
    owl2i newSize = owl2i(rcp(uiPixelScale)*owl2f(_newSize));
    
    windowSize = _newSize;
    const int panelWidth = panelWidthFor(newSize.x);
    const owl::common::vec2i renderSize(newSize.x - panelWidth, newSize.y);
    OWLViewer::resize(renderSize);
    fbDepth.resize(renderSize.x*renderSize.y);
    renderer->resize((const mini::common::vec2i&)renderSize,
                     fbPointer,
                     fbDepth.data()
                    );
  }
  
  void Viewer::render() 
  {
    if (xfDirty) {
      auto cm = xfEditor.getColorMap();
      xf.set((const mini::vec4f*)cm.data(),
             cm.size());
      emissionXfEditor.setColorOnly((const owl::common::vec4f *)cm.data(), cm.size());
      emissionXfDirty = true;
        
      renderer->setTransferFunction(xf);
      xfDirty = false;
      accumDirty = true;
    }

    if (scatterDirty) {
      renderer->setVolumeScatterSettings(scatterSettings);
      scatterDirty = false;
      accumDirty = true;
    }
    if (emissionXfDirty) {
      auto cm = emissionXfEditor.getColorMap();
      emissionXF.set((const mini::vec4f *)cm.data(), cm.size());
      emissionXF.domain = xf.domain;
      renderer->setEmissionTransferFunction(emissionXF);
      emissionXfDirty = false;
      accumDirty = true;
    }

    if (accumDirty) {
      renderer->resetAccumulation();
      accumDirty = false;
    }

    static double t0 = mini::common::getCurrentTime();
    renderer->renderFrame();
    double t1 = mini::common::getCurrentTime();

    static double sum_t = 0.f;
    static double sum_w = 0.f;
    sum_t = 0.8f*sum_t + (t1-t0);
    sum_w = 0.8f*sum_w + 1.f;
    float timePerFrame = float(sum_t / sum_w);
    float fps = 1.f/timePerFrame;
    std::string title = "HayThere ("+mini::common::prettyDouble(fps)+"fps)";
    setTitle(title.c_str());
    t0 = t1;
  }
}
