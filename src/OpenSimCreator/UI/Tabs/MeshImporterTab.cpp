#include "MeshImporterTab.hpp"

#include <OpenSimCreator/Bindings/SimTKHelpers.hpp>
#include <OpenSimCreator/Bindings/SimTKMeshLoader.hpp>
#include <OpenSimCreator/Model/UndoableModelStatePair.hpp>
#include <OpenSimCreator/ModelGraph/BodyEl.hpp>
#include <OpenSimCreator/ModelGraph/CommittableModelGraph.hpp>
#include <OpenSimCreator/ModelGraph/CommittableModelGraphActions.hpp>
#include <OpenSimCreator/ModelGraph/CrossrefDescriptor.hpp>
#include <OpenSimCreator/ModelGraph/CrossrefDirection.hpp>
#include <OpenSimCreator/ModelGraph/EdgeEl.hpp>
#include <OpenSimCreator/ModelGraph/GroundEl.hpp>
#include <OpenSimCreator/ModelGraph/ISceneElLookup.hpp>
#include <OpenSimCreator/ModelGraph/JointEl.hpp>
#include <OpenSimCreator/ModelGraph/MeshEl.hpp>
#include <OpenSimCreator/ModelGraph/ModelGraph.hpp>
#include <OpenSimCreator/ModelGraph/ModelGraphHelpers.hpp>
#include <OpenSimCreator/ModelGraph/ModelGraphIDs.hpp>
#include <OpenSimCreator/ModelGraph/ModelGraphOpenSimBridge.hpp>
#include <OpenSimCreator/ModelGraph/ModelGraphStrings.hpp>
#include <OpenSimCreator/ModelGraph/SceneEl.hpp>
#include <OpenSimCreator/ModelGraph/SceneElClass.hpp>
#include <OpenSimCreator/ModelGraph/SceneElCRTP.hpp>
#include <OpenSimCreator/ModelGraph/SceneElFlags.hpp>
#include <OpenSimCreator/ModelGraph/SceneElHelpers.hpp>
#include <OpenSimCreator/ModelGraph/SceneElVariant.hpp>
#include <OpenSimCreator/ModelGraph/StationEl.hpp>
#include <OpenSimCreator/Registry/ComponentRegistry.hpp>
#include <OpenSimCreator/Registry/StaticComponentRegistries.hpp>
#include <OpenSimCreator/UI/Middleware/MainUIStateAPI.hpp>
#include <OpenSimCreator/UI/Tabs/ModelEditorTab.hpp>
#include <OpenSimCreator/UI/Widgets/MainMenu.hpp>
#include <OpenSimCreator/Utils/OpenSimHelpers.hpp>

#include <imgui.h>
#include <IconsFontAwesome5.h>
#include <ImGuizmo.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/SimbodyEngine/Joint.h>
#include <oscar/Bindings/ImGuiHelpers.hpp>
#include <oscar/Bindings/ImGuizmoHelpers.hpp>
#include <oscar/Formats/CSV.hpp>
#include <oscar/Formats/OBJ.hpp>
#include <oscar/Formats/STL.hpp>
#include <oscar/Graphics/Color.hpp>
#include <oscar/Graphics/GraphicsHelpers.hpp>
#include <oscar/Graphics/Mesh.hpp>
#include <oscar/Graphics/MeshGenerators.hpp>
#include <oscar/Graphics/ShaderCache.hpp>
#include <oscar/Maths/AABB.hpp>
#include <oscar/Maths/CollisionTests.hpp>
#include <oscar/Maths/EasingFunctions.hpp>
#include <oscar/Maths/Line.hpp>
#include <oscar/Maths/Mat3.hpp>
#include <oscar/Maths/Mat4.hpp>
#include <oscar/Maths/Mat4x3.hpp>
#include <oscar/Maths/MathHelpers.hpp>
#include <oscar/Maths/Quat.hpp>
#include <oscar/Maths/RayCollision.hpp>
#include <oscar/Maths/Rect.hpp>
#include <oscar/Maths/Sphere.hpp>
#include <oscar/Maths/Segment.hpp>
#include <oscar/Maths/Transform.hpp>
#include <oscar/Maths/PolarPerspectiveCamera.hpp>
#include <oscar/Maths/Vec2.hpp>
#include <oscar/Maths/Vec3.hpp>
#include <oscar/Maths/Vec4.hpp>
#include <oscar/Platform/App.hpp>
#include <oscar/Platform/AppMetadata.hpp>
#include <oscar/Platform/Log.hpp>
#include <oscar/Platform/os.hpp>
#include <oscar/Scene/SceneCache.hpp>
#include <oscar/Scene/SceneDecoration.hpp>
#include <oscar/Scene/SceneHelpers.hpp>
#include <oscar/Scene/SceneRenderer.hpp>
#include <oscar/Scene/SceneRendererParams.hpp>
#include <oscar/Shims/Cpp23/utility.hpp>
#include <oscar/UI/Panels/PerfPanel.hpp>
#include <oscar/UI/Panels/UndoRedoPanel.hpp>
#include <oscar/UI/Tabs/TabHost.hpp>
#include <oscar/UI/Widgets/LogViewer.hpp>
#include <oscar/UI/Widgets/Popup.hpp>
#include <oscar/UI/Widgets/PopupManager.hpp>
#include <oscar/UI/Widgets/StandardPopup.hpp>
#include <oscar/Utils/Assertions.hpp>
#include <oscar/Utils/ClonePtr.hpp>
#include <oscar/Utils/Concepts.hpp>
#include <oscar/Utils/CStringView.hpp>
#include <oscar/Utils/FilesystemHelpers.hpp>
#include <oscar/Utils/ParentPtr.hpp>
#include <oscar/Utils/ScopeGuard.hpp>
#include <oscar/Utils/SetHelpers.hpp>
#include <oscar/Utils/Spsc.hpp>
#include <oscar/Utils/StringHelpers.hpp>
#include <oscar/Utils/UID.hpp>
#include <oscar/Utils/UndoRedo.hpp>
#include <oscar/Utils/VariantHelpers.hpp>
#include <SDL_events.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <exception>
#include <filesystem>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sstream>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <variant>

using osc::App;
using osc::AABB;
using osc::ClonePtr;
using osc::Color;
using osc::ConstructibleFrom;
using osc::ConstSceneElVariant;
using osc::CrossrefDescriptor;
using osc::CrossrefDirection;
using osc::CStringView;
using osc::DerivedFrom;
using osc::Identity;
using osc::Invocable;
using osc::ISceneElLookup;
using osc::Line;
using osc::Mat3;
using osc::Mat4;
using osc::Mat4x3;
using osc::Material;
using osc::MaterialPropertyBlock;
using osc::Mesh;
using osc::ModelCreationFlags;
using osc::ModelGraph;
using osc::ModelGraphIDs;
using osc::ModelGraphStrings;
using osc::operator<<;
using osc::Overload;
using osc::PolarPerspectiveCamera;
using osc::Quat;
using osc::RayCollision;
using osc::Rect;
using osc::RenderTexture;
using osc::SceneCache;
using osc::SceneDecoration;
using osc::SceneDecorationFlags;
using osc::SceneEl;
using osc::SceneElClass;
using osc::SceneElFlags;
using osc::SceneElVariant;
using osc::SceneRenderer;
using osc::SceneRendererParams;
using osc::ShaderCache;
using osc::StandardPopup;
using osc::Sphere;
using osc::Transform;
using osc::Vec2;
using osc::Vec3;
using osc::Vec4;
using osc::UID;

using osc::GroundEl;
using osc::MeshEl;
using osc::BodyEl;
using osc::JointEl;
using osc::StationEl;
using osc::EdgeEl;

using osc::CommittableModelGraph;

// generic helper functions
namespace
{
    // returns a string representation of a spatial position (e.g. (0.0, 1.0, 3.0))
    std::string PosString(Vec3 const& pos)
    {
        std::stringstream ss;
        ss.precision(4);
        ss << '(' << pos.x << ", " << pos.y << ", " << pos.z << ')';
        return std::move(ss).str();
    }

    // returns a camera that is in the initial position the camera should be in for this screen
    PolarPerspectiveCamera CreateDefaultCamera()
    {
        PolarPerspectiveCamera rv;
        rv.phi = std::numbers::pi_v<float>/4.0f;
        rv.theta = std::numbers::pi_v<float>/4.0f;
        rv.radius = 2.5f;
        return rv;
    }

    void SpacerDummy()
    {
        ImGui::Dummy({0.0f, 5.0f});
    }

    Color FaintifyColor(Color const& srcColor)
    {
        Color color = srcColor;
        color.a *= 0.2f;
        return color;
    }

    Color RedifyColor(Color const& srcColor)
    {
        constexpr float factor = 0.8f;
        return {srcColor[0], factor * srcColor[1], factor * srcColor[2], factor * srcColor[3]};
    }
}

// background mesh loading support
//
// loading mesh files can be slow, so all mesh loading is done on a background worker
// that:
//
//   - receives a mesh loading request
//   - loads the mesh
//   - sends the loaded mesh (or error) as a response
//
// the main (UI) thread then regularly polls the response channel and handles the (loaded)
// mesh appropriately
namespace
{
    // a mesh loading request
    struct MeshLoadRequest final {
        UID preferredAttachmentPoint;
        std::vector<std::filesystem::path> paths;
    };

    // a successfully-loaded mesh
    struct LoadedMesh final {
        std::filesystem::path path;
        Mesh meshData;
    };

    // an OK response to a mesh loading request
    struct MeshLoadOKResponse final {
        UID preferredAttachmentPoint;
        std::vector<LoadedMesh> meshes;
    };

    // an ERROR response to a mesh loading request
    struct MeshLoadErrorResponse final {
        UID preferredAttachmentPoint;
        std::filesystem::path path;
        std::string error;
    };

    // an OK or ERROR response to a mesh loading request
    using MeshLoadResponse = std::variant<MeshLoadOKResponse, MeshLoadErrorResponse>;

    // returns an OK or ERROR response to a mesh load request
    MeshLoadResponse respondToMeshloadRequest(MeshLoadRequest msg)  // NOLINT(performance-unnecessary-value-param)
    {
        std::vector<LoadedMesh> loadedMeshes;
        loadedMeshes.reserve(msg.paths.size());

        for (std::filesystem::path const& path : msg.paths)
        {
            try
            {
                loadedMeshes.push_back(LoadedMesh{path, osc::LoadMeshViaSimTK(path)});
            }
            catch (std::exception const& ex)
            {
                // swallow the exception and emit a log error
                //
                // older implementations used to cancel loading the entire batch by returning
                // an MeshLoadErrorResponse, but that wasn't a good idea because there are
                // times when a user will drag in a bunch of files and expect all the valid
                // ones to load (#303)

                osc::log::error("%s: error loading mesh file: %s", path.string().c_str(), ex.what());
            }
        }

        // ensure the UI thread redraws after the mesh is loaded
        App::upd().requestRedraw();

        return MeshLoadOKResponse{msg.preferredAttachmentPoint, std::move(loadedMeshes)};
    }

    // a class that loads meshes in a background thread
    //
    // the UI thread must `.poll()` this to check for responses
    class MeshLoader final {
    public:
        MeshLoader() : m_Worker{Worker::create(respondToMeshloadRequest)}
        {
        }

        void send(MeshLoadRequest req)
        {
            m_Worker.send(std::move(req));
        }

        std::optional<MeshLoadResponse> poll()
        {
            return m_Worker.poll();
        }

    private:
        using Worker = osc::spsc::Worker<MeshLoadRequest, MeshLoadResponse, decltype(respondToMeshloadRequest)>;
        Worker m_Worker;
    };
}

// 3D rendering support
//
// this code exists to make the modelgraph, and any other decorations (lines, hovers, selections, etc.)
// renderable in the UI
namespace
{
    // returns a transform that maps a sphere mesh (defined to be @ 0,0,0 with radius 1)
    // to some sphere in the scene (e.g. a body/ground)
    Transform SphereMeshToSceneSphereTransform(Sphere const& sceneSphere)
    {
        Transform t;
        t.scale *= sceneSphere.radius;
        t.position = sceneSphere.origin;
        return t;
    }

    // something that is being drawn in the scene
    struct DrawableThing final {
        UID id = ModelGraphIDs::Empty();
        UID groupId = ModelGraphIDs::Empty();
        Mesh mesh;
        Transform transform;
        Color color = Color::black();
        SceneDecorationFlags flags = SceneDecorationFlags::None;
        std::optional<Material> maybeMaterial;
        std::optional<MaterialPropertyBlock> maybePropertyBlock;
    };

    AABB calcBounds(DrawableThing const& dt)
    {
        return osc::TransformAABB(dt.mesh.getBounds(), dt.transform);
    }
}

// shared data support
//
// data that's shared between multiple UI states.
namespace
{
    // a class that holds hover user mousehover information
    class Hover final {
    public:
        Hover() :
            ID{ModelGraphIDs::Empty()},
            Pos{}
        {
        }

        Hover(UID id_, Vec3 pos_) :
            ID{id_},
            Pos{pos_}
        {
        }

        explicit operator bool () const
        {
            return ID != ModelGraphIDs::Empty();
        }

        void reset()
        {
            *this = Hover{};
        }

        UID ID;
        Vec3 Pos;
    };

    class SharedData final {
    public:
        SharedData() = default;

        explicit SharedData(std::vector<std::filesystem::path> meshFiles)
        {
            pushMeshLoadRequests(std::move(meshFiles));
        }


        //
        // OpenSim OUTPUT MODEL STUFF
        //

        bool hasOutputModel() const
        {
            return m_MaybeOutputModel != nullptr;
        }

        std::unique_ptr<OpenSim::Model>& updOutputModel()
        {
            return m_MaybeOutputModel;
        }

        void tryCreateOutputModel()
        {
            try
            {
                m_MaybeOutputModel = CreateOpenSimModelFromModelGraph(getModelGraph(), m_ModelCreationFlags, m_IssuesBuffer);
            }
            catch (std::exception const& ex)
            {
                osc::log::error("error occurred while trying to create an OpenSim model from the mesh editor scene: %s", ex.what());
            }
        }


        //
        // MODEL GRAPH STUFF
        //

        bool openOsimFileAsModelGraph()
        {
            std::optional<std::filesystem::path> const maybeOsimPath = osc::PromptUserForFile("osim");

            if (maybeOsimPath)
            {
                m_ModelGraphSnapshots = CommittableModelGraph{osc::CreateModelFromOsimFile(*maybeOsimPath)};
                m_MaybeModelGraphExportLocation = *maybeOsimPath;
                m_MaybeModelGraphExportedUID = m_ModelGraphSnapshots.getHeadID();
                return true;
            }
            else
            {
                return false;
            }
        }

        bool exportModelGraphTo(std::filesystem::path const& exportPath)
        {
            std::vector<std::string> issues;
            std::unique_ptr<OpenSim::Model> m;

            try
            {
                m = CreateOpenSimModelFromModelGraph(getModelGraph(), m_ModelCreationFlags, issues);
            }
            catch (std::exception const& ex)
            {
                osc::log::error("error occurred while trying to create an OpenSim model from the mesh editor scene: %s", ex.what());
            }

            if (m)
            {
                m->print(exportPath.string());
                m_MaybeModelGraphExportLocation = exportPath;
                m_MaybeModelGraphExportedUID = m_ModelGraphSnapshots.getHeadID();
                return true;
            }
            else
            {
                for (std::string const& issue : issues)
                {
                    osc::log::error("%s", issue.c_str());
                }
                return false;
            }
        }

        bool exportAsModelGraphAsOsimFile()
        {
            std::optional<std::filesystem::path> const maybeExportPath =
                osc::PromptUserForFileSaveLocationAndAddExtensionIfNecessary("osim");

            if (!maybeExportPath)
            {
                return false;  // user probably cancelled out
            }

            return exportModelGraphTo(*maybeExportPath);
        }

        bool exportModelGraphAsOsimFile()
        {
            if (m_MaybeModelGraphExportLocation.empty())
            {
                return exportAsModelGraphAsOsimFile();
            }

            return exportModelGraphTo(m_MaybeModelGraphExportLocation);
        }

        bool isModelGraphUpToDateWithDisk() const
        {
            return m_MaybeModelGraphExportedUID == m_ModelGraphSnapshots.getHeadID();
        }

        bool isCloseRequested() const
        {
            return m_CloseRequested;
        }

        void requestClose()
        {
            m_CloseRequested = true;
        }

        void resetRequestClose()
        {
            m_CloseRequested = false;
        }

        bool isNewMeshImpoterTabRequested() const
        {
            return m_NewTabRequested;
        }

        void requestNewMeshImporterTab()
        {
            m_NewTabRequested = true;
        }

        void resetRequestNewMeshImporter()
        {
            m_NewTabRequested = false;
        }

        std::string getDocumentName() const
        {
            if (m_MaybeModelGraphExportLocation.empty())
            {
                return "untitled.osim";
            }
            else
            {
                return m_MaybeModelGraphExportLocation.filename().string();
            }
        }

        std::string getRecommendedTitle() const
        {
            std::stringstream ss;
            ss << ICON_FA_CUBE << ' ' << getDocumentName();
            return std::move(ss).str();
        }

        ModelGraph const& getModelGraph() const
        {
            return m_ModelGraphSnapshots.getScratch();
        }

        ModelGraph& updModelGraph()
        {
            return m_ModelGraphSnapshots.updScratch();
        }

        CommittableModelGraph& updCommittableModelGraph()
        {
            return m_ModelGraphSnapshots;
        }

        void commitCurrentModelGraph(std::string_view commitMsg)
        {
            m_ModelGraphSnapshots.commitScratch(commitMsg);
        }

        bool canUndoCurrentModelGraph() const
        {
            return m_ModelGraphSnapshots.canUndo();
        }

        void undoCurrentModelGraph()
        {
            m_ModelGraphSnapshots.undo();
        }

        bool canRedoCurrentModelGraph() const
        {
            return m_ModelGraphSnapshots.canRedo();
        }

        void redoCurrentModelGraph()
        {
            m_ModelGraphSnapshots.redo();
        }

        std::unordered_set<UID> const& getCurrentSelection() const
        {
            return getModelGraph().getSelected();
        }

        void selectAll()
        {
            updModelGraph().selectAll();
        }

        void deSelectAll()
        {
            updModelGraph().deSelectAll();
        }

        bool hasSelection() const
        {
            return HasSelection(getModelGraph());
        }

        bool isSelected(UID id) const
        {
            return getModelGraph().isSelected(id);
        }


        //
        // MESH LOADING STUFF
        //

        void pushMeshLoadRequests(UID attachmentPoint, std::vector<std::filesystem::path> paths)
        {
            m_MeshLoader.send(MeshLoadRequest{attachmentPoint, std::move(paths)});
        }

        void pushMeshLoadRequests(std::vector<std::filesystem::path> paths)
        {
            pushMeshLoadRequests(ModelGraphIDs::Ground(), std::move(paths));
        }

        void pushMeshLoadRequest(UID attachmentPoint, std::filesystem::path const& path)
        {
            pushMeshLoadRequests(attachmentPoint, std::vector<std::filesystem::path>{path});
        }

        void pushMeshLoadRequest(std::filesystem::path const& meshFilePath)
        {
            pushMeshLoadRequest(ModelGraphIDs::Ground(), meshFilePath);
        }

        // called when the mesh loader responds with a fully-loaded mesh
        void popMeshLoaderHandleOKResponse(MeshLoadOKResponse& ok)
        {
            if (ok.meshes.empty())
            {
                return;
            }

            // add each loaded mesh into the model graph
            ModelGraph& mg = updModelGraph();
            mg.deSelectAll();

            for (LoadedMesh const& lm : ok.meshes)
            {
                SceneEl* el = mg.tryUpdElByID(ok.preferredAttachmentPoint);

                if (el)
                {
                    auto& mesh = mg.emplaceEl<MeshEl>(UID{}, ok.preferredAttachmentPoint, lm.meshData, lm.path);
                    mesh.setXform(el->getXForm(mg));
                    mg.select(mesh);
                    mg.select(*el);
                }
            }

            // commit
            {
                std::stringstream commitMsgSS;
                if (ok.meshes.empty())
                {
                    commitMsgSS << "loaded 0 meshes";
                }
                else if (ok.meshes.size() == 1)
                {
                    commitMsgSS << "loaded " << ok.meshes[0].path.filename();
                }
                else
                {
                    commitMsgSS << "loaded " << ok.meshes.size() << " meshes";
                }

                commitCurrentModelGraph(std::move(commitMsgSS).str());
            }
        }

        // called when the mesh loader responds with a mesh loading error
        void popMeshLoaderHandleErrorResponse(MeshLoadErrorResponse& err)
        {
            osc::log::error("%s: error loading mesh file: %s", err.path.string().c_str(), err.error.c_str());
        }

        void popMeshLoader()
        {
            for (auto maybeResponse = m_MeshLoader.poll(); maybeResponse.has_value(); maybeResponse = m_MeshLoader.poll())
            {
                MeshLoadResponse& meshLoaderResp = *maybeResponse;

                if (std::holds_alternative<MeshLoadOKResponse>(meshLoaderResp))
                {
                    popMeshLoaderHandleOKResponse(std::get<MeshLoadOKResponse>(meshLoaderResp));
                }
                else
                {
                    popMeshLoaderHandleErrorResponse(std::get<MeshLoadErrorResponse>(meshLoaderResp));
                }
            }
        }

