# SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(render)

if (HS_IMGUI)
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
    owlGLFWViewer
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
if (TARGET hayStackViewer)
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
  owlGLFWViewer
)
target_compile_definitions(hsServer PUBLIC -DHS_SERVER=1)

# ==================================================================
# viewer(-only) app that connects to a running haystack server group
# ==================================================================
if (TARGET hayStackViewer)
  add_executable(hsClient
    remoteViewer.cpp
    ClientServer.cpp
  )
  target_link_libraries(hsClient
    PUBLIC
    hayStackViewer
    hayMaker
  )
endif()
