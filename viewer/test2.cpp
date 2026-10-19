#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "owl/common/owl-common.h"
#include "imgui_internal.h"

// #include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

void cleanupImGui(ImGuiContext *context);



struct BaseWindow {
  BaseWindow(const char *title);

  virtual void drawCanvas() {}
  virtual void drawGui() {}
  
  virtual void draw();


  virtual void reshape(int w, int h)
  {
    // printf("reshape %i %i (%s)\n",w,h,title.c_str());
  }
  virtual void mouseMotion(int x, int y)
  {
    // printf("motion %i %i (%s)\n",x,y,title.c_str());
  }
  virtual void mouseButton(int button,
                           int action,
                           int mods)
  {
    // printf("button %x %x %x (%s)\n",button,action,mods,title.c_str());
  }

  const std::string title;
  GLFWwindow   *window;
  ImGuiContext *context;
};

struct MainWindow : public BaseWindow {
  MainWindow(const char *title)
    : BaseWindow(title)
  {}
  void drawCanvas() override;
  void drawGui() override;
};

struct AboutWindow : public BaseWindow {
  AboutWindow(const char *title)
    : BaseWindow(title)
  {}
};


static void glfwindow_reshape_cb(GLFWwindow *window, int width, int height )
{
  BaseWindow *bw = (BaseWindow *)glfwGetWindowUserPointer(window);
  bw->reshape(width,height);
}

static void glfwindow_mouseMotion_cb(GLFWwindow *window, double x, double y)
{
  BaseWindow *bw = (BaseWindow *)glfwGetWindowUserPointer(window);
  bw->mouseMotion((int)x,(int)y);
}

/*! callback for pressing _or_ releasing a mouse button*/
static void glfwindow_mouseButton_cb(GLFWwindow *window,
                                     int button,
                                     int action,
                                     int mods)
{
  BaseWindow *bw = (BaseWindow *)glfwGetWindowUserPointer(window);
  bw->mouseButton(button,action,mods);
}


int main() {

  if (!glfwInit()) {
    std::cout << "glfw error\n"; 
    return -1;
  }
    
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  BaseWindow *mainWin = new MainWindow("ImGUI + OpenGL");
  auto &main_window = mainWin->window;
  auto &main_context= mainWin->context;
  // auto [main_window, main_context] = createWindow("ImGUI + OpenGL");

  // if (!main_window || !main_context) {
  //   glfwTerminate();
  //   return -1;
  // }

  // BaseWindow *aboutWin = new AboutWindow("About");
  // auto &about_window = aboutWin->window;
  // auto &about_context= aboutWin->context;
  // auto [about_window, about_context] = createWindow("About");

  // if (!about_window || !about_context) {
  //   glfwTerminate();
  //   return -1;
  // }

  // bool show_about_window = true;

  while (!glfwWindowShouldClose(main_window)) {

    mainWin->draw();
    // glfwMakeContextCurrent(main_window);
      
    // int display_w, display_h;

    // glfwGetFramebufferSize(main_window, &display_w, &display_h);
    // glViewport(0, 0, display_w, display_h);

    // glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    // glClear(GL_COLOR_BUFFER_BIT);


    // ImGui::SetCurrentContext(main_context);
    // ImGui_ImplOpenGL3_NewFrame();
    // ImGui_ImplGlfw_NewFrame();
    // ImGui::NewFrame();

    // ImGui::SetNextWindowPos(ImVec2(0, 0));
    // ImGui::Begin("Main", nullptr,
    //              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
    //              ImGuiWindowFlags_AlwaysAutoResize |
    //              ImGuiWindowFlags_NoBackground);

    // ImGui::Text("Main window text");
    // ImGui::Button("Main window button");
            
    // ImGui::End();

    // ImGui::Render();
    // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // <--- CRASH HERE
        
    // glfwSwapBuffers(main_window);

    // if (show_about_window) {
      
    //   glfwMakeContextCurrent(about_window);

    //   int display_w, display_h;

    //   glfwGetFramebufferSize(about_window, &display_w, &display_h);
    //   glViewport(0, 0, display_w, display_h);

    //   glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    //   glClear(GL_COLOR_BUFFER_BIT);
        
    //   ImGui::SetCurrentContext(about_context);

    //   ImGui_ImplOpenGL3_NewFrame();
    //   ImGui_ImplGlfw_NewFrame();
    //   ImGui::NewFrame();

    //   ImGui::SetNextWindowPos(ImVec2(0, 0));
    //   ImGui::Begin("About", &show_about_window,
    //                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
    //                ImGuiWindowFlags_AlwaysAutoResize |
    //                ImGuiWindowFlags_NoBackground);

    //   ImGui::Text("ImGUI + OpenGL Demo ver 0.0.1");
    //   ImGui::Button("About window button");

    //   for (int i=0;i<50;i++) {
    //     std::string txt = "text "+std::to_string(i);
    //     ImGui::Text("%s",txt.c_str());
    //   }
      
      
    //   if (ImGui::Button("Close")) {
    //     show_about_window = false;
    //     glfwSetWindowShouldClose(about_window, true);
    //   }
        
    //   ImGui::End();
        
    //   ImGui::Render();
    //   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    //   glfwSwapBuffers(about_window);

    //   if (glfwWindowShouldClose(about_window)) {

    //     cleanupImGui(about_context);
                
    //     about_context = nullptr; // To avoid double cleanup at the end
    //     show_about_window = false; // If shouldClose triggered by the native X button

    //     glfwDestroyWindow(about_window);
    //   }
    // }

    glfwPollEvents();
        
    if (glfwGetKey(main_window, GLFW_KEY_Q) == GLFW_PRESS) {
      glfwSetWindowShouldClose(main_window, true);
    }
  }

  // cleanupImGui(about_context);
  // cleanupImGui(main_context);
    
  glfwTerminate();
  return 0;
}

