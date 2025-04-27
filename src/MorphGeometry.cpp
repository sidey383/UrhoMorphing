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
    batches_[0].material_->SetShaderParameter("MorphWeight", morphWeight_);
}

Material* MorphGeometry::GetMaterial() {
    return batches_[0].material_;
}

void MorphGeometry::SetMorphWeight(float weight)
{
    morphWeight__ = weight;
}

void MorphGeometry::AddMorpher(Morpher morpher) {
    morphDeltasMap_[morpher.name] = morpher;
    if (activeMorph_.Empty()) {
        activeMorph_ = morpher.name;
    }
}

Vector<String> MorphGeometry::GetMorpherNames() {
    return morphDeltasMap_.Keys();
}

void MorphGeometry::SetActiveMorpher(String name) {
    if (morphDeltasMap_.Contains(name) || name.Empty()) {
        activeMorph_ = name;
    }
}

String MorphGeometry::GetActiveMorpher() {
    return activeMorph_;
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
    
    for (i32 i = 0; i < vertices_.Size(); ++i) {
        vertices_[i].morphDelta_ = Vector3::ZERO;
    }
    if (!activeMorph_.Empty()) {
        Morpher morpher = morphDeltasMap_[activeMorph_];

        log->Write(LOG_DEBUG, 
            String("Load morpher for gemoetry ") + morpher.name + 
            String(" with ") + String(morpher.indexes.Size()) + 
            String("Nodes")
        );

        for (i32 i = 0; i < morpher.indexes.Size(); ++i)
        {
            auto index = morpher.indexes[i];
            if (index < vertices_.Size()) {
                vertices_[index].morphDelta_ = morpher.morphDeltas[i];
            }
        }
    }

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

    PrimitiveTypes::i32 vertexCount = vertexBuffer_->GetVertexSize();

    // Обновление границ объекта для корректного отображения
    boundingBox_.Clear();
    for (const auto& vertex : vertices_)
        boundingBox_.Merge(vertex.position_);
    log->Write(LOG_INFO,"Bounding box local: min=" + boundingBox_.min_.ToString() + ", max=" + boundingBox_.max_.ToString());

}

void MorphGeometry::UpdateBatches(const FrameInfo& frame)
{
    Drawable::UpdateBatches(frame);
    Log* log = context_->GetSubsystem<Log>();
    // log->Write(LOG_DEBUG, String("MorphWeight=") + String(morphWeight_) + String(" ") + String(morphWeight__) + String(" For ") + String((unsigned long long) this) + String(" ") + GetNode()->GetName() + String(" Morpher: ") + GetActiveMorpher()) ;
    GetMaterial()->SetShaderParameter("MorphWeight", morphWeight_);
}

void MorphGeometry::UpdateGeometry(const FrameInfo& frame)
{
    time_ += frame.timeStep_;
    if (morphWeight__ != -1.0f) {
        morphWeight_ = morphWeight__;
    } else {
        morphWeight_ = (1 + sin(time_)) / 2;
    }
}

void MorphGeometry::OnWorldBoundingBoxUpdate()
{
    worldBoundingBox_ = boundingBox_.Transformed(node_->GetWorldTransform());
}

}
