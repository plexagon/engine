// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/flow/layers/clip_path_layer.h"
#include "flutter/flow/paint_utils.h"

namespace flutter {

ClipPathLayer::ClipPathLayer(const SkPath& clip_path, Clip clip_behavior)
    : clip_path_(clip_path), clip_behavior_(clip_behavior) {
  FML_DCHECK(clip_behavior != Clip::none);
}

void ClipPathLayer::Diff(DiffContext* context, const Layer* old_layer) {
  DiffContext::AutoSubtreeRestore subtree(context);
  auto* prev = static_cast<const ClipPathLayer*>(old_layer);
  if (!context->IsSubtreeDirty()) {
    FML_DCHECK(prev);
    if (clip_behavior_ != prev->clip_behavior_ ||
        clip_path_ != prev->clip_path_) {
      context->MarkSubtreeDirty(context->GetOldLayerPaintRegion(old_layer));
    }
  }
  if (context->PushCullRect(clip_path_.getBounds())) {
    DiffChildren(context, prev);
  }
  context->SetLayerPaintRegion(this, context->CurrentSubtreeRegion());
}

void ClipPathLayer::Preroll(PrerollContext* context, const SkMatrix& matrix) {
  TRACE_EVENT0("flutter", "ClipPathLayer::Preroll");

  SkRect previous_cull_rect = context->cull_rect;
  SkRect clip_path_bounds = clip_path_.getBounds();
  if (!context->cull_rect.intersect(clip_path_bounds)) {
    context->cull_rect.setEmpty();
  }
  Layer::AutoPrerollSaveLayerState save =
      Layer::AutoPrerollSaveLayerState::Create(context, UsesSaveLayer());
  context->mutators_stack.PushClipPath(clip_path_);

  // We can pass opacity along to our children if they are
  // well behaved.
  context->subtree_can_inherit_opacity = true;

  SkRect child_paint_bounds = SkRect::MakeEmpty();
  PrerollChildren(context, matrix, &child_paint_bounds);
  if (child_paint_bounds.intersect(clip_path_bounds)) {
    set_paint_bounds(child_paint_bounds);
  }

  // If we use a SaveLayer then we can accept opacity on behalf
  // of our children and apply it in the saveLayer.
  if (UsesSaveLayer()) {
    context->subtree_can_inherit_opacity = true;
  }

  context->mutators_stack.Pop();
  context->cull_rect = previous_cull_rect;
}

void ClipPathLayer::Paint(PaintContext& context) const {
  TRACE_EVENT0("flutter", "ClipPathLayer::Paint");
  FML_DCHECK(needs_painting(context));

  SkAutoCanvasRestore save(context.internal_nodes_canvas, true);
  context.internal_nodes_canvas->clipPath(clip_path_,
                                          clip_behavior_ != Clip::hardEdge);

  if (!UsesSaveLayer()) {
    PaintChildren(context);
    return;
  }

  AutoCachePaint cache_paint(context);
  TRACE_EVENT0("flutter", "Canvas::saveLayer");
  context.internal_nodes_canvas->saveLayer(paint_bounds(), cache_paint.paint());

  PaintChildren(context);

  context.internal_nodes_canvas->restore();
  if (context.checkerboard_offscreen_layers) {
    DrawCheckerboard(context.internal_nodes_canvas, paint_bounds());
  }
}

}  // namespace flutter
