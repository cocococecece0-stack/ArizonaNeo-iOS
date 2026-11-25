#if defined(__APPLE__)

#include "ios_documents_host.hpp"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <string>

namespace arizona_neo::ios {

namespace {

std::string ns_to_std(NSString* value) {
    if (value == nil) return {};
    const char* bytes = [value UTF8String];
    return bytes != nullptr ? std::string(bytes) : std::string();
}

NSString* std_to_ns(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()];
}

} // namespace

IosDocumentsAccess::IosDocumentsAccess() = default;

IosDocumentsAccess::~IosDocumentsAccess() {
    stop_access();
}

arizona_neo::core::Result<std::string>
IosDocumentsAccess::create_bookmark(const std::string& path) {
    @autoreleasepool {
        NSURL* url = [NSURL fileURLWithPath:std_to_ns(path)];
        NSError* error = nil;
        NSData* data = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
                        includingResourceValuesForKeys:nil
                        relativeToURL:nil
                        error:&error];
        if (data == nil) {
            return arizona_neo::core::Result<std::string>::failure(
                arizona_neo::core::CoreError::IoError,
                error != nil ? ns_to_std(error.localizedDescription) : "bookmark creation failed");
        }
        return arizona_neo::core::Result<std::string>::success(
            ns_to_std([data base64EncodedStringWithOptions:0]));
    }
}

arizona_neo::core::Result<std::string>
IosDocumentsAccess::resolve_bookmark(const std::string& bookmark) {
    @autoreleasepool {
        NSData* data = [[NSData alloc] initWithBase64EncodedString:std_to_ns(bookmark)
                                                          options:0];
        if (data == nil) {
            return arizona_neo::core::Result<std::string>::failure(
                arizona_neo::core::CoreError::InvalidArgument, "invalid bookmark");
        }
        BOOL stale = NO;
        NSError* error = nil;
        NSURL* url = [NSURL URLByResolvingBookmarkData:data
                                             options:NSURLBookmarkResolutionWithSecurityScope
                                       relativeToURL:nil
                                 bookmarkDataIsStale:&stale
                                               error:&error];
        if (url == nil) {
            return arizona_neo::core::Result<std::string>::failure(
                arizona_neo::core::CoreError::AccessDenied,
                error != nil ? ns_to_std(error.localizedDescription) : "bookmark resolution failed");
        }
        if (stale) {
            return arizona_neo::core::Result<std::string>::failure(
                arizona_neo::core::CoreError::AccessDenied, "bookmark is stale");
        }
        stop_access();
        resolved_url_ = url;
        accessing_ = [url startAccessingSecurityScopedResource];
        if (!accessing_) {
            resolved_url_ = nil;
            return arizona_neo::core::Result<std::string>::failure(
                arizona_neo::core::CoreError::AccessDenied, "security scope start failed");
        }
        return arizona_neo::core::Result<std::string>::success(ns_to_std(url.path));
    }
}

void IosDocumentsAccess::stop_access() noexcept {
    if (accessing_ && resolved_url_ != nil) {
        [resolved_url_ stopAccessingSecurityScopedResource];
        accessing_ = false;
    }
    resolved_url_ = nil;
}

arizona_neo::core::Result<void>
IosDocumentsAccess::save(const std::string& key, const std::string& bookmark) {
    @autoreleasepool {
        NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
        [defaults setObject:std_to_ns(bookmark) forKey:std_to_ns(key)];
        return arizona_neo::core::Result<void>::success();
    }
}

arizona_neo::core::Result<std::string>
IosDocumentsAccess::load(const std::string& key) const {
    @autoreleasepool {
        NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
        NSString* value = [defaults stringForKey:std_to_ns(key)];
        if (value == nil) {
            return arizona_neo::core::Result<std::string>::failure(
                arizona_neo::core::CoreError::NotFound, "no stored bookmark");
        }
        return arizona_neo::core::Result<std::string>::success(ns_to_std(value));
    }
}

} // namespace arizona_neo::ios

@interface NeoLoaderDocumentsPicker : NSObject <UIDocumentPickerDelegate>
@property (nonatomic, copy) void (^completion)(NSURL*, NSString*);
@end

@implementation NeoLoaderDocumentsPicker

- (void)documentPicker:(UIDocumentPickerViewController*)controller
didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    (void)controller;
    if (urls.count == 0) {
        if (self.completion) self.completion(nil, @"no directory selected");
        return;
    }
    if (self.completion) self.completion(urls.firstObject, nil);
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
    (void)controller;
    if (self.completion) self.completion(nil, @"cancelled");
}

@end

void arizona_neo::ios::present_neoloader_folder_picker(
    void* presenter_view_controller, void (^completion)(NSURL*, NSString*)) {
    @autoreleasepool {
        UIViewController* presenter = static_cast<UIViewController*>(presenter_view_controller);
        if (presenter == nil) {
            if (completion) completion(nil, @"presenter is nil");
            return;
        }
        UTType* folderType = [UTType typeWithIdentifier:@"public.folder"];
        UIDocumentPickerViewController* picker =
            [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[folderType]
                                                                  asCopy:NO];
        picker.allowsMultipleSelection = NO;
        NeoLoaderDocumentsPicker* delegate = [[NeoLoaderDocumentsPicker alloc] init];
        delegate.completion = completion;
        picker.delegate = delegate;
        // Keep the delegate alive for the picker lifetime through the presenter.
        objc_setAssociatedObject(picker, "NeoLoaderDocumentsPicker", delegate,
                                OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [presenter presentViewController:picker animated:YES completion:nil];
    }
}

#endif
