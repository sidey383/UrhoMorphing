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

struct Morpher 
{
    String name;
    Vector<i32> indexes;
    Vector<Vector3> morphDeltas;
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
    Material* GetMaterial();
    void SetMorphWeight(float weight);
    void AddMorpher(Morpher morpher);
    Vector<String> GetMorpherNames();
    void SetActiveMorpher(String name);
    String GetActiveMorpher();

    void Commit();

protected:
    void UpdateBatches(const FrameInfo& frame) override;
    void UpdateGeometry(const FrameInfo& frame) override;
    UpdateGeometryType GetUpdateGeometryType() override;
    // void SetGeometryData();
    void OnWorldBoundingBoxUpdate() override;

protected:
    SharedPtr<VertexBuffer> vertexBuffer_;
    SharedPtr<IndexBuffer> indexBuffer_;
    SharedPtr<Material> material_;
    SharedPtr<Geometry> geometry_;
    Urho3D::HashMap<String, Morpher> morphDeltasMap_;
    String activeMorph_;
    SharedPtr<Texture2D> morphTexture_;
private:
    Vector<MorphVertex> vertices_;
    Vector<i32> indices_;
    float time_;
    float morphWeight_ = 1;
    float morphWeight__ = -1.0f;
};

}
