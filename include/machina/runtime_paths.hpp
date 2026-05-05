#pragma once

#include <filesystem>

namespace machina {

[[nodiscard]] std::filesystem::path
RuntimeAssetPath();
[[nodiscard]] std::filesystem::path
RuntimeMaterialXPath();

}
