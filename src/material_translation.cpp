#include <machina/material_translation.hpp>

#include <iomanip>
#include <sstream>

namespace machina {

std::string
FormatScalar(double value)
{
  std::ostringstream stream;
  stream << std::setprecision(9) << value;
  return stream.str();
}

std::string
FormatVector(const std::vector<double>& values)
{
  std::ostringstream stream;
  stream << std::setprecision(9);

  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      stream << ", ";
    }

    stream << values[index];
  }

  return stream.str();
}

std::optional<std::string>
MaterialXType(const std::string& usdType)
{
  if (usdType == "bool") {
    return "boolean";
  }

  if (usdType == "int") {
    return "integer";
  }

  if (usdType == "float" || usdType == "double") {
    return "float";
  }

  if (usdType == "float2" || usdType == "double2" || usdType == "texCoord2f" ||
      usdType == "texCoord2d") {
    return "vector2";
  }

  if (usdType == "float3" || usdType == "double3" || usdType == "vector3f" ||
      usdType == "vector3d" || usdType == "normal3f" || usdType == "normal3d") {
    return "vector3";
  }

  if (usdType == "color3f" || usdType == "color3d") {
    return "color3";
  }

  return std::nullopt;
}

std::string
NodeCategoryFromShaderId(const std::string& shaderId)
{
  std::string value = shaderId;

  if (value.starts_with("ND_")) {
    value.erase(0, 3);
  }

  const std::string suffix = "_surfaceshader";
  if (value.ends_with(suffix)) {
    value.erase(value.size() - suffix.size());
  }

  return value;
}

}
