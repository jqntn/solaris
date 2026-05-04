#include <entt/entt.hpp>
#include <machina/ecs.hpp>
#include <machina/level_description.hpp>
#include <machina/level_instantiator.hpp>

namespace machina {

void
LevelInstantiator::Instantiate(entt::registry& registry,
                               const LevelDescription& level) const
{
  for (const EntityDescription& description : level.entities) {
    const entt::entity entity = registry.create();
    registry.emplace<Name>(entity, description.name);
    registry.emplace<Transform>(entity, description.transform);
    registry.emplace<Renderable>(
      entity, description.mesh, description.material);
  }
}

}
