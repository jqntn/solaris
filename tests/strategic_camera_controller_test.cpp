#include <cmath>
#include <machina/strategic_camera_controller.hpp>
#include <print>
#include <raylib.h>
#include <string_view>

namespace {

[[nodiscard]] int
Fail(std::string_view message)
{
  std::println("{}", message);
  return 1;
}

[[nodiscard]] bool
Near(float left, float right)
{
  return std::fabs(left - right) <= 0.001f;
}

[[nodiscard]] bool
Same(Vector3 left, Vector3 right)
{
  return Near(left.x, right.x) && Near(left.y, right.y) &&
         Near(left.z, right.z);
}

[[nodiscard]] machina::StrategicCameraInput
BaseInput()
{
  return machina::StrategicCameraInput{ .frameTime = 1.0f,
                                        .screenWidth = 100,
                                        .screenHeight = 100,
                                        .mousePosition =
                                          Vector2{ 50.0f, 50.0f } };
}

[[nodiscard]] machina::StrategicCameraSettings
ImmediateSettings()
{
  machina::StrategicCameraSettings settings;
  settings.smoothingStrength = 0.0f;
  settings.defaultDistance = 10.0f;
  settings.minDistance = 4.0f;
  settings.maxDistance = 12.0f;
  settings.panSpeed = 10.0f;
  settings.mousePanSpeed = 0.001f;
  settings.edgeScrollThreshold = 10.0f;
  return settings;
}

[[nodiscard]] int
CheckZoomClamps()
{
  machina::StrategicCameraSettings settings = ImmediateSettings();
  settings.defaultDistance = 8.0f;
  machina::StrategicCameraController controller(settings);

  machina::StrategicCameraInput input = BaseInput();
  input.mouseWheel = 100.0f;
  controller.Update(input);
  if (!Near(controller.Distance(), settings.minDistance)) {
    return Fail("expected zoom-in to clamp to minimum distance");
  }

  input.mouseWheel = -100.0f;
  controller.Update(input);
  if (!Near(controller.Distance(), settings.maxDistance)) {
    return Fail("expected zoom-out to clamp to maximum distance");
  }

  return 0;
}

[[nodiscard]] int
CheckPanRespectsYaw()
{
  machina::StrategicCameraSettings settings = ImmediateSettings();
  settings.defaultYawDegrees = 90.0f;
  machina::StrategicCameraController controller(settings);

  machina::StrategicCameraInput input = BaseInput();
  input.panForward = 1.0f;
  controller.Update(input);

  const Vector3 target = controller.Target();
  if (target.x > -9.999f || !Near(target.z, 0.0f)) {
    return Fail("expected forward pan to follow current yaw");
  }

  return 0;
}

[[nodiscard]] int
CheckEdgePan()
{
  machina::StrategicCameraSettings settings = ImmediateSettings();
  settings.defaultYawDegrees = 0.0f;

  machina::StrategicCameraController edgeController(settings);
  machina::StrategicCameraInput edgeInput = BaseInput();
  edgeInput.mousePosition = Vector2{ 5.0f, 50.0f };
  edgeController.Update(edgeInput);
  if (!Near(edgeController.Target().x, -10.0f)) {
    return Fail("expected left edge to pan left");
  }

  machina::StrategicCameraController centerController(settings);
  machina::StrategicCameraInput centerInput = BaseInput();
  centerInput.mousePosition = Vector2{ 50.0f, 50.0f };
  centerController.Update(centerInput);
  if (!Same(centerController.Target(), Vector3{})) {
    return Fail("expected center cursor to avoid edge pan");
  }

  machina::StrategicCameraController blockedController(settings);
  machina::StrategicCameraInput blockedInput = BaseInput();
  blockedInput.mousePosition = Vector2{ 5.0f, 50.0f };
  blockedInput.mouseBlockedByUi = true;
  blockedController.Update(blockedInput);
  if (!Same(blockedController.Target(), Vector3{})) {
    return Fail("expected UI capture to block mouse edge pan");
  }

  return 0;
}

[[nodiscard]] int
CheckLeftDragPansMap()
{
  machina::StrategicCameraSettings settings = ImmediateSettings();
  settings.defaultYawDegrees = 0.0f;
  machina::StrategicCameraController controller(settings);

  machina::StrategicCameraInput input = BaseInput();
  input.panCamera = true;
  input.mouseDelta = Vector2{ 10.0f, 0.0f };
  controller.Update(input);

  if (!Near(controller.Target().x, -0.1f) ||
      !Near(controller.Target().z, 0.0f)) {
    return Fail("expected left mouse drag to pan the map plane");
  }

  machina::StrategicCameraController blockedController(settings);
  input.mouseBlockedByUi = true;
  blockedController.Update(input);
  if (!Same(blockedController.Target(), Vector3{})) {
    return Fail("expected UI capture to block left mouse drag pan");
  }

  return 0;
}

[[nodiscard]] int
CheckRightDragRotatesYaw()
{
  machina::StrategicCameraSettings settings = ImmediateSettings();
  settings.defaultYawDegrees = 0.0f;
  settings.rotationSpeedDegrees = 0.5f;
  machina::StrategicCameraController controller(settings);

  machina::StrategicCameraInput input = BaseInput();
  input.rotateCamera = true;
  input.mouseDelta = Vector2{ 10.0f, 0.0f };
  controller.Update(input);

  if (Near(controller.YawDegrees(), 0.0f)) {
    return Fail("expected right-drag to rotate yaw");
  }

  return 0;
}

[[nodiscard]] int
CheckFocusCentersTarget()
{
  machina::StrategicCameraController controller(ImmediateSettings());
  controller.FocusOn(Vector3{ 4.0f, 2.0f, 3.0f }, 7.0f);

  if (!Same(controller.Target(), Vector3{ 4.0f, 2.0f, 3.0f })) {
    return Fail("expected focus to center camera target");
  }

  if (!Near(controller.Distance(), 7.0f)) {
    return Fail("expected focus distance to apply");
  }

  const Camera camera = controller.Camera3D();
  if (!Same(camera.target, controller.Target()) ||
      camera.projection != CAMERA_PERSPECTIVE) {
    return Fail("expected Camera3D to expose a perspective camera at target");
  }

  return 0;
}

}

int
main()
{
  if (const int result = CheckZoomClamps(); result != 0) {
    return result;
  }
  if (const int result = CheckPanRespectsYaw(); result != 0) {
    return result;
  }
  if (const int result = CheckEdgePan(); result != 0) {
    return result;
  }
  if (const int result = CheckLeftDragPansMap(); result != 0) {
    return result;
  }
  if (const int result = CheckRightDragRotatesYaw(); result != 0) {
    return result;
  }
  if (const int result = CheckFocusCentersTarget(); result != 0) {
    return result;
  }

  return 0;
}
