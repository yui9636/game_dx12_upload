#pragma once
#include "Entity/Entity.h"

/**
 * @brief ・ｽe・ｽq・ｽﾖ係・ｽi・ｽK・ｽw・ｽ\・ｽ・ｽ・ｽj・ｽ・ｽﾛ趣ｿｽ・ｽ・ｽ・ｽ・ｽR・ｽ・ｽ・ｽ|・ｽ[・ｽl・ｽ・ｽ・ｽg
 * ・ｽA・ｽ[・ｽL・ｽ^・ｽC・ｽvECS・ｽ・ｽ・ｽ・ｽ・ｽﾉ、std::vector・ｽ・ｽg・ｽ・ｽﾈゑｿｽ・ｽo・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽN・ｽ・ｽ・ｽX・ｽg・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾌ用
 */
struct HierarchyComponent {
    EntityID parent = Entity::NULL_ID;
    EntityID firstChild = Entity::NULL_ID;
    EntityID prevSibling = Entity::NULL_ID;
    EntityID nextSibling = Entity::NULL_ID;
};
