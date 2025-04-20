#pragma once
#include <Urho3D/Engine/Application.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/Node.h>

namespace Urho3D {
    class Camera;
}

class FBXViewerApp : public Urho3D::Application {
    URHO3D_OBJECT(FBXViewerApp, Application);

public:
    explicit FBXViewerApp(Urho3D::Context* context);
    void Setup() override;
    void Start() override;

private:
    void MoveCamera(float timeStep);
    void HandleUpdate(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);
    void HandleKeyDown(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);
    void SetupLighting();
    void CreateScene();

    Urho3D::SharedPtr<Urho3D::Scene> scene_;
    Urho3D::SharedPtr<Urho3D::Node> cameraNode_;
    float yaw_{};
    float pitch_{};
};
