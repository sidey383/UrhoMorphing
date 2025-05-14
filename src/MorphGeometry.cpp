#include "MorphGeometry.h"
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Technique.h>
#include <Urho3D/GraphicsAPI/ShaderVariation.h>
#include <Urho3D/GraphicsAPI/Texture2D.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Container/Vector.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Core/Context.h>
#include <Urho3D/Scene/ValueAnimation.h>

#include <cmath>

#define FIRST_MORPH_NUMBER 2

inline void* GetMorphVertexPtr(void* buffer, size_t nodeNumber) {
    return (void*) (
        reinterpret_cast<uint8_t*>(buffer) +(
            (sizeof(Urho3D::MorphVertex) + sizeof(Urho3D::Vector3) * TOTAL_MORPH_COUNT) * nodeNumber
        )
    );
}

inline void* GetMorphDeltaPtr(void* buffer, size_t nodeNumber, size_t morphNumber) {
    return (void*) (
        reinterpret_cast<uint8_t*>(buffer) + (
           (sizeof(Urho3D::MorphVertex) + sizeof(Urho3D::Vector3) * TOTAL_MORPH_COUNT) * nodeNumber +
           sizeof(Urho3D::MorphVertex) + sizeof(Urho3D::Vector3) * morphNumber
        )
    );
}


