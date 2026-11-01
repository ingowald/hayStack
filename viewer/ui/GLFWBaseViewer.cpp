// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "GLFWBaseViewer.h"
#include "Camera.h"
#include "InspectMode.h"
#include "FlyMode.h"
#include <sstream>

// eventually to go into 'apps/'
// #define STB_IMAGE_WRITE_IMPLEMENTATION 1
#include "stb/stb_image_write.h"

namespace hs {
  namespace viewer {

    inline const char* getGLErrorString( GLenum error )
    {
      switch( error ) {
      case GL_NO_ERROR:            return "No error";
      case GL_INVALID_ENUM:        return "Invalid enum";
      case GL_INVALID_VALUE:       return "Invalid value";
      case GL_INVALID_OPERATION:   return "Invalid operation";
        //case GL_STACK_OVERFLOW:      return "Stack overflow";
        //case GL_STACK_UNDERFLOW:     return "Stack underflow";
      case GL_OUT_OF_MEMORY:       return "Out of memory";
        //case GL_TABLE_TOO_LARGE:     return "Table too large";
      default:                     return "Unknown GL error";
      }
    }

#define DO_GL_CHECK
#ifdef DO_GL_CHECK
#    define GL_CHECK( call )                                            \
    do                                                                  \
      {                                                                 \
        call;                                                           \
        GLenum err = glGetError();                                      \
        if( err != GL_NO_ERROR )                                        \
          {                                                             \
            std::stringstream ss;                                       \
            ss << "GL error " <<  getGLErrorString( err ) << " at "     \
               << __FILE__  << "(" <<  __LINE__  << "): " << #call      \
               << std::endl;                                            \
            std::cerr << ss.str() << std::endl;                         \
            throw std::runtime_error( ss.str().c_str() );               \
          }                                                             \
      }                                                                 \
    while (0)


#    define GL_CHECK_ERRORS( )                                          \
    do                                                                  \
      {                                                                 \
        GLenum err = glGetError();                                      \
        if( err != GL_NO_ERROR )                                        \
          {                                                             \
            std::stringstream ss;                                       \
            ss << "GL error " <<  getGLErrorString( err ) << " at "     \
               << __FILE__  << "(" <<  __LINE__  << ")";                \
            std::cerr << ss.str() << std::endl;                         \
            throw std::runtime_error( ss.str().c_str() );               \
          }                                                             \
      }                                                                 \
    while (0)

#else
#    define GL_CHECK( call )   do { call; } while(0)
#    define GL_CHECK_ERRORS( ) do { ;     } while(0)
#endif

    void initGLFW()
    {
      // cudaFree(0);
      static bool alreadyInitialized = false;
      if (alreadyInitialized) return;
      if (!glfwInit())
        exit(EXIT_FAILURE);
      // std::cout << "#hs.viewer: glfw initialized" << std::endl;
      alreadyInitialized = true;
    }

    /*! helper function that dumps the current frame buffer in a png
      file of given name */
    void GLFWBaseViewer::screenShot(const std::string &fileName)
    {
      const uint32_t *fb
        = (const uint32_t*)fbPointer;

      std::vector<uint32_t> pixels;
      for (int y=0;y<fbSize.y;y++) {
        const uint32_t *line = fb + (fbSize.y-1-y)*fbSize.x;
        for (int x=0;x<fbSize.x;x++) {
          pixels.push_back(line[x] | (0xff << 24));
        }
      }
      stbi_write_png(fileName.c_str(),fbSize.x,fbSize.y,4,
                     pixels.data(),fbSize.x*sizeof(uint32_t));
      std::cout << "#hs.viewer: frame buffer written to " << fileName << std::endl;
    }

    vec2i GLFWBaseViewer::getScreenSize()
    {
      initGLFW();
      int numModes = 0;
      auto monitor = glfwGetPrimaryMonitor();
      if (!monitor)
        throw std::runtime_error("could not query monitor...");
      const GLFWvidmode *modes
        = glfwGetVideoModes(monitor, &numModes);
      vec2i size(0,0);
      for (int i=0; i<numModes; i++)
        size = max(size,vec2i(modes[i].width,modes[i].height));
      return size;
    }

    float computeStableEpsilon(float f)
    {
      return abs(f) * float(1./(1<<21));
    }

    float computeStableEpsilon(const vec3f v)
    {
      return max(max(computeStableEpsilon(v.x),
                     computeStableEpsilon(v.y)),
                 computeStableEpsilon(v.z));
    }

