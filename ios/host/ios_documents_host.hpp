#pragma once

#if defined(__APPLE__)

#include "arizona_neo/core/user_documents_root.hpp"

#include <functional>
#include <string>

#import <Foundation/Foundation.h>

namespace arizona_neo::ios {

// Real implementation of the abstract DocumentsAccess/BookmarkStore contracts
// for iOS. It uses Foundation's security-scoped bookmarks and never stores a
// raw path as if it were durable access.
class IosDocumentsAccess final : public arizona_neo::core::DocumentsAccess,
                                 public arizona_neo::core::BookmarkStore {
public:
    IosDocumentsAccess();
    ~IosDocumentsAccess() override;

    arizona_neo::core::Result<std::string> create_bookmark(const std::string& path) override;
    arizona_neo::core::Result<std::string> resolve_bookmark(const std::string& bookmark) override;
    void stop_access() noexcept override;

    arizona_neo::core::Result<void> save(const std::string& key,
                                         const std::string& bookmark) override;
    arizona_neo::core::Result<std::string> load(const std::string& key) const override;

private:
    NSURL* resolved_url_ = nil;
    bool accessing_ = false;
};

// Presents the system directory picker. This must be called from the main
// thread with a real UIViewController supplied by the host application.
// Completion receives either a security-scoped URL or an error string.
void present_neoloader_folder_picker(void* presenter_view_controller,
                                    void (^completion)(NSURL* selected_url,
                                                       NSString* error));

} // namespace arizona_neo::ios

#endif
