#include "MixerGrpcService.hpp"
#include "ui_helpers.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace TotalMixer {

MixerGrpcService::MixerGrpcService(std::shared_ptr<AlsaBackend> backend) : backend(backend) {}

grpc::Status MixerGrpcService::SetMatrixGain(grpc::ServerContext* context, const fireface::MatrixGainRequest* request, fireface::Empty* response) {
    if (!backend || !backend->isConnected()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Backend not connected");
    }
    
    // gain_db is passed directly
    bool success = backend->setMatrixGain(request->output_ch(), request->source_ch(), request->gain_db());
    
    if (success) return grpc::Status::OK;
    return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to set matrix gain");
}

grpc::Status MixerGrpcService::SetOutputVolume(grpc::ServerContext* context, const fireface::OutputVolumeRequest* request, fireface::Empty* response) {
    if (!backend || !backend->isConnected()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Backend not connected");
    }

    bool success = backend->setOutputVolume(request->output_ch(), request->gain_db());
    
    if (success) return grpc::Status::OK;
    return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to set output volume");
}

grpc::Status MixerGrpcService::GetFullState(grpc::ServerContext* context, const fireface::Empty* request, fireface::MixerStateResponse* response) {
    if (!backend || !backend->isConnected()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Backend not connected");
    }

    // TODO: Ideally AlsaBackend should provide bulk fetch.
    // For now, iterate. This might be slow but it's one-off.
    
    // Matrix: 18 outs * 36 sources
    for (int src = 0; src < 36; ++src) {
        for (int out = 0; out < 18; ++out) {
            float db = backend->getMatrixGain(out, src);
            response->add_matrix_gains_db(db);
        }
    }
    
    // Outputs
    for (int out = 0; out < 18; ++out) {
        float db = backend->getOutputVolume(out);
        response->add_output_gains_db(db);
    }

    return grpc::Status::OK;
}

grpc::Status MixerGrpcService::StreamMeters(grpc::ServerContext* context, const fireface::Empty* request, grpc::ServerWriter<fireface::MeterData>* writer) {
    // Simple polling loop
    MeterData data;
    fireface::MeterData proto_data;
    
    while (!context->IsCancelled()) {
        // Poll backend
        // backend->getMeterLevels(data); // Not implemented in AlsaBackend yet
        
        // Dummy data for now or implement real polling
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Populate proto_data
        // writer->Write(proto_data);
    }
    
    return grpc::Status::OK;
}

}
