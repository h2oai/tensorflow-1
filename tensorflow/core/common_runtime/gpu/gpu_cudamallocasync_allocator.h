/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_CORE_COMMON_RUNTIME_GPU_GPU_CUDAMALLOCASYNC_ALLOCATOR_H_
#define TENSORFLOW_CORE_COMMON_RUNTIME_GPU_GPU_CUDAMALLOCASYNC_ALLOCATOR_H_

#include <memory>

#include "third_party/gpus/cuda/include/cuda.h"
#include "third_party/gpus/cuda/include/cuda_runtime_api.h"

#include "absl/container/flat_hash_map.h"
#include "tensorflow/core/common_runtime/gpu/gpu_id.h"
#include "tensorflow/core/framework/allocator.h"
#include "tensorflow/core/platform/macros.h"
#include "tensorflow/core/platform/stream_executor.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {

// An allocator that wraps cudaMallocAsync It has less fragmentation
// issue then the BFC memory allocator.  The compute-sanitizer tool
// help to detect OOB memory error of cudaMallocAsync. Use the
// environment variable TF_GPU_ALLOCATOR=cuda_malloc_async to enable
// it. It needs CUDA 11.2+. When using container, this only need the
// container driver to be 11.2. It have a WAR again a driver bug in
// multi-GPU with CUDA 11.2. The WAR creates extra context on GPU 0.
class GPUcudaMallocAsyncAllocator : public Allocator {
 public:
  explicit GPUcudaMallocAsyncAllocator(Allocator* allocator,
                                       PlatformGpuId platform_gpu_id,
                                       size_t pool_size,
                                       bool reserve_memory = false);
  ~GPUcudaMallocAsyncAllocator() override;
  string Name() override { return name_; }
  void* AllocateRaw(size_t alignment, size_t num_bytes) override;
  void DeallocateRaw(void* ptr) override;

  bool TracksAllocationSizes() const override;

  size_t RequestedSize(const void* ptr) const override;

  size_t AllocatedSize(const void* ptr) const override;

  absl::optional<AllocatorStats> GetStats() override;

  void ClearStats() override;

 private:
  Allocator* base_allocator_ = nullptr;  // owned

  se::StreamExecutor* stream_exec_;  // Not owned.

  // cudaMallocAsync is stream aware. But TF StreamExecutor use only 1
  // compute stream and already synchronize with the h2d, d2h and d2d
  // stream. So we do not need to ask cudaMallocAsync to add extra
  // synchronization.
  cudaStream_t cuda_stream_;
  string name_;
  //Not owned. The default pool of the associated GPU.
  //If null, then the instanciation failed and the first allocation
  //will return an error.
  CUmemoryPool pool_ = nullptr;

  TF_DISALLOW_COPY_AND_ASSIGN(GPUcudaMallocAsyncAllocator);

  // Stats.
  // Structures mutable after construction
  mutable mutex lock_;
  AllocatorStats stats_ GUARDED_BY(lock_);
  absl::flat_hash_map<const void*, size_t> size_map_ GUARDED_BY(lock_);
};

}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_COMMON_RUNTIME_GPU_GPU_CUDAMALLOCASYNC_ALLOCATOR_H_
