// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_ALLOCATOR_VK_H_
#define FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_ALLOCATOR_VK_H_

#include "flutter/fml/macros.h"
#include "flutter/fml/memory/ref_ptr.h"
#include "impeller/core/allocator.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/device_buffer_vk.h"
#include "impeller/renderer/backend/vulkan/device_holder.h"
#include "impeller/renderer/backend/vulkan/vk.h"

#include <array>
#include <cstdint>
#include <memory>

namespace impeller {

class AllocatorVK final : public Allocator {
 public:
  // |Allocator|
  ~AllocatorVK() override;

  std::shared_ptr<Texture> WrapTexture(const TextureDescriptor& desc,
                                       int64_t raw_texture) const override;

 private:
  friend class ContextVK;

  UniqueAllocatorVMA allocator_;
  UniquePoolVMA staging_buffer_pool_;
  std::weak_ptr<Context> context_;
  std::weak_ptr<DeviceHolder> device_holder_;
  ISize max_texture_size_;
  bool is_valid_ = false;
  bool supports_memoryless_textures_ = false;
  bool supports_framebuffer_fetch_ = false;
  // TODO(jonahwilliams): figure out why CI can't create these buffer pools.
  bool created_buffer_pool_ = true;
  uint32_t frame_count_ = 0;
  std::thread::id raster_thread_id_;

  AllocatorVK(std::weak_ptr<Context> context,
              uint32_t vulkan_api_version,
              const vk::PhysicalDevice& physical_device,
              const std::shared_ptr<DeviceHolder>& device_holder,
              const vk::Instance& instance,
              const CapabilitiesVK& capabilities);

  // |Allocator|
  bool IsValid() const;

  // |Allocator|
  void DidAcquireSurfaceFrame() override;

  // |Allocator|
  std::shared_ptr<DeviceBuffer> OnCreateBuffer(
      const DeviceBufferDescriptor& desc) override;

  // |Allocator|
  std::shared_ptr<Texture> OnCreateTexture(
      const TextureDescriptor& desc) override;

  // |Allocator|
  ISize GetMaxTextureSizeSupported() const override;

  AllocatorVK(const AllocatorVK&) = delete;

  AllocatorVK& operator=(const AllocatorVK&) = delete;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_RENDERER_BACKEND_VULKAN_ALLOCATOR_VK_H_
