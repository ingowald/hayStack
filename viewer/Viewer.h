// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "viewer/render/RenderEngineInterface.h"
#include "viewer/render/RenderEngineInterface.h"
#include "ui/GLFWBaseViewer.h"
#include "TransferFunctionEditor.h"

namespace hm {
  using namespace hs;

  struct Viewer : public viewer::GLFWBaseViewer
  {
    Viewer(BoundsData worldBounds,
           VolumeScatterSettings scatterSettings,
           RenderEngineInterface *const renderer,
           Camera defaultCamera);

    const BoundsData worldBounds;
    
    bool uiIsOn = true;
    int controlPanelWidth = 320;
    vec2i windowSize { 0, 0 };

    void toggleUI();
    int panelWidthFor(int width) const;
    
    void runImgui() override;

    vec2i viewportMousePosition(const vec2i &position) const;
    void mouseMotion(const vec2i &position) override;
    void mouseButtonLeft(const vec2i &position, bool pressed) override;
    void mouseButtonCenter(const vec2i &position, bool pressed) override;
    void mouseButtonRight(const vec2i &position, bool pressed) override;

    void screenShot();
    
    
    /*! this gets called when the user presses a key on the keyboard ... */
    void key(char key, const vec2i &where) override;

    /*! window notifies us that we got resized. We HAVE to override
      this to know our actual render dimensions, and get pointer
      to the device frame buffer that the viewer cated for us */
    void resize(const vec2i &newSize) override;

    /*! gets called whenever the viewer needs us to re-render out widget */
    void render() override;
    
    void cameraChanged() override;

    void loadXF(const std::string &fileName);
    
    //    TransferFunction xf;
    bool xfDirty = true;
    VolumeScatterSettings scatterSettings;
    bool scatterDirty = true;
    bool emissionXfDirty = true;
    bool accumDirty = true;
    RenderEngineInterface *const renderer;
    // hs::Comm::SP world;

    // the ui editor, that edits the color and alpha map (and nothing else)
    hs::viewer::TransferFunctionEditor xfEditor;
    hs::viewer::TransferFunctionEditor emissionXfEditor;
    hs::TransferFunction emissionXF;
    // the ui elements to edit abs/rel domains
    // range1f xfDomainAbs{0.f,1.f};
    // range1f xfDomainRel{0.f,100.f};
    float xfDensity = 1.f;
    // the objec that stores the _haystack_ idea of what a xf is (with
    // domain, color, and alpha)
    hs::TransferFunction xf;
    hs::Camera currentCamera;

    std::vector<float> fbDepth;
  };

}