    SimpleCamera::SimpleCamera(const Camera &camera)
    {
      auto &easy = *this;
      easy.lens.center = camera.position;
      easy.lens.radius = 0.f;
      easy.lens.du     = camera.frame.vx;
      easy.lens.dv     = camera.frame.vy;
      const float minFocalDistance
        = max(computeStableEpsilon(camera.position),
              computeStableEpsilon(camera.frame.vx));

      /*
        tan(fov/2) = (height/2) / dist
        -> height = 2*tan(fov/2)*dist
      */
      float screen_height
        = 2.f*tanf(camera.fovyInDegrees/2.f * (float)M_PI/180.f)
        * max(minFocalDistance,camera.focalDistance);
      easy.focalPlane.vertical   = screen_height * camera.frame.vy;
      easy.focalPlane.horizontal = screen_height * camera.aspect * camera.frame.vx;
      easy.focalPlane.lower_left
        =
        /* NEGATIVE z axis! */
        - max(minFocalDistance,camera.focalDistance) * camera.frame.vz
        - 0.5f * easy.focalPlane.vertical
        - 0.5f * easy.focalPlane.horizontal;
    }

    // ==================================================================
    // actual viewerwidget class
    // ==================================================================


//     void GLFWBaseViewer::resize(const vec2i &newSize, uint32_t *new_fbPointer)
//     {
//       glfwMakeContextCurrent(handle);
//       glfwFocusWindow(handle);
//       if (fbPointerIsMine) { 
//         delete[] fbPointer;
//         fbPointer = 0;
//         fbPointerIsMine = false;
//       }
//       fbPointer = new_fbPointer;
//       fbSize = newSize;
// #if 0
//       // we now create/resize the textre on 'draw'
//       if (fbTexture == 0) {
//         GL_CHECK(glGenTextures(1, &fbTexture));
//       }

//       GL_CHECK(glBindTexture(GL_TEXTURE_2D, fbTexture));
//       GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
//                             newSize.x, newSize.y,
//                             0, GL_RGBA,
//                             GL_UNSIGNED_BYTE, nullptr));
// #endif
//       setAspect(fbSize.x/float(fbSize.y));
//     }
    
    void GLFWBaseViewer::resize(const vec2i &newSize)
    {
      glfwMakeContextCurrent(handle);
      glfwFocusWindow(handle);
      if (fbPointerIsMine) { 
        delete[] fbPointer;
        fbPointer = 0;
        fbPointerIsMine = false;
      }
      fbPointer = new uint32_t[newSize.x*newSize.y];
      fbPointerIsMine = true;
      fbSize = newSize;
#if 0
      // we now create/resize the textre on 'draw'
      if (fbTexture == 0) {
        GL_CHECK(glGenTextures(1, &fbTexture));
      }

      GL_CHECK(glBindTexture(GL_TEXTURE_2D, fbTexture));
      GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                            newSize.x, newSize.y,
                            0, GL_RGBA,
                            GL_UNSIGNED_BYTE, nullptr));
#endif
      setAspect(fbSize.x/float(fbSize.y));
    }

    /*! re-draw the current frame. This function itself isn't
      virtual, but it calls the framebuffer's render(), which
      is */
    void GLFWBaseViewer::draw()
    {
      glfwMakeContextCurrent(handle);
      glEnable(GL_TEXTURE_2D);

#if 1
      if (fbTexture == 0) {
        GL_CHECK(glGenTextures(1, &fbTexture));
      }

      GL_CHECK(glBindTexture(GL_TEXTURE_2D, fbTexture));
      if (fbSize != fbTextureSize) {
        GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                              fbSize.x, fbSize.y,
                              0, GL_RGBA,
                              GL_UNSIGNED_BYTE, nullptr));
        fbTextureSize = fbSize;
      }
#endif

      
      GL_CHECK(glBindTexture(GL_TEXTURE_2D, fbTexture));
      // cudaDeviceSynchronize();
      GL_CHECK(glTexSubImage2D(GL_TEXTURE_2D,0,
			       0,0,
			       fbSize.x, fbSize.y,
			       GL_RGBA, GL_UNSIGNED_BYTE, fbPointer));
