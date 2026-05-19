#import <Cocoa/Cocoa.h>
#include "FileDialog.h"
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
        panel.allowedContentTypes = @[];  // allow any file type

        if ([panel runModal] == NSModalResponseOK) {
            NSURL* url = panel.URLs.firstObject;
            if (url) {
                const char* path = url.fileSystemRepresentation;
                if (path) result = path;
            }
        }
    };

    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }

    return result;
}

std::string nativeSaveFileDialog(const char* title, const char* defaultName)
{
    __block std::string result;

    void (^block)(void) = ^{
        NSSavePanel* panel = [NSSavePanel savePanel];
        panel.title = [NSString stringWithUTF8String:title];
        if (defaultName && defaultName[0] != '\0') {
            panel.nameFieldStringValue = [NSString stringWithUTF8String:defaultName];
        }

        if ([panel runModal] == NSModalResponseOK) {
            NSURL* url = panel.URL;
            if (url) {
                const char* path = url.fileSystemRepresentation;
                if (path) result = path;
            }
        }
    };

    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }

    return result;
}
