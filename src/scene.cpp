#include <machina/scene.hpp>

#include <utility>

namespace machina {

void
SceneStack::Push(std::unique_ptr<Scene> scene)
{
  QueueOrApply(Command{ .type = CommandType::Push, .scene = std::move(scene) });
}

void
SceneStack::Pop()
{
  QueueOrApply(Command{ .type = CommandType::Pop });
}

void
SceneStack::Replace(std::unique_ptr<Scene> scene)
{
  QueueOrApply(
    Command{ .type = CommandType::Replace, .scene = std::move(scene) });
}

void
SceneStack::Clear()
{
  QueueOrApply(Command{ .type = CommandType::Clear });
}

void
SceneStack::Update()
{
  if (scenes.empty()) {
    return;
  }

  updating = true;
  scenes.back()->Update(*this);
  updating = false;

  ApplyPendingCommands();
}

void
SceneStack::Draw(SceneDrawContext& context)
{
  for (const std::unique_ptr<Scene>& scene : scenes) {
    scene->Draw(context);
  }
}

void
SceneStack::DrawUi()
{
  for (const std::unique_ptr<Scene>& scene : scenes) {
    scene->DrawUi();
  }
}

bool
SceneStack::Empty() const
{
  return scenes.empty();
}

std::size_t
SceneStack::Size() const
{
  return scenes.size();
}

void
SceneStack::RequestQuit()
{
  quitRequested = true;
}

bool
SceneStack::ShouldQuit() const
{
  return quitRequested;
}

void
SceneStack::QueueOrApply(Command command)
{
  if (updating) {
    pendingCommands.push_back(std::move(command));
    return;
  }

  Apply(std::move(command));
}

void
SceneStack::Apply(Command command)
{
  switch (command.type) {
    case CommandType::Push:
      if (command.scene != nullptr) {
        scenes.push_back(std::move(command.scene));
      }
      break;

    case CommandType::Pop:
      if (!scenes.empty()) {
        scenes.pop_back();
      }
      break;

    case CommandType::Replace:
      if (!scenes.empty()) {
        scenes.pop_back();
      }
      if (command.scene != nullptr) {
        scenes.push_back(std::move(command.scene));
      }
      break;

    case CommandType::Clear:
      scenes.clear();
      break;
  }
}

void
SceneStack::ApplyPendingCommands()
{
  std::vector<Command> commands;
  commands.swap(pendingCommands);

  for (Command& command : commands) {
    Apply(std::move(command));
  }
}

}