        std::vector<std::filesystem::path> promptUserForMeshFiles() const
        {
            return osc::PromptUserForFiles(osc::GetCommaDelimitedListOfSupportedSimTKMeshFormats());
        }

        void promptUserForMeshFilesAndPushThemOntoMeshLoader()
        {
            pushMeshLoadRequests(promptUserForMeshFiles());
        }


        //
        // UI OVERLAY STUFF
        //

        Vec2 worldPosToScreenPos(Vec3 const& worldPos) const
        {
            return getCamera().projectOntoScreenRect(worldPos, get3DSceneRect());
        }

        void drawConnectionLineTriangleAtMidpoint(
            ImU32 color,
            Vec3 parent,
            Vec3 child) const
        {
            constexpr float triangleWidth = 6.0f * c_ConnectionLineWidth;
            constexpr float triangleWidthSquared = triangleWidth*triangleWidth;

            Vec2 const parentScr = worldPosToScreenPos(parent);
            Vec2 const childScr = worldPosToScreenPos(child);
            Vec2 const child2ParentScr = parentScr - childScr;

            if (osc::Dot(child2ParentScr, child2ParentScr) < triangleWidthSquared)
            {
                return;
            }

            Vec3 const midpoint = osc::Midpoint(parent, child);
            Vec2 const midpointScr = worldPosToScreenPos(midpoint);
            Vec2 const directionScr = osc::Normalize(child2ParentScr);
            Vec2 const directionNormalScr = {-directionScr.y, directionScr.x};

            Vec2 const p1 = midpointScr + (triangleWidth/2.0f)*directionNormalScr;
            Vec2 const p2 = midpointScr - (triangleWidth/2.0f)*directionNormalScr;
            Vec2 const p3 = midpointScr + triangleWidth*directionScr;

            ImGui::GetWindowDrawList()->AddTriangleFilled(p1, p2, p3, color);
        }

        void drawConnectionLine(
            ImU32 color,
            Vec3 const& parent,
            Vec3 const& child) const
        {
            // the line
            ImGui::GetWindowDrawList()->AddLine(worldPosToScreenPos(parent), worldPosToScreenPos(child), color, c_ConnectionLineWidth);

            // the triangle
            drawConnectionLineTriangleAtMidpoint(color, parent, child);
        }

        void drawConnectionLines(
            SceneEl const& el,
            ImU32 color,
            std::unordered_set<UID> const& excludedIDs) const
        {
            ModelGraph const& mg = getModelGraph();
            for (int i = 0, len = el.getNumCrossReferences(); i < len; ++i)
            {
                UID refID = el.getCrossReferenceConnecteeID(i);

                if (Contains(excludedIDs, refID))
                {
                    continue;
                }

                SceneEl const* other = mg.tryGetElByID(refID);

                if (!other)
                {
                    continue;
                }

                Vec3 child = el.getPos(mg);
                Vec3 parent = other->getPos(mg);

                if (el.getCrossReferenceDirection(i) == CrossrefDirection::ToChild)
                {
                    std::swap(parent, child);
                }

                drawConnectionLine(color, parent, child);
            }
        }

        void drawConnectionLines(SceneEl const& el, ImU32 color) const
        {
            drawConnectionLines(el, color, std::unordered_set<UID>{});
        }

        void drawConnectionLineToGround(SceneEl const& el, ImU32 color) const
        {
            if (el.getID() == ModelGraphIDs::Ground())
            {
                return;
            }

            drawConnectionLine(color, Vec3{}, el.getPos(getModelGraph()));
        }

        bool shouldShowConnectionLines(SceneEl const& el) const
        {
            return std::visit(Overload
            {
                []    (GroundEl const&)  { return false; },
                [this](MeshEl const&)    { return this->isShowingMeshConnectionLines(); },
                [this](BodyEl const&)    { return this->isShowingBodyConnectionLines(); },
                [this](JointEl const&)   { return this->isShowingJointConnectionLines(); },
                [this](StationEl const&) { return this->isShowingMeshConnectionLines(); },
                []    (EdgeEl const&)    { return false; },
            }, el.toVariant());
        }

        void drawConnectionLines(
            Color const& color,
            std::unordered_set<UID> const& excludedIDs) const
        {
            ModelGraph const& mg = getModelGraph();
            ImU32 colorU32 = ImGui::ColorConvertFloat4ToU32(Vec4{color});

            for (SceneEl const& el : mg.iter())
            {
                UID id = el.getID();

                if (Contains(excludedIDs, id))
                {
                    continue;
                }

                if (!shouldShowConnectionLines(el))
                {
                    continue;
                }

                if (el.getNumCrossReferences() > 0)
                {
                    drawConnectionLines(el, colorU32, excludedIDs);
                }
                else if (!IsAChildAttachmentInAnyJoint(mg, el))
                {
                    drawConnectionLineToGround(el, colorU32);
                }
            }
        }

        void drawConnectionLines(Color const& color) const
        {
            drawConnectionLines(color, {});
        }

        void drawConnectionLines(Hover const& currentHover) const
        {
            ModelGraph const& mg = getModelGraph();
            ImU32 color = ImGui::ColorConvertFloat4ToU32(Vec4{m_Colors.connectionLines});

            for (SceneEl const& el : mg.iter())
            {
                UID id = el.getID();

                if (id != currentHover.ID && !el.isCrossReferencing(currentHover.ID))
                {
                    continue;
                }

                if (!shouldShowConnectionLines(el))
                {
                    continue;
                }

                if (el.getNumCrossReferences() > 0)
                {
                    drawConnectionLines(el, color);
                }
                else if (!IsAChildAttachmentInAnyJoint(mg, el))
                {
                    drawConnectionLineToGround(el, color);
                }
            }
            //drawConnectionLines(m_Colors.connectionLines);
        }


        //
        // RENDERING STUFF
        //

        void setContentRegionAvailAsSceneRect()
        {
            set3DSceneRect(osc::ContentRegionAvailScreenRect());
        }

        void drawScene(std::span<DrawableThing> drawables)
        {
            // setup rendering params
            SceneRendererParams p;
            p.dimensions = osc::Dimensions(get3DSceneRect());
            p.antiAliasingLevel = App::get().getCurrentAntiAliasingLevel();
            p.drawRims = true;
            p.drawFloor = false;
            p.nearClippingPlane = m_3DSceneCamera.znear;
            p.farClippingPlane = m_3DSceneCamera.zfar;
            p.viewMatrix = m_3DSceneCamera.getViewMtx();
            p.projectionMatrix = m_3DSceneCamera.getProjMtx(osc::AspectRatio(p.dimensions));
            p.viewPos = m_3DSceneCamera.getPos();
            p.lightDirection = osc::RecommendedLightDirection(m_3DSceneCamera);
            p.lightColor = Color::white();
            p.ambientStrength *= 1.5f;
            p.backgroundColor = getColorSceneBackground();

            std::vector<SceneDecoration> decs;
            decs.reserve(drawables.size());
            for (DrawableThing const& dt : drawables)
            {
                decs.emplace_back(
                    dt.mesh,
                    dt.transform,
                    dt.color,
                    std::string{},
                    dt.flags,
                    dt.maybeMaterial,
                    dt.maybePropertyBlock
                );
            }

            // render
            m_SceneRenderer.render(decs, p);

            // send texture to ImGui
            osc::DrawTextureAsImGuiImage(m_SceneRenderer.updRenderTexture(), m_SceneRenderer.getDimensions());

            // handle hittesting, etc.
            setIsRenderHovered(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup));
        }

        bool isRenderHovered() const
        {
            return m_IsRenderHovered;
        }

        void setIsRenderHovered(bool newIsHovered)
        {
            m_IsRenderHovered = newIsHovered;
        }

        Rect const& get3DSceneRect() const
        {
            return m_3DSceneRect;
        }

        void set3DSceneRect(Rect const& newRect)
        {
            m_3DSceneRect = newRect;
        }

        Vec2 get3DSceneDims() const
        {
            return Dimensions(m_3DSceneRect);
        }

        PolarPerspectiveCamera const& getCamera() const
        {
            return m_3DSceneCamera;
        }

        PolarPerspectiveCamera& updCamera()
        {
            return m_3DSceneCamera;
        }

        void focusCameraOn(Vec3 const& focusPoint)
        {
            m_3DSceneCamera.focusPoint = -focusPoint;
        }

        RenderTexture& updSceneTex()
        {
            return m_SceneRenderer.updRenderTexture();
        }

        std::span<Color const> getColors() const
        {
            static_assert(offsetof(Colors, ground) == 0);
            static_assert(sizeof(Colors) % sizeof(Color) == 0);
            return {&m_Colors.ground, sizeof(m_Colors)/sizeof(Color)};
        }

        std::span<Color> updColors()
        {
            static_assert(offsetof(Colors, ground) == 0);
            static_assert(sizeof(Colors) % sizeof(Color) == 0);
            return {&m_Colors.ground, sizeof(m_Colors)/sizeof(Color)};
        }

        void setColor(size_t i, Color const& newColorValue)
        {
            updColors()[i] = newColorValue;
        }

        std::span<char const* const> getColorLabels() const
        {
            return c_ColorNames;
        }

        Color const& getColorSceneBackground() const
        {
            return m_Colors.sceneBackground;
        }

        Color const& getColorMesh() const
        {
            return m_Colors.meshes;
        }

        void setColorMesh(Color const& newColor)
        {
            m_Colors.meshes = newColor;
        }

        Color const& getColorGround() const
        {
            return m_Colors.ground;
        }

        Color const& getColorStation() const
        {
            return m_Colors.stations;
        }

        Color const& getColorEdge() const
        {
            return m_Colors.edges;
        }

        Color const& getColorConnectionLine() const
        {
            return m_Colors.connectionLines;
        }

        void setColorConnectionLine(Color const& newColor)
        {
            m_Colors.connectionLines = newColor;
        }

        std::span<bool const> getVisibilityFlags() const
        {
            static_assert(offsetof(VisibilityFlags, ground) == 0);
            static_assert(sizeof(VisibilityFlags) % sizeof(bool) == 0);
            return {&m_VisibilityFlags.ground, sizeof(m_VisibilityFlags)/sizeof(bool)};
        }

        std::span<bool> updVisibilityFlags()
        {
            static_assert(offsetof(VisibilityFlags, ground) == 0);
            static_assert(sizeof(VisibilityFlags) % sizeof(bool) == 0);
            return {&m_VisibilityFlags.ground, sizeof(m_VisibilityFlags)/sizeof(bool)};
        }

        void setVisibilityFlag(size_t i, bool newVisibilityValue)
        {
            updVisibilityFlags()[i] = newVisibilityValue;
        }

        std::span<char const* const> getVisibilityFlagLabels() const
        {
            return c_VisibilityFlagNames;
        }

        bool isShowingMeshes() const
        {
            return m_VisibilityFlags.meshes;
        }

        void setIsShowingMeshes(bool newIsShowing)
        {
            m_VisibilityFlags.meshes = newIsShowing;
        }

        bool isShowingBodies() const
        {
            return m_VisibilityFlags.bodies;
        }

        void setIsShowingBodies(bool newIsShowing)
        {
            m_VisibilityFlags.bodies = newIsShowing;
        }

        bool isShowingJointCenters() const
        {
            return m_VisibilityFlags.joints;
        }

        void setIsShowingJointCenters(bool newIsShowing)
        {
            m_VisibilityFlags.joints = newIsShowing;
        }

        bool isShowingGround() const
        {
            return m_VisibilityFlags.ground;
        }

        void setIsShowingGround(bool newIsShowing)
        {
            m_VisibilityFlags.ground = newIsShowing;
        }

        bool isShowingFloor() const
        {
            return m_VisibilityFlags.floor;
        }

        void setIsShowingFloor(bool newIsShowing)
        {
            m_VisibilityFlags.floor = newIsShowing;
        }

        bool isShowingStations() const
        {
            return m_VisibilityFlags.stations;
        }

        void setIsShowingStations(bool v)
        {
            m_VisibilityFlags.stations = v;
        }

        bool isShowingEdges() const
        {
            return m_VisibilityFlags.edges;
        }

        bool isShowingJointConnectionLines() const
        {
            return m_VisibilityFlags.jointConnectionLines;
        }

        void setIsShowingJointConnectionLines(bool newIsShowing)
        {
            m_VisibilityFlags.jointConnectionLines = newIsShowing;
        }

        bool isShowingMeshConnectionLines() const
        {
            return m_VisibilityFlags.meshConnectionLines;
        }

        void setIsShowingMeshConnectionLines(bool newIsShowing)
        {
            m_VisibilityFlags.meshConnectionLines = newIsShowing;
        }

        bool isShowingBodyConnectionLines() const
        {
            return m_VisibilityFlags.bodyToGroundConnectionLines;
        }

        void setIsShowingBodyConnectionLines(bool newIsShowing)
        {
            m_VisibilityFlags.bodyToGroundConnectionLines = newIsShowing;
        }

        bool isShowingStationConnectionLines() const
        {
            return m_VisibilityFlags.stationConnectionLines;
        }

        void setIsShowingStationConnectionLines(bool newIsShowing)
        {
            m_VisibilityFlags.stationConnectionLines = newIsShowing;
        }

        Transform getFloorTransform() const
        {
            Transform t;
            t.rotation = osc::AngleAxis(std::numbers::pi_v<float>/2.0f, Vec3{-1.0f, 0.0f, 0.0f});
            t.scale = {m_SceneScaleFactor * 100.0f, m_SceneScaleFactor * 100.0f, 1.0f};
            return t;
        }

        DrawableThing generateFloorDrawable() const
        {
            Transform t = getFloorTransform();
            t.scale *= 0.5f;

            Material material
            {
                App::singleton<ShaderCache>()->load(
                    App::resource("shaders/SolidColor.vert"),
                    App::resource("shaders/SolidColor.frag")
                )
            };
            material.setColor("uColor", m_Colors.gridLines);
            material.setTransparent(true);

            DrawableThing dt;
            dt.id = ModelGraphIDs::Empty();
            dt.groupId = ModelGraphIDs::Empty();
            dt.mesh = App::singleton<SceneCache>()->get100x100GridMesh();
            dt.transform = t;
            dt.color = m_Colors.gridLines;
            dt.flags = SceneDecorationFlags::None;
            dt.maybeMaterial = std::move(material);
            return dt;
        }

        float getSphereRadius() const
        {
            return 0.02f * m_SceneScaleFactor;
        }

        Sphere sphereAtTranslation(Vec3 const& translation) const
        {
            return Sphere{translation, getSphereRadius()};
        }

        void appendAsFrame(
            UID logicalID,
            UID groupID,
            Transform const& xform,
            std::vector<DrawableThing>& appendOut,
            float alpha = 1.0f,
            SceneDecorationFlags flags = SceneDecorationFlags::None,
            Vec3 legLen = {1.0f, 1.0f, 1.0f},
            Color coreColor = Color::white()) const
        {
            float const coreRadius = getSphereRadius();
            float const legThickness = 0.5f * coreRadius;

            // this is how much the cylinder has to be "pulled in" to the core to hide the edges
            float const cylinderPullback = coreRadius * std::sin((std::numbers::pi_v<float> * legThickness) / coreRadius);

            // emit origin sphere
            {
                Transform t;
                t.scale *= coreRadius;
                t.rotation = xform.rotation;
                t.position = xform.position;

                DrawableThing& sphere = appendOut.emplace_back();
                sphere.id = logicalID;
                sphere.groupId = groupID;
                sphere.mesh = m_SphereMesh;
                sphere.transform = t;
                sphere.color = Color{coreColor.r, coreColor.g, coreColor.b, coreColor.a * alpha};
                sphere.flags = flags;
            }

            // emit "legs"
            for (int i = 0; i < 3; ++i)
            {
                // cylinder meshes are -1.0f to 1.0f in Y, so create a transform that maps the
                // mesh onto the legs, which are:
                //
                // - 4.0f * leglen[leg] * radius long
                // - 0.5f * radius thick

                Vec3 const meshDirection = {0.0f, 1.0f, 0.0f};
                Vec3 cylinderDirection = {};
                cylinderDirection[i] = 1.0f;

                float const actualLegLen = 4.0f * legLen[i] * coreRadius;

                Transform t;
                t.scale.x = legThickness;
                t.scale.y = 0.5f * actualLegLen;  // cylinder is 2 units high
                t.scale.z = legThickness;
                t.rotation = osc::Normalize(xform.rotation * osc::Rotation(meshDirection, cylinderDirection));
                t.position = xform.position + (t.rotation * (((getSphereRadius() + (0.5f * actualLegLen)) - cylinderPullback) * meshDirection));

                Color color = {0.0f, 0.0f, 0.0f, alpha};
                color[i] = 1.0f;

                DrawableThing& se = appendOut.emplace_back();
                se.id = logicalID;
                se.groupId = groupID;
                se.mesh = m_CylinderMesh;
                se.transform = t;
                se.color = color;
                se.flags = flags;
            }
        }

        void appendAsCubeThing(
            UID logicalID,
            UID groupID,
            Transform const& xform,
            std::vector<DrawableThing>& appendOut) const
        {
            float const halfWidth = 1.5f * getSphereRadius();

            // core
            {
                Transform scaled{xform};
                scaled.scale *= halfWidth;

                DrawableThing& originCube = appendOut.emplace_back();
                originCube.id = logicalID;
                originCube.groupId = groupID;
                originCube.mesh = App::singleton<SceneCache>()->getBrickMesh();
                originCube.transform = scaled;
                originCube.color = Color::white();
                originCube.flags = SceneDecorationFlags::None;
            }

            // legs
            for (int i = 0; i < 3; ++i)
            {
                // cone mesh has a source height of 2, stretches from -1 to +1 in Y
                float const coneHeight = 0.75f * halfWidth;

                Vec3 const meshDirection = {0.0f, 1.0f, 0.0f};
                Vec3 coneDirection = {};
                coneDirection[i] = 1.0f;

                Transform t;
                t.scale.x = 0.5f * halfWidth;
                t.scale.y = 0.5f * coneHeight;
                t.scale.z = 0.5f * halfWidth;
                t.rotation = xform.rotation * osc::Rotation(meshDirection, coneDirection);
                t.position = xform.position + (t.rotation * ((halfWidth + (0.5f * coneHeight)) * meshDirection));

                Color color = {0.0f, 0.0f, 0.0f, 1.0f};
                color[i] = 1.0f;

                DrawableThing& legCube = appendOut.emplace_back();
                legCube.id = logicalID;
                legCube.groupId = groupID;
                legCube.mesh = App::singleton<SceneCache>()->getConeMesh();
                legCube.transform = t;
                legCube.color = color;
                legCube.flags = SceneDecorationFlags::None;
            }
        }


        //
        // HOVERTEST/INTERACTIVITY
        //

        std::span<bool const> getIneractivityFlags() const
        {
            static_assert(offsetof(InteractivityFlags, ground) == 0);
            static_assert(sizeof(InteractivityFlags) % sizeof(bool) == 0);
            return {&m_InteractivityFlags.ground, sizeof(m_InteractivityFlags)/sizeof(bool)};
        }

        std::span<bool> updInteractivityFlags()
        {
            static_assert(offsetof(InteractivityFlags, ground) == 0);
            static_assert(sizeof(InteractivityFlags) % sizeof(bool) == 0);
            return {&m_InteractivityFlags.ground, sizeof(m_InteractivityFlags)/sizeof(bool)};
        }

        void setInteractivityFlag(size_t i, bool newInteractivityValue)
        {
            updInteractivityFlags()[i] = newInteractivityValue;
        }

        std::span<char const* const> getInteractivityFlagLabels() const
        {
            return c_InteractivityFlagNames;
        }

        bool isMeshesInteractable() const
        {
            return m_InteractivityFlags.meshes;
        }

        void setIsMeshesInteractable(bool newIsInteractable)
        {
            m_InteractivityFlags.meshes = newIsInteractable;
        }

        bool isBodiesInteractable() const
        {
            return m_InteractivityFlags.bodies;
        }

        void setIsBodiesInteractable(bool newIsInteractable)
        {
            m_InteractivityFlags.bodies = newIsInteractable;
        }

        bool isJointCentersInteractable() const
        {
            return m_InteractivityFlags.joints;
        }

        void setIsJointCentersInteractable(bool newIsInteractable)
        {
            m_InteractivityFlags.joints = newIsInteractable;
        }

        bool isGroundInteractable() const
        {
            return m_InteractivityFlags.ground;
        }

        void setIsGroundInteractable(bool newIsInteractable)
        {
            m_InteractivityFlags.ground = newIsInteractable;
        }

        bool isStationsInteractable() const
        {
            return m_InteractivityFlags.stations;
        }

        void setIsStationsInteractable(bool v)
        {
            m_InteractivityFlags.stations = v;
        }

        float getSceneScaleFactor() const
        {
            return m_SceneScaleFactor;
        }

