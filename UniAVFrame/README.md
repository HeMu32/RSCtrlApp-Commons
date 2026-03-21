# UniAVFrame Usage Notes

Last edited 2026-3-21 23:50

## What UniAVFrame is for

`UniAVFrame` is the shared media-frame container used across `RSCtrlApp`.
It wraps frames coming from multiple backends and gives downstream code a
single object shape for:

- media type (`Video` / `Audio`)
- backend identity (`FFmpeg`, `Qt`, `OpenCV`, `BMDSDK`, ...)
- timestamps and media descriptions
- native backend handle access when the caller really needs it
- backend-agnostic RGBA cache access for video rendering / bridging

The class is intentionally a frame container, not a timeline synchronizer,
not an encoder adapter, and not an A/V alignment engine.

## Recommended usage flow

### 1. Create frames through `UniAVFrameFactory`

Do not construct `UniAVFrame` manually.
Use the factory matching the capture backend:

- `createFromFFmpeg(...)`
- `createFromQtImage(...)`
- `createFromQtAudioPCM(...)`
- `createFromOpenCV(...)`
- `createFromBMDSDK(...)`
- `createFromBMDSDKAudioPacket(...)`

This ensures:

- native ownership is attached correctly
- timestamps and media descriptions are initialized consistently
- RGBA cache creation happens through the supported path

### 2. Keep ownership as `std::shared_ptr<UniAVFrame>`

The intended lifecycle is shared ownership across dispatch and downstream use.

- producers create `std::shared_ptr<UniAVFrame>`
- dispatchers pass the shared pointer through
- receivers keep their own shared pointer if they need to use the frame later

Do not cache raw native pointers beyond the lifetime of the frame object.

### 3. Prefer backend-agnostic access first

When a consumer only needs image bytes for preview, UI, conversion, or generic
processing, prefer the cache/view APIs instead of native backend handles.

Recommended order for video consumers:

1. `rgbaCacheViewConst()`
2. `rgbaCacheRef()` if shared ownership of the cache bytes is needed
3. `rgbaCacheView()` only if mutation is truly intended
4. `nativeHandle()` only when backend-specific logic is required

Recommended order for generic memory readers:

1. `originalMemoryConst()`
2. `originalMemory()` only if mutation is truly intended

The old mutable view APIs remain available for compatibility, but new code
should prefer the const-view accessors whenever possible.

### 3.1 Recommended choice by current downstream role

Based on the current `RSCtrlApp` codebase, downstream users usually fall into
one of these roles:

- UI / frontend preview consumers
- local preview/live-out consumers
- remote bridge / streaming consumers
- backend-specific diagnostic or encoder consumers

The recommended access order is different for each role.

#### UI / frontend preview (`PtzFrontendBridge`-style)

Use RGBA cache view, not native backend handles.

- current code path: `src/ptz/PtzFrontendBridge.cpp`
- recommended access:
  1. `rgbaCacheViewConst()`
  2. `rgbaCacheRef()` if frame bytes must stay alive beyond the immediate scope
- avoid:
  - `nativeHandle()`
  - `originalMemory()` for direct UI image construction

Reason:

- frontend preview only needs a stable RGBA image
- RGBA cache already normalizes Qt / FFmpeg / OpenCV / BMD video paths
- native backend access only increases coupling and backend-specific branches

#### Local preview / display output (`QtPreviewOutput`-style)

Use RGBA cache view plus shared frame ownership.

- current code path: `3rdparty/RSCtrlApp-Commons/LiveOutDev/QtPreviewOutput.cpp`
- recommended access:
  1. keep the `std::shared_ptr<UniAVFrame>` alive
  2. read from `rgbaCacheViewConst()`
  3. if a Qt image/view needs to outlive the call stack, keep the frame alive or
     take an explicit image copy

Reason:

- preview output should not depend on native FFmpeg or BMD objects
- the existing `QImage` wrapping flow is already built around RGBA cache bytes

#### Remote bridge / streaming (`RemotePTZCmdRecv` -> `MasterShim`)

Treat video and audio differently.

- current code path:
  - `src/remote/RemotePTZCmdRecv.cpp`
  - `3rdparty/RSCA-Shim-StrmCtrl/src/MasterShim.cpp`

Recommended video flow:

1. try `nativeHandle()` only if backend is explicitly `FFmpeg`
2. otherwise convert from `rgbaCacheViewConst()` / RGBA cache path

Recommended audio flow:

1. only use `nativeHandle()` when backend is explicitly `FFmpeg`
2. if backend is Qt / BMD / other, add an explicit audio bridge layer instead of
   guessing `nativePtr` type

Important:

- current code already follows this policy for video
- current code only supports FFmpeg-native audio in `MasterShim`
- this is an intentional limitation, not a generic UniAVFrame guarantee

#### Backend-specific encode / inspect / debug utilities

Use `nativeHandle()` only when the consumer is explicitly tied to one backend.

Examples:

- FFmpeg encoder paths may use `AVFrame*`
- BMD-specific diagnostics may inspect `IDeckLinkVideoFrame*`

Required pattern:

