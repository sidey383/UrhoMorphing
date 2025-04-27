#define INITGUID

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>

#include "FBXViewerApp.h"
#include "FBXLoader.h"
#include "SceneUtils.h"
#include "MorphGeometry.h"
#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/Slider.h>
#include <Urho3D/UI/DropDownList.h>
#include <Urho3D/UI/UIEvents.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Input/Controls.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Engine/Application.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Graphics/Graphics.h>
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
    engineParameters_["LogLe.vel"] = LOG_DEBUG;
    engineParameters_["WindowResizable"] = true;
    RegisterAllComponents();
    context_->GetSubsystem<ResourceCache>()->AddResourceDir("Resources/CustomData");
}

void FBXViewerApp::Start() 
{
    CreateScene();
    SetupLighting();

    auto* ui = GetSubsystem<UI>();
    auto* cache = GetSubsystem<ResourceCache>();
    auto* style = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");
    ui->GetRoot()->SetDefaultStyle(style);
    for (auto* mg : findAllComponents<MorphGeometry>(scene_)) {
        CreateUI(mg);
    }

    CreateCameraUI();

    LogSceneContents(GetSubsystem<Log>(), scene_);

    SetupCamera();

    Renderer* renderer = GetSubsystem<Renderer>();
    renderer->SetViewport(0, new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetDrawShadows(true);

    SetInteractMode(0);
    

    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(FBXViewerApp, HandleUpdate));
    SubscribeToEvent(E_KEYDOWN, URHO3D_HANDLER(FBXViewerApp, HandleKeyDown));
}

float DegToRad(float deg) {
    return deg * 3.1415f / 180.0f;
}

void FBXViewerApp::SetupCamera() {
    BoundingBox sceneBoundingBox;
    auto* log = GetSubsystem<Log>();

    Vector<Node*> nodes;
    scene_->GetChildrenWithComponent<MorphGeometry>(nodes, true);

    log->Write(LOG_DEBUG, "GetChildrenWithComponent");

    for (auto* node : nodes)
    {
        auto* model = node->GetComponent<MorphGeometry>();
        if (model)
        {
            BoundingBox box = model->GetBoundingBox();
            box.Transform(node->GetWorldTransform());
            sceneBoundingBox.Merge(box);
        }
    }
    Vector3 center = sceneBoundingBox.Center();
    float radius = (sceneBoundingBox.max_ - sceneBoundingBox.min_).Length() * 0.5f;

    float fov = cameraNode_->GetComponent<Camera>()->GetFov(); // Угол обзора камеры в градусах
    float aspectRatio = cameraNode_->GetComponent<Camera>()->GetAspectRatio(); // Соотношение сторон

    // Вычисление необходимого расстояния от центра сцены до камеры
    float distance = radius / tanf(DegToRad(fov * 0.5f));

    // Установка позиции камеры
    Vector3 cameraDirection = Vector3::BACK; // Направление взгляда камеры
    cameraNode_->SetPosition(center - cameraDirection * distance);
    cameraNode_->LookAt(center);

    yaw_ = cameraNode_->GetRotation().YawAngle();
    pitch_ = cameraNode_->GetRotation().PitchAngle();
}

void FBXViewerApp::MoveCamera(float timeStep) {
    auto* input = GetSubsystem<Input>();
    const float MOUSE_SENSITIVITY = 0.1f;
    const float SPEED_STEP = 1.0f; // Шаг изменения скорости
    const float MIN_SPEED = 1.0f;
    const float MAX_SPEED = 100.0f;

    // Обработка вращения камеры при зажатой правой кнопке мыши
    if (input->GetMouseButtonDown(MOUSEB_RIGHT)) {
        IntVector2 move = input->GetMouseMove();
        yaw_ += MOUSE_SENSITIVITY * move.x_;
        pitch_ = Clamp(pitch_ + MOUSE_SENSITIVITY * move.y_, -90.0f, 90.0f);
        cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));
    }

    // Изменение скорости перемещения с помощью колесика мыши
    int wheelDelta = input->GetMouseMoveWheel();
    if (wheelDelta != 0) {
        moveSpeed_ += SPEED_STEP * wheelDelta;
        moveSpeed_ = Clamp(moveSpeed_, MIN_SPEED, MAX_SPEED);
    }

    // Перемещение камеры
    Vector3 dir;
    if (input->GetKeyDown(KEY_W)) dir += Vector3::FORWARD;
    if (input->GetKeyDown(KEY_S)) dir += Vector3::BACK;
    if (input->GetKeyDown(KEY_A)) dir += Vector3::LEFT;
    if (input->GetKeyDown(KEY_D)) dir += Vector3::RIGHT;
    if (input->GetKeyDown(KEY_Q)) dir += Vector3::DOWN;
    if (input->GetKeyDown(KEY_E)) dir += Vector3::UP;

    cameraNode_->Translate(dir * moveSpeed_ * timeStep);
}

