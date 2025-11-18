#pragma once

#include "SdPathUtils.h"

// Legacy shim: prefer including SdPathUtils.h directly from lib/Common.
namespace WebSdHelpers {
using SdPathUtils::sanitizeSdPath;
using SdPathUtils::parentPath;
using SdPathUtils::extractBaseName;
using SdPathUtils::removeSdPath;
using SdPathUtils::sanitizeSdFilename;
using SdPathUtils::buildUploadTarget;
} // namespace WebSdHelpers
