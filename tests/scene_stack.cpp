#include <machina/scene.hpp>

#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

enum class Action
{
  None,
  Push,
  Pop,
  Replace,
  Clear,
  Quit,
};

class RecordingScene final : public machina::Scene
{
public:
  RecordingScene(std::vector<std::string>& events,
                 std::string name,
                 Action action = Action::None,
                 std::unique_ptr<machina::Scene> nextScene = nullptr)
    : events(events)
    , name(std::move(name))
    , action(action)
    , nextScene(std::move(nextScene))
  {
  }

  void Update(machina::SceneStack& scenes) override
  {
    events.push_back("update:" + name);

    switch (action) {
      case Action::None:
        break;

      case Action::Push:
        scenes.Push(std::move(nextScene));
        break;

      case Action::Pop:
        scenes.Pop();
        break;

      case Action::Replace:
        scenes.Replace(std::move(nextScene));
        break;

      case Action::Clear:
        scenes.Clear();
        break;

      case Action::Quit:
        scenes.RequestQuit();
        break;
    }
  }

  void Draw() override { events.push_back("draw:" + name); }

private:
  std::vector<std::string>& events;
  std::string name;
  Action action = Action::None;
  std::unique_ptr<machina::Scene> nextScene;
};

[[nodiscard]] std::unique_ptr<machina::Scene>
Scene(std::vector<std::string>& events,
      std::string name,
      Action action = Action::None,
      std::unique_ptr<machina::Scene> nextScene = nullptr)
{
  return std::make_unique<RecordingScene>(
    events, std::move(name), action, std::move(nextScene));
}

[[nodiscard]] int
Fail(std::string_view message)
{
  std::println("{}", message);
  return 1;
}

[[nodiscard]] bool
Same(const std::vector<std::string>& left,
     const std::vector<std::string>& right)
{
  return left == right;
}

[[nodiscard]] int
CheckTopOnlyUpdateAndBottomToTopDraw()
{
  std::vector<std::string> events;
  machina::SceneStack scenes;
  scenes.Push(Scene(events, "bottom"));
  scenes.Push(Scene(events, "top"));

  scenes.Update();
  if (!Same(events, { "update:top" })) {
    return Fail("expected only the top scene to update");
  }

  events.clear();
  scenes.Draw();
  if (!Same(events, { "draw:bottom", "draw:top" })) {
    return Fail("expected scenes to draw from bottom to top");
  }

  return 0;
}

[[nodiscard]] int
CheckDeferredPush()
{
  std::vector<std::string> events;
  machina::SceneStack scenes;
  scenes.Push(Scene(events, "root", Action::Push, Scene(events, "pushed")));

  scenes.Update();
  if (!Same(events, { "update:root" }) || scenes.Size() != 2) {
    return Fail("expected push to apply after the current update");
  }

  events.clear();
  scenes.Update();
  if (!Same(events, { "update:pushed" })) {
    return Fail("expected pushed scene to become the top scene");
  }

  return 0;
}

[[nodiscard]] int
CheckDeferredPop()
{
  std::vector<std::string> events;
  machina::SceneStack scenes;
  scenes.Push(Scene(events, "bottom"));
  scenes.Push(Scene(events, "top", Action::Pop));

  scenes.Update();
  if (!Same(events, { "update:top" }) || scenes.Size() != 1) {
    return Fail("expected pop to apply after the current update");
  }

  events.clear();
  scenes.Draw();
  if (!Same(events, { "draw:bottom" })) {
    return Fail("expected popped scene to stop drawing");
  }

  return 0;
}

[[nodiscard]] int
CheckDeferredReplace()
{
  std::vector<std::string> events;
  machina::SceneStack scenes;
  scenes.Push(Scene(events, "bottom"));
  scenes.Push(
    Scene(events, "top", Action::Replace, Scene(events, "replacement")));

  scenes.Update();
  if (!Same(events, { "update:top" }) || scenes.Size() != 2) {
    return Fail("expected replace to preserve stack depth after update");
  }

  events.clear();
  scenes.Update();
  if (!Same(events, { "update:replacement" })) {
    return Fail("expected replacement scene to become the top scene");
  }

  return 0;
}

[[nodiscard]] int
CheckDeferredClearAndEmptyNoOp()
{
  std::vector<std::string> events;
  machina::SceneStack scenes;
  scenes.Push(Scene(events, "bottom"));
  scenes.Push(Scene(events, "top", Action::Clear));

  scenes.Update();
  if (!Same(events, { "update:top" }) || !scenes.Empty()) {
    return Fail("expected clear to empty the stack after update");
  }

  events.clear();
  scenes.Update();
  scenes.Draw();
  if (!events.empty()) {
    return Fail("expected empty stack update and draw to be no-ops");
  }

  return 0;
}

[[nodiscard]] int
CheckImmediateMutationsAndQuit()
{
  std::vector<std::string> events;
  machina::SceneStack scenes;

  scenes.Push(Scene(events, "first"));
  if (scenes.Size() != 1) {
    return Fail("expected immediate push outside update");
  }

  scenes.Replace(Scene(events, "replacement"));
  events.clear();
  scenes.Update();
  if (!Same(events, { "update:replacement" }) || scenes.Size() != 1) {
    return Fail("expected immediate replace outside update");
  }

  scenes.Pop();
  if (!scenes.Empty()) {
    return Fail("expected immediate pop outside update");
  }

  scenes.Push(Scene(events, "quitter", Action::Quit));
  events.clear();
  scenes.Update();
  if (!Same(events, { "update:quitter" }) || !scenes.ShouldQuit()) {
    return Fail("expected scene-driven quit request to be recorded");
  }

  return 0;
}

}

int
main()
{
  if (const int result = CheckTopOnlyUpdateAndBottomToTopDraw(); result != 0) {
    return result;
  }
  if (const int result = CheckDeferredPush(); result != 0) {
    return result;
  }
  if (const int result = CheckDeferredPop(); result != 0) {
    return result;
  }
  if (const int result = CheckDeferredReplace(); result != 0) {
    return result;
  }
  if (const int result = CheckDeferredClearAndEmptyNoOp(); result != 0) {
    return result;
  }
  if (const int result = CheckImmediateMutationsAndQuit(); result != 0) {
    return result;
  }

  return 0;
}