        void setSceneScaleFactor(float newScaleFactor)
        {
            m_SceneScaleFactor = newScaleFactor;
        }

        Hover doHovertest(std::vector<DrawableThing> const& drawables) const
        {
            auto cache = osc::App::singleton<SceneCache>();

            Rect const sceneRect = get3DSceneRect();
            Vec2 const mousePos = ImGui::GetMousePos();

            if (!IsPointInRect(sceneRect, mousePos))
            {
                // mouse isn't over the scene render
                return Hover{};
            }

            Vec2 const sceneDims = Dimensions(sceneRect);
            Vec2 const relMousePos = mousePos - sceneRect.p1;

            Line const ray = getCamera().unprojectTopLeftPosToWorldRay(relMousePos, sceneDims);
            bool const hittestMeshes = isMeshesInteractable();
            bool const hittestBodies = isBodiesInteractable();
            bool const hittestJointCenters = isJointCentersInteractable();
            bool const hittestGround = isGroundInteractable();
            bool const hittestStations = isStationsInteractable();

            UID closestID = ModelGraphIDs::Empty();
            float closestDist = std::numeric_limits<float>::max();
            for (DrawableThing const& drawable : drawables)
            {
                if (drawable.id == ModelGraphIDs::Empty())
                {
                    continue;  // no hittest data
                }

                if (drawable.groupId == ModelGraphIDs::BodyGroup() && !hittestBodies)
                {
                    continue;
                }

                if (drawable.groupId == ModelGraphIDs::MeshGroup() && !hittestMeshes)
                {
                    continue;
                }

                if (drawable.groupId == ModelGraphIDs::JointGroup() && !hittestJointCenters)
                {
                    continue;
                }

                if (drawable.groupId == ModelGraphIDs::GroundGroup() && !hittestGround)
                {
                    continue;
                }

                if (drawable.groupId == ModelGraphIDs::StationGroup() && !hittestStations)
                {
                    continue;
                }

                std::optional<RayCollision> const rc = osc::GetClosestWorldspaceRayCollision(
                    drawable.mesh,
                    cache->getBVH(drawable.mesh),
                    drawable.transform,
                    ray
                );

                if (rc && rc->distance < closestDist)
                {
                    closestID = drawable.id;
                    closestDist = rc->distance;
                }
            }

            Vec3 const hitPos = closestID != ModelGraphIDs::Empty() ? ray.origin + closestDist*ray.direction : Vec3{};

            return Hover{closestID, hitPos};
        }

        //
        // MODEL CREATION FLAGS
        //

        ModelCreationFlags getModelCreationFlags() const
        {
            return m_ModelCreationFlags;
        }

        void setModelCreationFlags(ModelCreationFlags newFlags)
        {
            m_ModelCreationFlags = newFlags;
        }

        //
        // SCENE ELEMENT STUFF (specific methods for specific scene element types)
        //

        void unassignMesh(MeshEl const& me)
        {
            updModelGraph().updElByID<MeshEl>(me.getID()).getParentID() = ModelGraphIDs::Ground();

            std::stringstream ss;
            ss << "unassigned '" << me.getLabel() << "' back to ground";
            commitCurrentModelGraph(std::move(ss).str());
        }

        DrawableThing generateMeshElDrawable(MeshEl const& meshEl) const
        {
            DrawableThing rv;
            rv.id = meshEl.getID();
            rv.groupId = ModelGraphIDs::MeshGroup();
            rv.mesh = meshEl.getMeshData();
            rv.transform = meshEl.getXForm();
            rv.color = meshEl.getParentID() == ModelGraphIDs::Ground() || meshEl.getParentID() == ModelGraphIDs::Empty() ? RedifyColor(getColorMesh()) : getColorMesh();
            rv.flags = SceneDecorationFlags::None;
            return rv;
        }

        DrawableThing generateBodyElSphere(BodyEl const& bodyEl, Color const& color) const
        {
            DrawableThing rv;
            rv.id = bodyEl.getID();
            rv.groupId = ModelGraphIDs::BodyGroup();
            rv.mesh = m_SphereMesh;
            rv.transform = SphereMeshToSceneSphereTransform(sphereAtTranslation(bodyEl.getXForm().position));
            rv.color = color;
            rv.flags = SceneDecorationFlags::None;
            return rv;
        }

        DrawableThing generateGroundSphere(Color const& color) const
        {
            DrawableThing rv;
            rv.id = ModelGraphIDs::Ground();
            rv.groupId = ModelGraphIDs::GroundGroup();
            rv.mesh = m_SphereMesh;
            rv.transform = SphereMeshToSceneSphereTransform(sphereAtTranslation({0.0f, 0.0f, 0.0f}));
            rv.color = color;
            rv.flags = SceneDecorationFlags::None;
            return rv;
        }

        DrawableThing generateStationSphere(StationEl const& el, Color const& color) const
        {
            DrawableThing rv;
            rv.id = el.getID();
            rv.groupId = ModelGraphIDs::StationGroup();
            rv.mesh = m_SphereMesh;
            rv.transform = SphereMeshToSceneSphereTransform(sphereAtTranslation(el.getPos(getModelGraph())));
            rv.color = color;
            rv.flags = SceneDecorationFlags::None;
            return rv;
        }

        DrawableThing generateEdgeCylinder(EdgeEl const&, Color const&) const
        {
            DrawableThing rv;
            return rv;  // TODO
        }

        void appendBodyElAsCubeThing(BodyEl const& bodyEl, std::vector<DrawableThing>& appendOut) const
        {
            appendAsCubeThing(bodyEl.getID(), ModelGraphIDs::BodyGroup(), bodyEl.getXForm(), appendOut);
        }

        void appendBodyElAsFrame(BodyEl const& bodyEl, std::vector<DrawableThing>& appendOut) const
        {
            appendAsFrame(bodyEl.getID(), ModelGraphIDs::BodyGroup(), bodyEl.getXForm(), appendOut);
        }

        void appendDrawables(
            SceneEl const& e,
            std::vector<DrawableThing>& appendOut) const
        {
            std::visit(Overload
            {
                [this, &appendOut](GroundEl const&)
                {
                    if (!isShowingGround())
                    {
                        return;
                    }

                    appendOut.push_back(generateGroundSphere(getColorGround()));
                },
                [this, &appendOut](MeshEl const& el)
                {
                    if (!isShowingMeshes())
                    {
                        return;
                    }

                    appendOut.push_back(generateMeshElDrawable(el));
                },
                [this, &appendOut](BodyEl const& el)
                {
                    if (!isShowingBodies())
                    {
                        return;
                    }

                    appendBodyElAsCubeThing(el, appendOut);
                },
                [this, &appendOut](JointEl const& el)
                {
                    if (!isShowingJointCenters())
                    {
                        return;
                    }

                    appendAsFrame(
                        el.getID(),
                        ModelGraphIDs::JointGroup(),
                        el.getXForm(),
                        appendOut,
                        1.0f,
                        SceneDecorationFlags::None,
                        GetJointAxisLengths(el)
                    );
                },
                [this, &appendOut](StationEl const& el)
                {
                    if (!isShowingStations())
                    {
                        return;
                    }

                    appendOut.push_back(generateStationSphere(el, getColorStation()));
                },
                [this, &appendOut](EdgeEl const& el)
                {
                    if (!isShowingEdges())
                    {
                        return;
                    }
                    appendOut.push_back(generateEdgeCylinder(el, getColorEdge()));

                }
            }, e.toVariant());
        }

        //
        // WINDOWS
        //
        enum PanelIndex_ {
            PanelIndex_History = 0,
            PanelIndex_Navigator,
            PanelIndex_Log,
            PanelIndex_Performance,
            PanelIndex_COUNT,
        };
        size_t getNumToggleablePanels() const
        {
            return static_cast<size_t>(PanelIndex_COUNT);
        }

        CStringView getNthPanelName(size_t n) const
        {
            return c_OpenedPanelNames[n];
        }

        bool isNthPanelEnabled(size_t n) const
        {
            return m_PanelStates[n];
        }

        void setNthPanelEnabled(size_t n, bool v)
        {
            m_PanelStates[n] = v;
        }

        bool isPanelEnabled(PanelIndex_ idx) const
        {
            return m_PanelStates[idx];
        }

        void setPanelEnabled(PanelIndex_ idx, bool v)
        {
            m_PanelStates[idx] = v;
        }

        osc::LogViewer& updLogViewer()
        {
            return m_Logviewer;
        }

        osc::PerfPanel& updPerfPanel()
        {
            return m_PerfPanel;
        }


        //
        // TOP-LEVEL STUFF
        //

        bool onEvent(SDL_Event const& e)
        {
            // if the user drags + drops a file into the window, assume it's a meshfile
            // and start loading it
            if (e.type == SDL_DROPFILE && e.drop.file != nullptr)
            {
                m_DroppedFiles.emplace_back(e.drop.file);
                return true;
            }

            return false;
        }

        void tick(float)
        {
            // push any user-drag-dropped files as one batch
            if (!m_DroppedFiles.empty())
            {
                std::vector<std::filesystem::path> buf;
                std::swap(buf, m_DroppedFiles);
                pushMeshLoadRequests(std::move(buf));
            }

            // pop any background-loaded meshes
            popMeshLoader();

            m_ModelGraphSnapshots.updScratch().garbageCollect();
        }

    private:
        // in-memory model graph (snapshots) that the user is manipulating
        CommittableModelGraph m_ModelGraphSnapshots;

        // (maybe) the filesystem location where the model graph should be saved
        std::filesystem::path m_MaybeModelGraphExportLocation;

        // (maybe) the UID of the model graph when it was last successfully saved to disk (used for dirty checking)
        UID m_MaybeModelGraphExportedUID = m_ModelGraphSnapshots.getHeadID();

        // a batch of files that the user drag-dropped into the UI in the last frame
        std::vector<std::filesystem::path> m_DroppedFiles;

        // loads meshes in a background thread
        MeshLoader m_MeshLoader;

        // sphere mesh used by various scene elements
        Mesh m_SphereMesh = osc::GenSphere(12, 12);

        // cylinder mesh used by various scene elements
        Mesh m_CylinderMesh = osc::GenUntexturedYToYCylinder(16);

        // main 3D scene camera
        PolarPerspectiveCamera m_3DSceneCamera = CreateDefaultCamera();

        // screenspace rect where the 3D scene is currently being drawn to
        Rect m_3DSceneRect = {};

        // renderer that draws the scene
        SceneRenderer m_SceneRenderer{
            App::config(),
            *App::singleton<SceneCache>(),
            *App::singleton<osc::ShaderCache>()
        };

        // COLORS
        //
        // these are runtime-editable color values for things in the scene
        struct Colors {
            Color ground{196.0f/255.0f, 196.0f/255.0f, 196.0f/255.0f, 1.0f};
            Color meshes{1.0f, 1.0f, 1.0f, 1.0f};
            Color stations{196.0f/255.0f, 0.0f, 0.0f, 1.0f};
            Color edges = Color::purple();
            Color connectionLines{0.6f, 0.6f, 0.6f, 1.0f};
            Color sceneBackground{48.0f/255.0f, 48.0f/255.0f, 48.0f/255.0f, 1.0f};
            Color gridLines{0.7f, 0.7f, 0.7f, 0.15f};
        } m_Colors;
        static constexpr auto c_ColorNames = std::to_array<char const*>(
        {
            "ground",
            "meshes",
            "stations",
            "edges",
            "connection lines",
            "scene background",
            "grid lines",
        });
        static_assert(c_ColorNames.size() == sizeof(decltype(m_Colors))/sizeof(Color));

        // VISIBILITY
        //
        // these are runtime-editable visibility flags for things in the scene
        struct VisibilityFlags {
            bool ground = true;
            bool meshes = true;
            bool bodies = true;
            bool joints = true;
            bool stations = true;
            bool edges = true;
            bool jointConnectionLines = true;
            bool meshConnectionLines = true;
            bool bodyToGroundConnectionLines = true;
            bool stationConnectionLines = true;
            bool floor = true;
        } m_VisibilityFlags;
        static constexpr auto c_VisibilityFlagNames = std::to_array<char const*>(
        {
            "ground",
            "meshes",
            "bodies",
            "joints",
            "stations",
            "edges",
            "joint connection lines",
            "mesh connection lines",
            "body-to-ground connection lines",
            "station connection lines",
            "grid lines",
        });
        static_assert(c_VisibilityFlagNames.size() == sizeof(decltype(m_VisibilityFlags))/sizeof(bool));

        // LOCKING
        //
        // these are runtime-editable flags that dictate what gets hit-tested
        struct InteractivityFlags {
            bool ground = true;
            bool meshes = true;
            bool bodies = true;
            bool joints = true;
            bool stations = true;
        } m_InteractivityFlags;
        static constexpr auto c_InteractivityFlagNames = std::to_array<char const*>(
        {
            "ground",
            "meshes",
            "bodies",
            "joints",
            "stations",
        });
        static_assert(c_InteractivityFlagNames.size() == sizeof(decltype(m_InteractivityFlags))/sizeof(bool));

        // WINDOWS
        //
        // these are runtime-editable flags that dictate which panels are open
        static inline constexpr size_t c_NumPanelStates = 4;
        std::array<bool, c_NumPanelStates> m_PanelStates{false, true, false, false};
        static constexpr auto c_OpenedPanelNames = std::to_array<char const*>(
        {
            "History",
            "Navigator",
            "Log",
            "Performance",
        });
        static_assert(c_OpenedPanelNames.size() == c_NumPanelStates);
        static_assert(PanelIndex_COUNT == c_NumPanelStates);
        osc::LogViewer m_Logviewer;
        osc::PerfPanel m_PerfPanel{"Performance"};

        // scale factor for all non-mesh, non-overlay scene elements (e.g.
        // the floor, bodies)
        //
        // this is necessary because some meshes can be extremely small/large and
        // scene elements need to be scaled accordingly (e.g. without this, a body
        // sphere end up being much larger than a mesh instance). Imagine if the
        // mesh was the leg of a fly
        float m_SceneScaleFactor = 1.0f;

        // buffer containing issues found in the modelgraph
        std::vector<std::string> m_IssuesBuffer;

        // model created by this wizard
        //
        // `nullptr` until the model is successfully created
        std::unique_ptr<OpenSim::Model> m_MaybeOutputModel = nullptr;

        // set to true after drawing the ImGui::Image
        bool m_IsRenderHovered = false;

        // true if the implementation wants the host to close the mesh importer UI
        bool m_CloseRequested = false;

        // true if the implementation wants the host to open a new mesh importer
        bool m_NewTabRequested = false;

        // changes how a model is created
        ModelCreationFlags m_ModelCreationFlags = ModelCreationFlags::None;

        static constexpr float c_ConnectionLineWidth = 1.0f;
    };
}

// UI layering support
//
// the visualizer can push the 3D visualizer into different modes (here, "layers") that
// have different behavior. E.g.:
//
// - normal mode (editing stuff)
// - picking another body in the scene mode
namespace
{
    class Layer;

    // the "parent" thing that is hosting the layer
    class LayerHost {
    protected:
        LayerHost() = default;
        LayerHost(LayerHost const&) = default;
        LayerHost(LayerHost&&) noexcept = default;
        LayerHost& operator=(LayerHost const&) = default;
        LayerHost& operator=(LayerHost&&) noexcept = default;
    public:
        virtual ~LayerHost() noexcept = default;

        void requestPop(Layer& layer)
        {
            implRequestPop(layer);
        }

    private:
        virtual void implRequestPop(Layer&) = 0;
    };

    // a layer that is hosted by the parent
    class Layer {
    protected:
        explicit Layer(LayerHost& parent) : m_Parent{&parent}
        {
        }
        Layer(Layer const&) = default;
        Layer(Layer&&) noexcept = default;
        Layer& operator=(Layer const&) = default;
        Layer& operator=(Layer&&) noexcept = default;
    public:
        virtual ~Layer() noexcept = default;

        bool onEvent(SDL_Event const& e)
        {
            return implOnEvent(e);
        }

        void tick(float dt)
        {
            implTick(dt);
        }

        void onDraw()
        {
            implOnDraw();
        }

    protected:
        void requestPop()
        {
            m_Parent->requestPop(*this);
        }

    private:
        virtual bool implOnEvent(SDL_Event const&) = 0;
        virtual void implTick(float) = 0;
        virtual void implOnDraw() = 0;

        LayerHost* m_Parent;
    };
}

// select 2 mesh points layer
namespace
{
    // runtime options for "Select two mesh points" UI layer
    struct Select2MeshPointsOptions final {

        // a function that is called when the implementation detects two points have
        // been clicked
        //
        // the function should return `true` if the points are accepted
        std::function<bool(Vec3, Vec3)> onTwoPointsChosen = [](Vec3, Vec3)
        {
            return true;
        };

        std::string header = "choose first (left-click) and second (right click) mesh positions (ESC to cancel)";
    };

    // UI layer that lets the user select two points on a mesh with left-click and
    // right-click
    class Select2MeshPointsLayer final : public Layer {
    public:
        Select2MeshPointsLayer(
            LayerHost& parent,
            std::shared_ptr<SharedData> shared,
            Select2MeshPointsOptions options) :

            Layer{parent},
            m_Shared{std::move(shared)},
            m_Options{std::move(options)}
        {
        }

    private:

        bool isBothPointsSelected() const
        {
            return m_MaybeFirstLocation && m_MaybeSecondLocation;
        }

        bool isAnyPointSelected() const
        {
            return m_MaybeFirstLocation || m_MaybeSecondLocation;
        }

        // handle the transition that may occur after the user clicks two points
        void handlePossibleTransitionToNextStep()
        {
            if (!isBothPointsSelected())
            {
                return;  // user hasn't selected two points yet
            }

            bool pointsAccepted = m_Options.onTwoPointsChosen(*m_MaybeFirstLocation, *m_MaybeSecondLocation);

            if (pointsAccepted)
            {
                requestPop();
            }
            else
            {
                // points were rejected, so reset them
                m_MaybeFirstLocation.reset();
                m_MaybeSecondLocation.reset();
            }
        }

        // handle any side-effects of the user interacting with whatever they are
        // hovered over
        void handleHovertestSideEffects()
        {
            if (!m_MaybeCurrentHover)
            {
                return;  // nothing hovered
            }
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                // LEFT CLICK: set first mouse location
                m_MaybeFirstLocation = m_MaybeCurrentHover.Pos;
                handlePossibleTransitionToNextStep();
            }
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                // RIGHT CLICK: set second mouse location
                m_MaybeSecondLocation = m_MaybeCurrentHover.Pos;
                handlePossibleTransitionToNextStep();
            }
        }

        // generate 3D drawable geometry for this particular layer
        std::vector<DrawableThing>& generateDrawables()
        {
            m_DrawablesBuffer.clear();

            ModelGraph const& mg = m_Shared->getModelGraph();

            for (MeshEl const& meshEl : mg.iter<MeshEl>())
            {
                m_DrawablesBuffer.emplace_back(m_Shared->generateMeshElDrawable(meshEl));
            }

            m_DrawablesBuffer.push_back(m_Shared->generateFloorDrawable());

            return m_DrawablesBuffer;
        }

        // draw tooltip that pops up when user is moused over a mesh
        void drawHoverTooltip()
        {
            if (!m_MaybeCurrentHover)
            {
                return;
            }

            ImGui::BeginTooltip();
            ImGui::Text("%s", PosString(m_MaybeCurrentHover.Pos).c_str());
            ImGui::TextDisabled("(left-click to assign as first point, right-click to assign as second point)");
            ImGui::EndTooltip();
        }

        // draw 2D overlay over the render, things like connection lines, dots, etc.
        void drawOverlay()
        {
            if (!isAnyPointSelected())
            {
                return;
            }

            Vec3 clickedWorldPos = m_MaybeFirstLocation ? *m_MaybeFirstLocation : *m_MaybeSecondLocation;
            Vec2 clickedScrPos = m_Shared->worldPosToScreenPos(clickedWorldPos);

            auto color = ImGui::ColorConvertFloat4ToU32({0.0f, 0.0f, 0.0f, 1.0f});

            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddCircleFilled(clickedScrPos, 5.0f, color);

            if (!m_MaybeCurrentHover) {
                return;
            }

            Vec2 hoverScrPos = m_Shared->worldPosToScreenPos(m_MaybeCurrentHover.Pos);

            dl->AddCircleFilled(hoverScrPos, 5.0f, color);
            dl->AddLine(clickedScrPos, hoverScrPos, color, 5.0f);
        }

        // draw 2D "choose something" text at the top of the render
        void drawHeaderText() const
        {
            if (m_Options.header.empty())
            {
                return;
            }

            ImU32 color = ImGui::ColorConvertFloat4ToU32({1.0f, 1.0f, 1.0f, 1.0f});
            Vec2 padding{10.0f, 10.0f};
            Vec2 pos = m_Shared->get3DSceneRect().p1 + padding;
            ImGui::GetWindowDrawList()->AddText(pos, color, m_Options.header.c_str());
        }

        // draw a user-clickable button for cancelling out of this choosing state
        void drawCancelButton()
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10.0f, 10.0f});
            osc::PushStyleColor(ImGuiCol_Button, Color::halfGrey());

            CStringView const text = ICON_FA_ARROW_LEFT " Cancel (ESC)";
            Vec2 const margin = {25.0f, 35.0f};
            Vec2 const buttonTopLeft = m_Shared->get3DSceneRect().p2 - (osc::CalcButtonSize(text) + margin);

            ImGui::SetCursorScreenPos(buttonTopLeft);
            if (ImGui::Button(text.c_str()))
            {
                requestPop();
            }

            osc::PopStyleColor();
            ImGui::PopStyleVar();
        }

        bool implOnEvent(SDL_Event const& e) final
        {
            return m_Shared->onEvent(e);
        }

        void implTick(float dt) final
        {
            m_Shared->tick(dt);

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                // ESC: user cancelled out
                requestPop();
            }

            bool isRenderHovered = m_Shared->isRenderHovered();

            if (isRenderHovered)
            {
                UpdatePolarCameraFromImGuiMouseInputs(m_Shared->updCamera(), m_Shared->get3DSceneDims());
            }
        }

        void implOnDraw() final
        {
            m_Shared->setContentRegionAvailAsSceneRect();
            std::vector<DrawableThing>& drawables = generateDrawables();
            m_MaybeCurrentHover = m_Shared->doHovertest(drawables);
            handleHovertestSideEffects();

            m_Shared->drawScene(drawables);
            drawOverlay();
            drawHoverTooltip();
            drawHeaderText();
            drawCancelButton();
        }

        // data that's shared between other UI states
        std::shared_ptr<SharedData> m_Shared;

        // options for this state
        Select2MeshPointsOptions m_Options;

        // (maybe) user mouse hover
        Hover m_MaybeCurrentHover;

        // (maybe) first mesh location
        std::optional<Vec3> m_MaybeFirstLocation;

        // (maybe) second mesh location
        std::optional<Vec3> m_MaybeSecondLocation;

        // buffer that's filled with drawable geometry during a drawcall
        std::vector<DrawableThing> m_DrawablesBuffer;
    };
}

