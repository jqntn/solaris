#include <machina/path_helpers.hpp>

#include <raylib.h>

namespace machina {

std::filesystem::path
ApplicationDirectory()
{
  const char* directory = GetApplicationDirectory();
  if (directory == nullptr || directory[0] == '\0') {
    return std::filesystem::current_path();
  }

  return std::filesystem::path(directory);
}

namespace {

[[nodiscard]] std::filesystem::path
RuntimePath(const char* relativePath)
{
  const std::filesystem::path executablePath =
    ApplicationDirectory() / relativePath;
  if (std::filesystem::exists(executablePath)) {
    return executablePath;
  }

  return std::filesystem::current_path() / relativePath;
}

}

std::filesystem::path
RuntimeAssetPath()
{
  return RuntimePath(MACHINA_ASSETS_ROOT);
}

std::filesystem::path
RuntimeMaterialXPath()
{
  return RuntimePath(MACHINA_MATERIALX_LIBRARY_ROOT);
}

}
