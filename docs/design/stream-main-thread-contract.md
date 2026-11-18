# Streaming launcher and main-thread contract

`Session::initialize()` accepts a `QWindow*` as its optional launcher-window
boundary. The current Qt Quick launcher passes its `QQuickWindow*` through that
base-class API. The session uses only `QWindow` operations: screen selection,
logical geometry, device-pixel ratio, window state, and minimized-state
restoration. A null launcher window remains the supported CLI/headless route.

This boundary is deliberately compile-checked, not unit-invoked:
`initialize()` initializes SDL and probes decoder/audio capabilities, so a
unit-style call would not isolate the window API. Real stream startup remains
a hardware integration gate.

## Main-thread ownership

During a stream, `Session::exec()` still owns the frontend main thread for the
SDL event/streaming loop. Qt event processing is intentionally suspended until
that loop exits and the existing deferred cleanup returns control to the Qt
frontend. This task makes no threading, SDL-loop, or process-boundary change.

Consequently, accepting `QWindow*` is only a narrow launcher-window seam; it
does not make a GTK frontend viable. Any future GTK streaming proof must first
choose and validate an explicit design for the GLib loop: a frozen hidden Qt
shell, a redesigned threaded event-loop ownership model, or a child streaming
process. Until then, GTK support is blocked at the stream-loop/process
boundary.
