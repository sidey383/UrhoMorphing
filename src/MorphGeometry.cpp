#include "MorphGeometry.h"
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Technique.h>
#include <Urho3D/GraphicsAPI/ShaderVariation.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Container/Vector.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Core/Context.h>

namespace Urho3D
{

MorphGeometry::MorphGeometry(Context* context) : Drawable(context, DrawableTypes::Geometry),
      morphWeight_(0.0f)
{
    geometry_ = new Geometry(context_);
    vertexBuffer_ = new VertexBuffer(context_);
    indexBuffer_ = new IndexBuffer(context_);
    batches_.Resize(1);
    batches_[0].geometryType_ = GEOM_STATIC;
}

// Geometry* GetLodGeometry(i32 batchIndex, i32 level)
// {
//     return geometry_;
// }

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
}

void MorphGeometry::SetMorphWeight(float weight)
{
    morphWeight_ = weight;
}

void MorphGeometry::Commit()
{
    assert(!vertices_.Empty());
    assert(!indices_.Empty());

    Log* log = context_->GetSubsystem<Log>();
    log->Write(LOG_INFO, "MorphGeometry::Commit");
    log->Write(LOG_INFO, String("vertices_ size: ") + String(vertices_.Size()));
    log->Write(LOG_INFO, String("indices_ size: ") + String(vertices_.Size()));
    // Определение формата вершин
    Vector<VertexElement> elements;
    elements.Push(VertexElement(TYPE_VECTOR3, SEM_POSITION));
    elements.Push(VertexElement(TYPE_VECTOR3, SEM_NORMAL));
    elements.Push(VertexElement(TYPE_VECTOR2, SEM_TEXCOORD));
    elements.Push(VertexElement(TYPE_VECTOR4, SEM_TANGENT));
    elements.Push(VertexElement(TYPE_VECTOR3, SEM_TEXCOORD, 1)); // Используем второй набор текстурных координат для morphDelta

    // Создание и настройка VertexBuffer
    vertexBuffer_ = new VertexBuffer(context_);
    vertexBuffer_->SetShadowed(true);
    vertexBuffer_->SetSize(vertices_.Size(), elements);
    vertexBuffer_->SetData(vertices_.Buffer());

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
    // Обновление границ объекта для корректного отображения
    boundingBox_.Clear();
    for (const auto& vertex : vertices_)
        boundingBox_.Merge(vertex.position_);
}

void MorphGeometry::UpdateBatches(const FrameInfo& frame)
{
    Log* log = context_->GetSubsystem<Log>();
    // Обновление параметров шейдера
    if (batches_[0].material_) {
        batches_[0].material_->SetShaderParameter("uMorphWeight", morphWeight_);
    }


    Drawable::UpdateBatches(frame);
}

void MorphGeometry::UpdateGeometry(const FrameInfo& frame)
{
    // Здесь можно добавить обновление геометрии, если необходимо
}

void MorphGeometry::OnWorldBoundingBoxUpdate()
{
    worldBoundingBox_.Define(vertices_.Empty() ? BoundingBox() : BoundingBox(vertices_.Front().position_, vertices_.Front().position_));
    for (i32 i = 1; i < vertices_.Size(); ++i)
        worldBoundingBox_.Merge(vertices_[i].position_);
}

}
