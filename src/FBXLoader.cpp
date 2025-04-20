#include "FBXLoader.h"
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Graphics/CustomGeometry.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Core/Context.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Graphics/Material.h>
#include <fbxsdk.h>

using namespace Urho3D;

SharedPtr<Node> LoadFBXToNode(Context* context, const String& fbxPath)
{
    FbxManager* manager = FbxManager::Create();
    auto* log = context->GetSubsystem<Log>();
    if (!manager)
    {
        log->Write(LOG_ERROR, "Failed to create FBX Manager");
        return SharedPtr<Node>();
    }

    FbxIOSettings* ioSettings = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ioSettings);

    String path = context->GetSubsystem<FileSystem>()->GetProgramDir() + fbxPath;
    FbxImporter* importer = FbxImporter::Create(manager, "");
    if (!importer->Initialize(path.CString(), -1, manager->GetIOSettings()))
    {
        String errorMessage = String("FBX Import Error: ") + importer->GetStatus().GetErrorString();
        log->Write(LOG_ERROR, errorMessage);
        return SharedPtr<Node>();
    } else {
        log->Write(LOG_INFO, "FBX file Imported\n");
    }

    FbxScene* fbxScene = FbxScene::Create(manager, "scene");
    importer->Import(fbxScene);
    importer->Destroy();

    FbxNode* root = fbxScene->GetRootNode();
    if (!root)
    {
        context->GetSubsystem<Log>()->Write(LOG_ERROR, "No root node in FBX scene");
        return SharedPtr<Node>();
    }

    // Ищем первый меш
    FbxMesh* fbxMesh = nullptr;
    for (int i = 0; i < root->GetChildCount(); ++i)
    {
        FbxNode* child = root->GetChild(i);
        fbxMesh = child->GetMesh();
        if (fbxMesh) break;
    }

    if (!fbxMesh)
    {
        context->GetSubsystem<Log>()->Write(LOG_ERROR, "No mesh found in FBX file");
        return SharedPtr<Node>();
    }

    FbxVector4* controlPoints = fbxMesh->GetControlPoints();
    SharedPtr<Node> node(new Node(context));
    CustomGeometry* geom = node->CreateComponent<CustomGeometry>();
    geom->SetNumGeometries(1);
    geom->SetDynamic(false);
    geom->BeginGeometry(0, TRIANGLE_LIST);

    for (int i = 0; i < fbxMesh->GetPolygonCount(); ++i)
    {
        int polySize = fbxMesh->GetPolygonSize(i);
        if (polySize != 3) 
        {
            context->GetSubsystem<Log>()->Write(LOG_ERROR, "Can't process not triangulate ploygon");
            return SharedPtr<Node>();
        }

        for (int j = 0; j < polySize; ++j)
        {
            int ctrlPointIndex = fbxMesh->GetPolygonVertex(i, j);
            FbxVector4 pos = controlPoints[ctrlPointIndex];
            Vector3 position((float)pos[0], (float)pos[1], (float)pos[2]);

            // Нормаль
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

    geom->Commit();

    // Назначим простой материал
    auto* cache = context->GetSubsystem<ResourceCache>();
    auto* material = cache->GetResource<Material>("Materials/Stone.xml");
    if (material)
        geom->SetMaterial(material);

    return node;
}