// choose specific element layer
namespace
{
    // options for when the UI transitions into "choose something" mode
    struct ChooseElLayerOptions final {

        // types of elements the user can choose in this screen
        bool canChooseBodies = true;
        bool canChooseGround = true;
        bool canChooseMeshes = true;
        bool canChooseJoints = true;
        bool canChooseStations = false;
        bool canChooseEdges = false;

        // (maybe) elements the assignment is ultimately assigning
        std::unordered_set<UID> maybeElsAttachingTo;

        // false implies the user is attaching "away from" what they select (used for drawing arrows)
        bool isAttachingTowardEl = true;

        // (maybe) elements that are being replaced by the user's choice
        std::unordered_set<UID> maybeElsBeingReplacedByChoice;

        // the number of elements the user must click before OnUserChoice is called
        int numElementsUserMustChoose = 1;

        // function that returns true if the "caller" is happy with the user's choice
        std::function<bool(std::span<UID>)> onUserChoice = [](std::span<UID>)
        {
            return true;
        };

        // user-facing header text
        std::string header = "choose something";
    };

    // "choose `n` things" UI layer
    //
    // this is what's drawn when the user's being prompted to choose scene elements
    class ChooseElLayer final : public Layer {
    public:
        ChooseElLayer(
            LayerHost& parent,
            std::shared_ptr<SharedData> shared,
            ChooseElLayerOptions options) :

            Layer{parent},
            m_Shared{std::move(shared)},
            m_Options{std::move(options)}
        {
        }

    private:
        // returns true if the user's mouse is hovering over the given scene element
        bool isHovered(SceneEl const& el) const
        {
            return el.getID() == m_MaybeHover.ID;
        }

        // returns true if the user has already selected the given scene element
        bool isSelected(SceneEl const& el) const
        {
            return std::find(m_SelectedEls.begin(), m_SelectedEls.end(), el.getID()) != m_SelectedEls.end();
        }

        // returns true if the user can (de)select the given element
        bool isSelectable(SceneEl const& el) const
        {
            if (Contains(m_Options.maybeElsAttachingTo, el.getID()))
            {
                return false;
            }

            return std::visit(Overload
            {
                [this](GroundEl const&)  { return m_Options.canChooseGround; },
                [this](MeshEl const&)    { return m_Options.canChooseMeshes; },
                [this](BodyEl const&)    { return m_Options.canChooseBodies; },
                [this](JointEl const&)   { return m_Options.canChooseJoints; },
                [this](StationEl const&) { return m_Options.canChooseStations; },
                [this](EdgeEl const&)    { return m_Options.canChooseEdges; },
            }, el.toVariant());
        }

        void select(SceneEl const& el)
        {
            if (!isSelectable(el))
            {
                return;
            }

            if (isSelected(el))
            {
                return;
            }

            m_SelectedEls.push_back(el.getID());
        }

        void deSelect(SceneEl const& el)
        {
            if (!isSelectable(el))
            {
                return;
            }

            std::erase_if(m_SelectedEls, [elID = el.getID()](UID id) { return id == elID; } );
        }

        void tryToggleSelectionStateOf(SceneEl const& el)
        {
            isSelected(el) ? deSelect(el) : select(el);
        }

        void tryToggleSelectionStateOf(UID id)
        {
            SceneEl const* el = m_Shared->getModelGraph().tryGetElByID(id);

            if (el)
            {
                tryToggleSelectionStateOf(*el);
            }
        }

        SceneDecorationFlags computeFlags(SceneEl const& el) const
        {
            if (isSelected(el))
            {
                return SceneDecorationFlags::IsSelected;
            }
            else if (isHovered(el))
            {
                return SceneDecorationFlags::IsHovered;
            }
            else
            {
                return SceneDecorationFlags::None;
            }
        }

        // returns a list of 3D drawable scene objects for this layer
        std::vector<DrawableThing>& generateDrawables()
        {
            m_DrawablesBuffer.clear();

            ModelGraph const& mg = m_Shared->getModelGraph();

            float fadedAlpha = 0.2f;
            float animScale = osc::EaseOutElastic(m_AnimationFraction);

            for (SceneEl const& el : mg.iter())
            {
                size_t start = m_DrawablesBuffer.size();
                m_Shared->appendDrawables(el, m_DrawablesBuffer);
                size_t end = m_DrawablesBuffer.size();

                bool isSelectableEl = isSelectable(el);
                SceneDecorationFlags flags = computeFlags(el);

                for (size_t i = start; i < end; ++i)
                {
                    DrawableThing& d = m_DrawablesBuffer[i];
                    d.flags = flags;

                    if (!isSelectableEl)
                    {
                        d.color.a = fadedAlpha;
                        d.id = ModelGraphIDs::Empty();
                        d.groupId = ModelGraphIDs::Empty();
                    }
                    else
                    {
                        d.transform.scale *= animScale;
                    }
                }
            }

            // floor
            m_DrawablesBuffer.push_back(m_Shared->generateFloorDrawable());

            return m_DrawablesBuffer;
        }

        void handlePossibleCompletion()
        {
            if (static_cast<int>(m_SelectedEls.size()) < m_Options.numElementsUserMustChoose)
            {
                return;  // user hasn't selected enough stuff yet
            }

            if (m_Options.onUserChoice(m_SelectedEls))
            {
                requestPop();
            }
            else
            {
                // choice was rejected?
            }
        }

        // handle any side-effects from the user's mouse hover
        void handleHovertestSideEffects()
        {
            if (!m_MaybeHover)
            {
                return;
            }

            drawHoverTooltip();

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                tryToggleSelectionStateOf(m_MaybeHover.ID);
                handlePossibleCompletion();
            }
        }

        // draw 2D tooltip that pops up when user is hovered over something in the scene
        void drawHoverTooltip() const
        {
            if (!m_MaybeHover)
            {
                return;
            }

            SceneEl const* se = m_Shared->getModelGraph().tryGetElByID(m_MaybeHover.ID);

            if (se)
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(se->getLabel().c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("(%s, click to choose)", se->getClass().getName().c_str());
                ImGui::EndTooltip();
            }
        }

        // draw 2D connection overlay lines that show what's connected to what in the graph
        //
        // depends on layer options
        void drawConnectionLines() const
        {
            if (!m_MaybeHover)
            {
                // user isn't hovering anything, so just draw all existing connection
                // lines, but faintly
                m_Shared->drawConnectionLines(FaintifyColor(m_Shared->getColorConnectionLine()));
                return;
            }

            // else: user is hovering *something*

            // draw all other connection lines but exclude the thing being assigned (if any)
            m_Shared->drawConnectionLines(FaintifyColor(m_Shared->getColorConnectionLine()), m_Options.maybeElsBeingReplacedByChoice);

            // draw strong connection line between the things being attached to and the hover
            for (UID elAttachingTo : m_Options.maybeElsAttachingTo)
            {
                Vec3 parentPos = GetPosition(m_Shared->getModelGraph(), elAttachingTo);
                Vec3 childPos = GetPosition(m_Shared->getModelGraph(), m_MaybeHover.ID);

                if (!m_Options.isAttachingTowardEl)
                {
                    std::swap(parentPos, childPos);
                }

                ImU32 strongColorU2 = ImGui::ColorConvertFloat4ToU32(Vec4{m_Shared->getColorConnectionLine()});

                m_Shared->drawConnectionLine(strongColorU2, parentPos, childPos);
            }
        }

        // draw 2D header text in top-left corner of the screen
        void drawHeaderText() const
        {
            if (m_Options.header.empty())
            {
                return;
            }

            ImU32 color = ImGui::ColorConvertFloat4ToU32({1.0f, 1.0f, 1.0f, 1.0f});
            Vec2 padding = Vec2{10.0f, 10.0f};
            Vec2 pos = m_Shared->get3DSceneRect().p1 + padding;
            ImGui::GetWindowDrawList()->AddText(pos, color, m_Options.header.c_str());
        }

        // draw a user-clickable button for cancelling out of this choosing state
        void drawCancelButton()
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10.0f, 10.0f});
            osc::PushStyleColor(ImGuiCol_Button, Color::halfGrey());

            CStringView const text = ICON_FA_ARROW_LEFT " Cancel (ESC)";
            Vec2 const margin = {25.0f, 35.0f};
            Vec2 const buttonTopLeft = m_Shared->get3DSceneRect().p2 - (osc::CalcButtonSize(text) + margin);

            ImGui::SetCursorScreenPos(buttonTopLeft);
            if (ImGui::Button(text.c_str()))
            {
                requestPop();
            }

            osc::PopStyleColor();
            ImGui::PopStyleVar();
        }

        bool implOnEvent(SDL_Event const& e) final
        {
            return m_Shared->onEvent(e);
        }

        void implTick(float dt) final
        {
            m_Shared->tick(dt);

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                // ESC: user cancelled out
                requestPop();
            }

            bool isRenderHovered = m_Shared->isRenderHovered();

            if (isRenderHovered)
            {
                UpdatePolarCameraFromImGuiMouseInputs(m_Shared->updCamera(), m_Shared->get3DSceneDims());
            }

            if (m_AnimationFraction < 1.0f)
            {
                m_AnimationFraction = std::clamp(m_AnimationFraction + 0.5f*dt, 0.0f, 1.0f);
                App::upd().requestRedraw();
            }
        }

        void implOnDraw() final
        {
            m_Shared->setContentRegionAvailAsSceneRect();

            std::vector<DrawableThing>& drawables = generateDrawables();

            m_MaybeHover = m_Shared->doHovertest(drawables);
            handleHovertestSideEffects();

            m_Shared->drawScene(drawables);
            drawConnectionLines();
            drawHeaderText();
            drawCancelButton();
        }

        // data that's shared between other UI states
        std::shared_ptr<SharedData> m_Shared;

        // options for this state
        ChooseElLayerOptions m_Options;

        // (maybe) user mouse hover
        Hover m_MaybeHover;

        // elements selected by user
        std::vector<UID> m_SelectedEls;

        // buffer that's filled with drawable geometry during a drawcall
        std::vector<DrawableThing> m_DrawablesBuffer;

        // fraction that the system is through its animation cycle: ranges from 0.0 to 1.0 inclusive
        float m_AnimationFraction = 0.0f;
    };
}

// popups
namespace
{
    class ImportStationsFromCSVPopup final : public StandardPopup {
    public:
        ImportStationsFromCSVPopup(
            std::string_view popupName_,
            std::shared_ptr<SharedData> shared_) :

            StandardPopup{popupName_},
            m_Shared{std::move(shared_)}
        {
            setModal(true);
        }

    private:
        struct StationDefinedInGround final {
            std::string name;
            Vec3 location;
        };

        struct StationsDefinedInGround final {
            std::vector<StationDefinedInGround> rows;
        };

        struct ImportedCSVData final {
            std::filesystem::path sourceDataPath;
            std::variant<StationsDefinedInGround> parsedData;
        };

        struct CSVImportError final {
            std::filesystem::path userSelectedPath;
            std::string message;
        };

        using CSVImportResult = std::variant<ImportedCSVData, CSVImportError>;

        struct RowParseError final {
            size_t lineNum;
            std::string errorMsg;
        };

        static std::variant<StationDefinedInGround, RowParseError> TryParseColumns(
            size_t lineNum,
            std::span<std::string const> columnsText)
        {
            if (columnsText.size() < 4)
            {
                return RowParseError{lineNum, "too few columns in this row (expecting at least 4)"};
            }

            std::string const& stationName = columnsText[0];

            std::optional<float> const maybeX = osc::FromCharsStripWhitespace(columnsText[1]);
            if (!maybeX)
            {
                return RowParseError{lineNum, "cannot parse X as a number"};
            }

            std::optional<float> const maybeY = osc::FromCharsStripWhitespace(columnsText[2]);
            if (!maybeY)
            {
                return RowParseError{lineNum, "cannot parse Y as a number"};
            }

            std::optional<float> const maybeZ = osc::FromCharsStripWhitespace(columnsText[3]);
            if (!maybeZ)
            {
                return RowParseError{lineNum, "cannot parse Z as a number"};
            }

            Vec3 const locationInGround = {*maybeX, *maybeY, *maybeZ};

            return StationDefinedInGround{stationName, locationInGround};
        }

        static std::string to_string(RowParseError const& e)
        {
            std::stringstream ss;
            ss << "line " << e.lineNum << ": " << e.errorMsg;
            return std::move(ss).str();
        }

        static bool IsWhitespaceRow(std::span<std::string const> cols)
        {
            return cols.size() == 1;
        }

        static CSVImportResult TryReadCSVInput(std::filesystem::path const& path, std::istream& input)
        {
            // input must contain at least one (header) row
            if (!osc::ReadCSVRow(input))
            {
                return CSVImportError{path, "cannot read a header row from the input (is the file empty?)"};
            }

            // then try to read each row as a data row, propagating errors
            // accordingly

            StationsDefinedInGround successfullyParsedStations;
            std::optional<RowParseError> maybeParseError;
            {
                size_t lineNum = 1;
                for (std::vector<std::string> row;
                    !maybeParseError && osc::ReadCSVRowIntoVector(input, row);
                    ++lineNum)
                {
                    if (IsWhitespaceRow(row))
                    {
                        continue;  // skip
                    }

                    // else: try parsing the row as a data row
                    std::visit(Overload
                    {
                        [&successfullyParsedStations](StationDefinedInGround const& success)
                        {
                            successfullyParsedStations.rows.push_back(success);
                        },
                        [&maybeParseError](RowParseError const& fail)
                        {
                            maybeParseError = fail;
                        },
                    }, TryParseColumns(lineNum, row));
                }
            }

            if (maybeParseError)
            {
                return CSVImportError{path, to_string(*maybeParseError)};
            }
            else
            {
                return ImportedCSVData{path, std::move(successfullyParsedStations)};
            }
        }

        static CSVImportResult TryReadCSVFile(std::filesystem::path const& path)
        {
            std::ifstream f{path};
            if (!f)
            {
                return CSVImportError{path, "cannot open the provided file for reading"};
            }
            f.setf(std::ios_base::skipws);

            return TryReadCSVInput(path, f);
        }

        void implDrawContent() final
        {
            drawHelpText();

            ImGui::Dummy({0.0f, 0.25f*ImGui::GetTextLineHeight()});
            if (m_MaybeImportResult.has_value())
            {
                ImGui::Separator();

                std::visit(Overload
                {
                    [this](ImportedCSVData const& data) { drawLoadedFileState(data); },
                    [this](CSVImportError const& error) { drawErrorLoadingFileState(error); }
                }, *m_MaybeImportResult);
            }
            else
            {
                drawSelectInitialFileState();
            }
            ImGui::Dummy({0.0f, 0.5f*ImGui::GetTextLineHeight()});
        }

        void drawHelpText()
        {
            ImGui::TextWrapped("Use this tool to import CSV data containing 3D locations as stations into the mesh importer scene. The CSV file should contain");
            ImGui::Bullet();
            ImGui::TextWrapped("A header row of four columns, ideally labelled 'name', 'x', 'y', and 'z'");
            ImGui::Bullet();
            ImGui::TextWrapped("Data rows containing four columns: name (string), x (number), y (number), and z (number)");

            constexpr CStringView c_ExampleInputText = "name,x,y,z\nstationatground,0,0,0\nstation2,1.53,0.2,1.7\nstation3,3.0,2.0,0.0\n";
            ImGui::TextWrapped("Example Input: ");
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_COPY))
            {
                osc::SetClipboardText(c_ExampleInputText);
            }
            osc::DrawTooltipBodyOnlyIfItemHovered("Copy example input to clipboard");
            ImGui::Indent();
            ImGui::TextWrapped("%s", c_ExampleInputText.c_str());
            ImGui::Unindent();
        }

        void drawSelectInitialFileState()
        {
            if (osc::ButtonCentered(ICON_FA_FILE " Select File"))
            {
                actionTryPromptingUserForCSVFile();
            }

            ImGui::Dummy({0.0f, 0.75f*ImGui::GetTextLineHeight()});

            drawDisabledOkCancelButtons("Cannot continue: nothing has been imported (select a file first)");
        }

        void drawErrorLoadingFileState(CSVImportError const& error)
        {
            ImGui::Text("Error loading %s: %s ", error.userSelectedPath.string().c_str(), error.message.c_str());
            if (ImGui::Button("Try Again (Select File)"))
            {
                actionTryPromptingUserForCSVFile();
            }

            ImGui::Dummy({0.0f, 0.25f*ImGui::GetTextLineHeight()});
            ImGui::Separator();
            ImGui::Dummy({0.0f, 0.5f*ImGui::GetTextLineHeight()});

            drawDisabledOkCancelButtons("Cannot continue: there is an error in the imported data (try again)");
        }

        void drawDisabledOkCancelButtons(CStringView disabledReason)
        {
            ImGui::BeginDisabled();
            ImGui::Button("OK");
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                osc::DrawTooltipBodyOnly(disabledReason);
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                close();
            }
        }

        void drawLoadedFileState(ImportedCSVData const& result)
        {
            std::visit(Overload
            {
                [this, &result](StationsDefinedInGround const& data) { drawLoadedFileStateData(result, data); },
            }, result.parsedData);

            ImGui::Dummy({0.0f, 0.25f*ImGui::GetTextLineHeight()});
            ImGui::Separator();
            ImGui::Dummy({0.0f, 0.5f*ImGui::GetTextLineHeight()});

            if (ImGui::Button("OK"))
            {
                actionAttachResultToModelGraph(result);
                close();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                close();
            }
        }

        void drawLoadedFileStateData(ImportedCSVData const& result, StationsDefinedInGround const& data)
        {
            osc::TextCentered(result.sourceDataPath.string());
            osc::TextCentered(std::string{"("} + std::to_string(data.rows.size()) + " data rows)");

            ImGui::Dummy({0.0f, 0.2f*ImGui::GetTextLineHeight()});
            if (ImGui::BeginTable("##importtable", 4, ImGuiTableFlags_ScrollY, {0.0f, 10.0f*ImGui::GetTextLineHeight()}))
            {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("X");
                ImGui::TableSetupColumn("Y");
                ImGui::TableSetupColumn("Z");
                ImGui::TableHeadersRow();

                int id = 0;
                for (StationDefinedInGround const& row : data.rows)
                {
                    ImGui::PushID(id++);
                    ImGui::TableNextRow();
                    int column = 0;
                    ImGui::TableSetColumnIndex(column++);
                    ImGui::TextUnformatted(row.name.c_str());
                    ImGui::TableSetColumnIndex(column++);
                    ImGui::Text("%f", row.location.x);
                    ImGui::TableSetColumnIndex(column++);
                    ImGui::Text("%f", row.location.y);
                    ImGui::TableSetColumnIndex(column++);
                    ImGui::Text("%f", row.location.z);
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
            ImGui::Dummy({0.0f, 0.2f*ImGui::GetTextLineHeight()});

            if (osc::ButtonCentered(ICON_FA_FILE " Select Different File"))
            {
                actionTryPromptingUserForCSVFile();
            }
        }

        void actionTryPromptingUserForCSVFile()
        {
            if (auto path = osc::PromptUserForFile("csv"))
            {
                m_MaybeImportResult = TryReadCSVFile(*path);
            }
        }

        void actionAttachResultToModelGraph(ImportedCSVData const& result)
        {
            std::visit(Overload
            {
                [this, &result](StationsDefinedInGround const& data) { actionAttachStationsInGroundToModelGraph(result, data); },
            }, result.parsedData);
        }

        void actionAttachStationsInGroundToModelGraph(
            ImportedCSVData const& result,
            StationsDefinedInGround const& data)
        {
            CommittableModelGraph& undoable = m_Shared->updCommittableModelGraph();

            ModelGraph& graph = undoable.updScratch();
            for (StationDefinedInGround const& station : data.rows)
            {
                graph.emplaceEl<StationEl>(
                    UID{},
                    ModelGraphIDs::Ground(),
                    station.location,
                    station.name
                );
            }

            std::stringstream ss;
            ss << "imported " << result.sourceDataPath;
            undoable.commitScratch(std::move(ss).str());
        }

        std::shared_ptr<SharedData> m_Shared;
        std::optional<CSVImportResult> m_MaybeImportResult;
    };
}

