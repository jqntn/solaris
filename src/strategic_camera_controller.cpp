#include <algorithm>
#include <cmath>
#include <machina/strategic_camera_controller.hpp>
#include <raymath.h>

namespace machina {

namespace {

[[nodiscard]] float
Axis(bool positive, bool negative)
{
  return (positive ? 1.0f : 0.0f) - (negative ? 1.0f : 0.0f);
}

[[nodiscard]] float
MinimumDistance(const StrategicCameraSettings& settings)
{
  return std::max(settings.minDistance, 0.01f);
}

[[nodiscard]] float
MaximumDistance(const StrategicCameraSettings& settings)
{
  return std::max(settings.maxDistance, MinimumDistance(settings));
}

[[nodiscard]] float
ClampDistance(float distance, const StrategicCameraSettings& settings)
{
  return std::clamp(
    distance, MinimumDistance(settings), MaximumDistance(settings));
}

[[nodiscard]] float
MinimumPitch(const StrategicCameraSettings& settings)
{
  return std::clamp(settings.minPitchDegrees, 1.0f, 89.0f);
}

[[nodiscard]] float
MaximumPitch(const StrategicCameraSettings& settings)
{
  return std::clamp(
    std::max(settings.maxPitchDegrees, MinimumPitch(settings)), 1.0f, 89.0f);
}

[[nodiscard]] float
ClampPitch(float pitchDegrees, const StrategicCameraSettings& settings)
{
  return std::clamp(
    pitchDegrees, MinimumPitch(settings), MaximumPitch(settings));
}

[[nodiscard]] float
NormalizeDegrees(float degrees)
{
  float normalized = std::fmod(degrees, 360.0f);
  if (normalized < 0.0f) {
    normalized += 360.0f;
  }

  return normalized;
}

[[nodiscard]] float
ShortestAngleDelta(float fromDegrees, float toDegrees)
{
  float delta = std::fmod(toDegrees - fromDegrees + 540.0f, 360.0f) - 180.0f;
  if (delta < -180.0f) {
    delta += 360.0f;
  }

  return delta;
}

[[nodiscard]] float
SmoothingAlpha(float frameTime, float smoothingStrength)
{
  if (smoothingStrength <= 0.0f) {
    return 1.0f;
  }

  return 1.0f - std::exp(-smoothingStrength * std::max(frameTime, 0.0f));
}

[[nodiscard]] Vector3
HorizontalForward(float yawDegrees)
{
  const float yawRadians = yawDegrees * DEG2RAD;
  return Vector3{ -std::sin(yawRadians), 0.0f, -std::cos(yawRadians) };
}

[[nodiscard]] Vector3
HorizontalRight(float yawDegrees)
{
  const float yawRadians = yawDegrees * DEG2RAD;
  return Vector3{ std::cos(yawRadians), 0.0f, -std::sin(yawRadians) };
}

[[nodiscard]] bool
MouseInsideScreen(const StrategicCameraInput& input)
{
  return input.screenWidth > 0 && input.screenHeight > 0 &&
         input.mousePosition.x >= 0.0f &&
         input.mousePosition.x < static_cast<float>(input.screenWidth) &&
         input.mousePosition.y >= 0.0f &&
         input.mousePosition.y < static_cast<float>(input.screenHeight);
}

[[nodiscard]] float
EdgeAxis(float position, int size, float threshold)
{
  if (size <= 0 || threshold <= 0.0f) {
    return 0.0f;
  }

  const float edge = std::min(threshold, static_cast<float>(size) * 0.5f);
  if (position < edge) {
    return -1.0f;
  }

  if (position >= static_cast<float>(size) - edge) {
    return 1.0f;
  }

  return 0.0f;
}

[[nodiscard]] Vector3
PlanarPanVector(float panRight, float panForward, float yawDegrees)
{
  return Vector3Add(Vector3Scale(HorizontalRight(yawDegrees), panRight),
                    Vector3Scale(HorizontalForward(yawDegrees), panForward));
}

[[nodiscard]] Vector3
PlanarPanDirection(float panRight, float panForward, float yawDegrees)
{
  Vector3 direction = PlanarPanVector(panRight, panForward, yawDegrees);
  const float length = Vector3Length(direction);
  if (length > 1.0f) {
    direction = Vector3Scale(direction, 1.0f / length);
  }

  return direction;
}

[[nodiscard]] Vector3
CameraOffset(float distance, float yawDegrees, float pitchDegrees)
{
  const float yawRadians = yawDegrees * DEG2RAD;
  const float pitchRadians = pitchDegrees * DEG2RAD;
  const float horizontalDistance = std::cos(pitchRadians) * distance;

  return Vector3{ std::sin(yawRadians) * horizontalDistance,
                  std::sin(pitchRadians) * distance,
                  std::cos(yawRadians) * horizontalDistance };
}

}

StrategicCameraInput
StrategicCameraControls::Read(StrategicCameraControlCapture capture)
{
  return StrategicCameraInput{
    .frameTime = GetFrameTime(),
    .screenWidth = GetScreenWidth(),
    .screenHeight = GetScreenHeight(),
    .mousePosition = GetMousePosition(),
    .mouseDelta = GetMouseDelta(),
    .mouseWheel = GetMouseWheelMove(),
    .panRight = Axis(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT),
                     IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)),
    .panForward = Axis(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP),
                       IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)),
    .panCamera = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
    .rotateCamera = IsMouseButtonDown(MOUSE_BUTTON_RIGHT),
    .boost = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT),
    .mouseBlockedByUi = capture.mouseBlockedByUi,
    .keyboardBlockedByUi = capture.keyboardBlockedByUi,
  };
}

