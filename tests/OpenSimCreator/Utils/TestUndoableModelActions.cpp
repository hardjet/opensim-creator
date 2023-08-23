#include "OpenSimCreator/Utils/UndoableModelActions.hpp"

#include "OpenSimCreator/Model/ObjectPropertyEdit.hpp"
#include "OpenSimCreator/Model/UndoableModelStatePair.hpp"
#include "testopensimcreator_config.hpp"

#include <gtest/gtest.h>
#include <OpenSim/Common/AbstractProperty.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/SimbodyEngine/Body.h>
#include <OpenSim/Simulation/SimbodyEngine/Coordinate.h>
#include <OpenSim/Simulation/SimbodyEngine/PinJoint.h>

#include <functional>
#include <memory>

// repro for #642
//
// @AdrianHendrik reported that trying to add a body with an invalid name entirely crashes
// OSC, which implies that the operation causes a segfault
TEST(OpenSimActions, ActionAddBodyToModelThrowsIfBodyNameIsInvalid)
{
    osc::UndoableModelStatePair model;

    osc::BodyDetails details;
    details.bodyName = "test 1";
    details.parentFrameAbsPath = "/ground";  // this is what the dialog defaults to

    ASSERT_ANY_THROW({ osc::ActionAddBodyToModel(model, details); });
}

// repro for #495
//
// @JuliaVanBeesel reported that, when editing an OpenSim model via the editor UI, if
// they then delete the backing file (e.g. via Windows explorer), the editor UI will
// then show an error message from an exception, rather than carrying on or warning
// that something not-quite-right has happened
TEST(OpenSimActions, ActionUpdateModelFromBackingFileReturnsFalseIfFileDoesNotExist)
{
    osc::UndoableModelStatePair model;

    // it just returns `false` if there's no backing file
    ASSERT_FALSE(osc::ActionUpdateModelFromBackingFile(model));

    // ... but if you say it has an invalid backing file path...
    model.setFilesystemPath("doesnt-exist");

    // then it should just return `false`, rather than (e.g.) exploding
    ASSERT_FALSE(osc::ActionUpdateModelFromBackingFile(model));
}

// repro for #654
//
// the bug is in OpenSim, but the action needs to hack around that bug until it is fixed
// upstream
TEST(OpenSimActions, ActionApplyRangeDeletionPropertyEditShouldThrow)
{
    // create undoable model with one body + joint
    auto undoableModel = []()
    {
        OpenSim::Model model;
        auto body = std::make_unique<OpenSim::Body>("body", 1.0, SimTK::Vec3{}, SimTK::Inertia{});
        auto joint = std::make_unique<OpenSim::PinJoint>();
        joint->setName("joint");
        joint->updCoordinate().setName("rotation");
        joint->connectSocket_parent_frame(model.getGround());
        joint->connectSocket_child_frame(*body);
        model.addJoint(joint.release());
        model.addBody(body.release());
        model.finalizeConnections();
        return osc::UndoableModelStatePair{std::make_unique<OpenSim::Model>(std::move(model))};
    }();

    osc::ObjectPropertyEdit edit
    {
        undoableModel.updModel().updComponent<OpenSim::Coordinate>("/jointset/joint/rotation").updProperty_range(),
        [](OpenSim::AbstractProperty& p)
        {
            p.clear();
        },
    };

    // should throw on application of the faulty edit, not at some later time
    ASSERT_ANY_THROW({ osc::ActionApplyPropertyEdit(undoableModel, edit); });
}
