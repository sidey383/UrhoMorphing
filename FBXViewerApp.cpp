#define INITGUID

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>

#include "FBXViewerApp.h"
#include "SceneHandler.h"
#include "FBXLoader.h"
#include <Urho3D/Input/Input.h>
#include <Urho3D/Input/Controls.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Engine/Application.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Graphics/CustomGeometry.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Light.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/RenderPath.h>
#include <Urho3D/IO/Log.h>  

using namespace Urho3D;

FBXViewerApp::FBXViewerApp(Context* context) : Application(context) {}

void FBXViewerApp::Setup() {
    engineParameters_["WindowTitle"] = "FBX Viewer with Urho3D";
    engineParameters_["FullScreen"] = false;
    engineParameters_["WindowWidth"] = 1280;
    engineParameters_["WindowHeight"] = 720;
    engineParameters_["LogName"] = "mylog.txt";
}

void FBXViewerApp::Start() {
    GetSubsystem<Log>()->Write(LOG_INFO, "Logging system is active\n");

    scene_ = CreateScene(context_, cameraNode_, yaw_, pitch_);
    SetupLighting(scene_);

    Renderer* renderer = GetSubsystem<Renderer>();
    renderer->SetViewport(0, new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetDrawShadows(true);

    SharedPtr<Node> fbxNode = LoadFBXToNode(context_, "FBXData/model.fbx");
    if (fbxNode)
        scene_->CreateChild("ImportedFBX")->AddChild(fbxNode);
    else
        GetSubsystem<Log>()->Write(LOG_ERROR, "Can't load model");

    Input* input = GetSubsystem<Input>();
    input->SetMouseVisible(false);
    input->SetMouseGrabbed(true);

    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(FBXViewerApp, HandleUpdate));
    SubscribeToEvent(E_KEYDOWN, URHO3D_HANDLER(FBXViewerApp, HandleKeyDown));
}

void FBXViewerApp::MoveCamera(float timeStep) {
    auto* input = GetSubsystem<Input>();
    const float MOUSE_SENSITIVITY = 0.1f;
    const float MOVE_SPEED = 5.0f;

    if (input->GetMouseButtonDown(MOUSEB_RIGHT)) {
        IntVector2 move = input->GetMouseMove();
        yaw_ += MOUSE_SENSITIVITY * move.x_;
        pitch_ = Clamp(pitch_ + MOUSE_SENSITIVITY * move.y_, -90.0f, 90.0f);
        cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));
    }

    Vector3 dir;
    if (input->GetKeyDown(KEY_W)) dir += Vector3::FORWARD;
    if (input->GetKeyDown(KEY_S)) dir += Vector3::BACK;
    if (input->GetKeyDown(KEY_A)) dir += Vector3::LEFT;
    if (input->GetKeyDown(KEY_D)) dir += Vector3::RIGHT;
    if (input->GetKeyDown(KEY_Q)) dir += Vector3::DOWN;
    if (input->GetKeyDown(KEY_E)) dir += Vector3::UP;

    cameraNode_->Translate(dir * MOVE_SPEED * timeStep);
}

void FBXViewerApp::HandleUpdate(StringHash, VariantMap& eventData) {
    using namespace Update;
    MoveCamera(eventData[P_TIMESTEP].GetFloat());
}

void FBXViewerApp::HandleKeyDown(StringHash, VariantMap& eventData) {
    using namespace KeyDown;
    if (eventData[P_KEY].GetI32() == KEY_ESCAPE)
        engine_->Exit();
}

URHO3D_DEFINE_APPLICATION_MAIN(FBXViewerApp)