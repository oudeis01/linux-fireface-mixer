#include "server/MixerGrpcService.hpp"
#include "AlsaBackend.hpp"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char** argv) {
    std::string server_address("0.0.0.0:50051");
    if (argc > 1) {
        server_address = argv[1];
    }

    std::cout << "TotalMixer Headless Server" << std::endl;
    std::cout << "Initializing ALSA Backend..." << std::endl;
    
    auto alsa_backend = std::make_shared<TotalMixer::AlsaBackend>();
    
    if (!alsa_backend->initialize()) {
        std::cerr << "Failed to initialize ALSA backend: " << alsa_backend->getStatusMessage() << std::endl;
        // We might want to keep running even if ALSA fails initially (e.g. device off), 
        // but for now let's fail fast.
        return 1;
    }

    TotalMixer::MixerGrpcService service(alsa_backend);

    grpc::ServerBuilder builder;
    // No SSL/TLS for local network audio control for now
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        std::cerr << "Failed to start gRPC server on " << server_address << std::endl;
        return 1;
    }
    
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();

    return 0;
}
