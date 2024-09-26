// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_COMMON_SNAPSHOT_CONTROLLER_IMPELLER_H_
#define FLUTTER_SHELL_COMMON_SNAPSHOT_CONTROLLER_IMPELLER_H_

#include "flutter/impeller/aiks/aiks_context.h"
#include "flutter/impeller/renderer/render_target.h"
#include "flutter/shell/common/snapshot_controller.h"
#include "impeller/runtime_stage/runtime_stage.h"

namespace flutter {

class SnapshotControllerImpeller : public SnapshotController {
 public:
  explicit SnapshotControllerImpeller(
      const SnapshotController::Delegate& delegate)
      : SnapshotController(delegate) {}

  void MakeRasterSnapshot(
      sk_sp<DisplayList> display_list,
      SkISize picture_size,
      std::function<void(const sk_sp<DlImage>&)> callback) override;

  sk_sp<DlImage> MakeRasterSnapshotSync(sk_sp<DisplayList> display_list,
                                        SkISize picture_size) override;

  sk_sp<SkImage> ConvertToRasterImage(sk_sp<SkImage> image) override;

  void CacheRuntimeStage(
      const std::shared_ptr<impeller::RuntimeStage>& runtime_stage) override;

  sk_sp<DlImage> MakeFromTexture(int64_t raw_texture, SkISize size) override;

  std::unique_ptr<Surface> MakeOffscreenSurface(int64_t raw_texture,
                                                const SkISize& size) override;

 private:
  FML_DISALLOW_COPY_AND_ASSIGN(SnapshotControllerImpeller);

  class OffscreenImpellerSurface : public Surface {
   public:
    OffscreenImpellerSurface(
        impeller::AiksContext* aiks_context,
        std::shared_ptr<impeller::RenderTarget> render_target);

    ~OffscreenImpellerSurface() override;

    bool IsValid() override;

    std::unique_ptr<SurfaceFrame> AcquireFrame(const SkISize& size) override;

    SkMatrix GetRootTransformation() const override;

    GrDirectContext* GetContext() override;

   private:
    impeller::AiksContext* _aiks_context;
    std::shared_ptr<impeller::RenderTarget> _render_target;
  };
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_COMMON_SNAPSHOT_CONTROLLER_IMPELLER_H_
