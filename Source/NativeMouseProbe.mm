// Counts the native -[<view> mouseDown:] events that reach the embedded WebView
// (WKWebView) versus Ardour's own GTK content view (GdkQuartzView). Both classes are
// looked up by name at load, so this links no extra frameworks and is inert in hosts that
// lack them (Reaper, Standalone — no GdkQuartzView). On a dropped click NEITHER counter
// advances: the press reaches no NSView at all. The original IMP is always called, so host
// behaviour is unchanged.
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <atomic>
#include "NativeMouseProbe.h"

static std::atomic<int> g_web { 0 };
static std::atomic<int> g_gtk { 0 };

int nativeMouseDowns() { return g_web.load(std::memory_order_relaxed); }
int gtkViewMouseDowns() { return g_gtk.load(std::memory_order_relaxed); }

static IMP g_origWeb = nullptr;
static void webDown(id self, SEL cmd, NSEvent* e)
{
    g_web.fetch_add(1, std::memory_order_relaxed);
    ((void (*)(id, SEL, NSEvent*)) g_origWeb)(self, cmd, e);
}

static IMP g_origGtk = nullptr;
static void gtkDown(id self, SEL cmd, NSEvent* e)
{
    g_gtk.fetch_add(1, std::memory_order_relaxed);
    ((void (*)(id, SEL, NSEvent*)) g_origGtk)(self, cmd, e);
}

// Class-targeted swizzle, safe whether or not the class overrides mouseDown: itself.
static void swizzle(const char* className, IMP newImp, IMP* origOut)
{
    Class cls = NSClassFromString([NSString stringWithUTF8String:className]);
    if (cls == nil)
        return;
    SEL    sel    = @selector(mouseDown:);
    Method method = class_getInstanceMethod(cls, sel);
    if (method == nullptr)
        return;
    *origOut = method_getImplementation(method);
    if (!class_addMethod(cls, sel, newImp, method_getTypeEncoding(method)))
        *origOut = method_setImplementation(class_getInstanceMethod(cls, sel), newImp);
}

__attribute__((constructor)) static void installNativeMouseProbe()
{
    swizzle("WKWebView", (IMP) webDown, &g_origWeb);
    swizzle("GdkQuartzView", (IMP) gtkDown, &g_origGtk);
}
