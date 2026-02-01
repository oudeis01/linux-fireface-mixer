cmake_minimum_required(VERSION 3.14) # 3.14 for FetchContent
project(totalmixer_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Dependency Check (Arch Linux) ---
find_package(X11)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

if (NOT X11_FOUND)
    message(FATAL_ERROR "\n\nError: X11 libraries not found.\n\nTo install on Arch Linux:\n  sudo pacman -S libx11 libxrandr libxinerama libxcursor libxi\n\n")
endif()

find_library(X11_RANDR_LIB Xrandr)
find_library(X11_INERAMA_LIB Xinerama)
find_library(X11_CURSOR_LIB Xcursor)
find_library(X11_XI_LIB Xi)

if (NOT (X11_RANDR_LIB AND X11_INERAMA_LIB AND X11_CURSOR_LIB AND X11_XI_LIB))
    message(FATAL_ERROR "\n\nError: One or more X11 extension libraries not found.\n\nTo install on Arch Linux:\n  sudo pacman -S libxrandr libxinerama libxcursor libxi\n\n")
endif()


# --- FetchContent Dependencies ---
include(FetchContent)

# 1. GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

# 2. Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking # Using docking branch is recommended for modern apps, or master
)
FetchContent_MakeAvailable(imgui)

# Define ImGui Library Target manually since it's just source files
# We include Core + Backends (GLFW + OpenGL3)
set(IMGUI_DIR ${imgui_SOURCE_DIR})
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC 
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)


# --- ALSA Check ---
find_package(ALSA)
if (NOT ALSA_FOUND)
    message(FATAL_ERROR "\n\nError: libasound (ALSA library) not found.\n\nTo install on Arch Linux:\n  sudo pacman -S alsa-lib\n\n")
endif()

# --- libsystemd Check ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)
include_directories(${SYSTEMD_INCLUDE_DIRS})


# --- Targets ---

# 1. CLI App
add_executable(totalmixer_cli 
    src/main.cpp 
    src/alsa_core.cpp
)
target_include_directories(totalmixer_cli PRIVATE src)
target_link_libraries(totalmixer_cli ${ALSA_LIBRARIES})

# 2. GUI App
add_executable(totalmixer_gui
    src/gui_main.cpp
    src/gui_app.cpp
    src/service_checker.cpp
    src/alsa_core.cpp
)
target_include_directories(totalmixer_gui PRIVATE src)
target_link_libraries(totalmixer_gui PRIVATE imgui ${ALSA_LIBRARIES} ${SYSTEMD_LIBRARIES} glfw)
