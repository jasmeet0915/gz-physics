/*
 * Copyright (C) 2019 Open Source Robotics Foundation
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

#include <dart/dynamics/BodyNode.hpp>
#include <dart/dynamics/Skeleton.hpp>
#include <dart/simulation/World.hpp>

#include <gtest/gtest.h>

#include <test/PhysicsPluginsList.hh>

#include <iostream>

#include <gz/physics/FindFeatures.hh>
#include <gz/plugin/Loader.hh>
#include <gz/physics/RequestEngine.hh>

#include <gz/math/eigen3/Conversions.hh>

// Features
#include <gz/physics/ConstructEmpty.hh>
#include <gz/physics/ForwardStep.hh>
#include <gz/physics/FrameSemantics.hh>
#include <gz/physics/FreeJoint.hh>
#include <gz/physics/FixedJoint.hh>
#include <gz/physics/GetEntities.hh>
#include <gz/physics/Joint.hh>
#include <gz/physics/Link.hh>
#include <gz/physics/Shape.hh>
#include <gz/physics/RevoluteJoint.hh>
#include <gz/physics/dartsim/World.hh>
#include <gz/physics/sdf/ConstructModel.hh>
#include <gz/physics/sdf/ConstructWorld.hh>
#include <gz/physics/sdf/ConstructLink.hh>

#include <sdf/Model.hh>
#include <sdf/Root.hh>
#include <sdf/World.hh>

#include "test/Utils.hh"
#include "ShapeFeatures.hh"

using namespace gz;

using TestFeatureList = gz::physics::FeatureList<
  physics::dartsim::RetrieveWorld,
  physics::AttachFixedJointFeature,
  physics::AddLinkExternalForceTorque,
  physics::LinkFrameSemantics,
  physics::DetachJointFeature,
  physics::SetJointTransformFromParentFeature,
  physics::ForwardStep,
  physics::FreeJointCast,
  physics::GetBasicJointState,
  physics::GetEntities,
  physics::RevoluteJointCast,
  physics::SetJointVelocityCommandFeature,
#if DART_VERSION_AT_LEAST(6, 10, 0)
  physics::GetShapeFrictionPyramidSlipCompliance,
  physics::SetShapeFrictionPyramidSlipCompliance,
#endif
  physics::sdf::ConstructSdfModel,
  physics::sdf::ConstructSdfWorld,
  physics::sdf::ConstructSdfLink
>;

using TestEnginePtr = physics::Engine3dPtr<TestFeatureList>;
using TestWorldPtr = physics::World3dPtr<TestFeatureList>;

class ShapeFeaturesFixture : public ::testing::Test
{
  protected: void SetUp() override
  {
    gz::plugin::Loader loader;
    loader.LoadLib(dartsim_plugin_LIB);

    gz::plugin::PluginPtr dartsim =
        loader.Instantiate("gz::physics::dartsim::Plugin");

    this->engine =
        gz::physics::RequestEngine3d<TestFeatureList>::From(dartsim);
    ASSERT_NE(nullptr, this->engine);
  }
  protected: TestEnginePtr engine;
};

// A predicate-formatter for asserting that two vectors are approximately equal.
class AssertVectorApprox
{
  public: explicit AssertVectorApprox(double _tol = 1e-6) : tol(_tol)
  {
  }

  public: ::testing::AssertionResult operator()(
              const char *_mExpr, const char *_nExpr, Eigen::Vector3d _m,
              Eigen::Vector3d _n)
  {
    if (gz::physics::test::Equal(_m, _n, this->tol))
      return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
           << _mExpr << " and " << _nExpr << " ([" << _m.transpose()
           << "] and [" << _n.transpose() << "]"
           << ") are not equal";
  }

  private: double tol;
};

#if DART_VERSION_AT_LEAST(6, 10, 0)
/////////////////////////////////////////////////
TEST_F(ShapeFeaturesFixture, PrimarySlipCompliance)
{
  sdf::Root root;
  const sdf::Errors errors = root.Load(TEST_WORLD_DIR "slip_compliance.sdf");
  ASSERT_TRUE(errors.empty()) << errors.front();

  const std::string modelName{"box"};
  const std::string linkName{"box_link"};
  const std::string shapeName{"box_collision"};

  auto world = this->engine->ConstructWorld(*root.WorldByIndex(0));

  auto model = world->GetModel(modelName);
  auto boxLink = model->GetLink(linkName);
  auto boxShape = boxLink->GetShape(shapeName);

  AssertVectorApprox vectorPredicate(1e-4);

  gz::physics::ForwardStep::Input input;
  gz::physics::ForwardStep::State state;
  gz::physics::ForwardStep::Output output;

  // Check that link is at rest
  {
    const auto frameData = boxLink->FrameDataRelativeToWorld();

    EXPECT_PRED_FORMAT2(vectorPredicate, Eigen::Vector3d::Zero(),
                        frameData.linearVelocity);
    EXPECT_PRED_FORMAT2(vectorPredicate, Eigen::Vector3d::Zero(),
                        frameData.angularVelocity);
    EXPECT_PRED_FORMAT2(vectorPredicate, Eigen::Vector3d::Zero(),
                        frameData.linearAcceleration);
    EXPECT_PRED_FORMAT2(vectorPredicate, Eigen::Vector3d::Zero(),
                        frameData.angularAcceleration);
  }

  const Eigen::Vector3d cmdForce{1, 0, 0};
  const double primarySlip = 0.5;

  // expect 0.0 initial slip
  EXPECT_DOUBLE_EQ(0.0, boxShape->GetPrimarySlipCompliance());

  boxShape->SetPrimarySlipCompliance(primarySlip);
  EXPECT_DOUBLE_EQ(primarySlip, boxShape->GetPrimarySlipCompliance());

  const std::size_t numSteps = 10000;
  for (std::size_t i = 0; i < numSteps; ++i)
  {
    world->Step(output, state, input);
    boxLink->AddExternalForce(
        physics::RelativeForce3d(physics::FrameID::World(), cmdForce),
        physics::RelativePosition3d(*boxLink, Eigen::Vector3d::Zero()));
  }

  {
    // velocity = slip * applied force
    const auto frameData = boxLink->FrameDataRelativeToWorld();
    EXPECT_PRED_FORMAT2(vectorPredicate, primarySlip * cmdForce,
                        (frameData.linearVelocity));
  }
}

/////////////////////////////////////////////////
TEST_F(ShapeFeaturesFixture, SecondarySlipCompliance)
{
  sdf::Root root;
  const sdf::Errors errors = root.Load(TEST_WORLD_DIR "slip_compliance.sdf");
  ASSERT_TRUE(errors.empty()) << errors.front();

  const std::string modelName{"box"};
  const std::string linkName{"box_link"};
  const std::string shapeName{"box_collision"};

  auto world = this->engine->ConstructWorld(*root.WorldByIndex(0));

  auto model = world->GetModel(modelName);
  auto boxLink = model->GetLink(linkName);
  auto boxShape = boxLink->GetShape(shapeName);

  AssertVectorApprox vectorPredicate(1e-4);

  gz::physics::ForwardStep::Input input;
  gz::physics::ForwardStep::State state;
  gz::physics::ForwardStep::Output output;

  // Check that link is at rest
  {
    const auto frameData = boxLink->FrameDataRelativeToWorld();

    EXPECT_PRED_FORMAT2(vectorPredicate, Eigen::Vector3d::Zero(),
                        frameData.linearVelocity);
    EXPECT_PRED_FORMAT2(vectorPredicate, Eigen::Vector3d::Zero(),
                        frameData.angularVelocity);
    EXPECT_PRED_FORMAT2(vectorPredicate, Eigen::Vector3d::Zero(),
                        frameData.linearAcceleration);
    EXPECT_PRED_FORMAT2(vectorPredicate, Eigen::Vector3d::Zero(),
                        frameData.angularAcceleration);
  }

  const Eigen::Vector3d cmdForce{0, 1, 0};
  const double secondarySlip = 0.25;

  // expect 0.0 initial slip
  EXPECT_DOUBLE_EQ(0.0, boxShape->GetSecondarySlipCompliance());

  boxShape->SetSecondarySlipCompliance(secondarySlip);
  EXPECT_DOUBLE_EQ(secondarySlip, boxShape->GetSecondarySlipCompliance());

  const std::size_t numSteps = 10000;
  for (std::size_t i = 0; i < numSteps; ++i)
  {
    world->Step(output, state, input);
    boxLink->AddExternalForce(
        physics::RelativeForce3d(physics::FrameID::World(), cmdForce),
        physics::RelativePosition3d(*boxLink, Eigen::Vector3d::Zero()));
  }

  {
    // velocity = slip * applied force
    const auto frameData = boxLink->FrameDataRelativeToWorld();
    EXPECT_PRED_FORMAT2(vectorPredicate, secondarySlip * cmdForce,
                        (frameData.linearVelocity));
  }
}
#endif

/////////////////////////////////////////////////
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
