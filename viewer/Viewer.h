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
           Camera defaultCamera,
           const std::map<std::string,VolumeTransferFunctionSet::SP>
           &loadedXFs = {});

    const BoundsData worldBounds;
    
    bool uiIsOn = true;
    int controlPanelWidth = 320;
    vec2i windowSize { 0, 0 };

    void toggleUI();
    int panelWidthFor(int width) const;
    
    void runImgui() override;
    /*! draws the imgui control panel (only called if uiIsOn) */
    void drawPanel();
    /*! draws the transfer function / volume rendering part of the
        control panel (only called if there is volume data) */
    void drawVolumeEditor();

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

    // void loadXF(const std::string &fileName);
    
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
    /*! the xfName of the volumes that the transfer function editor
        edits the transfer function of */
    std::string xfName = "xf0";
    /*! all xfNames used by the scene (from renderer->gatherXFNames()),
        in sorted order */
    std::vector<std::string> xfNames;

    /*! everything the volume editor edits for one xfName, so we can
        switch between transfer functions without losing edits */
    struct XFEditState {
      ColorMap            xfColorMap;
      ColorMap            emissionColorMap;
      hs::TransferFunction xf;
      hs::TransferFunction emissionXF;
      VolumeScatterSettings scatterSettings;
    };
    /*! stored editor state of each xfName that has been edited so far */
    std::map<std::string,XFEditState> xfEditStates;
    /*! the editor state the viewer started with; used for xfNames
        that haven't been edited yet */
    XFEditState defaultXFEditState;

    /*! creates an editor state from a loaded transfer function set */
    XFEditState makeXFEditState(const VolumeTransferFunctionSet &xfSet) const;
    /*! returns the editor's current state */
    XFEditState saveXFEditState();
    /*! sets the editor to given state, and marks everything dirty
        so it gets sent to the renderer under the current xfName */
    void loadXFEditState(const XFEditState &state);
    /*! saves state of current xfName, then switches the editor to
        the given xfName */
    void selectXF(const std::string &newXFName);
    hs::Camera currentCamera;

    std::vector<float> fbDepth;
  };

}