namespace Urho3D
{

MorphGeometry::MorphGeometry(Context* context) : Drawable(context, DrawableTypes::Geometry)
{
    geometry_ = new Geometry(context_);
    vertexBuffer_ = new VertexBuffer(context_);
    indexBuffer_ = new IndexBuffer(context_);
    batches_.Resize(1);
}

UpdateGeometryType MorphGeometry::GetUpdateGeometryType() {
    return UpdateGeometryType::UPDATE_MAIN_THREAD;
}

void MorphGeometry::SetVertices(const Vector<MorphVertex>& vertices)
{
    vertices_ = vertices;
}

void MorphGeometry::SetIndices(const Vector<i32>& indices)
{
    indices_ = indices;
}

void MorphGeometry::SetMaterial(Material* material)
{
    material_ = material;
    batches_[0].material_ = material_;
    for (i32 i = 0; i < TOTAL_MORPH_COUNT; i++) {
        batches_[0].material_->SetShaderParameter("MorphWeights" + String(i), morphWeights_[i]);
    }
    batches_[0].material_->SetShaderParameter("MorphCount", morphCount_);
}

Material* MorphGeometry::GetMaterial() {
    return batches_[0].material_;
}

void MorphGeometry::SetMorphWeight(String name, float weight)
{
    i32 index = activeMorphs_.IndexOf(name);
    Log* log = context_->GetSubsystem<Log>();
    if (index >= 0 && index < activeMorphs_.Size() && index < morphWeights_.Size()) {
        morphWeights_[index] = weight;
    }
}

void MorphGeometry::AddMorpher(Morpher morpher) {
    if (morpherOrder_.Size() >= TOTAL_MORPH_COUNT) return;
    bool addOrder = !morphDeltasMap_.Contains(morpher.name);
    morphDeltasMap_[morpher.name] = morpher;
    if (addOrder) morpherOrder_.Push(morpher.name);
    activeMorph_ = morpherOrder_.IndexOf(morpher.name);
}

Vector<String> MorphGeometry::GetMorpherNames() {
    return Vector<String>(activeMorphs_);
}

Morpher& MorphGeometry::getMorper(String name) {
    return morphDeltasMap_[name];
}

String MorphGeometry::GetActiveMorpher() {
    if (activeMorph_ >= 0 && activeMorph_ < morpherOrder_.Size())
        return morpherOrder_[activeMorph_];
    return String();
}

void MorphGeometry::Commit()
{
    assert(!vertices_.Empty());
    assert(!indices_.Empty());

    Log* log = context_->GetSubsystem<Log>();
    log->Write(LOG_INFO, "MorphGeometry::Commit");
    log->Write(LOG_INFO, String("vertices_ size: ") + String(vertices_.Size()));
    log->Write(LOG_INFO, String("indices_ size: ") + String(vertices_.Size()));
    // создание вершинного буфера
    CommitVertexData();

    // Создание и настройка IndexBuffer
    indexBuffer_ = new IndexBuffer(context_);
    indexBuffer_->SetShadowed(true);
    bool use32bit = sizeof(indices_[0]) == 4;
    indexBuffer_->SetSize(indices_.Size(), use32bit);
    indexBuffer_->SetData(indices_.Buffer());

    // Создание и настройка Geometry
    geometry_ = new Geometry(context_);
    geometry_->SetVertexBuffer(0, vertexBuffer_);
    geometry_->SetIndexBuffer(indexBuffer_);
    geometry_->SetDrawRange(TRIANGLE_LIST, 0, indices_.Size(), 0, vertices_.Size());
    batches_[0].geometry_ = geometry_;

    log->Write(LOG_INFO, String("vertexBuffer_ size: ") + String(vertexBuffer_->GetVertexSize()));
    log->Write(LOG_INFO, String("indexBuffer_ size: ") + String(indexBuffer_->GetIndexSize()));
    log->Write(LOG_INFO, String("indexBuffer_ size: ") + String(indexBuffer_->GetIndexSize()));

    PrimitiveTypes::i32 vertexCount = vertexBuffer_->GetVertexSize();

    // Обновление границ объекта для корректного отображения
    boundingBox_.Clear();
    for (const auto& vertex : vertices_)
        boundingBox_.Merge(vertex.position_);
    MarkForUpdate();
    log->Write(LOG_INFO,"Bounding box local: min=" + boundingBox_.min_.ToString() + ", max=" + boundingBox_.max_.ToString());

}

void MorphGeometry::CommitVertexData() {
    Vector<VertexElement> elements;
    morphCount_ = morpherOrder_.Size();
    activeMorphs_ = morpherOrder_;
    elements.Push(VertexElement(TYPE_VECTOR3, SEM_POSITION, 0));
    elements.Push(VertexElement(TYPE_VECTOR3, SEM_NORMAL));
    elements.Push(VertexElement(TYPE_VECTOR2, SEM_TEXCOORD));
    elements.Push(VertexElement(TYPE_VECTOR4, SEM_TANGENT));
    // Добавление морфируемых вершин
    for (i32 i = 0; i < TOTAL_MORPH_COUNT; i++) {
        elements.Push(VertexElement(TYPE_VECTOR3, SEM_TEXCOORD, FIRST_MORPH_NUMBER + i));
    }
    i32 vertexSize = sizeof(MorphVertex) + sizeof(Vector3) * TOTAL_MORPH_COUNT;
    vertexData_.Resize(vertexSize * vertices_.Size());

    // Инициаизация базовых значений вершин
    float* buffer = vertexData_.Buffer();
    MorphVertex* verticesBase = vertices_.Buffer();
    for (i32 i = 0; i < vertices_.Size(); ++i) {
        void* dst = GetMorphVertexPtr(buffer, i);
        MorphVertex* src = verticesBase + i;
        memcpy(dst, src, sizeof(MorphVertex));
        for (i32 j = 0; j < TOTAL_MORPH_COUNT; ++j) {
            *reinterpret_cast<Vector3*>(GetMorphDeltaPtr(buffer, i, j)) = Vector3::ZERO;
        }
    }

    // Запись весов для морфинга
    for (int i = 0; i < morphCount_; ++i) {
        auto morphName = activeMorphs_[i];
        Morpher morpher = morphDeltasMap_[morphName];
        for (i32 j = 0; j < morpher.indexes.Size(); ++j) {
            auto index = morpher.indexes[j];
            if (index < vertices_.Size()) {
                *reinterpret_cast<Vector3*>(GetMorphDeltaPtr(buffer, index, i)) = morpher.morphDeltas[j];
            }
        }
    }
    vertexBuffer_ = new VertexBuffer(context_);
    vertexBuffer_->SetShadowed(true);
    vertexBuffer_->SetSize(vertices_.Size(), elements);
    vertexBuffer_->SetData(buffer);
}

void MorphGeometry::UpdateBatches(const FrameInfo& frame)
{
    Drawable::UpdateBatches(frame);
    Log* log = context_->GetSubsystem<Log>();
    for (i32 i = 0; i < TOTAL_MORPH_COUNT; i++) {
        GetMaterial()->SetShaderParameter("MorphWeights" + String(i), morphWeights_[i]);
        Vector3 center;
        Vector3 axis;
        if (i < morpherOrder_.Size()) {
            Morpher& m = morphDeltasMap_[morpherOrder_[i]];
            center = m.center;
            axis = m.axis;
        } else {
            center = Vector3::ZERO;
            axis = Vector3::ZERO;
        }
        GetMaterial()->SetShaderParameter("MorphAxis" + String(i), axis);
        GetMaterial()->SetShaderParameter("MorphCenter" + String(i), center);
    }
    GetMaterial()->SetShaderParameter("MorphCount", morphCount_);
}

void MorphGeometry::UpdateGeometry(const FrameInfo& frame)
{
    time_ += frame.timeStep_;
}

void MorphGeometry::OnWorldBoundingBoxUpdate()
{
    Log* log = context_->GetSubsystem<Log>();
    worldBoundingBox_ = boundingBox_.Transformed(node_->GetWorldTransform());
    log->Write(LOG_INFO,"Update bounding box local: min=" + boundingBox_.min_.ToString() + ", max=" + boundingBox_.max_.ToString());
    log->Write(LOG_INFO,"Update bounding box world: min=" + worldBoundingBox_.min_.ToString() + ", max=" + worldBoundingBox_.max_.ToString());
}

}
