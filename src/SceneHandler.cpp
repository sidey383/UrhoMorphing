#include "SceneHandler.h"
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/Graphics/Light.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Core/Context.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Octree.h>


using namespace Urho3D;

SharedPtr<Scene> CreateScene(Context* context, SharedPtr<Node>& cameraNode, float& yaw, float& pitch)
{
    auto* log = context->GetSubsystem<Log>();
    SharedPtr<Scene> scene(new Scene(context));
    scene->CreateComponent<Octree>();

    // Камера
    cameraNode = scene->CreateChild("Camera");
    cameraNode->CreateComponent<Camera>();
    cameraNode->SetDirection(Vector3::BACK);
    cameraNode->SetPosition(Vector3(0, 1, 10));
    yaw = cameraNode->GetRotation().YawAngle();
    pitch = cameraNode->GetRotation().PitchAngle();

    return scene;
}

void SetupLighting(SharedPtr<Scene>& scene)
{
    Node* lightNode = scene->CreateChild("Light");
    lightNode->SetDirection(Vector3::BACK); // или в сторону камеры
    lightNode->SetPosition(Vector3(0, -20, 20));
    Light* light = lightNode->CreateComponent<Light>();
    light->SetLightType(LIGHT_DIRECTIONAL);

    SharedPtr<Zone> zone(scene->CreateComponent<Zone>());
    zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));
    zone->SetAmbientColor(Color(0.4f, 0.4f, 0.4f));
    zone->SetFogColor(Color(0.2f, 0.2f, 0.2f));
    zone->SetFogStart(100.0f);
    zone->SetFogEnd(300.0f);

}