#if 0
      // cudaDeviceSynchronize();

      glDisable(GL_LIGHTING);
      glColor3f(1, 1, 1);

      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();

      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, fbTexture);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      glDisable(GL_DEPTH_TEST);

      // PRINT(uiViewport);
      
      // if (uiViewport.lower.x != -1)
      //   glViewport(uiViewport.lower.x, uiViewport.lower.y,
      //              uiViewport.upper.x, uiViewport.upper.y);
      // else
        glViewport(0, 0, fbSize.x, fbSize.y);

      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      // if (uiViewport.lower.x != -1)
      //   glOrtho((float)uiViewport.lower.x,
      //           (float)uiViewport.upper.x,
      //           (float)uiViewport.lower.y,
      //           (float)uiViewport.upper.y,
      //           -1.f, 1.f);
      // else
        glOrtho(0.f, (float)fbSize.x, 0.f, (float)fbSize.y, -1.f, 1.f);
      

      glBegin(GL_QUADS);
      {
	glTexCoord2f(0.f, 0.f);
	glVertex3f(0.f, 0.f, 0.f);

	glTexCoord2f(0.f, 1.f);
	glVertex3f(0.f, (float)fbSize.y, 0.f);

	glTexCoord2f(1.f, 1.f);
	glVertex3f((float)fbSize.x, (float)fbSize.y, 0.f);

	glTexCoord2f(1.f, 0.f);
	glVertex3f((float)fbSize.x, 0.f, 0.f);
      }
      glEnd();