inline String ToStringWithPrecision(float value, int precision = 2)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), ("%." + String(precision) + "f").CString(), value);
    return String(buffer);
}

void FBXViewerApp::HandleUpdate(StringHash h, VariantMap& eventData) {
    auto* ui = GetSubsystem<UI>();
    auto* input = GetSubsystem<Input>();
    using namespace Update;
    if (GetInteractMode()->cameraActive) {
        MoveCamera(eventData[P_TIMESTEP].GetFloat());
    }
    if (cameraNode_ && cameraPositionText_)
    {
        Vector3 pos = cameraNode_->GetWorldPosition();
        float yaw = cameraNode_->GetRotation().YawAngle();
        float pitch = cameraNode_->GetRotation().PitchAngle();
        String text = "Camera: (" +
            ToStringWithPrecision((float) pos.x_, 2) + ", " +
            ToStringWithPrecision((float) pos.y_, 2) + ", " +
            ToStringWithPrecision((float) pos.z_, 2) + ", " +
            ToStringWithPrecision((float) yaw, 2) + ", " +
            ToStringWithPrecision((float) pitch, 2) + 
            ")";
        cameraPositionText_->SetText(text);
    }
}

void FBXViewerApp::SetInteractMode(int num) {
    auto* input = GetSubsystem<Input>();
    auto* log = GetSubsystem<Log>();
    num = (num % (sizeof(interactModeInfos) / sizeof(interactModeInfos[0])));
    log->Write(LOG_INFO, "Select interact mode number " + String(num));
    interactMode = interactModeInfos + num;
    input->SetMouseVisible(interactMode->mouseVisible);
    input->SetMouseGrabbed(!interactMode->mouseVisible);
    GetSubsystem<UI>()->GetRoot()->SetVisible(interactMode->showUI);
}

int FBXViewerApp::GetInteractModeNum() {
    return (int) (interactMode - interactModeInfos);
}

const InteractModeInfo* FBXViewerApp::GetInteractMode() {
    return  interactMode;
}

void FBXViewerApp::HandleKeyDown(StringHash, VariantMap& eventData) {
    auto* input = GetSubsystem<Input>();
    using namespace KeyDown;
    if (eventData[P_KEY].GetI32() == KEY_ESCAPE)
        engine_->Exit();
    if (eventData[P_KEY].GetI32() == KEY_TAB) 
    {
        SetInteractMode(GetInteractModeNum() + 1);
    }
    if (eventData[P_KEY].GetI32() == KEY_F11)
    {
        auto* graphics = GetSubsystem<Graphics>();
        graphics->ToggleFullscreen();
    }
}

void FBXViewerApp::CreateScene()
{
    auto* log = GetSubsystem<Log>();
    scene_ = SharedPtr<Scene>(new Scene(context_));
    scene_->CreateComponent<Octree>();

    SharedPtr<Node> fbxNode = LoadFBXToNode(context_, "CustomData/repo.fbx");
    if (fbxNode) {
        scene_->CreateChild("ImportedFBX")->AddChild(fbxNode);
        GetSubsystem<Log>()->Write(LOG_INFO, "Add fbx importet nodes");    
    } else {
        GetSubsystem<Log>()->Write(LOG_ERROR, "Can't load model");    
    }
    cameraNode_ = scene_->CreateChild("Camera");
    cameraNode_->CreateComponent<Camera>();
}

