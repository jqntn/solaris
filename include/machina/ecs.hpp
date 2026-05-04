#pragma once

#include <array>
#include <string>

namespace machina {

struct Name
{
  std::string value;
};

struct Transform
{
  std::array<float, 16> world;
};

struct Renderable
{
  std::size_t mesh = 0;
  std::size_t material = 0;
};

}
