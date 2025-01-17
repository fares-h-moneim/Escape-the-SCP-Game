#include "entity.hpp"
#include "../deserialize-utils.hpp"
#include "../components/component-deserializer.hpp"

#include <glm/gtx/euler_angles.hpp>

namespace our
{

    // This function returns the transformation matrix from the entity's local space to the world space
    // Remember that you can get the transformation matrix from this entity to its parent from "localTransform"
    // To get the local to world matrix, you need to combine this entities matrix with its parent's matrix and
    // its parent's parent's matrix and so on till you reach the root.
    glm::mat4 Entity::getLocalToWorldMatrix() const
    {
        // TODO: (Req 8) Write this function
        // go from localspace to worldspace
        // getting current object entity relative to its parent
        glm::mat4 localToWorldMatrix = localTransform.toMat4();
        // getting the parent of the current object
        const Entity *parent = this->parent;
        // while im still not top of the hierarchy
        while (parent != nullptr)
        {
            // multiply current object with its parent
            localToWorldMatrix = parent->localTransform.toMat4() * localToWorldMatrix;
            parent = parent->parent;
        }
        return localToWorldMatrix;
    }
    glm::mat4 Entity::getLocalToWorldMatrix_r() const
    {
        glm::mat4 localToWorldMatrix = localTransform.toMat4_r();
        // getting the parent of the current object
        const Entity *parent = this->parent;
        // while im still not top of the hierarchy
        while (parent != nullptr)
        {
            // multiply current object with its parent
            localToWorldMatrix = parent->localTransform.toMat4_r() * localToWorldMatrix;
            parent = parent->parent;
        }
        return localToWorldMatrix;

    }

    // Deserializes the entity data and components from a json object
    void Entity::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;
        name = data.value("name", name);
        localTransform.deserialize(data);
        if (data.contains("components"))
        {
            if (const auto &components = data["components"]; components.is_array())
            {
                for (auto &component : components)
                {
                    deserializeComponent(component, this);
                }
            }
        }
    }

}