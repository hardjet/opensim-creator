#include "SceneHelpers.hpp"

#include <oscar/Graphics/AntiAliasingLevel.hpp>
#include <oscar/Graphics/Color.hpp>
#include <oscar/Graphics/Mesh.hpp>
#include <oscar/Graphics/MeshCache.hpp>
#include <oscar/Graphics/MeshIndicesView.hpp>
#include <oscar/Graphics/MeshTopology.hpp>
#include <oscar/Graphics/ShaderCache.hpp>
#include <oscar/Maths/AABB.hpp>
#include <oscar/Maths/BVH.hpp>
#include <oscar/Maths/Constants.hpp>
#include <oscar/Maths/Line.hpp>
#include <oscar/Maths/MathHelpers.hpp>
#include <oscar/Maths/PolarPerspectiveCamera.hpp>
#include <oscar/Maths/RayCollision.hpp>
#include <oscar/Maths/Rect.hpp>
#include <oscar/Maths/Segment.hpp>
#include <oscar/Maths/Transform.hpp>
#include <oscar/Platform/AppConfig.hpp>
#include <oscar/Scene/SceneDecoration.hpp>
#include <oscar/Scene/SceneRendererParams.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

namespace
{
    void DrawGrid(
        osc::MeshCache& cache,
        glm::quat const& rotation,
        std::function<void(osc::SceneDecoration&&)> const& out)
    {
        osc::Mesh const grid = cache.get100x100GridMesh();

        osc::Transform t;
        t.scale *= glm::vec3{50.0f, 50.0f, 1.0f};
        t.rotation = rotation;

        osc::Color const color = {0.7f, 0.7f, 0.7f, 0.15f};

        out(osc::SceneDecoration{grid, t, color});
    }
}

void osc::DrawBVH(
    MeshCache& cache,
    BVH const& sceneBVH,
    std::function<void(SceneDecoration&&)> const& out)
{
    sceneBVH.forEachLeafOrInnerNodeUnordered([cube = cache.getCubeWireMesh(), &out](BVHNode const& node)
    {
        Transform t;
        t.scale *= 0.5f * Dimensions(node.getBounds());
        t.position = Midpoint(node.getBounds());
        out(SceneDecoration{cube, t, Color::black()});
    });
}

void osc::DrawAABB(
    MeshCache& cache,
    AABB const& aabb,
    std::function<void(SceneDecoration&&)> const& out)
{
    Mesh const cube = cache.getCubeWireMesh();

    Transform t;
    t.scale = 0.5f * Dimensions(aabb);
    t.position = Midpoint(aabb);

    out(SceneDecoration{cube, t, Color::black()});
}

void osc::DrawAABBs(
    MeshCache& cache,
    nonstd::span<AABB const> aabbs,
    std::function<void(SceneDecoration&&)> const& out)
{
    Mesh const cube = cache.getCubeWireMesh();

    for (AABB const& aabb : aabbs)
    {
        Transform t;
        t.scale = 0.5f * Dimensions(aabb);
        t.position = Midpoint(aabb);

        out(SceneDecoration{cube, t, Color::black()});
    }
}

void osc::DrawBVHLeafNodes(
    MeshCache& cache,
    BVH const& bvh,
    std::function<void(SceneDecoration&&)> const& out)
{
    bvh.forEachLeafNode([&cache, &out](BVHNode const& node)
    {
        DrawAABB(cache, node.getBounds(), out);
    });
}

void osc::DrawXZFloorLines(
    MeshCache& cache,
    std::function<void(SceneDecoration&&)> const& out,
    float scale)
{
    Mesh const yLine = cache.getYLineMesh();

    // X line
    {
        Transform t;
        t.scale *= scale;
        t.rotation = glm::angleAxis(fpi2, glm::vec3{0.0f, 0.0f, 1.0f});

        out(SceneDecoration{yLine, t, Color::red()});
    }

    // Z line
    {
        Transform t;
        t.scale *= scale;
        t.rotation = glm::angleAxis(fpi2, glm::vec3{1.0f, 0.0f, 0.0f});

        out(SceneDecoration{yLine, t, Color::blue()});
    }
}

void osc::DrawXZGrid(
    MeshCache& cache,
    std::function<void(SceneDecoration&&)> const& out)
{
    glm::quat const rotation = glm::angleAxis(fpi2, glm::vec3{1.0f, 0.0f, 0.0f});
    DrawGrid(cache, rotation, out);
}

void osc::DrawXYGrid(
    MeshCache& cache,
    std::function<void(SceneDecoration&&)> const& out)
{
    auto const rotation = glm::identity<glm::quat>();
    DrawGrid(cache, rotation, out);
}

void osc::DrawYZGrid(
    MeshCache& cache,
    std::function<void(SceneDecoration&&)> const& out)
{
    glm::quat const rotation = glm::angleAxis(fpi2, glm::vec3{0.0f, 1.0f, 0.0f});
    DrawGrid(cache, rotation, out);
}

osc::ArrowProperties::ArrowProperties() :
    worldspaceStart{},
    worldspaceEnd{},
    tipLength{},
    neckThickness{},
    headThickness{},
    color{Color::black()}
{
}

