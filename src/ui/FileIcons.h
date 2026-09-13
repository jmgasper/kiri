#pragma once
#include <memory>
#include <string>

class BBitmap;
namespace kiri {
// Shared native MIME icons, resolved once per type rather than per paint/file.
std::shared_ptr<const BBitmap> FileIcon(const std::string& path,bool directory=false);
}
