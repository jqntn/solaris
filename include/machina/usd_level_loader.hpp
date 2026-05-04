#pragma once

#include <filesystem>
#include <machina/level_description.hpp>

namespace machina {

class UsdLevelLoader
{
public:
  [[nodiscard]] LevelDescription Load(const std::filesystem::path& path) const;
};

}