void FBXViewerApp::CreateCameraUI() {
    auto* ui = GetSubsystem<UI>();
    auto* cache = GetSubsystem<ResourceCache>();
    auto* root = ui->GetRoot();
    auto* style = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");
    // Контейнер для ввода позиции камеры
    auto* cameraPanel = root->CreateChild<UIElement>();
    cameraPanel->SetDefaultStyle(style);
    cameraPanel->SetLayout(LM_VERTICAL);
    cameraPanel->SetAlignment(HA_RIGHT, VA_TOP);
    cameraPanel->SetPosition(10, 10);
    cameraPanel->SetMinSize(500, 200);
    cameraPanel->SetLayoutSpacing(5);

    cameraPositionText_ = cameraPanel->CreateChild<Text>();
    cameraPositionText_->SetStyleAuto();
    cameraPositionText_->SetText("Camera Position");
    
    // Поля ввода
    cameraPosX_ = cameraPanel->CreateChild<LineEdit>();
    cameraPosX_->SetStyleAuto();
    cameraPosX_->SetMinSize(200, 20);
    cameraPosX_->SetText(String("X"));
    
    cameraPosY_ = cameraPanel->CreateChild<LineEdit>();
    cameraPosY_->SetStyleAuto();
    cameraPosY_->SetMinSize(200, 20);
    cameraPosY_->SetText("Y");
    
    cameraPosZ_ = cameraPanel->CreateChild<LineEdit>();
    cameraPosZ_->SetStyleAuto();
    cameraPosZ_->SetMinSize(200, 20);
    cameraPosZ_->SetText("Z");

    cameraPosYaw_ = cameraPanel->CreateChild<LineEdit>();
    cameraPosYaw_->SetStyleAuto();
    cameraPosYaw_->SetMinSize(200, 20);
    cameraPosYaw_->SetText("Z");

    cameraPosPitch_ = cameraPanel->CreateChild<LineEdit>();
    cameraPosPitch_->SetStyleAuto();
    cameraPosPitch_->SetMinSize(200, 20);
    cameraPosPitch_->SetText("Z");
    
    // Кнопка "Применить"
    applyCameraButton_ = cameraPanel->CreateChild<Button>();
    applyCameraButton_->SetStyleAuto();
    applyCameraButton_->SetMinSize(200, 20);
    applyCameraButton_->SetMinHeight(24);
    
    auto* buttonText = applyCameraButton_->CreateChild<Text>();
    buttonText->SetAlignment(HA_CENTER, VA_CENTER);
    buttonText->SetText("Set Camera Pos");
    
    // Подписка на нажатие кнопки
    SubscribeToEvent(applyCameraButton_, E_PRESSED, URHO3D_HANDLER(FBXViewerApp, HandleApplyCameraPosition));
    
}

void FBXViewerApp::CreateUI(MorphGeometry* geometry) {
    if (geometry->GetMorpherNames().Empty())return;
    Log* log = GetSubsystem<Log>();
    auto* cache = GetSubsystem<ResourceCache>();
    // Получение корневого элемента UI
    UI* ui = GetSubsystem<UI>();
    UIElement* root = ui->GetRoot();
    auto* layout = root->CreateChild<UIElement>();
    layout->SetLayout(LM_VERTICAL); // Горизонтальная LM_HORIZONTAL тоже есть
    layout->SetAlignment(HA_LEFT, VA_TOP); // Привязка в верхний левый угол
    layout->SetSize(300, 400); // Общий размер
    layout->SetPosition(10, 10); // Где контейнер будет на экране
    layout->SetLayoutSpacing(10); // Отступы между элементами

    // Создание DropDownList
    DropDownList* morphList = layout->CreateChild<DropDownList>();
    morphList->SetStyleAuto();
    morphList->SetMinSize(100, 20);
    morphList->SetVar("node", geometry->GetNode());
    SubscribeToEvent(morphList, E_ITEMSELECTED, URHO3D_HANDLER(FBXViewerApp, HandleDropDownListChanged));

    // Заполнение списка морф-таргетов
    for (const auto& morph : geometry->GetMorpherNames())
    {
        log->Write(LOG_INFO, String("Load morph select ") + morph);
        SharedPtr<Text> item(new Text(context_));
        item->SetText(morph);
        item->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 10);
        item->SetHorizontalAlignment(HA_CENTER);
        item->SetVerticalAlignment(VA_CENTER);
        morphList->AddItem(item);
    }

    // Создание Slider
    Slider* morphSlider = layout->CreateChild<Slider>();
    morphSlider->SetStyleAuto();
    morphSlider->SetMinSize(200, 20);
    morphSlider->SetRange(1.0f); // Диапазон от 0.0 до 1.0
    morphSlider->SetVar("node", geometry->GetNode());
    SubscribeToEvent(morphSlider, E_SLIDERCHANGED, URHO3D_HANDLER(FBXViewerApp, HandleSliderChanged));
    log->Write(LOG_INFO, String("Load morph slider"));

}

