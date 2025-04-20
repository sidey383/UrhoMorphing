#pragma once
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/Node.h>

Urho3D::SharedPtr<Urho3D::Scene> CreateScene(Urho3D::Context* context, Urho3D::SharedPtr<Urho3D::Node>& cameraNode, float& yaw, float& pitch);
void SetupLighting(Urho3D::SharedPtr<Urho3D::Scene>& scene);
