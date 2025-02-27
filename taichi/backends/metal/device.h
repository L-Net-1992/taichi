#pragma once

#include <memory>

#include "taichi/backends/device.h"

namespace taichi {
namespace lang {

class MemoryPool;

namespace metal {

struct MTLDevice;

struct ComputeDeviceParams {
  MTLDevice *device{nullptr};
  MemoryPool *mem_pool{nullptr};
};

std::unique_ptr<taichi::lang::Device> make_compute_device(
    const ComputeDeviceParams &params);

}  // namespace metal
}  // namespace lang
}  // namespace taichi