StrategicCameraController::StrategicCameraController()
  : StrategicCameraController(StrategicCameraSettings{})
{
}

StrategicCameraController::StrategicCameraController(
  StrategicCameraSettings cameraSettings)
  : settings(cameraSettings)
  , distance(ClampDistance(settings.defaultDistance, settings))
  , desiredDistance(distance)
  , yawDegrees(NormalizeDegrees(settings.defaultYawDegrees))
  , desiredYawDegrees(yawDegrees)
  , pitchDegrees(ClampPitch(settings.pitchDegrees, settings))
  , desiredPitchDegrees(pitchDegrees)
{
}

void
StrategicCameraController::Update(const StrategicCameraInput& input)
{
  const float frameTime = std::max(input.frameTime, 0.0f);
  float inputPanRight =
    input.keyboardBlockedByUi ? 0.0f : std::clamp(input.panRight, -1.0f, 1.0f);
  float inputPanForward = input.keyboardBlockedByUi
                            ? 0.0f
                            : std::clamp(input.panForward, -1.0f, 1.0f);

  if (!input.mouseBlockedByUi && MouseInsideScreen(input)) {
    if (input.panCamera) {
      desiredTarget = Vector3Add(
        desiredTarget,
        Vector3Scale(
          PlanarPanVector(
            -input.mouseDelta.x, input.mouseDelta.y, desiredYawDegrees),
          std::max(desiredDistance, 0.01f) * settings.mousePanSpeed));
    } else {
      inputPanRight += EdgeAxis(
        input.mousePosition.x, input.screenWidth, settings.edgeScrollThreshold);
      inputPanForward -= EdgeAxis(input.mousePosition.y,
                                  input.screenHeight,
                                  settings.edgeScrollThreshold);
    }

    if (input.rotateCamera) {
      desiredYawDegrees = NormalizeDegrees(
        desiredYawDegrees - input.mouseDelta.x * settings.rotationSpeedDegrees);
      desiredPitchDegrees =
        ClampPitch(desiredPitchDegrees +
                     input.mouseDelta.y * settings.rotationSpeedDegrees,
                   settings);
    }

    if (input.mouseWheel != 0.0f) {
      desiredDistance = ClampDistance(
        desiredDistance * std::exp(-input.mouseWheel * settings.zoomSpeed),
        settings);
    }
  }

  const Vector3 panDirection =
    PlanarPanDirection(inputPanRight, inputPanForward, desiredYawDegrees);
  if (Vector3Length(panDirection) > 0.0f) {
    const float baseDistance = std::max(settings.defaultDistance, 0.01f);
    const float distanceScale =
      std::clamp(desiredDistance / baseDistance, 0.25f, 6.0f);
    const float boostScale = input.boost ? settings.boostMultiplier : 1.0f;
    const float panDistance =
      settings.panSpeed * frameTime * distanceScale * boostScale;
    desiredTarget =
      Vector3Add(desiredTarget, Vector3Scale(panDirection, panDistance));
  }

  const float alpha = SmoothingAlpha(frameTime, settings.smoothingStrength);
  target = Vector3Lerp(target, desiredTarget, alpha);
  distance += (desiredDistance - distance) * alpha;
  yawDegrees = NormalizeDegrees(
    yawDegrees + ShortestAngleDelta(yawDegrees, desiredYawDegrees) * alpha);
  pitchDegrees += (desiredPitchDegrees - pitchDegrees) * alpha;
}

void
StrategicCameraController::FocusOn(Vector3 focus,
                                   std::optional<float> focusDistance)
{
  desiredTarget = focus;
  target = focus;

  if (focusDistance.has_value()) {
    desiredDistance = ClampDistance(*focusDistance, settings);
    distance = desiredDistance;
  }
}

Camera
StrategicCameraController::Camera3D() const
{
  const Vector3 position =
    Vector3Add(target, CameraOffset(distance, yawDegrees, pitchDegrees));
  return Camera{ .position = position,
                 .target = target,
                 .up = Vector3{ 0.0f, 1.0f, 0.0f },
                 .fovy = std::clamp(settings.fovyDegrees, 1.0f, 120.0f),
                 .projection = CAMERA_PERSPECTIVE };
}

Vector3
StrategicCameraController::Target() const
{
  return target;
}

float
StrategicCameraController::Distance() const
{
  return distance;
}

float
StrategicCameraController::YawDegrees() const
{
  return yawDegrees;
}

float
StrategicCameraController::PitchDegrees() const
{
  return pitchDegrees;
}

}
