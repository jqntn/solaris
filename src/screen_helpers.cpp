#include <machina/screen_helpers.hpp>

#include <raylib.h>

namespace machina {

int
GetVisibleScreenWidth()
{
  return GetScreenWidth() - 1;
}

int
GetVisibleScreenHeight()
{
  return GetScreenHeight() - 1;
}

}
