#define INITGUID

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>

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

#include <fbxsdk.h>

using namespace Urho3D;


class FBXViewerApp : public Application {
private:
	float yaw_ = 0.0f;
	float pitch_ = 0.0f;
	
	SharedPtr<Node> LoadFBXToNode(Context* context, const String& fbxPath) {
		FbxManager* manager = FbxManager::Create();
		FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
		manager->SetIOSettings(ios);

		String path = GetSubsystem<FileSystem>()->GetProgramDir() + fbxPath;
		FbxImporter* importer = FbxImporter::Create(manager, "");
		if (!importer->Initialize(path.CString(), -1, manager->GetIOSettings()))
		{
			String errorMessage = String("FBX Import Error: ") + importer->GetStatus().GetErrorString();
			GetSubsystem<Log>()->Write(LOG_ERROR, errorMessage);
			return SharedPtr<Node>();
		} else {
			GetSubsystem<Log>()->Write(LOG_INFO, "FBX file Imported\n");
		}

		FbxScene* sceneFBX = FbxScene::Create(manager, "Scene");
		importer->Import(sceneFBX);
		importer->Destroy();

		FbxNode* rootNode = sceneFBX->GetRootNode();
		if (!rootNode)
		{
			GetSubsystem<Log>()->Write(LOG_ERROR, "No mesh found in FBX file\n");
			manager->Destroy();
			return SharedPtr<Node>();
		} else {
			GetSubsystem<Log>()->Write(LOG_INFO, "Load FBX root node\n");
		}

		FbxMesh* fbxMesh = nullptr;
		FbxNode* meshNode = nullptr;
		for (int i = 0; i < rootNode->GetChildCount(); ++i)
		{
			FbxNode* child = rootNode->GetChild(i);
			if (child->GetMesh())
			{
				fbxMesh = child->GetMesh();
				meshNode = child;
				break;
			}
		}

		if (!fbxMesh)
		{
			GetSubsystem<Log>()->Write(LOG_ERROR, "No mesh found in FBX file\n");
			manager->Destroy();
			return SharedPtr<Node>();
		} else {
			GetSubsystem<Log>()->Write(LOG_INFO, "Find Mesh in FBX file\n");
		}

		// Создаём узел (но не добавляем в сцену!)
		SharedPtr<Node> node(new Node(context));
		node->SetName("FBXNode");

		CustomGeometry* geom = node->CreateComponent<CustomGeometry>();
		geom->SetNumGeometries(1);
		geom->SetDynamic(false);
		geom->BeginGeometry(0, TRIANGLE_LIST);

		int vertexCount = fbxMesh->GetControlPointsCount();
		FbxVector4* controlPoints = fbxMesh->GetControlPoints();

		for (int i = 0; i < fbxMesh->GetPolygonCount(); ++i)
		{
			int polySize = fbxMesh->GetPolygonSize(i);
			for (int j = 0; j < polySize; ++j)
			{
				int ctrlPointIndex = fbxMesh->GetPolygonVertex(i, j);
				FbxVector4 pos = controlPoints[ctrlPointIndex];
				Vector3 position((float)pos[0], (float)pos[1], (float)pos[2]);

				// Нормали
				Vector3 normal = Vector3::UP;
				FbxVector4 fbxNormal;
				if (fbxMesh->GetPolygonVertexNormal(i, j, fbxNormal))
					normal = Vector3((float)fbxNormal[0], (float)fbxNormal[1], (float)fbxNormal[2]);

				// UV
				Vector2 uv = Vector2::ZERO;
				FbxStringList uvSetNames;
				fbxMesh->GetUVSetNames(uvSetNames);
				if (uvSetNames.GetCount() > 0)
				{
					FbxVector2 fbxUV;
					bool unmapped;
					if (fbxMesh->GetPolygonVertexUV(i, j, uvSetNames[0], fbxUV, unmapped))
						uv = Vector2((float)fbxUV[0], 1.0f - (float)fbxUV[1]);
				}

				geom->DefineVertex(position);
				geom->DefineNormal(normal);
				geom->DefineTexCoord(uv);
				geom->DefineTangent(Vector4(1.0f, 0.0f, 0.0f, 1.0f));
			}
		}
		auto* cache = GetSubsystem<ResourceCache>();
		auto* material = cache->GetResource<Material>("Materials/Stone.xml");
		geom->SetMaterial(material);
		geom->SetViewMask(0x80000000); // Просто для управления видимостью
		geom->SetOccludee(false);

		geom->GetMaterial()->SetFillMode(FILL_WIREFRAME);
		geom->Commit();
		manager->Destroy();
		

		return node;
	}

