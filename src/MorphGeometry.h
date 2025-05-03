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

#define TOTAL_MORPH_COUNT 11

namespace Urho3D
{

struct MorphVertex
{
    Vector3 position_;
    Vector3 normal_;
    Vector2 texCoord_;
    Vector4 tangent_;
};

struct Morpher 
{
    String name;
    Vector<i32> indexes;
    Vector<Vector3> morphDeltas;
    Vector3 center;
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
    void SetMorphWeight(String name, float weight);
    void AddMorpher(Morpher morpher);
    Morpher& getMorper(String name);
    Vector<String> GetMorpherNames();
    String GetActiveMorpher();

    void Commit();

protected:
    void UpdateBatches(const FrameInfo& frame) override;
    void UpdateGeometry(const FrameInfo& frame) override;
    UpdateGeometryType GetUpdateGeometryType() override;
    void OnWorldBoundingBoxUpdate() override;

protected:
    SharedPtr<VertexBuffer> vertexBuffer_;
    SharedPtr<IndexBuffer> indexBuffer_;
    SharedPtr<Material> material_;
    SharedPtr<Geometry> geometry_;
    Vector<String> morpherOrder_;
    HashMap<String, Morpher> morphDeltasMap_;
    i32 activeMorph_;
    SharedPtr<Texture2D> morphTexture_;
private:
    void CommitVertexData();

    Vector<float> vertexData_;
    Vector<MorphVertex> vertices_;
    Vector<String> activeMorphs_;
    Vector<i32> indices_;
    Vector<float> morphWeights_ = Vector(TOTAL_MORPH_COUNT, 0.0f);
    i32 morphCount_ = 0;
    float time_;
};

}
