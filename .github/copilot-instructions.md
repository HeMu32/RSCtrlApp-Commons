# Copilot Instructions for RSCtrlApp-Commons

This repository is a *small common library* used by the larger `RSCtrlApp` project. It does not build on its own and is normally included as part of the main solution (Visual Studio/CMake/Qt) in the parent repository. The files here are almost entirely **interface declarations** and a few lightweight utility classes.

## Big picture

- **Folder-per-component**: each top‑level directory corresponds to a subsystem used by the controller application.
  * `CamCtrl/` – minimal blocking camera control interfaces (PTP vendor escapes etc.).
  * `GimbalDev/` – control of a 3‑axis servo platform plus a concrete `FocalLengthHandler` implementation.
  * `DevEnum/` – device enumeration interface.
  * `FrameRecv/`, `FrameGuider/`, `LiveInputDev/`, `LiveOutDev/` – abstract A/V frame handling and routing.  Most of these are just comments today; the expectation is that implementations living in other projects inherit from these pure–virtual base classes.
  * `UniAVFrame/` – a header describing a cross‑format frame container used by the app (Qt, OpenCV, ffmpeg, BMD SDK, etc.).

- **Interface convention**: classes start with `I` and contain only pure virtual methods (sometimes `= default` destructor).  Error signalling is usually a `bool`/`int` return value; raw PTP codes and fixed‑width integers are used consistently.  Implementation details are left to the consuming application.

- **Transport abstraction**: `IPTPTransport` in `CamCtrl` hides the underlying PTP vendor escape mechanism.  The caller typically holds an `std::shared_ptr<IPTPTransport>` and may provide its own implementation (libusb, vendor SDK, WIA, etc.).

- **Concrete code**: the only non‑interface code here is `FocalLengthHandler`, which uses Qt (`QSettings`, `QDebug`) and STL containers to manage focal‑length ↔ motor‑position calibration data.  It is thread‑safe, persists to `calibrationConfig.ini`, and demonstrates the error/return style used elsewhere.

## Developer workflows

- **Building / compiling** – there is no standalone build; open the parent project's solution (likely on the desktop path given by the workspace) and add these headers/sources as needed.  If you need a quick compile, create a tiny test project in Visual Studio that includes the desired headers.

- **Adding a new interface** – create a new folder if appropriate, name the header `IYourThing.h`, prefix the class with `I`, and write Doxygen‑style comments.  Keep dependencies to a minimum (prefer fixed‑width integers and forward declarations).  Example: see `CamCtrl/ISimpleCamCtrl.h`.

- **Implementing an interface** – implementers are expected to live in the main application, not this repo.  Follow the style used by `FocalLengthHandler.cpp` for threading and persistence if you need similar functionality.

- **Persisting settings** – use `QSettings` for simple `.ini`‑based storage.  The handler saves calibration points as a `QStringList` and stores crop/aspect ratios in a separate group; follow the pattern shown in `FocalLengthHandler.cpp`.

- **Thread safety** – when maintaining shared state inside a concrete class, guard access with `std::mutex` and `std::lock_guard`, see `FocalLengthHandler` for an example.

- **PTP helper macros** – common constants (`ESCAPE_PTP_VENDOR_COMMAND`, etc.) and `PTP_HR_SUCCEEDED/FAILED` helpers live in `CamCtrl/PTPTransport.h`.

## Project‑specific conventions

- Comments are mostly English; some Chinese comments appear in the gimbal code.  Keep new comments consistent with existing style.

- Use integer types from `<cstdint>` everywhere in interfaces.

- Interfaces may inherit from others; e.g. `IFrameGuider` inherits from `IFrameRecv` and is expected to hold a reference to an `IGimbalDev`.  This dependency is only documented in the header comment.

- No unit tests or CI in this repo – tests are located in the parent project if any.  Do not add a test harness here unless requested by the higher‑level repo.

- There are no platform‑specific macros; keep code cross‑platform, but Qt dependencies are allowed only in concrete implementations (not in headers) to avoid dragging Qt into every component.

## Integration points and dependencies

- The only third‑party dependency visible here is Qt (used in `FocalLengthHandler`).  Consumers of the interfaces must link against whatever libraries they need.

- The `UniAVFrame` description indicates a shared type for audio/video frames across Qt, OpenCV, ffmpeg, and Blackmagic DeckLink SDK.  Use this header as the central place to coordinate frame lifetimes and conversions; the actual implementation is in the main application.

- This repository is referenced by the larger `RSCtrlApp` workspace; expect header include paths like `#include "CamCtrl/ISimpleCamCtrl.h"`.

## Useful examples

- **Minimal camera control**: see `CamCtrl/ISimpleCamCtrl.h` for the blocking API, and note `SetPtpTransport` which allows injection of a shared `IPTPTransport` instance.

- **Calibration storage**: `GimbalDev/FocalLengthHandler.{h,cpp}` demonstrates threading, Qt persistence, and a calibration algorithm with interpolation.

- **PTP transport interface**: simple data structure `PTP_EscapeResult` plus helper macros.

## Questions & feedback

If anything is unclear or you notice missing pieces (build instructions, expected include paths, additional dependencies), let me know and we can iterate this guidance.