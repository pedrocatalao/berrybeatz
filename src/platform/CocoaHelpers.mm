#import <Cocoa/Cocoa.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#include "CocoaHelpers.h"
#include <string>

std::string nativeOpenFileDialog(const char* title)
{
    __block std::string result;

    void (^block)(void) = ^{
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.title = [NSString stringWithUTF8String:title];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;
        panel.allowedContentTypes = @[];

        if ([panel runModal] == NSModalResponseOK) {
            NSURL* url = panel.URLs.firstObject;
            if (url) {
                const char* path = url.fileSystemRepresentation;
                if (path) result = path;
            }
        }
    };

    if ([NSThread isMainThread]) block();
    else dispatch_sync(dispatch_get_main_queue(), block);

    return result;
}

std::string nativeSaveFileDialog(const char* title, const char* defaultName)
{
    __block std::string result;

    void (^block)(void) = ^{
        NSSavePanel* panel = [NSSavePanel savePanel];
        panel.title = [NSString stringWithUTF8String:title];
        if (defaultName && defaultName[0] != '\0')
            panel.nameFieldStringValue = [NSString stringWithUTF8String:defaultName];

        if ([panel runModal] == NSModalResponseOK) {
            NSURL* url = panel.URL;
            if (url) {
                const char* path = url.fileSystemRepresentation;
                if (path) result = path;
            }
        }
    };

    if ([NSThread isMainThread]) block();
    else dispatch_sync(dispatch_get_main_queue(), block);

    return result;
}

// ─── Smooth window drag via macOS compositor ──────────────────────────────────

static __strong NSEvent* s_lastMouseDown = nil;
static id       s_monitor       = nil;

void setupWindowEventMonitor(GLFWwindow*)
{
    if (s_monitor) return;
    @autoreleasepool {
        s_monitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown
                                                          handler:^NSEvent*(NSEvent* ev) {
            s_lastMouseDown = ev;
            return ev;
        }];
    }
}

void beginWindowDrag(GLFWwindow* glfwWin)
{
    @autoreleasepool {
        if (!s_lastMouseDown) return;
        NSWindow* win = glfwGetCocoaWindow(glfwWin);
        [win performWindowDragWithEvent:s_lastMouseDown];
    }
}

// ─── Dock icon ───────────────────────────────────────────────────────────────

void setDockIcon()
{
    @autoreleasepool {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        NSString* path = [[NSBundle mainBundle] pathForResource:@"AppIcon" ofType:@"icns"];
        if (path) {
            NSImage* icon = [[NSImage alloc] initWithContentsOfFile:path];
            if (icon) [NSApp setApplicationIconImage:icon];
        }
    }
}

// ─── Rounded window corners ───────────────────────────────────────────────────

void setWindowCornerRadius(GLFWwindow* glfwWin, float radius)
{
    @autoreleasepool {
        NSWindow* win = glfwGetCocoaWindow(glfwWin);
        win.contentView.wantsLayer = YES;
        win.contentView.layer.cornerRadius = radius;
        win.contentView.layer.masksToBounds = YES;
        // Round top-right corner only
        win.contentView.layer.maskedCorners = kCALayerMaxXMinYCorner;
    }
}

// ─── (unused legacy) transparent title bar ────────────────────────────────────

void setupTransparentTitleBar(GLFWwindow* glfwWin, float& titleBarH, float& trafficW)
{
    @autoreleasepool {
        NSWindow* win = glfwGetCocoaWindow(glfwWin);

        // Measure title bar height before enabling full-size content view
        CGFloat frameH   = win.frame.size.height;
        CGFloat contentH = [win contentRectForFrameRect:win.frame].size.height;
        titleBarH = (float)(frameH - contentH);

        // Extend content view into title bar, hide the title string
        win.titlebarAppearsTransparent = YES;
        win.titleVisibility            = NSWindowTitleHidden;
        win.styleMask                 |= NSWindowStyleMaskFullSizeContentView;

        // Match window bg to toolbar colour so resize/expose don't flash
        win.backgroundColor = [NSColor colorWithRed:30.f/255.f
                                              green:30.f/255.f
                                               blue:46.f/255.f
                                              alpha:1.f];

        // Safe left inset: right edge of zoom (fullscreen) button + small margin
        NSButton* zoom = [win standardWindowButton:NSWindowZoomButton];
        trafficW = zoom ? (float)(NSMaxX(zoom.frame) + 8.f) : 80.f;
    }
}
