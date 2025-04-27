#pragma once
#include "MorphGeometry.h"
#include <Urho3D/Engine/Application.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/LineEdit.h>
#include <Urho3D/UI/Button.h>

namespace Urho3D {
    class Camera;
}

struct InteractModeInfo
{
    bool mouseVisible;
    bool showUI;
    bool cameraActive;
};

static const InteractModeInfo interactModeInfos[] = {
    { false, false, true },
    { false, true, true },
    { true, true, false },
};

enum InteractMode {
    INTERACT_CAMERA_MODE,
    INTERACT_UI_MODE,
    INTERACT_CAMERA_NOUI_MODE,

};

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
    void HandleSliderChanged(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);
    void HandleDropDownListChanged(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);
    void HandleApplyCameraPosition(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);
    void SetupLighting();
    void CreateScene();
    void RegisterAllComponents();
    void CreateUI(Urho3D::MorphGeometry* geometry);
    void CreateCameraUI();
    void SetupCamera();
    void SetInteractMode(int num);
    int GetInteractModeNum();
    const InteractModeInfo* GetInteractMode();

    Urho3D::SharedPtr<Urho3D::Scene> scene_;
    Urho3D::SharedPtr<Urho3D::Node> cameraNode_;
    Urho3D::SharedPtr<Urho3D::LineEdit> cameraPosX_;
    Urho3D::SharedPtr<Urho3D::LineEdit> cameraPosY_;
    Urho3D::SharedPtr<Urho3D::LineEdit> cameraPosZ_;
    Urho3D::SharedPtr<Urho3D::Button> applyCameraButton_;
    Urho3D::SharedPtr<Urho3D::Text> cameraPositionText_;
    float yaw_{};
    float pitch_{};
    float moveSpeed_ = 1.0f;
    const InteractModeInfo* interactMode = interactModeInfos;
};
