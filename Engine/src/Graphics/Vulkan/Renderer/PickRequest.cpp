#include "PickRequest.hpp"
#include <entt/entt.hpp>

static_assert(sizeof(Cori::Graphics::EntityValueType) == sizeof(entt::entt_traits<entt::entity>::entity_type), "Entt entity value type changed, patch the renderer as well");
static_assert(Cori::Graphics::s_NullEntityID == entt::null, "Entt id type null value changed, patch the renderer as well");

//not only the shader layout has to be changed, but also the logic of picking as currently it relies on both Z and entity id fitting into one 64 bit atomic