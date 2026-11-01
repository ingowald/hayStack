// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "GLFW/glfw3.h"
#ifdef WIN32
#include <windows.h>
#include <gl/GL.h>
#endif
// #include <cuda_runtime.h>
// #include <cuda_gl_interop.h>
#include "Camera.h"

#include <functional>

#define HSVIEWER_HAVE_DEPTH 1

namespace hs {
  namespace viewer {

    /*! base abstraction for a camera that can generate rays. For this
      viewer, we assume we're dealine with a camera that has a
      rectangular viewing plane that's in focus, and a circular (and
      possible single-point) lens for depth of field. At some later
      point this should also capture time.{t0,t1} for motion blur, but
      let's leave this out for now. */
    struct SimpleCamera
    {
      inline SimpleCamera() {}
      SimpleCamera(const Camera &camera);

      struct {
        /*! 3D *vector* that points from lens center to the lower-left
            corner of the camera's virtual image plane (or focal
            plane, when using depth of field). Note this is a
            *direction*, not the point itself, because this more
            numerically stable for certain model configs */
        vec3f lower_left;
        /*! vector that spans the horizontal (x) axis of the virtual
            image plane / focal plane */
        vec3f horizontal;
        /*! vector that spans the vertial (y) axis of the virtual
            image plane / focal plane */
        vec3f vertical;
      } focalPlane;
      struct {
        /*! 3D *point* at which the center of the camera's lens is
            located */
        vec3f center;
        /*! @{ normalized 3D vectors that span the plane that the
            camera's lens is located in */
        vec3f du;
        vec3f dv;
        /*! opening radius (aperture) of the lens, for renderers that
          can do depth of field */
        float radius { 0.f };
      } lens;
    };
    
    /*! snaps a given vector to one of the three coordinate axis;
      useful for pbrt models in which the upvector sometimes isn't
      axis-aligend */
    inline vec3f getUpVector(const vec3f &v)
    {
      int dim = arg_max(abs(v));
      vec3f up(0);
      up[dim] = v[dim] < 0.f ? -1.f : 1.f;
      return up;
    }

    // ------------------------------------------------------------------
    /*! a helper widget that opens and manages a viewer window,
      including some virtual mouse and frame buffer */
    struct GLFWBaseViewer {

      static inline vec3f getUpVector(const vec3f &v) { return hs::viewer::getUpVector(v); }

      GLFWBaseViewer(const std::string &title = "HS Sample Viewer",
                const vec2i &initWindowSize=vec2i(1200,800),
                bool visible=true,
                bool enableVsync=true);

      virtual void runImgui() {  }
      
      /*! window notifies us that we got resized */     
      virtual void resize(const vec2i &newSize);
      // virtual void resize(const vec2i &newSize, uint32_t *fbPointer);

      void setTitle(const std::string &s);

      /*! gets called whenever the viewer needs us to re-render out widget */
      virtual void render() {}
      
      /*! draw framebuffer using OpenGL */
      virtual void draw();

      struct ButtonState {
        bool  isPressed        { false };
        vec2i posFirstPressed  { -1 };
        vec2i posLastSeen      { -1 };
        bool  shiftWhenPressed { false };
        bool  ctrlWhenPressed  { false };
        bool  altWhenPressed   { false };
      };

      ButtonState leftButton;
      ButtonState rightButton;
      ButtonState centerButton;
      vec2i       lastMousePosition { -1,-1 };

      /*! this gets called when the window determines that the mouse
        got _moved_ to the given position */
      virtual void mouseMotion(const vec2i &newMousePosition);

      /*! mouse got dragged with left button pressedn, by 'delta'
          pixels, at last position where */
      virtual void mouseDragLeft  (const vec2i &where, const vec2i &delta);
      
      /*! mouse got dragged with left button pressedn, by 'delta'
          pixels, at last position where */
      virtual void mouseDragCenter(const vec2i &where, const vec2i &delta);
      
      /*! mouse got dragged with left button pressedn, by 'delta'
          pixels, at last position where */
      virtual void mouseDragRight (const vec2i &where, const vec2i &delta);
      
      /*! mouse button got either pressed or released at given location */
      virtual void mouseButtonLeft  (const vec2i &where, bool pressed);
      
      /*! mouse button got either pressed or released at given location */
      virtual void mouseButtonCenter(const vec2i &where, bool pressed);
      