// mesh importer tab implementation
class osc::MeshImporterTab::Impl final : public LayerHost {
public:
    explicit Impl(ParentPtr<MainUIStateAPI> const& parent_) :
        m_Parent{parent_},
        m_Shared{std::make_shared<SharedData>()}
    {
    }

    Impl(
        ParentPtr<MainUIStateAPI> const& parent_,
        std::vector<std::filesystem::path> meshPaths_) :

        m_Parent{parent_},
        m_Shared{std::make_shared<SharedData>(std::move(meshPaths_))}
    {
    }

    UID getID() const
    {
        return m_TabID;
    }

    CStringView getName() const
    {
        return m_Name;
    }

    bool isUnsaved() const
    {
        return !m_Shared->isModelGraphUpToDateWithDisk();
    }

    bool trySave()
    {
        if (m_Shared->isModelGraphUpToDateWithDisk())
        {
            // nothing to save
            return true;
        }
        else
        {
            // try to save the changes
            return m_Shared->exportAsModelGraphAsOsimFile();
        }
    }

    void onMount()
    {
        App::upd().makeMainEventLoopWaiting();
        m_PopupManager.onMount();
    }

    void onUnmount()
    {
        App::upd().makeMainEventLoopPolling();
    }

    bool onEvent(SDL_Event const& e)
    {
        if (m_Shared->onEvent(e))
        {
            return true;
        }

        if (m_Maybe3DViewerModal)
        {
            std::shared_ptr<Layer> ptr = m_Maybe3DViewerModal;  // ensure it stays alive - even if it pops itself during the drawcall
            if (ptr->onEvent(e))
            {
                return true;
            }
        }

        return false;
    }

    void onTick()
    {
        auto const dt = static_cast<float>(App::get().getFrameDeltaSinceLastFrame().count());

        m_Shared->tick(dt);

        if (m_Maybe3DViewerModal)
        {
            std::shared_ptr<Layer> ptr = m_Maybe3DViewerModal;  // ensure it stays alive - even if it pops itself during the drawcall
            ptr->tick(dt);
        }

        // if some screen generated an OpenSim::Model, transition to the main editor
        if (m_Shared->hasOutputModel())
        {
            auto ptr = std::make_unique<UndoableModelStatePair>(std::move(m_Shared->updOutputModel()));
            ptr->setFixupScaleFactor(m_Shared->getSceneScaleFactor());
            m_Parent->addAndSelectTab<ModelEditorTab>(m_Parent, std::move(ptr));
        }

        m_Name = m_Shared->getRecommendedTitle();

        if (m_Shared->isCloseRequested())
        {
            m_Parent->closeTab(m_TabID);
            m_Shared->resetRequestClose();
        }

        if (m_Shared->isNewMeshImpoterTabRequested())
        {
            m_Parent->addAndSelectTab<MeshImporterTab>(m_Parent);
            m_Shared->resetRequestNewMeshImporter();
        }
    }

    void drawMainMenu()
    {
        drawMainMenuFileMenu();
        drawMainMenuEditMenu();
        drawMainMenuWindowMenu();
        drawMainMenuAboutMenu();
    }

    void onDraw()
    {
        // enable panel docking
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        // handle keyboards using ImGui's input poller
        if (!m_Maybe3DViewerModal)
        {
            updateFromImGuiKeyboardState();
        }

        if (!m_Maybe3DViewerModal && m_Shared->isRenderHovered() && !ImGuizmo::IsUsing())
        {
            UpdatePolarCameraFromImGuiMouseInputs(m_Shared->updCamera(), m_Shared->get3DSceneDims());
        }

        // draw history panel (if enabled)
        if (m_Shared->isPanelEnabled(SharedData::PanelIndex_History))
        {
            bool v = true;
            if (ImGui::Begin("history", &v))
            {
                drawHistoryPanelContent();
            }
            ImGui::End();

            m_Shared->setPanelEnabled(SharedData::PanelIndex_History, v);
        }

        // draw navigator panel (if enabled)
        if (m_Shared->isPanelEnabled(SharedData::PanelIndex_Navigator))
        {
            bool v = true;
            if (ImGui::Begin("navigator", &v))
            {
                drawNavigatorPanelContent();
            }
            ImGui::End();

            m_Shared->setPanelEnabled(SharedData::PanelIndex_Navigator, v);
        }

        // draw log panel (if enabled)
        if (m_Shared->isPanelEnabled(SharedData::PanelIndex_Log))
        {
            bool v = true;
            if (ImGui::Begin("Log", &v, ImGuiWindowFlags_MenuBar))
            {
                m_Shared->updLogViewer().onDraw();
            }
            ImGui::End();

            m_Shared->setPanelEnabled(SharedData::PanelIndex_Log, v);
        }

        // draw performance panel (if enabled)
        if (m_Shared->isPanelEnabled(SharedData::PanelIndex_Performance))
        {
            osc::PerfPanel& pp = m_Shared->updPerfPanel();

            pp.open();
            pp.onDraw();
            if (!pp.isOpen())
            {
                m_Shared->setPanelEnabled(SharedData::PanelIndex_Performance, false);
            }
        }

        // draw contextual 3D modal (if there is one), else: draw standard 3D viewer
        drawMainViewerPanelOrModal();

        // draw any active popups over the scene
        m_PopupManager.onDraw();
    }

private:

    //
    // ACTIONS
    //

    // pop the current UI layer
    void implRequestPop(Layer&) final
    {
        m_Maybe3DViewerModal.reset();
        App::upd().requestRedraw();
    }

    // try to select *only* what is currently hovered
    void selectJustHover()
    {
        if (!m_MaybeHover)
        {
            return;
        }

        m_Shared->updModelGraph().select(m_MaybeHover.ID);
    }

    // try to select what is currently hovered *and* anything that is "grouped"
    // with the hovered item
    //
    // "grouped" here specifically means other meshes connected to the same body
    void selectAnythingGroupedWithHover()
    {
        if (!m_MaybeHover)
        {
            return;
        }

        SelectAnythingGroupedWith(m_Shared->updModelGraph(), m_MaybeHover.ID);
    }

    // add a body element to whatever's currently hovered at the hover (raycast) position
    void tryAddBodyToHoveredElement()
    {
        if (!m_MaybeHover)
        {
            return;
        }

        AddBody(m_Shared->updCommittableModelGraph(), m_MaybeHover.Pos, {m_MaybeHover.ID});
    }

    void tryCreatingJointFromHoveredElement()
    {
        if (!m_MaybeHover)
        {
            return;  // nothing hovered
        }

        ModelGraph const& mg = m_Shared->getModelGraph();

        SceneEl const* hoveredSceneEl = mg.tryGetElByID(m_MaybeHover.ID);

        if (!hoveredSceneEl)
        {
            return;  // current hover isn't in the current model graph
        }

        UID maybeID = GetStationAttachmentParent(mg, *hoveredSceneEl);

        if (maybeID == ModelGraphIDs::Ground() || maybeID == ModelGraphIDs::Empty())
        {
            return;  // can't attach to it as-if it were a body
        }

        auto const* bodyEl = mg.tryGetElByID<BodyEl>(maybeID);
        if (!bodyEl)
        {
            return;  // suggested attachment parent isn't in the current model graph?
        }

        transitionToChoosingJointParent(*bodyEl);
    }

    // try transitioning the shown UI layer to one where the user is assigning a mesh
    void tryTransitionToAssigningHoverAndSelectionNextFrame()
    {
        ModelGraph const& mg = m_Shared->getModelGraph();

        std::unordered_set<UID> meshes;
        meshes.insert(mg.getSelected().begin(), mg.getSelected().end());
        if (m_MaybeHover)
        {
            meshes.insert(m_MaybeHover.ID);
        }

        std::erase_if(meshes, [&mg](UID meshID) { return !mg.containsEl<MeshEl>(meshID); });

        if (meshes.empty())
        {
            return;  // nothing to assign
        }

        std::unordered_set<UID> attachments;
        for (UID meshID : meshes)
        {
            attachments.insert(mg.getElByID<MeshEl>(meshID).getParentID());
        }

        transitionToAssigningMeshesNextFrame(meshes, attachments);
    }

    void tryAddingStationAtMousePosToHoveredElement()
    {
        if (!m_MaybeHover)
        {
            return;
        }

        AddStationAtLocation(m_Shared->updCommittableModelGraph(), m_MaybeHover.ID, m_MaybeHover.Pos);
    }

    //
    // TRANSITIONS
    //
    // methods for transitioning the main 3D UI to some other state
    //

    // transition the shown UI layer to one where the user is assigning a mesh
    void transitionToAssigningMeshesNextFrame(std::unordered_set<UID> const& meshes, std::unordered_set<UID> const& existingAttachments)
    {
        ChooseElLayerOptions opts;
        opts.canChooseBodies = true;
        opts.canChooseGround = true;
        opts.canChooseJoints = false;
        opts.canChooseMeshes = false;
        opts.maybeElsAttachingTo = meshes;
        opts.isAttachingTowardEl = false;
        opts.maybeElsBeingReplacedByChoice = existingAttachments;
        opts.header = "choose mesh attachment (ESC to cancel)";
        opts.onUserChoice = [shared = m_Shared, meshes](std::span<UID> choices)
        {
            if (choices.empty())
            {
                return false;
            }

            return TryAssignMeshAttachments(shared->updCommittableModelGraph(), meshes, choices.front());
        };

        // request a state transition
        m_Maybe3DViewerModal = std::make_shared<ChooseElLayer>(*this, m_Shared, opts);
    }

    // transition the shown UI layer to one where the user is choosing a joint parent
    void transitionToChoosingJointParent(BodyEl const& child)
    {
        ChooseElLayerOptions opts;
        opts.canChooseBodies = true;
        opts.canChooseGround = true;
        opts.canChooseJoints = false;
        opts.canChooseMeshes = false;
        opts.header = "choose joint parent (ESC to cancel)";
        opts.maybeElsAttachingTo = {child.getID()};
        opts.isAttachingTowardEl = false;  // away from the body
        opts.onUserChoice = [shared = m_Shared, childID = child.getID()](std::span<UID> choices)
        {
            if (choices.empty())
            {
                return false;
            }

            return TryCreateJoint(shared->updCommittableModelGraph(), childID, choices.front());
        };
        m_Maybe3DViewerModal = std::make_shared<ChooseElLayer>(*this, m_Shared, opts);
    }

    // transition the shown UI layer to one where the user is choosing which element in the scene to point
    // an element's axis towards
    void transitionToChoosingWhichElementToPointAxisTowards(SceneEl& el, int axis)
    {
        ChooseElLayerOptions opts;
        opts.canChooseBodies = true;
        opts.canChooseGround = true;
        opts.canChooseJoints = true;
        opts.canChooseMeshes = false;
        opts.canChooseStations = true;
        opts.maybeElsAttachingTo = {el.getID()};
        opts.header = "choose what to point towards (ESC to cancel)";
        opts.onUserChoice = [shared = m_Shared, id = el.getID(), axis](std::span<UID> choices)
        {
            if (choices.empty())
            {
                return false;
            }

            return PointAxisTowards(shared->updCommittableModelGraph(), id, axis, choices.front());
        };
        m_Maybe3DViewerModal = std::make_shared<ChooseElLayer>(*this, m_Shared, opts);
    }

    // transition the shown UI layer to one where the user is choosing two elements that the given axis
    // should be aligned along (i.e. the direction vector from the first element to the second element
    // becomes the direction vector of the given axis)
    void transitionToChoosingTwoElementsToAlignAxisAlong(SceneEl& el, int axis)
    {
        ChooseElLayerOptions opts;
        opts.canChooseBodies = true;
        opts.canChooseGround = true;
        opts.canChooseJoints = true;
        opts.canChooseMeshes = false;
        opts.canChooseStations = true;
        opts.maybeElsAttachingTo = {el.getID()};
        opts.header = "choose two elements to align the axis along (ESC to cancel)";
        opts.numElementsUserMustChoose = 2;
        opts.onUserChoice = [shared = m_Shared, id = el.getID(), axis](std::span<UID> choices)
        {
            if (choices.size() < 2)
            {
                return false;
            }

            return TryOrientElementAxisAlongTwoElements(
                shared->updCommittableModelGraph(),
                id,
                axis,
                choices[0],
                choices[1]
            );
        };
        m_Maybe3DViewerModal = std::make_shared<ChooseElLayer>(*this, m_Shared, opts);
    }

    void transitionToChoosingWhichElementToTranslateTo(SceneEl& el)
    {
        ChooseElLayerOptions opts;
        opts.canChooseBodies = true;
        opts.canChooseGround = true;
        opts.canChooseJoints = true;
        opts.canChooseMeshes = false;
        opts.canChooseStations = true;
        opts.maybeElsAttachingTo = {el.getID()};
        opts.header = "choose what to translate to (ESC to cancel)";
        opts.onUserChoice = [shared = m_Shared, id = el.getID()](std::span<UID> choices)
        {
            if (choices.empty())
            {
                return false;
            }

            return TryTranslateElementToAnotherElement(shared->updCommittableModelGraph(), id, choices.front());
        };
        m_Maybe3DViewerModal = std::make_shared<ChooseElLayer>(*this, m_Shared, opts);
    }

    void transitionToChoosingElementsToTranslateBetween(SceneEl& el)
    {
        ChooseElLayerOptions opts;
        opts.canChooseBodies = true;
        opts.canChooseGround = true;
        opts.canChooseJoints = true;
        opts.canChooseMeshes = false;
        opts.canChooseStations = true;
        opts.maybeElsAttachingTo = {el.getID()};
        opts.header = "choose two elements to translate between (ESC to cancel)";
        opts.numElementsUserMustChoose = 2;
        opts.onUserChoice = [shared = m_Shared, id = el.getID()](std::span<UID> choices)
        {
            if (choices.size() < 2)
            {
                return false;
            }

            return TryTranslateBetweenTwoElements(
                shared->updCommittableModelGraph(),
                id,
                choices[0],
                choices[1]
            );
        };
        m_Maybe3DViewerModal = std::make_shared<ChooseElLayer>(*this, m_Shared, opts);
    }

    void transitionToCopyingSomethingElsesOrientation(SceneEl& el)
    {
        ChooseElLayerOptions opts;
        opts.canChooseBodies = true;
        opts.canChooseGround = true;
        opts.canChooseJoints = true;
        opts.canChooseMeshes = true;
        opts.maybeElsAttachingTo = {el.getID()};
        opts.header = "choose which orientation to copy (ESC to cancel)";
        opts.onUserChoice = [shared = m_Shared, id = el.getID()](std::span<UID> choices)
        {
            if (choices.empty())
            {
                return false;
            }

            return TryCopyOrientation(shared->updCommittableModelGraph(), id, choices.front());
        };
        m_Maybe3DViewerModal = std::make_shared<ChooseElLayer>(*this, m_Shared, opts);
    }

    // transition the shown UI layer to one where the user is choosing two mesh points that
    // the element should be oriented along
    void transitionToOrientingElementAlongTwoMeshPoints(SceneEl& el, int axis)
    {
        Select2MeshPointsOptions opts;
        opts.onTwoPointsChosen = [shared = m_Shared, id = el.getID(), axis](Vec3 a, Vec3 b)
        {
            return TryOrientElementAxisAlongTwoPoints(shared->updCommittableModelGraph(), id, axis, a, b);
        };
        m_Maybe3DViewerModal = std::make_shared<Select2MeshPointsLayer>(*this, m_Shared, opts);
    }

    // transition the shown UI layer to one where the user is choosing two mesh points that
    // the element sould be translated to the midpoint of
    void transitionToTranslatingElementAlongTwoMeshPoints(SceneEl& el)
    {
        Select2MeshPointsOptions opts;
        opts.onTwoPointsChosen = [shared = m_Shared, id = el.getID()](Vec3 a, Vec3 b)
        {
            return TryTranslateElementBetweenTwoPoints(shared->updCommittableModelGraph(), id, a, b);
        };
        m_Maybe3DViewerModal = std::make_shared<Select2MeshPointsLayer>(*this, m_Shared, opts);
    }

    void transitionToTranslatingElementToMeshAverageCenter(SceneEl& el)
    {
        ChooseElLayerOptions opts;
        opts.canChooseBodies = false;
        opts.canChooseGround = false;
        opts.canChooseJoints = false;
        opts.canChooseMeshes = true;
        opts.header = "choose a mesh (ESC to cancel)";
        opts.onUserChoice = [shared = m_Shared, id = el.getID()](std::span<UID> choices)
        {
            if (choices.empty())
            {
                return false;
            }

            return TryTranslateToMeshAverageCenter(shared->updCommittableModelGraph(), id, choices.front());
        };
        m_Maybe3DViewerModal = std::make_shared<ChooseElLayer>(*this, m_Shared, opts);
    }

    void transitionToTranslatingElementToMeshBoundsCenter(SceneEl& el)
    {
        ChooseElLayerOptions opts;
        opts.canChooseBodies = false;
        opts.canChooseGround = false;
        opts.canChooseJoints = false;
        opts.canChooseMeshes = true;
        opts.header = "choose a mesh (ESC to cancel)";
        opts.onUserChoice = [shared = m_Shared, id = el.getID()](std::span<UID> choices)
        {
            if (choices.empty())
            {
                return false;
            }

            return TryTranslateToMeshBoundsCenter(shared->updCommittableModelGraph(), id, choices.front());
        };
        m_Maybe3DViewerModal = std::make_shared<ChooseElLayer>(*this, m_Shared, opts);
    }

    void transitionToTranslatingElementToMeshMassCenter(SceneEl& el)
    {
        ChooseElLayerOptions opts;
        opts.canChooseBodies = false;
        opts.canChooseGround = false;
        opts.canChooseJoints = false;
        opts.canChooseMeshes = true;
        opts.header = "choose a mesh (ESC to cancel)";
        opts.onUserChoice = [shared = m_Shared, id = el.getID()](std::span<UID> choices)
        {
            if (choices.empty())
            {
                return false;
            }

            return TryTranslateToMeshMassCenter(shared->updCommittableModelGraph(), id, choices.front());
        };
        m_Maybe3DViewerModal = std::make_shared<ChooseElLayer>(*this, m_Shared, opts);
    }

    // transition the shown UI layer to one where the user is choosing another element that
    // the element should be translated to the midpoint of
    void transitionToTranslatingElementToAnotherElementsCenter(SceneEl& el)
    {
        ChooseElLayerOptions opts;
        opts.canChooseBodies = true;
        opts.canChooseGround = true;
        opts.canChooseJoints = true;
        opts.canChooseMeshes = true;
        opts.maybeElsAttachingTo = {el.getID()};
        opts.header = "choose where to place it (ESC to cancel)";
        opts.onUserChoice = [shared = m_Shared, id = el.getID()](std::span<UID> choices)
        {
            if (choices.empty())
            {
                return false;
            }

            return TryTranslateElementToAnotherElement(shared->updCommittableModelGraph(), id, choices.front());
        };
        m_Maybe3DViewerModal = std::make_shared<ChooseElLayer>(*this, m_Shared, opts);
    }

    void transitionToReassigningCrossRef(SceneEl& el, int crossrefIdx)
    {
        int nRefs = el.getNumCrossReferences();

        if (crossrefIdx < 0 || crossrefIdx >= nRefs)
        {
            return;  // invalid index?
        }

        SceneEl const* old = m_Shared->getModelGraph().tryGetElByID(el.getCrossReferenceConnecteeID(crossrefIdx));

        if (!old)
        {
            return;  // old el doesn't exist?
        }

        ChooseElLayerOptions opts;
        opts.canChooseBodies = dynamic_cast<BodyEl const*>(old) || dynamic_cast<GroundEl const*>(old);
        opts.canChooseGround = dynamic_cast<BodyEl const*>(old) || dynamic_cast<GroundEl const*>(old);
        opts.canChooseJoints = dynamic_cast<JointEl const*>(old);
        opts.canChooseMeshes = dynamic_cast<MeshEl const*>(old);
        opts.maybeElsAttachingTo = {el.getID()};
        opts.header = "choose what to attach to";
        opts.onUserChoice = [shared = m_Shared, id = el.getID(), crossrefIdx](std::span<UID> choices)
        {
            if (choices.empty())
            {
                return false;
            }
            return TryReassignCrossref(shared->updCommittableModelGraph(), id, crossrefIdx, choices.front());
        };
        m_Maybe3DViewerModal = std::make_shared<ChooseElLayer>(*this, m_Shared, opts);
    }

