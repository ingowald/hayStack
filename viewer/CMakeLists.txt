# SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
# SPDX-License-Identifier: Apache-2.0

option(HS_VIEWER "Build hsViewer" ON)
option(HS_CLIENT_SERVER "Build hsClient/hsServer" ON)


add_subdirectory(render)


if (HS_VIEWER OR HS_CLIENT_SERVER)
  add_subdirectory(ui)
  
  # ==================================================================
  # viewer window/ui stuff for either remote hsClient or local hsViewer
  # ==================================================================
  add_library(hayStackViewer STATIC
    Viewer.h
    Viewer.cpp
    TransferFunctionEditor.cpp
    TransferFunctionEditor.h
  )
  target_compile_definitions(hayStackViewer PUBLIC -DHS_VIEWER=1)
  target_link_libraries(hayStackViewer PUBLIC
    glfwBaseViewer
    hayStack
  )
endif()

# ------------------------------------------------------------------
add_executable(hsOffline
  main.cpp
)
target_link_libraries(hsOffline
  PUBLIC
  hayStack
  hayStackDataLoader
  hayMaker
)

# ==================================================================
# the regular haystack viewer window where the viewer tiself is either
# stanadlone or on mpi rank 0
# ==================================================================
if (HS_VIEWER)
  add_executable(hsViewer
    main.cpp
  )
  target_link_libraries(hsViewer
    PUBLIC
    hayStack
    hayStackDataLoader
    hayMaker
    hayStackViewer
  )
  target_compile_definitions(hsViewer PUBLIC -DHS_VIEWER=1)
endif()

if (HS_CLIENT_SERVER)
  # ==================================================================
  # a haystack server that loads a (possibly distribtributed) scene,
  # and let's a remote viewer client connect to it
  # ==================================================================
  add_executable(hsServer
    main.cpp
    ClientServer.cpp
  )
  target_link_libraries(hsServer
    PUBLIC
    hayStack
    hayStackDataLoader
    hayMaker
    glfwBaseViewer
  )
  target_compile_definitions(hsServer PUBLIC -DHS_SERVER=1)

  # ==================================================================
  # viewer(-only) app that connects to a running haystack server group
  # ==================================================================
  add_executable(hsClient
    main.cpp #remoteViewer.cpp
    ClientServer.cpp
  )
  target_link_libraries(hsClient
    PUBLIC
    hayStackViewer
    hayStackDataLoader
    hayMaker
  )
  target_compile_definitions(hsClient PUBLIC -DHS_CLIENT=1)
endif()
