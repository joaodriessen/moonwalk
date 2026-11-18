# The launcher and the streaming main loop

`Session::initialize()` accepts an optional `QWindow*`. The Qt Quick launcher
passes its `QQuickWindow` through that interface. The session reads screen
selection, geometry, device-pixel ratio, and window state from it. A missing
launcher window is valid for command-line paths.

`Session::exec()` runs the SDL stream loop on the main thread. It processes
Qt events while Stream Controls is open, and periodically processes events
excluding user input for work such as the clipboard or file transfer. The
launcher returns to normal Qt event processing when the loop exits. Remote
cleanup can continue asynchronously.

The compiler checks the launcher interface. Stream startup also initializes
SDL and probes audio and video, so use an integration run to check an actual
launch. A frontend built around another event loop would need its own design
for how that loop shares control with SDL and Qt.