    // ensure any stale references into the modelgrah are cleaned up
    void garbageCollectStaleRefs()
    {
        ModelGraph const& mg = m_Shared->getModelGraph();

        if (m_MaybeHover && !mg.containsEl(m_MaybeHover.ID))
        {
            m_MaybeHover.reset();
        }

        if (m_MaybeOpenedContextMenu && !mg.containsEl(m_MaybeOpenedContextMenu.ID))
        {
            m_MaybeOpenedContextMenu.reset();
        }
    }

    // delete currently-selected scene elements
    void deleteSelected()
    {
        DeleteSelected(m_Shared->updCommittableModelGraph());
        garbageCollectStaleRefs();
    }

    // delete a particular scene element
    void deleteEl(UID elID)
    {
        DeleteEl(m_Shared->updCommittableModelGraph(), elID);
        garbageCollectStaleRefs();
    }

    // update this scene from the current keyboard state, as saved by ImGui
    bool updateFromImGuiKeyboardState()
    {
        if (ImGui::GetIO().WantCaptureKeyboard)
        {
            return false;
        }

        bool shiftDown = osc::IsShiftDown();
        bool ctrlOrSuperDown = osc::IsCtrlOrSuperDown();

        if (ctrlOrSuperDown && ImGui::IsKeyPressed(ImGuiKey_N))
        {
            // Ctrl+N: new scene
            m_Shared->requestNewMeshImporterTab();
            return true;
        }
        else if (ctrlOrSuperDown && ImGui::IsKeyPressed(ImGuiKey_O))
        {
            // Ctrl+O: open osim
            m_Shared->openOsimFileAsModelGraph();
            return true;
        }
        else if (ctrlOrSuperDown && shiftDown && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            // Ctrl+Shift+S: export as: export scene as osim to user-specified location
            m_Shared->exportAsModelGraphAsOsimFile();
            return true;
        }
        else if (ctrlOrSuperDown && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            // Ctrl+S: export: export scene as osim according to typical export heuristic
            m_Shared->exportModelGraphAsOsimFile();
            return true;
        }
        else if (ctrlOrSuperDown && ImGui::IsKeyPressed(ImGuiKey_W))
        {
            // Ctrl+W: close
            m_Shared->requestClose();
            return true;
        }
        else if (ctrlOrSuperDown && ImGui::IsKeyPressed(ImGuiKey_Q))
        {
            // Ctrl+Q: quit application
            App::upd().requestQuit();
            return true;
        }
        else if (ctrlOrSuperDown && ImGui::IsKeyPressed(ImGuiKey_A))
        {
            // Ctrl+A: select all
            m_Shared->selectAll();
            return true;
        }
        else if (ctrlOrSuperDown && shiftDown && ImGui::IsKeyPressed(ImGuiKey_Z))
        {
            // Ctrl+Shift+Z: redo
            m_Shared->redoCurrentModelGraph();
            return true;
        }
        else if (ctrlOrSuperDown && ImGui::IsKeyPressed(ImGuiKey_Z))
        {
            // Ctrl+Z: undo
            m_Shared->undoCurrentModelGraph();
            return true;
        }
        else if (osc::IsAnyKeyDown({ImGuiKey_Delete, ImGuiKey_Backspace}))
        {
            // Delete/Backspace: delete any selected elements
            deleteSelected();
            return true;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_B))
        {
            // B: add body to hovered element
            tryAddBodyToHoveredElement();
            return true;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_A))
        {
            // A: assign a parent for the hovered element
            tryTransitionToAssigningHoverAndSelectionNextFrame();
            return true;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_J))
        {
            // J: try to create a joint
            tryCreatingJointFromHoveredElement();
            return true;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_T))
        {
            // T: try to add a station to the current hover
            tryAddingStationAtMousePosToHoveredElement();
            return true;
        }
        else if (UpdateImguizmoStateFromKeyboard(m_ImGuizmoState.op, m_ImGuizmoState.mode))
        {
            return true;
        }
        else if (UpdatePolarCameraFromImGuiKeyboardInputs(m_Shared->updCamera(), m_Shared->get3DSceneRect(), calcSceneAABB()))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    void drawNothingContextMenuContentHeader()
    {
        ImGui::Text(ICON_FA_BOLT " Actions");
        ImGui::SameLine();
        ImGui::TextDisabled("(nothing clicked)");
        ImGui::Separator();
    }

    void drawSceneElContextMenuContentHeader(SceneEl const& e)
    {
        ImGui::Text("%s %s", e.getClass().getIconUTF8().c_str(), e.getLabel().c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", GetContextMenuSubHeaderText(m_Shared->getModelGraph(), e).c_str());
        ImGui::SameLine();
        osc::DrawHelpMarker(e.getClass().getName(), e.getClass().getDescription());
        ImGui::Separator();
    }

    void drawSceneElPropEditors(SceneEl const& e)
    {
        ModelGraph& mg = m_Shared->updModelGraph();

        // label/name editor
        if (e.canChangeLabel())
        {
            std::string buf{static_cast<std::string_view>(e.getLabel())};
            if (osc::InputString("Name", buf))
            {
                mg.updElByID(e.getID()).setLabel(buf);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                std::stringstream ss;
                ss << "changed " << e.getClass().getName() << " name";
                m_Shared->commitCurrentModelGraph(std::move(ss).str());
            }
            ImGui::SameLine();
            osc::DrawHelpMarker("Component Name", "This is the name that the component will have in the exported OpenSim model.");
        }

        // position editor
        if (e.canChangePosition())
        {
            Vec3 translation = e.getPos(mg);
            if (ImGui::InputFloat3("Translation", osc::ValuePtr(translation), "%.6f"))
            {
                mg.updElByID(e.getID()).setPos(mg, translation);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                std::stringstream ss;
                ss << "changed " << e.getLabel() << "'s translation";
                m_Shared->commitCurrentModelGraph(std::move(ss).str());
            }
            ImGui::SameLine();
            osc::DrawHelpMarker("Translation", ModelGraphStrings::c_TranslationDescription);
        }

        // rotation editor
        if (e.canChangeRotation())
        {
            Vec3 eulerDegs = osc::Rad2Deg(osc::EulerAngles(e.getRotation(m_Shared->getModelGraph())));

            if (ImGui::InputFloat3("Rotation (deg)", osc::ValuePtr(eulerDegs), "%.6f"))
            {
                Quat quatRads = Quat{osc::Deg2Rad(eulerDegs)};
                mg.updElByID(e.getID()).setRotation(mg, quatRads);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                std::stringstream ss;
                ss << "changed " << e.getLabel() << "'s rotation";
                m_Shared->commitCurrentModelGraph(std::move(ss).str());
            }
            ImGui::SameLine();
            osc::DrawHelpMarker("Rotation", "These are the rotation Euler angles for the component in ground. Positive rotations are anti-clockwise along that axis.\n\nNote: the numbers may contain slight rounding error, due to backend constraints. Your values *should* be accurate to a few decimal places.");
        }

        // scale factor editor
        if (e.canChangeScale())
        {
            Vec3 scaleFactors = e.getScale(mg);
            if (ImGui::InputFloat3("Scale", osc::ValuePtr(scaleFactors), "%.6f"))
            {
                mg.updElByID(e.getID()).setScale(mg, scaleFactors);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                std::stringstream ss;
                ss << "changed " << e.getLabel() << "'s scale";
                m_Shared->commitCurrentModelGraph(std::move(ss).str());
            }
            ImGui::SameLine();
            osc::DrawHelpMarker("Scale", "These are the scale factors of the component in ground. These scale-factors are applied to the element before any other transform (it scales first, then rotates, then translates).");
        }
    }

    // draw content of "Add" menu for some scene element
    void drawAddOtherToSceneElActions(SceneEl& el, Vec3 const& clickPos)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{10.0f, 10.0f});
        ScopeGuard const g1{[]() { ImGui::PopStyleVar(); }};

        int imguiID = 0;
        ImGui::PushID(imguiID++);
        ScopeGuard const g2{[]() { ImGui::PopID(); }};

        if (CanAttachMeshTo(el))
        {
            if (ImGui::MenuItem(ICON_FA_CUBE " Meshes"))
            {
                m_Shared->pushMeshLoadRequests(el.getID(), m_Shared->promptUserForMeshFiles());
            }
            osc::DrawTooltipIfItemHovered("Add Meshes", ModelGraphStrings::c_MeshDescription);
        }
        ImGui::PopID();

        ImGui::PushID(imguiID++);
        if (el.hasPhysicalSize())
        {
            if (ImGui::BeginMenu(ICON_FA_CIRCLE " Body"))
            {
                if (ImGui::MenuItem(ICON_FA_COMPRESS_ARROWS_ALT " at center"))
                {
                    AddBody(m_Shared->updCommittableModelGraph(), el.getPos(m_Shared->getModelGraph()), el.getID());
                }
                osc::DrawTooltipIfItemHovered("Add Body", ModelGraphStrings::c_BodyDescription.c_str());

                if (ImGui::MenuItem(ICON_FA_MOUSE_POINTER " at click position"))
                {
                    AddBody(m_Shared->updCommittableModelGraph(), clickPos, el.getID());
                }
                osc::DrawTooltipIfItemHovered("Add Body", ModelGraphStrings::c_BodyDescription.c_str());

                if (ImGui::MenuItem(ICON_FA_DOT_CIRCLE " at ground"))
                {
                    AddBody(m_Shared->updCommittableModelGraph());
                }
                osc::DrawTooltipIfItemHovered("Add body", ModelGraphStrings::c_BodyDescription.c_str());

                if (auto const* meshEl = dynamic_cast<MeshEl const*>(&el))
                {
                    if (ImGui::MenuItem(ICON_FA_BORDER_ALL " at bounds center"))
                    {
                        Vec3 const location = Midpoint(meshEl->calcBounds());
                        AddBody(m_Shared->updCommittableModelGraph(), location, meshEl->getID());
                    }
                    osc::DrawTooltipIfItemHovered("Add Body", ModelGraphStrings::c_BodyDescription.c_str());

                    if (ImGui::MenuItem(ICON_FA_DIVIDE " at mesh average center"))
                    {
                        Vec3 const location = AverageCenter(*meshEl);
                        AddBody(m_Shared->updCommittableModelGraph(), location, meshEl->getID());
                    }
                    osc::DrawTooltipIfItemHovered("Add Body", ModelGraphStrings::c_BodyDescription.c_str());

                    if (ImGui::MenuItem(ICON_FA_WEIGHT " at mesh mass center"))
                    {
                        Vec3 const location = MassCenter(*meshEl);
                        AddBody(m_Shared->updCommittableModelGraph(), location, meshEl->getID());
                    }
                    osc::DrawTooltipIfItemHovered("Add body", ModelGraphStrings::c_BodyDescription.c_str());
                }

                ImGui::EndMenu();
            }
        }
        else
        {
            if (ImGui::MenuItem(ICON_FA_CIRCLE " Body"))
            {
                AddBody(m_Shared->updCommittableModelGraph(), el.getPos(m_Shared->getModelGraph()), el.getID());
            }
            osc::DrawTooltipIfItemHovered("Add Body", ModelGraphStrings::c_BodyDescription.c_str());
        }
        ImGui::PopID();

        ImGui::PushID(imguiID++);
        if (auto const* body = dynamic_cast<BodyEl const*>(&el))
        {
            if (ImGui::MenuItem(ICON_FA_LINK " Joint"))
            {
                transitionToChoosingJointParent(*body);
            }
            osc::DrawTooltipIfItemHovered("Creating Joints", "Create a joint from this body (the \"child\") to some other body in the model (the \"parent\").\n\nAll bodies in an OpenSim model must eventually connect to ground via joints. If no joint is added to the body then OpenSim Creator will automatically add a WeldJoint between the body and ground.");
        }
        ImGui::PopID();

        ImGui::PushID(imguiID++);
        if (CanAttachStationTo(el))
        {
            if (el.hasPhysicalSize())
            {
                if (ImGui::BeginMenu(ICON_FA_MAP_PIN " Station"))
                {
                    if (ImGui::MenuItem(ICON_FA_COMPRESS_ARROWS_ALT " at center"))
                    {
                        AddStationAtLocation(m_Shared->updCommittableModelGraph(), el, el.getPos(m_Shared->getModelGraph()));
                    }
                    osc::DrawTooltipIfItemHovered("Add Station", ModelGraphStrings::c_StationDescription);

                    if (ImGui::MenuItem(ICON_FA_MOUSE_POINTER " at click position"))
                    {
                        AddStationAtLocation(m_Shared->updCommittableModelGraph(), el, clickPos);
                    }
                    osc::DrawTooltipIfItemHovered("Add Station", ModelGraphStrings::c_StationDescription);

                    if (ImGui::MenuItem(ICON_FA_DOT_CIRCLE " at ground"))
                    {
                        AddStationAtLocation(m_Shared->updCommittableModelGraph(), el, Vec3{});
                    }
                    osc::DrawTooltipIfItemHovered("Add Station", ModelGraphStrings::c_StationDescription);

                    if (dynamic_cast<MeshEl const*>(&el))
                    {
                        if (ImGui::MenuItem(ICON_FA_BORDER_ALL " at bounds center"))
                        {
                            AddStationAtLocation(m_Shared->updCommittableModelGraph(), el, Midpoint(el.calcBounds(m_Shared->getModelGraph())));
                        }
                        osc::DrawTooltipIfItemHovered("Add Station", ModelGraphStrings::c_StationDescription);
                    }

                    ImGui::EndMenu();
                }
            }
            else
            {
                if (ImGui::MenuItem(ICON_FA_MAP_PIN " Station"))
                {
                    AddStationAtLocation(m_Shared->updCommittableModelGraph(), el, el.getPos(m_Shared->getModelGraph()));
                }
                osc::DrawTooltipIfItemHovered("Add Station", ModelGraphStrings::c_StationDescription);
            }
        }
        ImGui::PopID();

        ImGui::PushID(imguiID++);
        if (CanAttachEdgeTo(el))
        {
            if (ImGui::MenuItem(ICON_FA_ARROWS_ALT "Edge"))
            {
                // TODO: transition into picking the other side of the edge
            }
            osc::DrawTooltipIfItemHovered("Add Edge", EdgeEl::Class().getDescription());
        }
        // ~ScopeGuard: implicitly calls ImGui::PopID()
    }

    void drawNothingActions()
    {
        if (ImGui::MenuItem(ICON_FA_CUBE " Add Meshes"))
        {
            m_Shared->promptUserForMeshFilesAndPushThemOntoMeshLoader();
        }
        osc::DrawTooltipIfItemHovered("Add Meshes to the model", ModelGraphStrings::c_MeshDescription);

        if (ImGui::BeginMenu(ICON_FA_PLUS " Add Other"))
        {
            drawAddOtherMenuItems();

            ImGui::EndMenu();
        }
    }

    void drawSceneElActions(SceneEl& el, Vec3 const& clickPos)
    {
        if (ImGui::MenuItem(ICON_FA_CAMERA " Focus camera on this"))
        {
            m_Shared->focusCameraOn(Midpoint(el.calcBounds(m_Shared->getModelGraph())));
        }
        osc::DrawTooltipIfItemHovered("Focus camera on this scene element", "Focuses the scene camera on this element. This is useful for tracking the camera around that particular object in the scene");

        if (ImGui::BeginMenu(ICON_FA_PLUS " Add"))
        {
            drawAddOtherToSceneElActions(el, clickPos);
            ImGui::EndMenu();
        }

        if (auto const* body = dynamic_cast<BodyEl const*>(&el))
        {
            if (ImGui::MenuItem(ICON_FA_LINK " Join to"))
            {
                transitionToChoosingJointParent(*body);
            }
            osc::DrawTooltipIfItemHovered("Creating Joints", "Create a joint from this body (the \"child\") to some other body in the model (the \"parent\").\n\nAll bodies in an OpenSim model must eventually connect to ground via joints. If no joint is added to the body then OpenSim Creator will automatically add a WeldJoint between the body and ground.");
        }

        if (el.canDelete())
        {
            if (ImGui::MenuItem(ICON_FA_TRASH " Delete"))
            {
                DeleteEl(m_Shared->updCommittableModelGraph(), el.getID());
                garbageCollectStaleRefs();
                ImGui::CloseCurrentPopup();
            }
            osc::DrawTooltipIfItemHovered("Delete", "Deletes the component from the model. Deletion is undo-able (use the undo/redo feature). Anything attached to this element (e.g. joints, meshes) will also be deleted.");
        }
    }

    // draw the "Translate" menu for any generic `SceneEl`
    void drawTranslateMenu(SceneEl& el)
    {
        if (!el.canChangePosition())
        {
            return;  // can't change its position
        }

        if (!ImGui::BeginMenu(ICON_FA_ARROWS_ALT " Translate"))
        {
            return;  // top-level menu isn't open
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{10.0f, 10.0f});

        for (int i = 0, len = el.getNumCrossReferences(); i < len; ++i)
        {
            std::string label = "To " + el.getCrossReferenceLabel(i);
            if (ImGui::MenuItem(label.c_str()))
            {
                TryTranslateElementToAnotherElement(m_Shared->updCommittableModelGraph(), el.getID(), el.getCrossReferenceConnecteeID(i));
            }
        }

        if (ImGui::MenuItem("To (select something)"))
        {
            transitionToChoosingWhichElementToTranslateTo(el);
        }

        if (el.getNumCrossReferences() == 2)
        {
            std::string label = "Between " + el.getCrossReferenceLabel(0) + " and " + el.getCrossReferenceLabel(1);
            if (ImGui::MenuItem(label.c_str()))
            {
                UID a = el.getCrossReferenceConnecteeID(0);
                UID b = el.getCrossReferenceConnecteeID(1);
                TryTranslateBetweenTwoElements(m_Shared->updCommittableModelGraph(), el.getID(), a, b);
            }
        }

        if (ImGui::MenuItem("Between two scene elements"))
        {
            transitionToChoosingElementsToTranslateBetween(el);
        }

        if (ImGui::MenuItem("Between two mesh points"))
        {
            transitionToTranslatingElementAlongTwoMeshPoints(el);
        }

        if (ImGui::MenuItem("To mesh bounds center"))
        {
            transitionToTranslatingElementToMeshBoundsCenter(el);
        }
        osc::DrawTooltipIfItemHovered("Translate to mesh bounds center", "Translates the given element to the center of the selected mesh's bounding box. The bounding box is the smallest box that contains all mesh vertices");

        if (ImGui::MenuItem("To mesh average center"))
        {
            transitionToTranslatingElementToMeshAverageCenter(el);
        }
        osc::DrawTooltipIfItemHovered("Translate to mesh average center", "Translates the given element to the average center point of vertices in the selected mesh.\n\nEffectively, this adds each vertex location in the mesh, divides the sum by the number of vertices in the mesh, and sets the translation of the given object to that location.");

        if (ImGui::MenuItem("To mesh mass center"))
        {
            transitionToTranslatingElementToMeshMassCenter(el);
        }
        osc::DrawTooltipIfItemHovered("Translate to mesh mess center", "Translates the given element to the mass center of the selected mesh.\n\nCAREFUL: the algorithm used to do this heavily relies on your triangle winding (i.e. normals) being correct and your mesh being a closed surface. If your mesh doesn't meet these requirements, you might get strange results (apologies: the only way to get around that problems involves complicated voxelization and leak-detection algorithms :( )");

        ImGui::PopStyleVar();
        ImGui::EndMenu();
    }

    // draw the "Reorient" menu for any generic `SceneEl`
    void drawReorientMenu(SceneEl& el)
    {
        if (!el.canChangeRotation())
        {
            return;  // can't change its rotation
        }

        if (!ImGui::BeginMenu(ICON_FA_REDO " Reorient"))
        {
            return;  // top-level menu isn't open
        }
        osc::DrawTooltipIfItemHovered("Reorient the scene element", "Rotates the scene element in without changing its position");

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{10.0f, 10.0f});

        {
            auto DrawMenuContent = [&](int axis)
            {
                for (int i = 0, len = el.getNumCrossReferences(); i < len; ++i)
                {
                    std::string label = "Towards " + el.getCrossReferenceLabel(i);

                    if (ImGui::MenuItem(label.c_str()))
                    {
                        PointAxisTowards(m_Shared->updCommittableModelGraph(), el.getID(), axis, el.getCrossReferenceConnecteeID(i));
                    }
                }

                if (ImGui::MenuItem("Towards (select something)"))
                {
                    transitionToChoosingWhichElementToPointAxisTowards(el, axis);
                }

                if (ImGui::MenuItem("Along line between (select two elements)"))
                {
                    transitionToChoosingTwoElementsToAlignAxisAlong(el, axis);
                }

                if (ImGui::MenuItem("90 degress"))
                {
                    RotateAxisXRadians(m_Shared->updCommittableModelGraph(), el, axis, std::numbers::pi_v<float>/2.0f);
                }

                if (ImGui::MenuItem("180 degrees"))
                {
                    RotateAxisXRadians(m_Shared->updCommittableModelGraph(), el, axis, std::numbers::pi_v<float>);
                }

                if (ImGui::MenuItem("Along two mesh points"))
                {
                    transitionToOrientingElementAlongTwoMeshPoints(el, axis);
                }
            };

            if (ImGui::BeginMenu("x"))
            {
                DrawMenuContent(0);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("y"))
            {
                DrawMenuContent(1);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("z"))
            {
                DrawMenuContent(2);
                ImGui::EndMenu();
            }
        }

        if (ImGui::MenuItem("copy"))
        {
            transitionToCopyingSomethingElsesOrientation(el);
        }

        if (ImGui::MenuItem("reset"))
        {
            el.setXform(m_Shared->getModelGraph(), Transform{.position = el.getPos(m_Shared->getModelGraph())});
            m_Shared->commitCurrentModelGraph("reset " + el.getLabel() + " orientation");
        }

        ImGui::PopStyleVar();
        ImGui::EndMenu();
    }

    // draw the "Mass" editor for a `BodyEl`
    void drawMassEditor(BodyEl const& bodyEl)
    {
        auto curMass = static_cast<float>(bodyEl.getMass());
        if (ImGui::InputFloat("Mass", &curMass, 0.0f, 0.0f, "%.6f"))
        {
            m_Shared->updModelGraph().updElByID<BodyEl>(bodyEl.getID()).setMass(static_cast<double>(curMass));
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            m_Shared->commitCurrentModelGraph("changed body mass");
        }
        ImGui::SameLine();
        osc::DrawHelpMarker("Mass", "The mass of the body. OpenSim defines this as 'unitless'; however, models conventionally use kilograms.");
    }

    // draw the "Joint Type" editor for a `JointEl`
    void drawJointTypeEditor(JointEl const& jointEl)
    {
        size_t currentIdx = jointEl.getJointTypeIndex();
        auto const& registry = osc::GetComponentRegistry<OpenSim::Joint>();
        auto const nameAccessor = [&registry](size_t i) { return registry[i].name(); };

        if (osc::Combo("Joint Type", &currentIdx, registry.size(), nameAccessor))
        {
            m_Shared->updModelGraph().updElByID<JointEl>(jointEl.getID()).setJointTypeIndex(currentIdx);
            m_Shared->commitCurrentModelGraph("changed joint type");
        }
        ImGui::SameLine();
        osc::DrawHelpMarker("Joint Type", "This is the type of joint that should be added into the OpenSim model. The joint's type dictates what types of motion are permitted around the joint center. See the official OpenSim documentation for an explanation of each joint type.");
    }

    // draw the "Reassign Connection" menu, which lets users change an element's cross reference
    void drawReassignCrossrefMenu(SceneEl& el)
    {
        int nRefs = el.getNumCrossReferences();

        if (nRefs == 0)
        {
            return;
        }

        if (ImGui::BeginMenu(ICON_FA_EXTERNAL_LINK_ALT " Reassign Connection"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{10.0f, 10.0f});

            for (int i = 0; i < nRefs; ++i)
            {
                CStringView label = el.getCrossReferenceLabel(i);
                if (ImGui::MenuItem(label.c_str()))
                {
                    transitionToReassigningCrossRef(el, i);
                }
            }

            ImGui::PopStyleVar();
            ImGui::EndMenu();
        }
    }

    void actionPromptUserToSaveMeshAsOBJ(
        Mesh const& mesh)
    {
        // prompt user for a save location
        std::optional<std::filesystem::path> const maybeUserSaveLocation =
            osc::PromptUserForFileSaveLocationAndAddExtensionIfNecessary("obj");
        if (!maybeUserSaveLocation)
        {
            return;  // user didn't select a save location
        }
        std::filesystem::path const& userSaveLocation = *maybeUserSaveLocation;

        // write transformed mesh to output
        std::ofstream outputFileStream
        {
            userSaveLocation,
            std::ios_base::out | std::ios_base::trunc | std::ios_base::binary,
        };
        if (!outputFileStream)
        {
            std::string const error = osc::CurrentErrnoAsString();
            osc::log::error("%s: could not save obj output: %s", userSaveLocation.string().c_str(), error.c_str());
            return;
        }

        AppMetadata const& appMetadata = App::get().getMetadata();
        ObjMetadata const objMetadata
        {
            osc::CalcFullApplicationNameWithVersionAndBuild(appMetadata),
        };

        osc::WriteMeshAsObj(
            outputFileStream,
            mesh,
            objMetadata,
            ObjWriterFlags::NoWriteNormals
        );
    }

    void actionPromptUserToSaveMeshAsSTL(
        Mesh const& mesh)
    {
        // prompt user for a save location
        std::optional<std::filesystem::path> const maybeUserSaveLocation =
            osc::PromptUserForFileSaveLocationAndAddExtensionIfNecessary("stl");
        if (!maybeUserSaveLocation)
        {
            return;  // user didn't select a save location
        }
        std::filesystem::path const& userSaveLocation = *maybeUserSaveLocation;

        // write transformed mesh to output
        std::ofstream outputFileStream
        {
            userSaveLocation,
            std::ios_base::out | std::ios_base::trunc | std::ios_base::binary,
        };
        if (!outputFileStream)
        {
            std::string const error = osc::CurrentErrnoAsString();
            osc::log::error("%s: could not save obj output: %s", userSaveLocation.string().c_str(), error.c_str());
            return;
        }

        AppMetadata const& appMetadata = App::get().getMetadata();
        StlMetadata const stlMetadata
        {
            osc::CalcFullApplicationNameWithVersionAndBuild(appMetadata),
        };

        osc::WriteMeshAsStl(outputFileStream, mesh, stlMetadata);
    }

    void drawSaveMeshMenu(MeshEl const& el)
    {
        if (ImGui::BeginMenu(ICON_FA_FILE_EXPORT " Export"))
        {
            ImGui::TextDisabled("With Respect to:");
            ImGui::Separator();
            for (SceneEl const& sceneEl : m_Shared->getModelGraph().iter())
            {
                if (ImGui::BeginMenu(sceneEl.getLabel().c_str()))
                {
                    ImGui::TextDisabled("Format:");
                    ImGui::Separator();

                    if (ImGui::MenuItem(".obj"))
                    {
                        Transform const sceneElToGround = sceneEl.getXForm(m_Shared->getModelGraph());
                        Transform const meshVertToGround = el.getXForm();
                        Mat4 const meshVertToSceneElVert = osc::ToInverseMat4(sceneElToGround) * osc::ToMat4(meshVertToGround);

                        Mesh mesh = el.getMeshData();
                        mesh.transformVerts(meshVertToSceneElVert);
                        actionPromptUserToSaveMeshAsOBJ(mesh);
                    }

                    if (ImGui::MenuItem(".stl"))
                    {
                        Transform const sceneElToGround = sceneEl.getXForm(m_Shared->getModelGraph());
                        Transform const meshVertToGround = el.getXForm();
                        Mat4 const meshVertToSceneElVert = osc::ToInverseMat4(sceneElToGround) * osc::ToMat4(meshVertToGround);

                        Mesh mesh = el.getMeshData();
                        mesh.transformVerts(meshVertToSceneElVert);
                        actionPromptUserToSaveMeshAsSTL(mesh);
                    }

                    ImGui::EndMenu();
                }
            }
            ImGui::EndMenu();
        }
    }

    // draw context menu content for when user right-clicks nothing
    void drawNothingContextMenuContent()
    {
        drawNothingContextMenuContentHeader();
        SpacerDummy();
        drawNothingActions();
    }

    // draw context menu content for a `GroundEl`
    void drawContextMenuContent(GroundEl& el, Vec3 const& clickPos)
    {
        drawSceneElContextMenuContentHeader(el);
        SpacerDummy();
        drawSceneElActions(el, clickPos);
    }

    // draw context menu content for a `BodyEl`
    void drawContextMenuContent(BodyEl& el, Vec3 const& clickPos)
    {
        drawSceneElContextMenuContentHeader(el);

        SpacerDummy();

        drawSceneElPropEditors(el);
        drawMassEditor(el);

        SpacerDummy();

        drawTranslateMenu(el);
        drawReorientMenu(el);
        drawReassignCrossrefMenu(el);
        drawSceneElActions(el, clickPos);
    }

    // draw context menu content for a `MeshEl`
    void drawContextMenuContent(MeshEl& el, Vec3 const& clickPos)
    {
        drawSceneElContextMenuContentHeader(el);

        SpacerDummy();

        drawSceneElPropEditors(el);

        SpacerDummy();

        drawTranslateMenu(el);
        drawReorientMenu(el);
        drawSaveMeshMenu(el);
        drawReassignCrossrefMenu(el);
        drawSceneElActions(el, clickPos);
    }

    // draw context menu content for a `JointEl`
    void drawContextMenuContent(JointEl& el, Vec3 const& clickPos)
    {
        drawSceneElContextMenuContentHeader(el);

        SpacerDummy();

        drawSceneElPropEditors(el);
        drawJointTypeEditor(el);

        SpacerDummy();

        drawTranslateMenu(el);
        drawReorientMenu(el);
        drawReassignCrossrefMenu(el);
        drawSceneElActions(el, clickPos);
    }

    // draw context menu content for a `StationEl`
    void drawContextMenuContent(StationEl& el, Vec3 const& clickPos)
    {
        drawSceneElContextMenuContentHeader(el);

        SpacerDummy();

        drawSceneElPropEditors(el);

        SpacerDummy();

        drawTranslateMenu(el);
        drawReorientMenu(el);
        drawReassignCrossrefMenu(el);
        drawSceneElActions(el, clickPos);
    }

    void drawContextMenuContent(EdgeEl& el, Vec3 const&)
    {
        drawSceneElContextMenuContentHeader(el);

        SpacerDummy();

        drawSceneElPropEditors(el);

        SpacerDummy();

        // TODO
    }

    // draw context menu content for some scene element
    void drawContextMenuContent(SceneEl& el, Vec3 const& clickPos)
    {
        std::visit(Overload
        {
            [this, &clickPos](GroundEl& el)  { this->drawContextMenuContent(el, clickPos); },
            [this, &clickPos](MeshEl& el)    { this->drawContextMenuContent(el, clickPos); },
            [this, &clickPos](BodyEl& el)    { this->drawContextMenuContent(el, clickPos); },
            [this, &clickPos](JointEl& el)   { this->drawContextMenuContent(el, clickPos); },
            [this, &clickPos](StationEl& el) { this->drawContextMenuContent(el, clickPos); },
            [this, &clickPos](EdgeEl& el)    { this->drawContextMenuContent(el, clickPos); },
        }, el.toVariant());
    }

    // draw a context menu for the current state (if applicable)
    void drawContextMenuContent()
    {
        if (!m_MaybeOpenedContextMenu)
        {
            // context menu not open, but just draw the "nothing" menu
            PushID(UID::empty());
            ScopeGuard const g{[]() { ImGui::PopID(); }};
            drawNothingContextMenuContent();
        }
        else if (m_MaybeOpenedContextMenu.ID == ModelGraphIDs::RightClickedNothing())
        {
            // context menu was opened on "nothing" specifically
            PushID(UID::empty());
            ScopeGuard const g{[]() { ImGui::PopID(); }};
            drawNothingContextMenuContent();
        }
        else if (SceneEl* el = m_Shared->updModelGraph().tryUpdElByID(m_MaybeOpenedContextMenu.ID))
        {
            // context menu was opened on a scene element that exists in the modelgraph
            PushID(el->getID());
            ScopeGuard const g{[]() { ImGui::PopID(); }};
            drawContextMenuContent(*el, m_MaybeOpenedContextMenu.Pos);
        }


        // context menu should be closed under these conditions
        if (osc::IsAnyKeyPressed({ImGuiKey_Enter, ImGuiKey_Escape}))
        {
            m_MaybeOpenedContextMenu.reset();
            ImGui::CloseCurrentPopup();
        }
    }

    // draw the content of the (undo/redo) "History" panel
    void drawHistoryPanelContent()
    {
        osc::UndoRedoPanel::DrawContent(m_Shared->updCommittableModelGraph());
    }

    void drawNavigatorElement(SceneElClass const& c)
    {
        ModelGraph& mg = m_Shared->updModelGraph();

        ImGui::Text("%s %s", c.getIconUTF8().c_str(), c.getNamePluralized().c_str());
        ImGui::SameLine();
        osc::DrawHelpMarker(c.getNamePluralized(), c.getDescription());
        SpacerDummy();
        ImGui::Indent();

        bool empty = true;
        for (SceneEl const& el : mg.iter())
        {
            if (el.getClass() != c)
            {
                continue;
            }

            empty = false;

            UID id = el.getID();
            int styles = 0;

            if (id == m_MaybeHover.ID)
            {
                osc::PushStyleColor(ImGuiCol_Text, Color::yellow());
                ++styles;
            }
            else if (m_Shared->isSelected(id))
            {
                osc::PushStyleColor(ImGuiCol_Text, Color::yellow());
                ++styles;
            }

            ImGui::Text("%s", el.getLabel().c_str());

            ImGui::PopStyleColor(styles);

            if (ImGui::IsItemHovered())
            {
                m_MaybeHover = {id, {}};
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                if (!osc::IsShiftDown())
                {
                    m_Shared->updModelGraph().deSelectAll();
                }
                m_Shared->updModelGraph().select(id);
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            {
                m_MaybeOpenedContextMenu = Hover{id, {}};
                ImGui::OpenPopup("##maincontextmenu");
                App::upd().requestRedraw();
            }
        }

        if (empty)
        {
            ImGui::TextDisabled("(no %s)", c.getNamePluralized().c_str());
        }
        ImGui::Unindent();
    }

    void drawNavigatorPanelContent()
    {
        for (SceneElClass const& c : GetSceneElClasses())
        {
            drawNavigatorElement(c);
            SpacerDummy();
        }

        // a navigator element might have opened the context menu in the navigator panel
        //
        // this can happen when the user right-clicks something in the navigator
        if (ImGui::BeginPopup("##maincontextmenu"))
        {
            drawContextMenuContent();
            ImGui::EndPopup();
        }
    }

    void drawAddOtherMenuItems()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{10.0f, 10.0f});

        if (ImGui::MenuItem(ICON_FA_CUBE " Meshes"))
        {
            m_Shared->promptUserForMeshFilesAndPushThemOntoMeshLoader();
        }
        osc::DrawTooltipIfItemHovered("Add Meshes", ModelGraphStrings::c_MeshDescription);

        if (ImGui::MenuItem(ICON_FA_CIRCLE " Body"))
        {
            AddBody(m_Shared->updCommittableModelGraph());
        }
        osc::DrawTooltipIfItemHovered("Add Body", ModelGraphStrings::c_BodyDescription);

        if (ImGui::MenuItem(ICON_FA_MAP_PIN " Station"))
        {
            ModelGraph& mg = m_Shared->updModelGraph();
            auto& e = mg.emplaceEl<StationEl>(UID{}, ModelGraphIDs::Ground(), Vec3{}, StationEl::Class().generateName());
            SelectOnly(mg, e);
        }
        osc::DrawTooltipIfItemHovered("Add Station", StationEl::Class().getDescription());

        if (ImGui::MenuItem(ICON_FA_ARROWS_ALT " Edge"))
        {
            // TODO: transition into selecting two elements (or cancelling)
        }
        osc::DrawTooltipIfItemHovered("Add Edge", EdgeEl::Class().getDescription());

        ImGui::PopStyleVar();
    }

    void draw3DViewerOverlayTopBar()
    {
        int imguiID = 0;

        if (ImGui::Button(ICON_FA_CUBE " Add Meshes"))
        {
            m_Shared->promptUserForMeshFilesAndPushThemOntoMeshLoader();
        }
        osc::DrawTooltipIfItemHovered("Add Meshes to the model", ModelGraphStrings::c_MeshDescription);

        ImGui::SameLine();

        ImGui::Button(ICON_FA_PLUS " Add Other");
        osc::DrawTooltipIfItemHovered("Add components to the model");

        if (ImGui::BeginPopupContextItem("##additemtoscenepopup", ImGuiPopupFlags_MouseButtonLeft))
        {
            drawAddOtherMenuItems();
            ImGui::EndPopup();
        }

        ImGui::SameLine();

        ImGui::Button(ICON_FA_PAINT_ROLLER " Colors");
        osc::DrawTooltipIfItemHovered("Change scene display colors", "This only changes the decroative display colors of model elements in this screen. Color changes are not saved to the exported OpenSim model. Changing these colors can be handy for spotting things, or constrasting scene elements more strongly");

        if (ImGui::BeginPopupContextItem("##addpainttoscenepopup", ImGuiPopupFlags_MouseButtonLeft))
        {
            std::span<Color const> colors = m_Shared->getColors();
            std::span<char const* const> labels = m_Shared->getColorLabels();
            OSC_ASSERT(colors.size() == labels.size() && "every color should have a label");

            for (size_t i = 0; i < colors.size(); ++i)
            {
                Color colorVal = colors[i];
                ImGui::PushID(imguiID++);
                if (ImGui::ColorEdit4(labels[i], osc::ValuePtr(colorVal)))
                {
                    m_Shared->setColor(i, colorVal);
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();

        ImGui::Button(ICON_FA_EYE " Visibility");
        osc::DrawTooltipIfItemHovered("Change what's visible in the 3D scene", "This only changes what's visible in this screen. Visibility options are not saved to the exported OpenSim model. Changing these visibility options can be handy if you have a lot of overlapping/intercalated scene elements");

        if (ImGui::BeginPopupContextItem("##changevisibilitypopup", ImGuiPopupFlags_MouseButtonLeft))
        {
            std::span<bool const> visibilities = m_Shared->getVisibilityFlags();
            std::span<char const* const> labels = m_Shared->getVisibilityFlagLabels();
            OSC_ASSERT(visibilities.size() == labels.size() && "every visibility flag should have a label");

            for (size_t i = 0; i < visibilities.size(); ++i)
            {
                bool v = visibilities[i];
                ImGui::PushID(imguiID++);
                if (ImGui::Checkbox(labels[i], &v))
                {
                    m_Shared->setVisibilityFlag(i, v);
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();

        ImGui::Button(ICON_FA_LOCK " Interactivity");
        osc::DrawTooltipIfItemHovered("Change what your mouse can interact with in the 3D scene", "This does not prevent being able to edit the model - it only affects whether you can click that type of element in the 3D scene. Combining these flags with visibility and custom colors can be handy if you have heavily overlapping/intercalated scene elements.");

        if (ImGui::BeginPopupContextItem("##changeinteractionlockspopup", ImGuiPopupFlags_MouseButtonLeft))
        {
            std::span<bool const> interactables = m_Shared->getIneractivityFlags();
            std::span<char const* const> labels =  m_Shared->getInteractivityFlagLabels();
            OSC_ASSERT(interactables.size() == labels.size());

            for (size_t i = 0; i < interactables.size(); ++i)
            {
                bool v = interactables[i];
                ImGui::PushID(imguiID++);
                if (ImGui::Checkbox(labels[i], &v))
                {
                    m_Shared->setInteractivityFlag(i, v);
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();

        DrawGizmoOpSelector(m_ImGuizmoState.op);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0.0f, 0.0f});
        ImGui::SameLine();
        ImGui::PopStyleVar();

        // local/global dropdown
        DrawGizmoModeSelector(m_ImGuizmoState.mode);
        ImGui::SameLine();

        // scale factor
        {
            CStringView const tooltipTitle = "Change scene scale factor";
            CStringView const tooltipDesc = "This rescales *some* elements in the scene. Specifically, the ones that have no 'size', such as body frames, joint frames, and the chequered floor texture.\n\nChanging this is handy if you are working on smaller or larger models, where the size of the (decorative) frames and floor are too large/small compared to the model you are working on.\n\nThis is purely decorative and does not affect the exported OpenSim model in any way.";

            float sf = m_Shared->getSceneScaleFactor();
            ImGui::SetNextItemWidth(ImGui::CalcTextSize("1000.00").x);
            if (ImGui::InputFloat("scene scale factor", &sf))
            {
                m_Shared->setSceneScaleFactor(sf);
            }
            osc::DrawTooltipIfItemHovered(tooltipTitle, tooltipDesc);
        }
    }

    std::optional<AABB> calcSceneAABB() const
    {
        std::optional<AABB> rv;
        for (DrawableThing const& drawable : m_DrawablesBuffer)
        {
            if (drawable.id != ModelGraphIDs::Empty())
            {
                AABB const bounds = calcBounds(drawable);
                rv = rv ? Union(*rv, bounds) : bounds;
            }
        }
        return rv;
    }

    void draw3DViewerOverlayBottomBar()
    {
        ImGui::PushID("##3DViewerOverlay");

        // bottom-left axes overlay
        {
            ImGuiStyle const& style = ImGui::GetStyle();
            Rect const& r = m_Shared->get3DSceneRect();
            Vec2 const topLeft =
            {
                r.p1.x + style.WindowPadding.x,
                r.p2.y - style.WindowPadding.y - CalcAlignmentAxesDimensions().y,
            };
            ImGui::SetCursorScreenPos(topLeft);
            DrawAlignmentAxes(m_Shared->getCamera().getViewMtx());
        }

        Rect sceneRect = m_Shared->get3DSceneRect();
        Vec2 trPos = {sceneRect.p1.x + 100.0f, sceneRect.p2.y - 55.0f};
        ImGui::SetCursorScreenPos(trPos);

        if (ImGui::Button(ICON_FA_SEARCH_MINUS))
        {
            m_Shared->updCamera().radius *= 1.2f;
        }
        osc::DrawTooltipIfItemHovered("Zoom Out");

        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_SEARCH_PLUS))
        {
            m_Shared->updCamera().radius *= 0.8f;
        }
        osc::DrawTooltipIfItemHovered("Zoom In");

        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_EXPAND_ARROWS_ALT))
        {
            if (std::optional<AABB> const sceneAABB = calcSceneAABB())
            {
                osc::AutoFocus(m_Shared->updCamera(), *sceneAABB, osc::AspectRatio(m_Shared->get3DSceneDims()));
            }
        }
        osc::DrawTooltipIfItemHovered("Autoscale Scene", "Zooms camera to try and fit everything in the scene into the viewer");

        ImGui::SameLine();

        if (ImGui::Button("X"))
        {
            m_Shared->updCamera().theta = std::numbers::pi_v<float>/2.0f;
            m_Shared->updCamera().phi = 0.0f;
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            m_Shared->updCamera().theta = -std::numbers::pi_v<float>/2.0f;
            m_Shared->updCamera().phi = 0.0f;
        }
        osc::DrawTooltipIfItemHovered("Face camera facing along X", "Right-clicking faces it along X, but in the opposite direction");

        ImGui::SameLine();

        if (ImGui::Button("Y"))
        {
            m_Shared->updCamera().theta = 0.0f;
            m_Shared->updCamera().phi = std::numbers::pi_v<float>/2.0f;
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            m_Shared->updCamera().theta = 0.0f;
            m_Shared->updCamera().phi = -std::numbers::pi_v<float>/2.0f;
        }
        osc::DrawTooltipIfItemHovered("Face camera facing along Y", "Right-clicking faces it along Y, but in the opposite direction");

        ImGui::SameLine();

        if (ImGui::Button("Z"))
        {
            m_Shared->updCamera().theta = 0.0f;
            m_Shared->updCamera().phi = 0.0f;
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            m_Shared->updCamera().theta = std::numbers::pi_v<float>;
            m_Shared->updCamera().phi = 0.0f;
        }
        osc::DrawTooltipIfItemHovered("Face camera facing along Z", "Right-clicking faces it along Z, but in the opposite direction");

        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_CAMERA))
        {
            m_Shared->updCamera() = CreateDefaultCamera();
        }
        osc::DrawTooltipIfItemHovered("Reset camera", "Resets the camera to its default position (the position it's in when the wizard is first loaded)");

        ImGui::PopID();
    }

    void draw3DViewerOverlayConvertToOpenSimModelButton()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10.0f, 10.0f});

        constexpr CStringView mainButtonText = "Convert to OpenSim Model " ICON_FA_ARROW_RIGHT;
        constexpr CStringView settingButtonText = ICON_FA_COG;
        constexpr Vec2 spacingBetweenMainAndSettingsButtons = {1.0f, 0.0f};
        constexpr Vec2 margin = {25.0f, 35.0f};

        Vec2 const mainButtonDims = osc::CalcButtonSize(mainButtonText);
        Vec2 const settingButtonDims = osc::CalcButtonSize(settingButtonText);
        Vec2 const viewportBottomRight = m_Shared->get3DSceneRect().p2;

        Vec2 const buttonTopLeft =
        {
            viewportBottomRight.x - (margin.x + spacingBetweenMainAndSettingsButtons.x + settingButtonDims.x + mainButtonDims.x),
            viewportBottomRight.y - (margin.y + mainButtonDims.y),
        };

        ImGui::SetCursorScreenPos(buttonTopLeft);
        osc::PushStyleColor(ImGuiCol_Button, Color::darkGreen());
        if (ImGui::Button(mainButtonText.c_str()))
        {
            m_Shared->tryCreateOutputModel();
        }
        osc::PopStyleColor();

        ImGui::PopStyleVar();
        osc::DrawTooltipIfItemHovered("Convert current scene to an OpenSim Model", "This will attempt to convert the current scene into an OpenSim model, followed by showing the model in OpenSim Creator's OpenSim model editor screen.\n\nYour progress in this tab will remain untouched.");

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10.0f, 10.0f});
        ImGui::SameLine(0.0f, spacingBetweenMainAndSettingsButtons.x);
        ImGui::Button(settingButtonText.c_str());
        ImGui::PopStyleVar();

        if (ImGui::BeginPopupContextItem("##settingspopup", ImGuiPopupFlags_MouseButtonLeft))
        {
            ModelCreationFlags const flags = m_Shared->getModelCreationFlags();

            {
                bool v = flags & ModelCreationFlags::ExportStationsAsMarkers;
                if (ImGui::Checkbox("Export Stations as Markers", &v))
                {
                    ModelCreationFlags const newFlags = v ?
                        flags + ModelCreationFlags::ExportStationsAsMarkers :
                        flags - ModelCreationFlags::ExportStationsAsMarkers;
                    m_Shared->setModelCreationFlags(newFlags);
                }
            }

            ImGui::EndPopup();
        }
    }

    void draw3DViewerOverlay()
    {
        draw3DViewerOverlayTopBar();
        draw3DViewerOverlayBottomBar();
        draw3DViewerOverlayConvertToOpenSimModelButton();
    }

    void drawSceneElTooltip(SceneEl const& e) const
    {
        ImGui::BeginTooltip();
        ImGui::Text("%s %s", e.getClass().getIconUTF8().c_str(), e.getLabel().c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", GetContextMenuSubHeaderText(m_Shared->getModelGraph(), e).c_str());
        ImGui::EndTooltip();
    }

    void drawHoverTooltip()
    {
        if (!m_MaybeHover)
        {
            return;  // nothing is hovered
        }

        if (SceneEl const* e = m_Shared->getModelGraph().tryGetElByID(m_MaybeHover.ID))
        {
            drawSceneElTooltip(*e);
        }
    }

    // draws 3D manipulator overlays (drag handles, etc.)
    void drawSelection3DManipulatorGizmos()
    {
        if (!m_Shared->hasSelection())
        {
            return;  // can only manipulate if selecting something
        }

        // if the user isn't *currently* manipulating anything, create an
        // up-to-date manipulation matrix
        //
        // this is so that ImGuizmo can *show* the manipulation axes, and
        // because the user might start manipulating during this frame
        if (!ImGuizmo::IsUsing())
        {
            auto it = m_Shared->getCurrentSelection().begin();
            auto end = m_Shared->getCurrentSelection().end();

            if (it == end)
            {
                return;  // sanity exit
            }

            ModelGraph const& mg = m_Shared->getModelGraph();

            int n = 0;

            Transform ras = GetTransform(mg, *it);
            ++it;
            ++n;

            while (it != end)
            {
                ras += GetTransform(mg, *it);
                ++it;
                ++n;
            }

            ras /= static_cast<float>(n);
            ras.rotation = osc::Normalize(ras.rotation);

            m_ImGuizmoState.mtx = ToMat4(ras);
        }

        // else: is using OR nselected > 0 (so draw it)

        Rect sceneRect = m_Shared->get3DSceneRect();

        ImGuizmo::SetRect(
            sceneRect.p1.x,
            sceneRect.p1.y,
            Dimensions(sceneRect).x,
            Dimensions(sceneRect).y
        );
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::AllowAxisFlip(false);  // user's didn't like this feature in UX sessions

        Mat4 delta;
        SetImguizmoStyleToOSCStandard();
        bool manipulated = ImGuizmo::Manipulate(
            osc::ValuePtr(m_Shared->getCamera().getViewMtx()),
            osc::ValuePtr(m_Shared->getCamera().getProjMtx(AspectRatio(sceneRect))),
            m_ImGuizmoState.op,
            m_ImGuizmoState.mode,
            osc::ValuePtr(m_ImGuizmoState.mtx),
            osc::ValuePtr(delta),
            nullptr,
            nullptr,
            nullptr
        );

        bool isUsingThisFrame = ImGuizmo::IsUsing();
        bool wasUsingLastFrame = m_ImGuizmoState.wasUsingLastFrame;
        m_ImGuizmoState.wasUsingLastFrame = isUsingThisFrame;  // so next frame can know

                                                               // if the user was using the gizmo last frame, and isn't using it this frame,
                                                               // then they probably just finished a manipulation, which should be snapshotted
                                                               // for undo/redo support
        if (wasUsingLastFrame && !isUsingThisFrame)
        {
            m_Shared->commitCurrentModelGraph("manipulated selection");
            App::upd().requestRedraw();
        }

        // if no manipulation happened this frame, exit early
        if (!manipulated)
        {
            return;
        }

        Vec3 translation;
        Vec3 rotation;
        Vec3 scale;
        ImGuizmo::DecomposeMatrixToComponents(
            osc::ValuePtr(delta),
            osc::ValuePtr(translation),
            osc::ValuePtr(rotation),
            osc::ValuePtr(scale)
        );
        rotation = osc::Deg2Rad(rotation);

        for (UID id : m_Shared->getCurrentSelection())
        {
            SceneEl& el = m_Shared->updModelGraph().updElByID(id);
            switch (m_ImGuizmoState.op) {
            case ImGuizmo::ROTATE:
                el.applyRotation(m_Shared->getModelGraph(), rotation, m_ImGuizmoState.mtx[3]);
                break;
            case ImGuizmo::TRANSLATE:
                el.applyTranslation(m_Shared->getModelGraph(), translation);
                break;
            case ImGuizmo::SCALE:
                el.applyScale(m_Shared->getModelGraph(), scale);
                break;
            default:
                break;
            }
        }
    }

    // perform a hovertest on the current 3D scene to determine what the user's mouse is over
    Hover hovertestScene(std::vector<DrawableThing> const& drawables)
    {
        if (!m_Shared->isRenderHovered())
        {
            return m_MaybeHover;
        }

        if (ImGuizmo::IsUsing())
        {
            return Hover{};
        }

        return m_Shared->doHovertest(drawables);
    }

    // handle any side effects for current user mouse hover
    void handleCurrentHover()
    {
        if (!m_Shared->isRenderHovered())
        {
            return;  // nothing hovered
        }

        bool const lcClicked = osc::IsMouseReleasedWithoutDragging(ImGuiMouseButton_Left);
        bool const shiftDown = osc::IsShiftDown();
        bool const altDown = osc::IsAltDown();
        bool const isUsingGizmo = ImGuizmo::IsUsing();

        if (!m_MaybeHover && lcClicked && !isUsingGizmo && !shiftDown)
        {
            // user clicked in some empty part of the screen: clear selection
            m_Shared->deSelectAll();
        }
        else if (m_MaybeHover && lcClicked && !isUsingGizmo)
        {
            // user clicked hovered thing: select hovered thing
            if (!shiftDown)
            {
                // user wasn't holding SHIFT, so clear selection
                m_Shared->deSelectAll();
            }

            if (altDown)
            {
                // ALT: only select the thing the mouse is over
                selectJustHover();
            }
            else
            {
                // NO ALT: select the "grouped items"
                selectAnythingGroupedWithHover();
            }
        }
    }

    // generate 3D scene drawables for current state
    std::vector<DrawableThing>& generateDrawables()
    {
        m_DrawablesBuffer.clear();

        for (SceneEl const& e : m_Shared->getModelGraph().iter())
        {
            m_Shared->appendDrawables(e, m_DrawablesBuffer);
        }

        if (m_Shared->isShowingFloor())
        {
            m_DrawablesBuffer.push_back(m_Shared->generateFloorDrawable());
        }

        return m_DrawablesBuffer;
    }

    // draws main 3D viewer panel
    void draw3DViewer()
    {
        m_Shared->setContentRegionAvailAsSceneRect();

        std::vector<DrawableThing>& sceneEls = generateDrawables();

        // hovertest the generated geometry
        m_MaybeHover = hovertestScene(sceneEls);
        handleCurrentHover();

        // assign rim highlights based on hover
        for (DrawableThing& dt : sceneEls)
        {
            dt.flags = computeFlags(m_Shared->getModelGraph(), dt.id, m_MaybeHover.ID);
        }

        // draw 3D scene (effectively, as an ImGui::Image)
        m_Shared->drawScene(sceneEls);
        if (m_Shared->isRenderHovered() && osc::IsMouseReleasedWithoutDragging(ImGuiMouseButton_Right) && !ImGuizmo::IsUsing())
        {
            m_MaybeOpenedContextMenu = m_MaybeHover;
            ImGui::OpenPopup("##maincontextmenu");
        }

        bool ctxMenuShowing = false;
        if (ImGui::BeginPopup("##maincontextmenu"))
        {
            ctxMenuShowing = true;
            drawContextMenuContent();
            ImGui::EndPopup();
        }

        if (m_Shared->isRenderHovered() && m_MaybeHover && (ctxMenuShowing ? m_MaybeHover.ID != m_MaybeOpenedContextMenu.ID : true))
        {
            drawHoverTooltip();
        }

        // draw overlays/gizmos
        drawSelection3DManipulatorGizmos();
        m_Shared->drawConnectionLines(m_MaybeHover);
    }

    void drawMainMenuFileMenu()
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem(ICON_FA_FILE " New", "Ctrl+N"))
            {
                m_Shared->requestNewMeshImporterTab();
            }

            ImGui::Separator();

            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Import", "Ctrl+O"))
            {
                m_Shared->openOsimFileAsModelGraph();
            }
            osc::DrawTooltipIfItemHovered("Import osim into mesh importer", "Try to import an existing osim file into the mesh importer.\n\nBEWARE: the mesh importer is *not* an OpenSim model editor. The import process will delete information from your osim in order to 'jam' it into this screen. The main purpose of this button is to export/import mesh editor scenes, not to edit existing OpenSim models.");

            if (ImGui::MenuItem(ICON_FA_SAVE " Export", "Ctrl+S"))
            {
                m_Shared->exportModelGraphAsOsimFile();
            }
            osc::DrawTooltipIfItemHovered("Export mesh impoter scene to osim", "Try to export the current mesh importer scene to an osim.\n\nBEWARE: the mesh importer scene may not map 1:1 onto an OpenSim model, so re-importing the scene *may* change a few things slightly. The main utility of this button is to try and save some progress in the mesh importer.");

            if (ImGui::MenuItem(ICON_FA_SAVE " Export As", "Shift+Ctrl+S"))
            {
                m_Shared->exportAsModelGraphAsOsimFile();
            }
            osc::DrawTooltipIfItemHovered("Export mesh impoter scene to osim", "Try to export the current mesh importer scene to an osim.\n\nBEWARE: the mesh importer scene may not map 1:1 onto an OpenSim model, so re-importing the scene *may* change a few things slightly. The main utility of this button is to try and save some progress in the mesh importer.");

            ImGui::Separator();

            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Import Stations from CSV"))
            {
                auto popup = std::make_shared<ImportStationsFromCSVPopup>(
                    "Import Stations from CSV",
                    m_Shared
                );
                popup->open();
                m_PopupManager.push_back(std::move(popup));
            }

            ImGui::Separator();

            if (ImGui::MenuItem(ICON_FA_TIMES " Close", "Ctrl+W"))
            {
                m_Shared->requestClose();
            }

            if (ImGui::MenuItem(ICON_FA_TIMES_CIRCLE " Quit", "Ctrl+Q"))
            {
                App::upd().requestQuit();
            }

            ImGui::EndMenu();
        }
    }

    void drawMainMenuEditMenu()
    {
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem(ICON_FA_UNDO " Undo", "Ctrl+Z", false, m_Shared->canUndoCurrentModelGraph()))
            {
                m_Shared->undoCurrentModelGraph();
            }
            if (ImGui::MenuItem(ICON_FA_REDO " Redo", "Ctrl+Shift+Z", false, m_Shared->canRedoCurrentModelGraph()))
            {
                m_Shared->redoCurrentModelGraph();
            }
            ImGui::EndMenu();
        }
    }

    void drawMainMenuWindowMenu()
    {

        if (ImGui::BeginMenu("Window"))
        {
            for (size_t i = 0; i < m_Shared->getNumToggleablePanels(); ++i)
            {
                bool isEnabled = m_Shared->isNthPanelEnabled(i);
                if (ImGui::MenuItem(m_Shared->getNthPanelName(i).c_str(), nullptr, isEnabled))
                {
                    m_Shared->setNthPanelEnabled(i, !isEnabled);
                }
            }
            ImGui::EndMenu();
        }
    }

    void drawMainMenuAboutMenu()
    {
        osc::MainMenuAboutTab{}.onDraw();
    }

    // draws main 3D viewer, or a modal (if one is active)
    void drawMainViewerPanelOrModal()
    {
        if (m_Maybe3DViewerModal)
        {
            // ensure it stays alive - even if it pops itself during the drawcall
            std::shared_ptr<Layer> const ptr = m_Maybe3DViewerModal;

            // open it "over" the whole UI as a "modal" - so that the user can't click things
            // outside of the panel
            ImGui::OpenPopup("##visualizermodalpopup");
            ImGui::SetNextWindowSize(m_Shared->get3DSceneDims());
            ImGui::SetNextWindowPos(m_Shared->get3DSceneRect().p1);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});

            ImGuiWindowFlags const modalFlags =
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoResize;

            if (ImGui::BeginPopupModal("##visualizermodalpopup", nullptr, modalFlags))
            {
                ImGui::PopStyleVar();
                ptr->onDraw();
                ImGui::EndPopup();
            }
            else
            {
                ImGui::PopStyleVar();
            }
        }
        else
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
            if (ImGui::Begin("wizard_3dViewer"))
            {
                ImGui::PopStyleVar();
                draw3DViewer();
                ImGui::SetCursorPos(Vec2{ImGui::GetCursorStartPos()} + Vec2{10.0f, 10.0f});
                draw3DViewerOverlay();
            }
            else
            {
                ImGui::PopStyleVar();
            }
            ImGui::End();
        }
    }

    // tab data
    UID m_TabID;
    ParentPtr<MainUIStateAPI> m_Parent;
    std::string m_Name = "MeshImporterTab";

    // data shared between states
    std::shared_ptr<SharedData> m_Shared;

    // buffer that's filled with drawable geometry during a drawcall
    std::vector<DrawableThing> m_DrawablesBuffer;

    // (maybe) hover + worldspace location of the hover
    Hover m_MaybeHover;

    // (maybe) the scene element that the user opened a context menu for
    Hover m_MaybeOpenedContextMenu;

    // (maybe) the next state the host screen should transition to
    std::shared_ptr<Layer> m_Maybe3DViewerModal;

    // ImGuizmo state
    struct {
        bool wasUsingLastFrame = false;
        Mat4 mtx = Identity<Mat4>();
        ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
        ImGuizmo::MODE mode = ImGuizmo::WORLD;
    } m_ImGuizmoState;

    // manager for active modal popups (importer popups, etc.)
    PopupManager m_PopupManager;
};


