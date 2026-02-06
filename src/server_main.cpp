#include "AlsaBackend.hpp"
#ifdef ENABLE_GRPC
#include "server/MixerGrpcService.hpp"
#include <grpcpp/grpcpp.h>
#endif
#ifdef ENABLE_OSC
#include "server/MixerOscService.hpp"
#endif
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

void print_help(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --port-grpc <HOST:PORT>     gRPC listen address (default 0.0.0.0:50051)\n";
    std::cout << "  --port-osc <PORT>           OSC listen port (default 9000)\n";
    std::cout << "  --help, -h                  Show this help message\n";
}

int main(int argc, char** argv) {
    std::string grpc_port = "0.0.0.0:50051";
    std::string osc_port = "9000";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "--port-grpc" && i + 1 < argc) {
            grpc_port = argv[++i];
        } else if (arg == "--port-osc" && i + 1 < argc) {
            osc_port = argv[++i];
        }
    }

    std::cout << "TotalMixer Headless Server" << std::endl;
    std::cout << "Initializing ALSA Backend..." << std::endl;
    
    auto backend = std::make_shared<TotalMixer::AlsaBackend>();
    
    if (!backend->initialize()) {
        std::cerr << "Failed to initialize ALSA backend: " << backend->getStatusMessage() << std::endl;
        return 1;
    }

    #ifdef ENABLE_GRPC
    TotalMixer::MixerGrpcService grpc_service(backend);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(grpc_port, grpc::InsecureServerCredentials());
    builder.RegisterService(&grpc_service);
    
    std::unique_ptr<grpc::Server> grpc_server(builder.BuildAndStart());
    if (grpc_server) {
        std::cout << "gRPC Server listening on " << grpc_port << std::endl;
    } else {
        std::cerr << "Failed to start gRPC server" << std::endl;
    }
    #endif

    #ifdef ENABLE_OSC
    TotalMixer::MixerOscService osc_service(backend, osc_port);
    if (osc_service.start()) {
        std::cout << "OSC Server listening on UDP " << osc_port << std::endl;
    } else {
        std::cerr << "Failed to start OSC server" << std::endl;
    }
    #endif

    std::cout << "Server running. Press Ctrl+C to stop." << std::endl;

    // Main Wait Loop
    #ifdef ENABLE_GRPC
    if (grpc_server) {
        grpc_server->Wait();
    } else {
        while(true) std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    #else
    while(true) std::this_thread::sleep_for(std::chrono::seconds(1));
    #endif

    return 0;
}
