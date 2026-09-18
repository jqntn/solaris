#pragma once

#include <optional>
#include <string>
#include <vector>

namespace machina {

[[nodiscard]] std::string
FormatScalar(double value);

[[nodiscard]] std::string
FormatVector(const std::vector<double>& values);

[[nodiscard]] std::optional<std::string>
MaterialXType(const std::string& usdType);

[[nodiscard]] std::string
NodeCategoryFromShaderId(const std::string& shaderId);

}