#endif
      glFlush();
    }

    /*! re-computes the 'camera' from the 'cameracontrol', and notify
      app that the camera got changed */
    void GLFWBaseViewer::updateCamera()
    {
      // camera.digestInto(simpleCamera);
      // if (isActive)
      camera.lastModified = getCurrentTime();
    }

    void GLFWBaseViewer::enableInspectMode(RotateMode rm,
				      const box3f &validPoiRange,
				      float minPoiDist,
				      float maxPoiDist)
    {
      inspectModeManipulator
        = std::make_shared<CameraInspectMode>
        (this,validPoiRange,minPoiDist,maxPoiDist,
         rm==POI? CameraInspectMode::POI: CameraInspectMode::Arcball);
      cameraManipulator = inspectModeManipulator;
    }

    void GLFWBaseViewer::enableInspectMode(const box3f &validPoiRange,
                                      float minPoiDist,
                                      float maxPoiDist)
    {
      enableInspectMode(POI,validPoiRange,minPoiDist,maxPoiDist);
    }

    void GLFWBaseViewer::enableFlyMode()
    {
      flyModeManipulator
        = std::make_shared<CameraFlyMode>(this);
      cameraManipulator = flyModeManipulator;
    }

    /*! this gets called when the window determines that the mouse got
      _moved_ to the given position */
    void GLFWBaseViewer::mouseMotion(const vec2i &newMousePosition)
    {
      if (lastMousePosition != vec2i(-1)) {
        if (leftButton.isPressed && leftButton.ctrlWhenPressed)
          mouseDragCenter(newMousePosition,newMousePosition-lastMousePosition);
        else if (leftButton.isPressed)
          mouseDragLeft  (newMousePosition,newMousePosition-lastMousePosition);
        else if (centerButton.isPressed)
          mouseDragCenter(newMousePosition,newMousePosition-lastMousePosition);
        else if (rightButton.isPressed)
          mouseDragRight (newMousePosition,newMousePosition-lastMousePosition);
      }
      lastMousePosition = newMousePosition;
    }

    void GLFWBaseViewer::mouseDragLeft  (const vec2i &where, const vec2i &delta)
    {
      if (cameraManipulator) cameraManipulator->mouseDragLeft(where,delta);
    }

    /*! mouse got dragged with left button pressedn, by 'delta'
      pixels, at last position where */
    void GLFWBaseViewer::mouseDragCenter(const vec2i &where, const vec2i &delta)
    {
      if (cameraManipulator) cameraManipulator->mouseDragCenter(where,delta);
    }

    /*! mouse got dragged with left button pressedn, by 'delta'
      pixels, at last position where */
    void GLFWBaseViewer::mouseDragRight (const vec2i &where, const vec2i &delta)
    {
      if (cameraManipulator) cameraManipulator->mouseDragRight(where,delta);
    }

    /*! mouse button got either pressed or released at given
      location */
    void GLFWBaseViewer::mouseButtonLeft  (const vec2i &where, bool pressed)
    {
      if (cameraManipulator) cameraManipulator->mouseButtonLeft(where,pressed);
      
      lastMousePosition = where;
    }

    /*! mouse button got either pressed or released at given location */
    void GLFWBaseViewer::mouseButtonCenter(const vec2i &where, bool pressed)
    {
      if (cameraManipulator) cameraManipulator->mouseButtonCenter(where,pressed);

      lastMousePosition = where;
    }

    /*! mouse button got either pressed or released at given location */
    void GLFWBaseViewer::mouseButtonRight (const vec2i &where, bool pressed)
    {
      if (cameraManipulator) cameraManipulator->mouseButtonRight(where,pressed);

      lastMousePosition = where;
    }

    /*! this gets called when the user presses a key on the keyboard ... */
    void GLFWBaseViewer::key(char key, const vec2i &where)
    {
      if (cameraManipulator) cameraManipulator->key(key,where);
    }

    /*! this gets called when the user presses a key on the keyboard ... */
    void GLFWBaseViewer::special(int key, int mods, const vec2i &where)
    {
      if (cameraManipulator) cameraManipulator->special(key,where);
    }


    static void glfw_error_callback(int error, const char* description)
    {
      fprintf(stderr, "Error: %s\n", description);
    }


    void GLFWBaseViewer::setTitle(const std::string &s)
    {
      glfwSetWindowTitle(handle,s.c_str());
    }

    GLFWBaseViewer::GLFWBaseViewer(const std::string &title,
                         const vec2i &initWindowSize,
                         bool visible, bool enableVsync)
    {
      glfwSetErrorCallback(glfw_error_callback);

      initGLFW();

      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
      glfwWindowHint(GLFW_VISIBLE, visible);
      glfwWindowHint(GLFW_FOCUSED, GL_TRUE);
 
      // Select GL version + let the backend select a GLSL version
      const char* glsl_version = nullptr;
#if defined(IMGUI_IMPL_OPENGL_ES2)
      // GL ES 2.0 + GLSL 100 (WebGL 1.0)
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
      glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
      // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
      glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
      // GL 3.2 + generally GLSL 150
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
      glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
      glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
      // GL 3.0 + generally GLSL 130
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
      //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
      //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

      // Create window with graphics context
      float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only

      handle = glfwCreateWindow(initWindowSize.x, initWindowSize.y,
                                title.c_str(), NULL, NULL);

      glfwGetWindowContentScale(handle, &uiPixelScale.x, &uiPixelScale.y);
 
      if (!handle) {
        glfwTerminate();
        exit(EXIT_FAILURE);
      }

      glfwSetWindowUserPointer(handle, this);
      glfwMakeContextCurrent(handle);
      glfwSwapInterval( (enableVsync) ? 1 : 0 );

      // Setup Dear ImGui context
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      ImGuiIO& io = ImGui::GetIO(); (void)io;
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

      // Setup Dear ImGui style
      ImGui::StyleColorsDark();
      //ImGui::StyleColorsLight();

      // Setup scaling
      ImGuiStyle& style = ImGui::GetStyle();
      style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
      style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

      // Setup Platform/Renderer backends
      ImGui_ImplGlfw_InitForOpenGL(handle, false);
      // ImGui_ImplGlfw_InitForOpenGL(window, true);
      // #ifdef __EMSCRIPTEN__
      //   ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
      // #endif
      ImGui_ImplOpenGL3_Init(glsl_version);

      
    }


    /*! callback for a window resizing event */
    static void glfwindow_reshape_cb(GLFWwindow* window, int width, int height )
    {
      GLFWBaseViewer *gw = static_cast<GLFWBaseViewer*>(glfwGetWindowUserPointer(window));
      assert(gw);
      gw->resize(vec2i(width,height));
    }

    /*! callback for a key press */
    static void glfwindow_char_cb(GLFWwindow *window,
                                  unsigned int key)
    {
      GLFWBaseViewer *gw = static_cast<GLFWBaseViewer*>(glfwGetWindowUserPointer(window));
      assert(gw);
      gw->key(key,gw->getMousePos());

    }

    /*! callback for a key press */
    static void glfwindow_key_cb(GLFWwindow *window,
                                 int key,
                                 int scancode,
                                 int action,
                                 int mods)
    {
      GLFWBaseViewer *gw = static_cast<GLFWBaseViewer*>(glfwGetWindowUserPointer(window));
      assert(gw);
      if (action == GLFW_PRESS) {
        gw->special(key,mods,gw->getMousePos());
      }
    }

    /*! callback for _moving_ the mouse to a new position */
    static void glfwindow_mouseMotion_cb(GLFWwindow *window, double x, double y)
    {
      GLFWBaseViewer *gw = static_cast<GLFWBaseViewer*>(glfwGetWindowUserPointer(window));
      assert(gw);
      gw->mouseMotion(vec2i((int)x, (int)y));
    }

    /*! callback for pressing _or_ releasing a mouse button*/
    static void glfwindow_mouseButton_cb(GLFWwindow *window,
                                         int button,
                                         int action,
                                         int mods)
    {
      GLFWBaseViewer *gw = static_cast<GLFWBaseViewer*>(glfwGetWindowUserPointer(window));
      assert(gw);
      
      ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
      
      // Check if ImGui wants to capture the mouse
      ImGuiIO& io = ImGui::GetIO();
      if (io.WantCaptureMouse) {
        return; // ImGui handled the input; do not let application logic run
      }
  
      gw->mouseButton(button,action,mods);
    }

    void GLFWBaseViewer::mouseButton(int button, int action, int mods)
    {
      const bool pressed = (action == GLFW_PRESS);
      lastMousePos = getMousePos();
      switch(button) {
      case GLFW_MOUSE_BUTTON_LEFT:
        leftButton.isPressed        = pressed;
        leftButton.shiftWhenPressed = (mods & GLFW_MOD_SHIFT  );
        leftButton.ctrlWhenPressed  = (mods & GLFW_MOD_CONTROL);
        leftButton.altWhenPressed   = (mods & GLFW_MOD_ALT    );
        mouseButtonLeft(lastMousePos, pressed);
        break;
      case GLFW_MOUSE_BUTTON_MIDDLE:
        centerButton.isPressed = pressed;
        centerButton.shiftWhenPressed = (mods & GLFW_MOD_SHIFT  );
        centerButton.ctrlWhenPressed  = (mods & GLFW_MOD_CONTROL);
        centerButton.altWhenPressed   = (mods & GLFW_MOD_ALT    );
        mouseButtonCenter(lastMousePos, pressed);
        break;
      case GLFW_MOUSE_BUTTON_RIGHT:
        rightButton.isPressed = pressed;
        rightButton.shiftWhenPressed = (mods & GLFW_MOD_SHIFT  );
        rightButton.ctrlWhenPressed  = (mods & GLFW_MOD_CONTROL);
        rightButton.altWhenPressed   = (mods & GLFW_MOD_ALT    );
        mouseButtonRight(lastMousePos, pressed);
        break;
      }
    }

    void GLFWBaseViewer::setCameraOptions(float fovy,
                                     float focalDistance)

    {
      camera.setFovy(fovy);
      camera.setFocalDistance(focalDistance);

      updateCamera();
    }

    /*! set a new orientation for the camera, update the camera, and
      notify the app */
    void GLFWBaseViewer::setCameraOrientation(/* camera origin    : */const vec3f &origin,
                                         /* point of interest: */const vec3f &interest,
                                         /* up-vector        : */const vec3f &up,
                                         /* fovy, in degrees : */float fovyInDegrees)
    {
      camera.setOrientation(origin,interest,up,fovyInDegrees,false);
      updateCamera();
    }

    void GLFWBaseViewer::getCameraOrientation(/* camera origin    : */vec3f &origin,
                                         /* point of interest: */vec3f &interest,
                                         /* up-vector        : */vec3f &up,
                                         /* fovy, in degrees : */float & fovyInDegrees)
    {
      origin = camera.position;
      interest = -camera.poiDistance * camera.frame.vz + camera.position;
      up = camera.upVector;
      fovyInDegrees = camera.fovyInDegrees;
    }

    void GLFWBaseViewer::showAndRun()
    {
      showAndRun([]() {return true; }); // run until closed manually
    }

    void GLFWBaseViewer::showAndRun(std::function<bool()> keepgoing)
    {
      int width, height;
      glfwGetFramebufferSize(handle, &width, &height);
      resize(vec2i(width,height));
      glfwSetFramebufferSizeCallback(handle, glfwindow_reshape_cb);
      glfwSetMouseButtonCallback(handle, glfwindow_mouseButton_cb);
      glfwSetKeyCallback(handle, glfwindow_key_cb);
      glfwSetCharCallback(handle, glfwindow_char_cb);
      glfwSetCursorPosCallback(handle, glfwindow_mouseMotion_cb);

      glfwFocusWindow (handle);
	
      while (!glfwWindowShouldClose(handle) && keepgoing()) {
        static double lastCameraUpdate = -1.f;
        if (camera.lastModified != lastCameraUpdate) {
          cameraChanged();
          lastCameraUpdate = camera.lastModified;
        }
        render();
        draw();

        glfwSwapBuffers(handle);
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        runImgui();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      }

      ImGui_ImplOpenGL3_Shutdown();
      ImGui_ImplGlfw_Shutdown();
      ImGui::DestroyContext();
      
      glfwDestroyWindow(handle);
      
      glfwTerminate();
    }

    /*! tell GLFW to set desired active window size (GLFW my choose
      something smaller if it can't fit this on screen */
    void GLFWBaseViewer::setWindowSize(const vec2i &desiredSize) const
    {
      glfwSetWindowSize(handle,desiredSize.x,desiredSize.y);
    }

  } // ::hs::viewer
} // ::hs