void osc::DrawArrow(
    MeshCache& cache,
    ArrowProperties const& props,
    std::function<void(SceneDecoration&&)> const& out)
{
    glm::vec3 startToEnd = props.worldspaceEnd - props.worldspaceStart;
    float const len = glm::length(startToEnd);
    glm::vec3 const dir = startToEnd/len;

    glm::vec3 const neckStart = props.worldspaceStart;
    glm::vec3 const neckEnd = props.worldspaceStart + (len - props.tipLength)*dir;
    glm::vec3 const headStart = neckEnd;
    glm::vec3 const headEnd = props.worldspaceEnd;

    // emit neck cylinder
    Transform const neckXform = YToYCylinderToSegmentTransform({neckStart, neckEnd}, props.neckThickness);
    out(SceneDecoration{cache.getCylinderMesh(), neckXform, props.color});

    // emit head cone
    Transform const headXform = YToYCylinderToSegmentTransform({headStart, headEnd}, props.headThickness);
    out(SceneDecoration{cache.getConeMesh(), headXform, props.color});
}

void osc::DrawLineSegment(
    MeshCache& cache,
    Segment const& segment,
    Color const& color,
    float radius,
    std::function<void(SceneDecoration&&)> const& out)
{
    Transform const cylinderXform = YToYCylinderToSegmentTransform(segment, radius);
    out(SceneDecoration{cache.getCylinderMesh(), cylinderXform, color});
}

osc::AABB osc::GetWorldspaceAABB(SceneDecoration const& cd)
{
    return TransformAABB(cd.mesh.getBounds(), cd.transform);
}

void osc::UpdateSceneBVH(nonstd::span<SceneDecoration const> sceneEls, BVH& bvh)
{
    std::vector<AABB> aabbs;
    aabbs.reserve(sceneEls.size());
    for (SceneDecoration const& el : sceneEls)
    {
        aabbs.push_back(GetWorldspaceAABB(el));
    }

    bvh.buildFromAABBs(aabbs);
}

std::vector<osc::SceneCollision> osc::GetAllSceneCollisions(
    BVH const& bvh,
    nonstd::span<SceneDecoration const> decorations,
    Line const& ray)
{
    // use scene BVH to intersect the ray with the scene
    std::vector<BVHCollision> const sceneCollisions = bvh.getRayAABBCollisions(ray);

    // perform ray-triangle intersections tests on the scene hits
    std::vector<SceneCollision> rv;
    for (BVHCollision const& c : sceneCollisions)
    {
        SceneDecoration const& decoration = decorations[c.id];
        std::optional<RayCollision> const maybeCollision = GetClosestWorldspaceRayCollision(decoration.mesh, decoration.transform, ray);

        if (maybeCollision)
        {
            rv.emplace_back(decoration.id, static_cast<size_t>(c.id), maybeCollision->position, maybeCollision->distance);
        }
    }
    return rv;
}

std::optional<osc::RayCollision> osc::GetClosestWorldspaceRayCollision(
    Mesh const& mesh,
    Transform const& transform,
    Line const& worldspaceRay)
{
    if (mesh.getTopology() != MeshTopology::Triangles)
    {
        return std::nullopt;
    }

    // map the ray into the mesh's modelspace, so that we compute a ray-mesh collision
    Line const modelspaceRay = InverseTransformLine(worldspaceRay, transform);

    MeshIndicesView const indices = mesh.getIndices();
    std::optional<BVHCollision> const maybeCollision = indices.isU16() ?
        mesh.getBVH().getClosestRayIndexedTriangleCollision(mesh.getVerts(), indices.toU16Span(), modelspaceRay) :
        mesh.getBVH().getClosestRayIndexedTriangleCollision(mesh.getVerts(), indices.toU32Span(), modelspaceRay);

    if (maybeCollision)
    {
        // map the ray back into worldspace
        glm::vec3 const locationWorldspace = transform * maybeCollision->position;
        float const distance = glm::length(locationWorldspace - worldspaceRay.origin);
        return RayCollision{distance, locationWorldspace};
    }
    else
    {
        return std::nullopt;
    }
}

std::optional<osc::RayCollision> osc::GetClosestWorldspaceRayCollision(
    PolarPerspectiveCamera const& camera,
    Mesh const& mesh,
    Rect const& renderScreenRect,
    glm::vec2 mouseScreenPos)
{
    Line const ray = camera.unprojectTopLeftPosToWorldRay(
        mouseScreenPos - renderScreenRect.p1,
        Dimensions(renderScreenRect)
    );

    return osc::GetClosestWorldspaceRayCollision(
        mesh,
        Transform{},
        ray
    );
}

osc::SceneRendererParams osc::CalcStandardDarkSceneRenderParams(
    PolarPerspectiveCamera const& camera,
    AntiAliasingLevel antiAliasingLevel,
    glm::vec2 renderDims)
{
    osc::SceneRendererParams rv;
    rv.dimensions = renderDims;
    rv.antiAliasingLevel = antiAliasingLevel;
    rv.drawMeshNormals = false;
    rv.drawFloor = false;
    rv.viewMatrix = camera.getViewMtx();
    rv.projectionMatrix = camera.getProjMtx(AspectRatio(renderDims));
    rv.viewPos = camera.getPos();
    rv.lightDirection = RecommendedLightDirection(camera);
    rv.backgroundColor = {0.1f, 0.1f, 0.1f, 1.0f};
    return rv;
}

osc::Material osc::CreateWireframeOverlayMaterial(
    AppConfig const& config,
    ShaderCache& cache)
{
    std::filesystem::path const vertShader = config.getResourceDir() / "oscar/shaders/SceneRenderer/SolidColor.vert";
    std::filesystem::path const fragShader = config.getResourceDir() / "oscar/shaders/SceneRenderer/SolidColor.frag";
    Material material{cache.load(vertShader, fragShader)};
    material.setColor("uDiffuseColor", {0.0f, 0.0f, 0.0f, 0.6f});
    material.setWireframeMode(true);
    material.setTransparent(true);
    return material;
}