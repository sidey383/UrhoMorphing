#pragma once

#include <Urho3D/Graphics/Drawable.h>
#include <Urho3D/Graphics/Geometry.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/GraphicsAPI/VertexBuffer.h>
#include <Urho3D/GraphicsAPI/IndexBuffer.h>
#include <Urho3D/Container/Vector.h>
#include <Urho3D/Math/Vector3.h>
#include <Urho3D/Math/Vector2.h>
#include <Urho3D/Math/Vector4.h>

namespace Urho3D
{

struct MorphVertex
{
    Vector3 position_;
    Vector3 normal_;
    Vector2 texCoord_;
    Vector4 tangent_;
    Vector3 morphDelta_;
};

class MorphGeometry : public Drawable
{
    URHO3D_OBJECT(MorphGeometry, Drawable);

public:
    explicit MorphGeometry(Context* context);
    ~MorphGeometry() override = default;

    void SetVertices(const Vector<MorphVertex>& vertices);
    void SetIndices(const Vector<i32>& indices);
    void SetMaterial(Material* material);
    void SetMorphWeight(float weight);

    void Commit();

protected:
    void UpdateBatches(const FrameInfo& frame) override;
    void UpdateGeometry(const FrameInfo& frame) override;
    // void SetGeometryData();
    void OnWorldBoundingBoxUpdate() override;

protected:
    SharedPtr<VertexBuffer> vertexBuffer_;
    SharedPtr<IndexBuffer> indexBuffer_;
    SharedPtr<Material> material_;
    SharedPtr<Geometry> geometry_;
private:
    Vector<MorphVertex> vertices_;
    Vector<i32> indices_;
    float morphWeight_;
};

}