      /*! mouse button got either pressed or released at given location */
      virtual void mouseButtonRight (const vec2i &where, bool pressed);

      /*! this gets called when the user presses a key on the keyboard ... */
      virtual void key(char key, const vec2i &/*where*/);
      /*! this gets called when the user presses a 'special' key on
          the keyboard (cursor keys) ... */
      virtual void special(int key, int mods, const vec2i &/*where*/);

      /*! set a new window aspect ratio for the camera, update the
        camera, and notify the app */
      void setAspect(const float aspect)
      {
        camera.setAspect(aspect);
        updateCamera();
      }

      /*! set a new orientation for the camera, update the camera, and
        notify the app */
      void setCameraOrientation(/* camera origin    : */const vec3f &origin,
                                /* point of interest: */const vec3f &interest,
                                /* up-vector        : */const vec3f &up,
                                /* fovy, in degrees : */float fovyInDegrees);
      
      void getCameraOrientation(/* camera origin    : */vec3f &origin,
                                /* point of interest: */vec3f &interest,
                                /* up-vector        : */vec3f &up,
                                /* fovy, in degrees : */float & fovyInDegrees);

      void setCameraOptions(float fovy,
                            float focalDistance);

      /*! this function gets called whenever any camera manipulator
        updates the camera. gets called AFTER all values have been updated */
      virtual void cameraChanged() {}

      /*! return currently active window size */
      vec2i getWindowSize() const { return fbSize; }
      static vec2i getScreenSize();
      
      /*! tell GLFW to set desired active window size (GLFW my choose
          something smaller if it can't fit this on screen */
      void setWindowSize(const vec2i &desiredSize) const;
      
      Camera &getCamera() { return camera; }

      /*! helper function that dumps the current frame buffer in a png
        file of given name */
      void screenShot(const std::string &fileName);
      
      
      const SimpleCamera getSimplifiedCamera() const
      {
        return SimpleCamera(camera);
      }

      std::shared_ptr<CameraManipulator> cameraManipulator;
      std::shared_ptr<CameraManipulator> inspectModeManipulator;
      std::shared_ptr<CameraManipulator> flyModeManipulator;

      void enableFlyMode();
      enum RotateMode { POI, Arcball };
      void enableInspectMode(RotateMode rm,
                             const box3f &validPoiRange=box3f(),
                             float minPoiDist=1e-3f,
                             float maxPoiDist=std::numeric_limits<float>::infinity());
      void enableInspectMode(const box3f &validPoiRange=box3f(),
                             float minPoiDist=1e-3f,
                             float maxPoiDist=std::numeric_limits<float>::infinity());
      void setWorldScale(const float worldScale)
      {
        camera.motionSpeed = worldScale / sqrtf(3.f);
      }

      /*! re-computes the 'camera' from the 'cameracontrol', and notify
          app that the camera got changed */
      void updateCamera();

      void showAndRun();
      void showAndRun(std::function<bool()> cb); // version that stops when cb() returns false
      
      void mouseButton(int button, int action, int mods);
      
      /*! the full camera state we are manipulating */
      Camera camera;

      inline vec2i getMousePos() const
      {
        double x,y;
        glfwGetCursorPos(handle,&x,&y);
        return vec2i((int)x, (int)y);
      }

      /*! stores the window manager's automatic display/pixel
          scale. on retina distplays glfw may sometimes adapt some
          automatic scaling of UI elements that is independent of the
          acutal number of pixels in the phyiscal screen */
      vec2f uiPixelScale { 1.f,1.f };
      // box2i uiViewport { { -1,-1}, {-1,-1 }};
    private:
      friend struct CameraManipulator;
      friend struct CameraInspectMode;
      friend struct CameraFlyMode;

    protected:

    
      
      vec2i    fbSize { 0,0 };
      vec2i    fbTextureSize {0,0};
      GLuint   fbTexture  {0};
      // cudaGraphicsResource_t cuDisplayTexture { 0 };
      uint32_t *fbPointer { nullptr };
      bool      fbPointerIsMine = false;
      
      /*! the glfw window handle */
      GLFWwindow *handle { nullptr };
      vec2i lastMousePos = { -1,-1 };

      /*! tracks whether we could successfully do cuda resource
          binding to the GL display texture; if not, we'll have to
          fall back to a slower path with glTexImage */
      // bool resourceSharingSuccessful;
    };

  } // ::hs::viewer
} // ::hs