void FBXViewerApp::HandleApplyCameraPosition(StringHash eventType, VariantMap& eventData)
{
    Log* log = GetSubsystem<Log>();
    if (!cameraNode_)
        return;

    float x = ToFloat(cameraPosX_->GetText());
    float y = ToFloat(cameraPosY_->GetText());
    float z = ToFloat(cameraPosZ_->GetText());

    float yaw = ToFloat(cameraPosYaw_->GetText());
    float pitch = ToFloat(cameraPosPitch_->GetText());

    cameraNode_->SetWorldPosition(Vector3(x, y, z));

    yaw_ = yaw;
    pitch_ = pitch;
    cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

    log->Write(LOG_INFO, "Camera position set to (" + ToStringWithPrecision(x, 2) + ", " + ToStringWithPrecision(y, 2) + ", " + ToStringWithPrecision(z, 2) + ", " + ToStringWithPrecision(yaw, 2) + ", " + ToStringWithPrecision(pitch, 2) + ")");
}

void FBXViewerApp::HandleSliderChanged(StringHash eventType, VariantMap& eventData)
{
    auto* slider = static_cast<Slider*>(eventData[SliderChanged::P_ELEMENT].GetPtr());

    GetSubsystem<Log>()->Write(LOG_DEBUG, "Update slider");
    float value = slider->GetValue();
    URHO3D_LOGINFO("Slider changed to value: " + String(value));

    Node* node = static_cast<Node*>(slider->GetVar("node").GetPtr());
    if (node)
    {
        MorphGeometry* geometry = node->GetComponent<MorphGeometry>();
        if (geometry)
        {
            GetSubsystem<Log>()->Write(LOG_DEBUG, String("Set value for morph weight ") + String(value) + String(" With active morph") + geometry->GetActiveMorpher() + String(" For ") + String((unsigned long long) geometry) + String(" ") + geometry->GetNode()->GetName());
            geometry->SetMorphWeight(value);
        }
    } else {
        GetSubsystem<Log>()->Write(LOG_DEBUG, "Can't found MorphGeometry for slider");
    }
}

void FBXViewerApp::HandleDropDownListChanged(StringHash eventType, VariantMap& eventData)
{
    auto* log = GetSubsystem<Log>();
    using namespace ItemSelected;
    auto* list = static_cast<DropDownList*>(eventData[P_ELEMENT].GetPtr());

    int selected = eventData[P_SELECTION].GetI32();
    URHO3D_LOGINFO("Dropdown selected item index: " + String(selected));

    // Можно получить выбранный текст
    if (list->GetNumItems() > selected)
    {
        auto* selectedItem = static_cast<Text*>(list->GetItem(selected));
        if (selectedItem)
        {
            Node* node = static_cast<Node*>(list->GetVar("node").GetPtr());
            if (node)
            {
                MorphGeometry* geometry = node->GetComponent<MorphGeometry>();
                if (geometry)
                {
                    GetSubsystem<Log>()->Write(LOG_DEBUG, String("Set value for morph target ") + selectedItem->GetText() + String(" With active morph ") + geometry->GetActiveMorpher() + String(" For ") + String((unsigned long long) geometry) + String(" ") + geometry->GetNode()->GetName());
                    geometry->SetActiveMorpher(selectedItem->GetText());
                    geometry->Commit();
                }
            } else {
                log->Write(LOG_INFO, "Can't found MorphGeometry for select " + selectedItem->GetText());
            }
            
        }
    }
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
    zone->SetFogStart(1000.0f);
    zone->SetFogEnd(3000.0f);

}

void FBXViewerApp::RegisterAllComponents()
{
    context_->RegisterFactory<MorphGeometry>();
}

URHO3D_DEFINE_APPLICATION_MAIN(FBXViewerApp)