cmake_minimum_required(VERSION 3.14) # 3.14 for FetchContent
project(totalmixer_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Build Options ---
option(ENABLE_GRPC "Enable gRPC backend support" ON)
option(ENABLE_OSC  "Enable OSC backend support (requires liblo)" ON)
option(BUILD_GUI   "Build GUI application" ON)
option(BUILD_SERVER "Build Headless Server" ON)

# --- Common Dependencies ---
find_package(Threads REQUIRED)
find_package(ALSA REQUIRED)
if (NOT ALSA_FOUND)
    message(FATAL_ERROR "libasound (ALSA) not found. Install alsa-lib.")
endif()

# --- GUI Setup ---
if (BUILD_GUI)
    find_package(X11)
    find_package(OpenGL REQUIRED)

    if (NOT X11_FOUND)
        message(FATAL_ERROR "X11 libraries not found.")
    endif()

    find_library(X11_RANDR_LIB Xrandr)
    find_library(X11_INERAMA_LIB Xinerama)
    find_library(X11_CURSOR_LIB Xcursor)
    find_library(X11_XI_LIB Xi)

    if (NOT (X11_RANDR_LIB AND X11_INERAMA_LIB AND X11_CURSOR_LIB AND X11_XI_LIB))
        message(FATAL_ERROR "One or more X11 extension libraries not found.")
    endif()

    include(FetchContent)

    # GLFW
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
    )
    FetchContent_MakeAvailable(glfw)

    # Dear ImGui
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        docking
    )
    FetchContent_MakeAvailable(imgui)

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

    # libsystemd
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SYSTEMD REQUIRED libsystemd)
endif()

# --- gRPC Setup ---
if (ENABLE_GRPC)
    find_package(Protobuf REQUIRED)
    find_package(gRPC CONFIG REQUIRED)

    set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
    set(_REFLECTION_grpc_grpcpp_reflection grpc::grpc++_reflection)
    set(_GRPC_GRPCPP grpc::grpc++)

    if(NOT _GRPC_CPP_PLUGIN_EXECUTABLE)
        find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
    endif()

    get_filename_component(hw_proto "src/protos/fireface.proto" ABSOLUTE)
    get_filename_component(hw_proto_path "${hw_proto}" PATH)

    set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/fireface.pb.cc")
    set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/fireface.pb.h")
    set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/fireface.grpc.pb.cc")
    set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/fireface.grpc.pb.h")

    add_custom_command(
          OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
          COMMAND ${Protobuf_PROTOC_EXECUTABLE}
          ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
               --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
               -I "${hw_proto_path}"
               --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
               "${hw_proto}"
          DEPENDS "${hw_proto}")

    add_library(hw_grpc_proto
      ${hw_proto_srcs}
      ${hw_grpc_srcs}
      ${hw_proto_hdrs}
      ${hw_grpc_hdrs})
    target_link_libraries(hw_grpc_proto
      ${_REFLECTION_grpc_grpcpp_reflection}
      ${_GRPC_GRPCPP}
      ${_PROTOBUF_LIBPROTOBUF})
    target_include_directories(hw_grpc_proto PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
    
    add_definitions(-DENABLE_GRPC)
endif()

# --- OSC Setup (liblo) ---
if (ENABLE_OSC)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBLO REQUIRED liblo)
    add_definitions(-DENABLE_OSC)
endif()

# --- Source Configuration ---
set(COMMON_SOURCES
    src/alsa_core.cpp
    src/AlsaBackend.cpp
)

set(GUI_SOURCES
    src/gui_main.cpp
    src/gui_app.cpp
    src/service_checker.cpp
)

set(SERVER_SOURCES
    src/server_main.cpp
)

if (ENABLE_GRPC)
    list(APPEND COMMON_SOURCES src/backends/GrpcClientBackend.cpp)
    list(APPEND SERVER_SOURCES src/server/MixerGrpcService.cpp)
endif()

if (ENABLE_OSC)
    list(APPEND COMMON_SOURCES src/backends/OscClientBackend.cpp)
    list(APPEND SERVER_SOURCES src/server/MixerOscService.cpp)
endif()

# --- Targets ---

if (BUILD_GUI)
    add_executable(totalmixer_gui ${GUI_SOURCES} ${COMMON_SOURCES})
    target_include_directories(totalmixer_gui PRIVATE src)
    target_link_libraries(totalmixer_gui PRIVATE imgui ${ALSA_LIBRARIES} ${SYSTEMD_LIBRARIES} glfw)
    
    if (ENABLE_GRPC)
        target_link_libraries(totalmixer_gui PRIVATE hw_grpc_proto)
    endif()
    
    if (ENABLE_OSC)
        target_include_directories(totalmixer_gui PRIVATE ${LIBLO_INCLUDE_DIRS})
        target_link_libraries(totalmixer_gui PRIVATE ${LIBLO_LIBRARIES})
    endif()
endif()

if (BUILD_SERVER)
    add_executable(totalmixer_server ${SERVER_SOURCES} ${COMMON_SOURCES})
    target_include_directories(totalmixer_server PRIVATE src)
    target_link_libraries(totalmixer_server PRIVATE ${ALSA_LIBRARIES})
    
    if (ENABLE_GRPC)
        target_link_libraries(totalmixer_server PRIVATE hw_grpc_proto)
    endif()
    
    if (ENABLE_OSC)
        target_include_directories(totalmixer_server PRIVATE ${LIBLO_INCLUDE_DIRS})
        target_link_libraries(totalmixer_server PRIVATE ${LIBLO_LIBRARIES})
    endif()
endif()

# CLI App (Simple test tool, always build)
add_executable(totalmixer_cli 
    src/main.cpp 
    src/alsa_core.cpp
)
target_include_directories(totalmixer_cli PRIVATE src)
target_link_libraries(totalmixer_cli ${ALSA_LIBRARIES})