// public API (PIMPL)

osc::MeshImporterTab::MeshImporterTab(
    ParentPtr<MainUIStateAPI> const& parent_) :

    m_Impl{std::make_unique<Impl>(parent_)}
{
}

osc::MeshImporterTab::MeshImporterTab(
    ParentPtr<MainUIStateAPI> const& parent_,
    std::vector<std::filesystem::path> files_) :

    m_Impl{std::make_unique<Impl>(parent_, std::move(files_))}
{
}

osc::MeshImporterTab::MeshImporterTab(MeshImporterTab&&) noexcept = default;
osc::MeshImporterTab& osc::MeshImporterTab::operator=(MeshImporterTab&&) noexcept = default;
osc::MeshImporterTab::~MeshImporterTab() noexcept = default;

osc::UID osc::MeshImporterTab::implGetID() const
{
    return m_Impl->getID();
}

CStringView osc::MeshImporterTab::implGetName() const
{
    return m_Impl->getName();
}

bool osc::MeshImporterTab::implIsUnsaved() const
{
    return m_Impl->isUnsaved();
}

bool osc::MeshImporterTab::implTrySave()
{
    return m_Impl->trySave();
}

void osc::MeshImporterTab::implOnMount()
{
    m_Impl->onMount();
}

void osc::MeshImporterTab::implOnUnmount()
{
    m_Impl->onUnmount();
}

bool osc::MeshImporterTab::implOnEvent(SDL_Event const& e)
{
    return m_Impl->onEvent(e);
}

void osc::MeshImporterTab::implOnTick()
{
    m_Impl->onTick();
}

void osc::MeshImporterTab::implOnDrawMainMenu()
{
    m_Impl->drawMainMenu();
}

void osc::MeshImporterTab::implOnDraw()
{
    m_Impl->onDraw();
}
