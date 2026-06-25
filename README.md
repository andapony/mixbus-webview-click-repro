# Ardour / Mixbus: first click after a pointer move is dropped in an embedded WebView

A minimal JUCE plugin that reproduces a macOS bug in **Ardour / Mixbus**: when a plugin
editor embeds a `WebBrowserComponent` (a `WKWebView`), the **first `mousedown` on a
control that has a native `title` tooltip, after the pointer moves onto it, is silently
dropped**. You have to click a second time; the second click on the same spot works, then
moving to another control drops the first click again.

The **same VST3 in Reaper**, and this plugin's **Standalone** build, are unaffected — every
click registers on the first try.

## Root cause (what we found)

The press is consumed **inside the host, before it reaches any NSView**. We swizzled
`-[WKWebView mouseDown:]`, `-[GdkQuartzView mouseDown:]`, and `-[NSApplication
sendEvent:]` in a JUCE plugin loaded into Mixbus. On a dropped click:

- neither the WebView's nor the GTK content view's `mouseDown:` fires, **and**
- **no `NSLeftMouseDown` reaches `-[NSApplication sendEvent:]`** at all.

On the working *second* click, all three fire normally.

So GDK's quartz event loop pulls the `NSLeftMouseDown` out of the queue and handles it
itself without dispatching it through AppKit. Our reading: GDK resolves the event to a
`GdkWindow` using its per-display `toplevel_under_pointer` cache (maintained from
`MouseEntered`/`MouseExited` crossings). A **native `title` tooltip** on the control
installs macOS tooltip tracking on hover; its crossing events leave that cache briefly
stale right after the pointer moves onto the control, so GDK maps the press to the wrong
(GTK) window and consumes it instead of letting AppKit deliver it to the embedded plugin
subview. The press itself re-establishes the state, so the next click routes correctly.

A control with **no** `title` is clean — which is the comparison this repro makes.

## Build (macOS)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

JUCE is fetched automatically (pinned to 8.0.13; any recent JUCE 7/8 reproduces it). The
VST3 is auto-copied to `~/Library/Audio/Plug-Ins/VST3/`; the Standalone app is under
`build/WebViewClickRepro_artefacts/Release/Standalone/`.

## Reproduce

Load the **VST3 in Mixbus/Ardour** and open the editor. Click the **with `title`** buttons
(A/B/C) alternately, moving the pointer between them — the first click after each move is
dropped (the `page clicks received` counter does not advance, and the native counters in the
label do not advance either). The **no `title`** buttons (X/Y/Z) are clean.

For comparison, load the **same VST3 in Reaper**, or run the **Standalone** app: every click
registers on the first try, with the native counters advancing each time.

## Files

- `Source/Plugin.cpp` — the whole plugin (processor + editor + the embedded page).
- `Source/NativeMouseProbe.{h,mm}` — the `WKWebView` / `GdkQuartzView` `mouseDown:`
  counters shown in the label.
- `CMakeLists.txt` — fetches JUCE, builds VST3 + Standalone.

## License

Throwaway diagnostic. It links JUCE (GPLv3 / commercial); treat this repro as GPLv3.
