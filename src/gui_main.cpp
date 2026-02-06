#include "gui_app.hpp"
#include "AlsaBackend.hpp"
#ifdef ENABLE_GRPC
#include "backends/GrpcClientBackend.hpp"
#endif
#ifdef ENABLE_OSC
#include "backends/OscClientBackend.hpp"
#endif
#include <iostream>
#include <memory>
#include <string>

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int argc, char** argv) {
    std::string grpc_addr;
    std::string osc_addr;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--connect-grpc" && i + 1 < argc) {
            grpc_addr = argv[++i];
        } else if (arg == "--connect-osc" && i + 1 < argc) {
            osc_addr = argv[++i];
        } else if (arg == "--connect" && i + 1 < argc) { // Default alias
            grpc_addr = argv[++i];
        }
    }

    // 1. Setup GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window title
    std::string win_title = "Linux TotalMix v2";
    if (!grpc_addr.empty()) win_title += " (gRPC: " + grpc_addr + ")";
    else if (!osc_addr.empty()) win_title += " (OSC: " + osc_addr + ")";
    else win_title += " (Local ALSA)";

    GLFWwindow* window = glfwCreateWindow(1400, 950, win_title.c_str(), nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // 2. Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // 3. Initialize GUI App Logic
    std::shared_ptr<TotalMixer::IMixerBackend> backend;
    
    if (!grpc_addr.empty()) {
        #ifdef ENABLE_GRPC
        std::cout << "Mode: Remote Client (gRPC " << grpc_addr << ")" << std::endl;
        backend = std::make_shared<TotalMixer::GrpcClientBackend>(grpc_addr);
        #else
        std::cerr << "Error: gRPC support was disabled at build time." << std::endl;
        return 1;
        #endif
    } else if (!osc_addr.empty()) {
        #ifdef ENABLE_OSC
        std::cout << "Mode: Remote Client (OSC " << osc_addr << ")" << std::endl;
        std::string host = osc_addr;
        std::string port = "9000";
        size_t colon = osc_addr.find(':');
        if (colon != std::string::npos) {
            host = osc_addr.substr(0, colon);
            port = osc_addr.substr(colon + 1);
        }
        backend = std::make_shared<TotalMixer::OscClientBackend>(host, port);
        #else
        std::cerr << "Error: OSC support was disabled at build time." << std::endl;
        return 1;
        #endif
    } else {
        std::cout << "Mode: Local ALSA" << std::endl;
        backend = std::make_shared<TotalMixer::AlsaBackend>();
    }

    TotalMixer::TotalMixerGUI app(backend);

    // 4. Main Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Setup Main Window Docking (Full Viewport)
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        
        // Render App
        app.Render();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
