/*
 * Copyright (C) 2021 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#ifndef GZ_PHYSICS_BULLET_SRC_GETENTITIESFEATURE_HH_
#define GZ_PHYSICS_BULLET_SRC_GETENTITIESFEATURE_HH_

#include <string>

#include <gz/physics/ConstructEmpty.hh>
#include <gz/physics/GetEntities.hh>
#include <gz/physics/RemoveEntities.hh>
#include <gz/physics/Implements.hh>

#include "Base.hh"

namespace gz {
namespace physics {
namespace bullet {

struct EntityManagementFeatureList : gz::physics::FeatureList<
  RemoveModelFromWorld,
  ConstructEmptyWorldFeature
> { };

class EntityManagementFeatures :
    public virtual Base,
    public virtual Implements3d<EntityManagementFeatureList>
{

  // ----- Remove entities -----
  public: bool RemoveModelByIndex(
      const Identity &_worldID, std::size_t _modelIndex) override;

  public: bool RemoveModelByName(
      const Identity &_worldID,
      const std::string &_modelName) override;

  public: bool RemoveModel(const Identity &_modelID) override;

  public: bool ModelRemoved(const Identity &_modelID) const override;

  // ----- Construct empty entities -----
  public: Identity ConstructEmptyWorld(
      const Identity &_engineID, const std::string & _name) override;
};

}  // namespace bullet
}  // namespace physics
}  // namespace gz

#endif
