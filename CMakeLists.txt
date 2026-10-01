cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

project(fps-starter-box3d-recast-behaviortree-opengl-sdl3-desktop CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# Locate System OpenGL & Threads Packages
# -----------------------------------------------------------------------------
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# -----------------------------------------------------------------------------
# Locate SDL3 and SDL3_image Packages
# -----------------------------------------------------------------------------
if(WIN32)
    set(SDL3_DIR "C:/libs/sdl3-3.4.16-mingw/lib/cmake/SDL3")
    set(SDL3_image_DIR "C:/libs/sdl3_image-3.4.6-mingw/lib/cmake/SDL3_image")
else()
    set(SDL3_DIR "$ENV{HOME}/libs/sdl3-3.4.16-ubuntu/lib/cmake/SDL3")
    set(SDL3_image_DIR "$ENV{HOME}/libs/sdl3_image-3.4.6-ubuntu/lib/cmake/SDL3_image")
endif()

find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)

# -----------------------------------------------------------------------------
# Set Include Paths for Header-Only, Source, and Physics Libraries
# -----------------------------------------------------------------------------
if(WIN32)
    set(GLM_INCLUDE_DIR "C:/libs/glm-1.0.3/glm")
    set(CGLTF_INCLUDE_DIR "C:/libs/cgltf-1.15")
    set(GLAD_INCLUDE_DIR "C:/libs/glad-0.1.36/opengl-3.3/include")
    set(GLAD_SOURCE "C:/libs/glad-0.1.36/opengl-3.3/src/glad.c")
    set(BOX3D_DIR "C:/libs/box3d-0.1.0-mingw")
    set(RECAST_DIR "C:/libs/recastnavigation-mingw")
    set(BTCPP_PATH "C:/libs/behaviortree-4.9.0-mingw")
    set(TINYXML2_PATH "C:/libs/tinyxml2-11.0.0-mingw")
else()
    set(GLM_INCLUDE_DIR "$ENV{HOME}/libs/glm-1.0.3")
    set(CGLTF_INCLUDE_DIR "$ENV{HOME}/libs/cgltf-1.15")
    set(GLAD_INCLUDE_DIR "$ENV{HOME}/libs/glad-0.1.36/opengl-3.3/include")
    set(GLAD_SOURCE "$ENV{HOME}/libs/glad-0.1.36/opengl-3.3/src/glad.c")
    set(BOX3D_DIR "$ENV{HOME}/libs/box3d-0.1.0-ubuntu")
    set(RECAST_DIR "$ENV{HOME}/libs/recastnavigation-ubuntu")
    set(BTCPP_PATH "$ENV{HOME}/libs/behaviortree-4.9.0-ubuntu")
    set(TINYXML2_PATH "$ENV{HOME}/libs/tinyxml2-11.0.0-ubuntu")
endif()

# -----------------------------------------------------------------------------
# Define Imported Static Library for Box3D, Nav, and BT
# -----------------------------------------------------------------------------
add_library(Box3D_Static STATIC IMPORTED)
set_target_properties(Box3D_Static PROPERTIES IMPORTED_LOCATION "${BOX3D_DIR}/lib/libbox3d.a")

add_library(Recast_Static STATIC IMPORTED)
set_target_properties(Recast_Static PROPERTIES IMPORTED_LOCATION "${RECAST_DIR}/lib/libRecast.a")

add_library(Detour_Static STATIC IMPORTED)
set_target_properties(Detour_Static PROPERTIES IMPORTED_LOCATION "${RECAST_DIR}/lib/libDetour.a")

add_library(DebugUtils_Static STATIC IMPORTED)
set_target_properties(DebugUtils_Static PROPERTIES IMPORTED_LOCATION "${RECAST_DIR}/lib/libDebugUtils.a")

add_library(DetourCrowd_Static STATIC IMPORTED)
set_target_properties(DetourCrowd_Static PROPERTIES IMPORTED_LOCATION "${RECAST_DIR}/lib/libDetourCrowd.a")

add_library(DetourTileCache_Static STATIC IMPORTED)
set_target_properties(DetourTileCache_Static PROPERTIES IMPORTED_LOCATION "${RECAST_DIR}/lib/libDetourTileCache.a")

add_library(tinyxml2 STATIC IMPORTED)
set_target_properties(tinyxml2 PROPERTIES
    IMPORTED_LOCATION "${TINYXML2_PATH}/lib/libtinyxml2.a"
    INTERFACE_INCLUDE_DIRECTORIES "${TINYXML2_PATH}/include"
)

add_library(behaviortree_cpp STATIC IMPORTED)
set_target_properties(behaviortree_cpp PROPERTIES
    IMPORTED_LOCATION "${BTCPP_PATH}/lib/libbehaviortree_cpp.a"
    INTERFACE_INCLUDE_DIRECTORIES "${BTCPP_PATH}/include"
    INTERFACE_LINK_LIBRARIES "tinyxml2"
)

# -----------------------------------------------------------------------------
# Target Definition & Sources
# -----------------------------------------------------------------------------
add_executable(app
    src/app.h
    src/enemy_ai.h
    src/enemy_ai.cpp
    src/fps_controller.h
    src/fps_controller.cpp
    src/main.cpp
    src/model_utils.h
    src/model_utils.cpp
    src/physics_debug.h
    src/physics_debug.cpp
    src/texture_utils.h
    src/texture_utils.cpp
    src/shader_utils.h
    src/shader_utils.cpp
    ${GLAD_SOURCE}
)

target_include_directories(app PRIVATE 
    ${GLM_INCLUDE_DIR}
    ${CGLTF_INCLUDE_DIR}
    ${GLAD_INCLUDE_DIR}
    ${BOX3D_DIR}/include
    ${RECAST_DIR}/include/recastnavigation
)

# -----------------------------------------------------------------------------
# Copy Assets Directory to Build Output
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(
        TARGET app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:app>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:app>/assets"
        COMMENT "Copying assets directory"
    )
endif()

# -----------------------------------------------------------------------------
# Link Libraries & Flags
# -----------------------------------------------------------------------------
target_link_libraries(app PRIVATE 
    SDL3_image::SDL3_image-static
    SDL3::SDL3-static
    OpenGL::GL
    Box3D_Static
    Detour_Static
    DebugUtils_Static
    DetourCrowd_Static
    DetourTileCache_Static
    behaviortree_cpp
    tinyxml2
    Threads::Threads
)

if(WIN32)
    target_compile_options(app PRIVATE -Os)
    target_link_options(app PRIVATE -static -s -static-libgcc -static-libstdc++)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(app PRIVATE -mconsole)
    else()
        target_link_options(app PRIVATE -mwindows)
    endif()
endif()