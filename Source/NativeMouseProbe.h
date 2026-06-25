#pragma once
// Running counts of native -[<view> mouseDown:] events, used to discover WHERE a click
// lands in the host. nativeMouseDowns() counts presses the embedded WebView received;
// gtkViewMouseDowns() counts presses the host's GTK content view (GdkQuartzView) received
// — non-zero only under Ardour/Mixbus. Comparing them on a dropped click reveals whether
// the host routed the press to the GTK view instead of the plugin's WebView subview.
int nativeMouseDowns();
int gtkViewMouseDowns();
