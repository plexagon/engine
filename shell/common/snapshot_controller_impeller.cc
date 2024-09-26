// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/common/snapshot_controller_impeller.h"

#include <algorithm>

#include "flutter/flow/surface.h"
#include "flutter/fml/make_copyable.h"
#include "flutter/fml/trace_event.h"
#include "flutter/impeller/display_list/dl_dispatcher.h"
#include "flutter/impeller/display_list/dl_image_impeller.h"
#include "flutter/impeller/geometry/size.h"
#include "flutter/shell/common/snapshot_controller.h"

namespace flutter {

namespace {
sk_sp<DlImage> DoMakeRasterSnapshot(
    const sk_sp<DisplayList>& display_list,
    SkISize size,
    const std::shared_ptr<impeller::AiksContext>& context) {
  TRACE_EVENT0("flutter", __FUNCTION__);
  impeller::DlDispatcher dispatcher;
  display_list->Dispatch(dispatcher);
  impeller::Picture picture = dispatcher.EndRecordingAsPicture();
  if (context) {
    auto max_size = context->GetContext()
                        ->GetResourceAllocator()
                        ->GetMaxTextureSizeSupported();
    double scale_factor_x =
        static_cast<double>(max_size.width) / static_cast<double>(size.width());
    double scale_factor_y = static_cast<double>(max_size.height) /
                            static_cast<double>(size.height());
    double scale_factor =
        std::min(1.0, std::min(scale_factor_x, scale_factor_y));

    auto render_target_size = impeller::ISize(size.width(), size.height());

    // Scale down the render target size to the max supported by the
    // GPU if necessary. Exceeding the max would otherwise cause a
    // null result.
    if (scale_factor < 1.0) {
      render_target_size.width *= scale_factor;
      render_target_size.height *= scale_factor;
    }

    std::shared_ptr<impeller::Image> image =
        picture.ToImage(*context, render_target_size);
    if (image) {
      return impeller::DlImageImpeller::Make(image->GetTexture(),
                                             DlImage::OwningContext::kRaster);
    }
  }

  return nullptr;
}

sk_sp<DlImage> DoMakeRasterSnapshot(
    sk_sp<DisplayList> display_list,
    SkISize picture_size,
    const std::shared_ptr<const fml::SyncSwitch>& sync_switch,
    const std::shared_ptr<impeller::AiksContext>& context) {
  sk_sp<DlImage> result;
  sync_switch->Execute(fml::SyncSwitch::Handlers()
                           .SetIfTrue([&] {
                             // Do nothing.
                           })
                           .SetIfFalse([&] {
                             result = DoMakeRasterSnapshot(
                                 display_list, picture_size, context);
                           }));

  return result;
}
}  // namespace

sk_sp<DlImage> SnapshotControllerImpeller::MakeFromTexture(int64_t raw_texture,
                                                           SkISize size) {
  const auto& delegate = GetDelegate();
  if (!delegate.GetSurface() || !delegate.GetAiksContext()) {
    return nullptr;
  }
  auto context = GetDelegate().GetSurface()->GetAiksContext();
  impeller::TextureDescriptor desc;
  desc.storage_mode = impeller::StorageMode::kHostVisible;
  desc.format = impeller::PixelFormat::kB8G8R8A8UNormInt;
  desc.size = {size.width(), size.height()};
  desc.mip_count = 1;
  auto texture = context->GetContext()->GetResourceAllocator()->WrapTexture(
      desc, raw_texture);
  return impeller::DlImageImpeller::Make(texture);
}

void SnapshotControllerImpeller::MakeRasterSnapshot(
    sk_sp<DisplayList> display_list,
    SkISize picture_size,
    std::function<void(const sk_sp<DlImage>&)> callback) {
  sk_sp<DlImage> result;
  std::shared_ptr<const fml::SyncSwitch> sync_switch =
      GetDelegate().GetIsGpuDisabledSyncSwitch();
  sync_switch->Execute(
      fml::SyncSwitch::Handlers()
          .SetIfTrue([&] {
            std::shared_ptr<impeller::AiksContext> context =
                GetDelegate().GetAiksContext();
            if (context) {
              context->GetContext()->StoreTaskForGPU(
                  [context, sync_switch, display_list = std::move(display_list),
                   picture_size, callback = std::move(callback)] {
                    callback(DoMakeRasterSnapshot(display_list, picture_size,
                                                  sync_switch, context));
                  });
            } else {
              callback(nullptr);
            }
          })
          .SetIfFalse([&] {
            callback(DoMakeRasterSnapshot(display_list, picture_size,
                                          GetDelegate().GetAiksContext()));
          }));
}

sk_sp<DlImage> SnapshotControllerImpeller::MakeRasterSnapshotSync(
    sk_sp<DisplayList> display_list,
    SkISize picture_size) {
  return DoMakeRasterSnapshot(display_list, picture_size,
                              GetDelegate().GetIsGpuDisabledSyncSwitch(),
                              GetDelegate().GetAiksContext());
}

std::unique_ptr<Surface> SnapshotControllerImpeller::MakeOffscreenSurface(
    int64_t raw_texture,
    const SkISize& size) {
  const auto surface_size = impeller::ISize(size.width(), size.height());
  const auto aiks_context = GetDelegate().GetSurface()->GetAiksContext();
  auto context = aiks_context->GetContext();
  auto offscreen_render_target =
      impeller::RenderTarget::CreateOffscreenFromTexture(raw_texture, *context,
                                                         surface_size);
  return std::make_unique<OffscreenImpellerSurface>(aiks_context.get(),
                                                    offscreen_render_target);
}

void SnapshotControllerImpeller::CacheRuntimeStage(
    const std::shared_ptr<impeller::RuntimeStage>& runtime_stage) {
  impeller::RuntimeEffectContents runtime_effect;
  runtime_effect.SetRuntimeStage(runtime_stage);
  auto context = GetDelegate().GetAiksContext();
  if (!context) {
    return;
  }
  runtime_effect.BootstrapShader(context->GetContentContext());
}

sk_sp<SkImage> SnapshotControllerImpeller::ConvertToRasterImage(
    sk_sp<SkImage> image) {
  FML_UNREACHABLE();
}

SnapshotControllerImpeller::OffscreenImpellerSurface::OffscreenImpellerSurface(
    impeller::AiksContext* aiks_context,
    std::shared_ptr<impeller::RenderTarget> render_target)
    : _aiks_context(aiks_context), _render_target(render_target) {}

SnapshotControllerImpeller::OffscreenImpellerSurface::
    ~OffscreenImpellerSurface() = default;

bool SnapshotControllerImpeller::OffscreenImpellerSurface::IsValid() {
  return _aiks_context != nullptr;
}

std::unique_ptr<SurfaceFrame>
SnapshotControllerImpeller::OffscreenImpellerSurface::AcquireFrame(
    const SkISize& size) {
  const auto weak_render_target =
      std::weak_ptr<impeller::RenderTarget>(_render_target);
  const auto submit_callback = fml::MakeCopyable(
      [aiks_context = _aiks_context, weak_render_target = weak_render_target,
       size = size](SurfaceFrame& surface_frame,
                    DlCanvas* canvas) mutable -> bool {
        if (!aiks_context) {
          return false;
        }

        const auto render_target = weak_render_target.lock();
        if (!render_target) {
          return false;
        }

        auto display_list = surface_frame.BuildDisplayList();
        if (!display_list) {
          FML_LOG(ERROR) << "Could not build display list for surface frame.";
          return false;
        }

        impeller::DlDispatcher impeller_dispatcher(
            impeller::IRect::MakeXYWH(0, 0, size.width(), size.height()));
        display_list->Dispatch(impeller_dispatcher, SkIRect::MakeSize(size));
        auto picture = impeller_dispatcher.EndRecordingAsPicture();

        aiks_context->Render(picture, *render_target,
                             /*reset_host_buffer=*/true);
        return true;
      });
  return std::make_unique<SurfaceFrame>(
      nullptr,                          // surface
      SurfaceFrame::FramebufferInfo{},  // framebuffer info
      submit_callback,                  // submit callback
      size,                             // frame size
      nullptr,                          // context result
      true);                            // display list fallback
}

SkMatrix
SnapshotControllerImpeller::OffscreenImpellerSurface::GetRootTransformation()
    const {
  return {};
}

GrDirectContext*
SnapshotControllerImpeller::OffscreenImpellerSurface::GetContext() {
  return nullptr;
}

}  // namespace flutter