void cleanupImGui(ImGuiContext *context) {

  if (context) {
    ImGui::SetCurrentContext(context);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }
}    



BaseWindow::BaseWindow(const char *title)
  : title(title)
{
  window = glfwCreateWindow(800, 600,
                            "OpenGL + ImGui", nullptr, nullptr);
  if (window == nullptr) 
    throw std::runtime_error("could not open glfw window");
    
  // Show the default cursor in X11
  GLFWcursor* cursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
  glfwSetCursor(window, cursor);
    
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
    
  auto monitor = glfwGetPrimaryMonitor();
  float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(monitor);
 
  // if (!gladLoadGL()) {
  //     std::cout << "cannot init glad\n";
  //     return {};
  // }
    
  // IMGUI
  IMGUI_CHECKVERSION();
    
  context = ImGui::CreateContext();
  ImGui::SetCurrentContext(context);
  ImGui::GetIO().IniFilename = nullptr;
    
  // auto& style = ImGui::GetStyle();
  // style.ScaleAllSizes(main_scale);
  // style.FontScaleDpi = main_scale;
    
  ImGui_ImplGlfw_InitForOpenGL(window, true); 
  ImGui_ImplOpenGL3_Init();




  glfwMakeContextCurrent(window);
  ImGui::SetCurrentContext(context);
  // ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         
  io.ConfigViewportsNoDecoration = false;
  io.ConfigViewportsNoAutoMerge = true;
 
  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  // Setup scaling
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(main_scale);        // Bake a fixed style
  // scale. (until we have
  // a solution for
  // dynamic style
  // scaling, changing
  // this requires
  // resetting Style +
  // calling this again)
  style.FontScaleDpi = main_scale;        // Set initial font
  // scale. (in docking
  // branch: using
  // io.ConfigDpiScaleFonts=true
  // automatically
  // overrides this for
  // every window
  // depending on the
  // current monitor)


      


  glfwSetFramebufferSizeCallback(window, glfwindow_reshape_cb);
  glfwSetMouseButtonCallback(window, glfwindow_mouseButton_cb);
  glfwSetCursorPosCallback(window, glfwindow_mouseMotion_cb);
  glfwSetWindowUserPointer(window, this);
}

#if 1
void MainWindow::drawCanvas()
{
  int display_w, display_h;

  glfwGetFramebufferSize(window, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void MainWindow::drawGui()
{
#if 1
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking
    | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
    | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
    | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  // ImGuiViewport *viewport = ImGui::GetMainViewport();
  // ImGui::SetNextWindowPos(viewport->WorkPos);
  // ImGui::SetNextWindowSize(viewport->WorkSize);
  // ImGui::SetNextWindowViewport(viewport->ID);

  // ImGui::Begin("DockSpace", nullptr, windowFlags);
  // ImGui::PopStyleVar(3);
  // ImGuiID dockspaceId = ImGui::GetID("DockSpaceID");
  // ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);



  ImGuiID dockspace_id = ImGui::GetID("My Dockspace");
  ImGuiViewport* viewport = ImGui::GetMainViewport();

  // Create settings
  if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
    {
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

      ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
      ImGuiID dock_id_main = dockspace_id;

      // ImGuiID dock_id_top = 0;
      // ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Up, 0.f,//0.50f,
      //                             &dock_id_top, &dock_id_main);
      
      ImGuiID dock_id_left = 0;
      ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f,
                                  &dock_id_left, &dock_id_main);
      
      ImGui::DockBuilderDockWindow("MainCanvas", dock_id_main);
      ImGui::DockBuilderDockWindow("Properties", dock_id_left);
      // ImGui::DockBuilderDockWindow("MenuBar", dock_id_top);
      ImGui::DockBuilderFinish(dockspace_id);
      
      ImGui::PopStyleVar(3);
    }

  // Submit dockspace
  ImGui::DockSpaceOverViewport(dockspace_id, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

  // Submit windows
  ImGui::Begin("Properties");
  ImGui::Text("Left GUI");
  ImGui::Button("Main window button");
  ImGui::End();


      // ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
      // ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
      // ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  // ImGui::Begin("MenuBar");
  // , nullptr,
  //              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
  //              ImGuiWindowFlags_AlwaysAutoResize |
  //              ImGuiWindowFlags_NoBackground);
  // ImGui::Text("Menu Bar Text");
   // ImGui::End();
      // ImGui::PopStyleVar(3);
  

  
#else
  ImGui::Begin("Main", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
               ImGuiWindowFlags_AlwaysAutoResize |
               ImGuiWindowFlags_NoBackground);

  ImGui::Text("Main window text");
  ImGui::Button("Main window button");
            
  ImGui::End();
#endif
}

void BaseWindow::draw()
{
  glfwMakeContextCurrent(window);
  drawCanvas();

  ImGui::SetCurrentContext(context);
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  
  drawGui();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // <--- CRASH HERE
        
  glfwSwapBuffers(window);
}
#else
void MainWindow::draw()
{
  glfwMakeContextCurrent(window);
      
  int display_w, display_h;

  glfwGetFramebufferSize(window, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);


  ImGui::SetCurrentContext(context);
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::Begin("Main", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
               ImGuiWindowFlags_AlwaysAutoResize |
               ImGuiWindowFlags_NoBackground);

  ImGui::Text("Main window text");
  ImGui::Button("Main window button");
            
  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // <--- CRASH HERE
        
  glfwSwapBuffers(window);
}  
#endif