1. check `native.backend`
2. check `native.nativePtr != nullptr`
3. cast only after both checks succeed

If a module cannot clearly state which backend it supports, it should not use
`nativeHandle()` directly.

### 4. Use `nativeHandle()` only with an explicit backend check

`nativeHandle()` is not a generic "give me bytes" API.
It is a backend escape hatch.

Always check `backend()` or `nativeHandle().backend` before casting
`nativeHandle().nativePtr`.

Example pattern:

```cpp
const auto native = spFrame->nativeHandle();
if (native.backend == UniAV::InputBackend::FFmpeg && native.nativePtr != nullptr)
{
    const auto* pAvFrame = static_cast<const AVFrame*>(native.nativePtr);
    // backend-specific use
}
```

Do not assume that all video or audio frames expose the same native type.

Also do not assume that `originalMemory()` is interchangeable with
`nativeHandle()`. They solve different problems:

- `nativeHandle()` = backend object escape hatch
- `originalMemory()` = best-effort raw byte view of the original frame data
- `rgbaCacheView*()` = normalized video image access path

## Backend-specific expectations

### FFmpeg video/audio

- `nativeHandle().nativePtr` points to `AVFrame*`
- `originalMemory()` / `originalMemoryConst()` usually expose frame data memory
- video path also builds an RGBA cache through the pool

### Qt image

- native handle points to a stored `QImage*`
- video path builds RGBA cache through the pool
- consumers that only need display/preview should prefer RGBA cache access
- current downstream recommendation: frontend and preview code should not depend
  on the stored `QImage*`; treat it as backend-private

### Qt audio PCM

- frame stores copied PCM bytes for safety
- no RGBA cache is involved
- downstream code should use audio metadata + original memory view
- current remote bridge code does not yet auto-convert this to FFmpeg audio
  frames; such conversion must be introduced explicitly if needed

### BMD video

- native handle points to `IDeckLinkVideoFrame*`
- original pixel memory is not exposed as a stable raw pointer after SDK access
- `originalMemory().data` may be `nullptr`
- use RGBA cache for generic consumers
- current downstream recommendation: generic display / frontend / remote fallback
  code should treat BMD video exactly as an RGBA-cache source unless it is a
  deliberately BMD-specific module

### BMD audio

- native handle points to `IDeckLinkAudioInputPacket*`
- original memory view points to packet bytes guarded by UniAVFrame lifetime
- this is audio-only; no RGBA cache is involved
- current remote bridge code does not automatically adapt this into `AVFrame`
  audio; explicit bridging is still a future task

## RGBA cache and pool behavior

Video backends that need a unified image representation build RGBA cache through
`IUniAVFramePool`.

Current behavior:

- pool allocation now honors `alignmentBytes`
- cache-building paths currently request aligned buffers (typically 32-byte)
- pooled buffers are reused only when both size and alignment are sufficient

This means alignment-sensitive downstream code can rely on the requested pool
alignment more safely than before, but callers still should not hardcode extra
assumptions beyond the explicit API contract.

In the current codebase, this aligned RGBA cache path is already exercised by:

- FFmpeg video frames converted to RGBA cache
- Qt image frames converted to RGBA cache
- OpenCV frames converted to RGBA cache
- BMD video frames converted to RGBA cache

So aligned pool allocation is not just a future-facing API cleanup; it is part
of the active video path today.

## Current limitations

- `MemoryView` / `RGBAImageView` mutable APIs are still present for compatibility
- not all existing call sites have migrated to const-view accessors yet
- Qt video runtime timing may still use provisional/request-derived values rather
  than a guaranteed true negotiated hardware frame rate
- non-FFmpeg audio frames are not automatically bridged into FFmpeg-native audio
  consumers; such bridging must be done explicitly at a higher layer

At the time of writing, existing major downstream paths still mostly use the
legacy mutable view APIs:

- `PtzFrontendBridge` uses `rgbaCacheView()`
- `QtPreviewOutput` uses `rgbaCacheView()`
- generic remote video fallback in `MasterShim` uses RGBA cache

The new const-view APIs are available now, but migration of existing consumers
is incremental rather than complete.

## Practical guidance for downstream modules

### UI / preview code

- prefer `rgbaCacheViewConst()` for new code
- existing code using `rgbaCacheView()` is still valid, but should be treated as
  compatibility usage rather than the preferred long-term pattern
- do not use `nativeHandle()` unless the UI is backend-specific

### streaming / encoding bridges

- if the bridge is FFmpeg-specific, check backend first, then use `nativeHandle()`
- if the backend is not FFmpeg, add an explicit conversion/adapter step instead of
  guessing the native pointer type
- for remote video bridging, RGBA-cache fallback is already an accepted path
- for remote audio bridging, no equivalent generic fallback exists yet

### diagnostics / logging

- log `mediaType()`, `backend()`, `videoDesc()`, `audioDesc()`, and timestamps
- avoid logging raw pointer values as if they represented ownership

## Summary

Use `UniAVFrameFactory` to create frames, keep them as shared pointers, prefer
const view accessors for generic consumers, and treat `nativeHandle()` as a
backend-specific escape hatch that always requires an explicit backend check.
