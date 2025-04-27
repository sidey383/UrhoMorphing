#pragma once
#include <Urho3D/IO/Log.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Container/Vector.h>

void LogSceneContents(Urho3D::Log* log, Urho3D::Scene* scene);

template<typename T>
void collectAll(Urho3D::Vector<T*>& container, Urho3D::Node* node) {

    if (node->HasComponent<T>()) {
        container.Push(node->GetComponent<T>());
    }
    for (Node* child : node->GetChildren())
        collectAll(container, child);
}


template<typename T> 
Urho3D::Vector<T*> findAllComponents(Urho3D::Scene* scene)
{
    if (!scene) return Vector<T*>();
    Urho3D::Vector<T*> container;
    collectAll<T>(container, scene);
    return container;
}