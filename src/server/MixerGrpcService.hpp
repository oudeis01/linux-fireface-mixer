#pragma once

#include "fireface.grpc.pb.h"
#include "AlsaBackend.hpp"
#include <memory>

namespace TotalMixer {

class MixerGrpcService final : public fireface::MixerService::Service {
public:
    explicit MixerGrpcService(std::shared_ptr<AlsaBackend> backend);

    grpc::Status SetMatrixGain(grpc::ServerContext* context, const fireface::MatrixGainRequest* request, fireface::Empty* response) override;
    grpc::Status SetOutputVolume(grpc::ServerContext* context, const fireface::OutputVolumeRequest* request, fireface::Empty* response) override;
    grpc::Status GetFullState(grpc::ServerContext* context, const fireface::Empty* request, fireface::MixerStateResponse* response) override;
    
    // Streaming not fully implemented yet, but defined
    grpc::Status StreamMeters(grpc::ServerContext* context, const fireface::Empty* request, grpc::ServerWriter<fireface::MeterData>* writer) override;

private:
    std::shared_ptr<AlsaBackend> backend;
};

}