	void MoveCamera(float timeStep)
	{
		Input* input = GetSubsystem<Input>();

		const float MOUSE_SENSITIVITY = 0.1f;
		const float MOVE_SPEED = 5.0f;

		if (input->GetMouseButtonDown(MOUSEB_RIGHT))
		{
			IntVector2 mouseMove = input->GetMouseMove();
			yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
			pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
			pitch_ = Clamp(pitch_, -90.0f, 90.0f);

			cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));
		}

		Vector3 moveDir = Vector3::ZERO;
		if (input->GetKeyDown(KEY_W)) moveDir += Vector3::FORWARD;
		if (input->GetKeyDown(KEY_S)) moveDir += Vector3::BACK;
		if (input->GetKeyDown(KEY_A)) moveDir += Vector3::LEFT;
		if (input->GetKeyDown(KEY_D)) moveDir += Vector3::RIGHT;
		if (input->GetKeyDown(KEY_Q)) moveDir += Vector3::DOWN;
		if (input->GetKeyDown(KEY_E)) moveDir += Vector3::UP;

		if (moveDir != Vector3::ZERO)
			cameraNode_->Translate(moveDir * MOVE_SPEED * timeStep);
	}
	
	void HandleUpdate(StringHash eventType, VariantMap& eventData)
	{
		using namespace Update;
		float timeStep = eventData[P_TIMESTEP].GetFloat();

		MoveCamera(timeStep);
	}

	void HandleKeyDown(StringHash eventType, VariantMap& eventData)
	{
		using namespace KeyDown;
		int key = eventData[P_KEY].GetI32();
		if (key == KEY_ESCAPE)
		{
			engine_->Exit(); // Закрыть приложение
		}
	}	
	
public:
    SharedPtr<Scene> scene_;
    SharedPtr<Node> cameraNode_;

    FBXViewerApp(Context* context) : Application(context) {}

    void Setup() override {
        engineParameters_["WindowTitle"] = "FBX Viewer with Urho3D";
        engineParameters_["FullScreen"] = false;
        engineParameters_["WindowWidth"] = 1280;
        engineParameters_["WindowHeight"] = 720;
		engineParameters_["LogName"] = "mylog.txt";
    }

	void Start() override {

		GetSubsystem<Log>()->Write(LOG_INFO, "Logging system is active\n");

		// Сцена и камера
		scene_ = new Scene(context_);
		scene_->CreateComponent<Octree>();

		cameraNode_ = scene_->CreateChild("Camera");
		cameraNode_->CreateComponent<Camera>();
		cameraNode_->SetDirection(Vector3::BACK);
		cameraNode_->SetPosition(Vector3(0, 1, 10));

		yaw_ = cameraNode_->GetRotation().YawAngle();
		pitch_ = cameraNode_->GetRotation().PitchAngle();
		
		Node* lightNode = scene_->CreateChild("Light");
		lightNode->SetDirection(Vector3::BACK); // или в сторону камеры
		// Свет
		lightNode->SetPosition(Vector3(0, -20, 20));
		Light* light = lightNode->CreateComponent<Light>();
		light->SetLightType(LIGHT_DIRECTIONAL);

		SharedPtr<Zone> zone(scene_->CreateComponent<Zone>());
		zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));
		zone->SetAmbientColor(Color(0.6f, 0.6f, 0.6f)); // мягкий серый свет
		zone->SetFogColor(Color::BLACK);               // без тумана


		Renderer* renderer = GetSubsystem<Renderer>();
		renderer->SetViewport(0, new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
		renderer->SetDrawShadows(true);
		// renderer->SetViewOverrideFlags(VO_FILL_WIREFRAME);



		// Загрузка FBX
		SharedPtr<Node> fbxNode = LoadFBXToNode(context_, "FBXData/model.fbx");
		if (fbxNode)
		{
			Node* sceneChild = scene_->CreateChild("ImportedFBX");
			sceneChild->AddChild(fbxNode);
		} else {
			URHO3D_LOGERROR("Can't found model");
		}
		Input* input = GetSubsystem<Input>();
		input->SetMouseVisible(false); // скрыть курсор
		input->SetMouseGrabbed(true);  // захват мыши

		SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(FBXViewerApp, HandleUpdate));
		SubscribeToEvent(E_KEYDOWN, URHO3D_HANDLER(FBXViewerApp, HandleKeyDown));
	}

};

URHO3D_DEFINE_APPLICATION_MAIN(FBXViewerApp)
