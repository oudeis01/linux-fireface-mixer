#include "backends/GrpcClientBackend.hpp"
#include <iostream>

namespace TotalMixer {

GrpcClientBackend::GrpcClientBackend(const std::string& target_address) 
    : target_address(target_address) {
    matrix_cache.resize(18 * 36, -144.0f);
    output_cache.resize(18, -144.0f);
}

GrpcClientBackend::~GrpcClientBackend() {
    shutdown();
}

bool GrpcClientBackend::initialize() {
    try {
        auto channel = grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials());
        // Simple connectivity check
        auto state = channel->GetState(true);
        if (state == grpc_connectivity_state::GRPC_CHANNEL_SHUTDOWN) {
            status_msg = "Channel Shutdown";
            return false;
        }
        
        stub = fireface::MixerService::NewStub(channel);
        
        // Initial sync
        syncFullState();
        
        connected = true;
        status_msg = "Connected to " + target_address;
        return true;
    } catch (...) {
        status_msg = "Failed to create gRPC channel";
        return false;
    }
}

void GrpcClientBackend::shutdown() {
    stub.reset();
    connected = false;
    status_msg = "Disconnected";
}

bool GrpcClientBackend::isConnected() const { return connected; }
std::string GrpcClientBackend::getDeviceName() const { return "Remote Fireface (" + target_address + ")"; }
std::string GrpcClientBackend::getStatusMessage() const { return status_msg; }

bool GrpcClientBackend::setMatrixGain(int output_ch, int source_ch, float gain_db) {
    if (!connected || !stub) return false;

    fireface::MatrixGainRequest request;
    request.set_output_ch(output_ch);
    request.set_source_ch(source_ch);
    request.set_gain_db(gain_db);
    
    fireface::Empty response;
    grpc::ClientContext context;
    
    grpc::Status status = stub->SetMatrixGain(&context, request, &response);
    
    if (status.ok()) {
        // Update local cache optimistically
        int idx = source_ch * 18 + output_ch;
        if (idx < (int)matrix_cache.size()) {
            matrix_cache[idx] = gain_db;
        }
        return true;
    } else {
        std::cerr << "RPC Error: " << status.error_message() << std::endl;
        return false;
    }
}

bool GrpcClientBackend::setOutputVolume(int output_ch, float gain_db) {
    if (!connected || !stub) return false;

    fireface::OutputVolumeRequest request;
    request.set_output_ch(output_ch);
    request.set_gain_db(gain_db);
    
    fireface::Empty response;
    grpc::ClientContext context;
    
    grpc::Status status = stub->SetOutputVolume(&context, request, &response);
    
    if (status.ok()) {
        if (output_ch < (int)output_cache.size()) {
            output_cache[output_ch] = gain_db;
        }
        return true;
    }
    return false;
}

float GrpcClientBackend::getMatrixGain(int output_ch, int source_ch) {
    int idx = source_ch * 18 + output_ch;
    if (idx >= 0 && idx < (int)matrix_cache.size()) {
        return matrix_cache[idx];
    }
    return -100.0f;
}

float GrpcClientBackend::getOutputVolume(int output_ch) {
    if (output_ch >= 0 && output_ch < (int)output_cache.size()) {
        return output_cache[output_ch];
    }
    return -100.0f;
}

bool GrpcClientBackend::getMeterLevels(MeterData& out_meters) {
    // TODO: Implement streaming receiver
    return false;
}

void GrpcClientBackend::update() {
    // Check channel state or process stream
}

void GrpcClientBackend::syncFullState() {
    if (!stub) return;
    
    fireface::Empty request;
    fireface::MixerStateResponse response;
    grpc::ClientContext context;
    
    grpc::Status status = stub->GetFullState(&context, request, &response);
    
    if (status.ok()) {
        for (int i = 0; i < response.matrix_gains_db_size(); ++i) {
            if (i < (int)matrix_cache.size()) {
                matrix_cache[i] = response.matrix_gains_db(i);
            }
        }
        for (int i = 0; i < response.output_gains_db_size(); ++i) {
            if (i < (int)output_cache.size()) {
                output_cache[i] = response.output_gains_db(i);
            }
        }
    }
}

} // namespace TotalMixer
