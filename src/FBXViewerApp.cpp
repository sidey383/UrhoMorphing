#define INITGUID

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>

#include "FBXViewerApp.h"
#include "FBXLoader.h"
#include "SceneUtils.h"
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
    engineParameters_["LogName"] = "run.log";
}

void FBXViewerApp::Start() {
    GetSubsystem<Log>()->Write(LOG_INFO, "Logging system is active\n");

    CreateScene();
    SetupLighting();

    LogSceneContents(GetSubsystem<Log>(), scene_);

    Renderer* renderer = GetSubsystem<Renderer>();
    renderer->SetViewport(0, new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetDrawShadows(true);

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

void FBXViewerApp::CreateScene()
{
    auto* log = GetSubsystem<Log>();
    scene_ = SharedPtr<Scene>(new Scene(context_));
    scene_->CreateComponent<Octree>();

    // Камера
    cameraNode_ = scene_->CreateChild("Camera");
    cameraNode_->CreateComponent<Camera>();
    cameraNode_->SetDirection(Vector3::BACK);
    cameraNode_->SetPosition(Vector3(0, 1, 10));
    yaw_ = cameraNode_->GetRotation().YawAngle();
    pitch_ = cameraNode_->GetRotation().PitchAngle();

    SharedPtr<Node> fbxNode = LoadFBXToNode(context_, "FBXData/model.fbx");
    if (fbxNode)
        scene_->CreateChild("ImportedFBX")->AddChild(fbxNode);
    else
        GetSubsystem<Log>()->Write(LOG_ERROR, "Can't load model");    
}

void FBXViewerApp::SetupLighting()
{
    Node* lightNode = scene_->CreateChild("Light");
    lightNode->SetDirection(Vector3::BACK);
    lightNode->SetPosition(Vector3(0, 20, 20));
    Light* light = lightNode->CreateComponent<Light>();
    light->SetLightType(LIGHT_DIRECTIONAL);

    SharedPtr<Zone> zone(scene_->CreateComponent<Zone>());
    zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));
    zone->SetAmbientColor(Color(0.4f, 0.4f, 0.4f));
    zone->SetFogColor(Color(0.2f, 0.2f, 0.2f));
    zone->SetFogStart(100.0f);
    zone->SetFogEnd(300.0f);

}

URHO3D_DEFINE_APPLICATION_MAIN(FBXViewerApp)