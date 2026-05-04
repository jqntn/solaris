#pragma once

#include <optional>
#include <raylib.h>

namespace machina {

struct StrategicCameraSettings
{
  float defaultYawDegrees = 45.0f;
  float pitchDegrees = 42.0f;
  float defaultDistance = 10.0f;
  float minDistance = 3.0f;
  float maxDistance = 120.0f;
  float panSpeed = 6.0f;
  float mousePanSpeed = 0.001f;
  float zoomSpeed = 0.14f;
  float rotationSpeedDegrees = 0.11f;
  float edgeScrollThreshold = 18.0f;
  float smoothingStrength = 14.0f;
  float boostMultiplier = 2.75f;
  float fovyDegrees = 60.0f;
};

struct StrategicCameraInput
{
  float frameTime = 0.0f;
  int screenWidth = 0;
  int screenHeight = 0;
  Vector2 mousePosition = {};
  Vector2 mouseDelta = {};
  float mouseWheel = 0.0f;
  float panRight = 0.0f;
  float panForward = 0.0f;
  bool panCamera = false;
  bool rotateCamera = false;
  bool boost = false;
  bool mouseBlockedByUi = false;
  bool keyboardBlockedByUi = false;
};

struct StrategicCameraControlCapture
{
  bool mouseBlockedByUi = false;
  bool keyboardBlockedByUi = false;
};

class StrategicCameraControls
{
public:
  [[nodiscard]] static StrategicCameraInput Read(
    StrategicCameraControlCapture capture);
};

class StrategicCameraController
{
public:
  StrategicCameraController();
  explicit StrategicCameraController(StrategicCameraSettings cameraSettings);

  void Update(const StrategicCameraInput& input);
  void FocusOn(Vector3 focus, std::optional<float> distance = std::nullopt);

  [[nodiscard]] Camera Camera3D() const;
  [[nodiscard]] Vector3 Target() const;
  [[nodiscard]] float Distance() const;
  [[nodiscard]] float YawDegrees() const;

private:
  StrategicCameraSettings settings;
  Vector3 target = {};
  Vector3 desiredTarget = {};
  float distance = 0.0f;
  float desiredDistance = 0.0f;
  float yawDegrees = 0.0f;
  float desiredYawDegrees = 0.0f;
};

}
