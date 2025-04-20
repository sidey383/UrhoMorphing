#include "FBXLoader.h"
#include "MorphGeometry.h"
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

SharedPtr<Node> BuildUrhoGeometryMorphFromFBXMesh(Context* context, FbxMesh* fbxMesh, Vector3 position = Vector3::ZERO)
{
    auto* log = context->GetSubsystem<Log>();
    log->Write(LOG_INFO, "RUN BuildUrhoGeometryMorphFromFBXMesh");

    // Извлечение контрольных точек
    FbxVector4* controlPoints = fbxMesh->GetControlPoints();
    int controlPointCount = fbxMesh->GetControlPointsCount();

    // Извлечение морф-таргета
    Vector<Vector3> morphDeltas(controlPointCount, Vector3::ZERO);
    for (int deformerIndex = 0; deformerIndex < fbxMesh->GetDeformerCount(); ++deformerIndex)
    {
        // FbxBlendShape* blendShape = FbxCast<FbxBlendShape>(fbxMesh->GetDeformer(deformerIndex, FbxDeformer::eBlendShape));
        // if (!blendShape)
        //     continue;

        // for (int channelIndex = 0; channelIndex < blendShape->GetBlendShapeChannelCount(); ++channelIndex)
        // {
        //     FbxBlendShapeChannel* channel = blendShape->GetBlendShapeChannel(channelIndex);
        //     if (!channel || channel->GetTargetShapeCount() == 0)
        //         continue;

        //     FbxShape* shape = channel->GetTargetShape(0);
        //     int numVertices = shape->GetControlPointsCount();
        //     FbxVector4* shapePoints = shape->GetControlPoints();

        //     for (int i = 0; i < numVertices; ++i)
        //     {
        //         FbxVector4 base = controlPoints[i];
        //         FbxVector4 delta = shapePoints[i] - base;
        //         morphDeltas[i] = Vector3((float)delta[0], (float)delta[1], (float)delta[2]);
        //     }
        // }
    }

    // Создание вершин
    Vector<MorphVertex> vertices;
    Vector<i32> indices;
    for (int i = 0; i < fbxMesh->GetPolygonCount(); ++i)
    {
        int polySize = fbxMesh->GetPolygonSize(i);
        if (polySize != 3)
        {
            log->Write(LOG_ERROR, "Can't process non-triangulated polygon");
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

            MorphVertex vertex;
            vertex.position_ = position;
            vertex.normal_ = normal;
            vertex.texCoord_ = uv;
            vertex.tangent_ = Vector4(1.0f, 0.0f, 0.0f, 1.0f);
            //vertex.morphDelta_ = morphDeltas[ctrlPointIndex];

            vertices.Push(vertex);
            indices.Push(vertices.Size() - 1);
        }
    }

    // Создание узла и компонента MorphGeometry
    SharedPtr<Node> node(new Node(context));
    node->SetPosition(position);
    auto* morphGeometry = node->CreateComponent<MorphGeometry>();
    morphGeometry->SetVertices(vertices);
    morphGeometry->SetIndices(indices);

    auto* cache = context->GetSubsystem<ResourceCache>();
    auto* material = cache->GetResource<Material>("Materials/Stone.xml");
    if (material)
        morphGeometry->SetMaterial(material);

    morphGeometry->SetMorphWeight(0.0f); // Начальный вес морфинга
    morphGeometry->Commit();
    log->Write(LOG_INFO, "Success compete BuildUrhoGeometryMorphFromFBXMesh");

    return node;
}


SharedPtr<Node> BuildUrhoGeometryFromFBXMesh(Context* context, FbxMesh* fbxMesh, Vector3 position = Vector3::ZERO)
{
    auto* log = context->GetSubsystem<Log>();
    log->Write(LOG_INFO, "RUN BuildUrhoGeometryFromFBXMesh");
    FbxVector4* controlPoints = fbxMesh->GetControlPoints();
    SharedPtr<Node> node(new Node(context));
    node->SetPosition(position);
    CustomGeometry* geom = node->CreateComponent<CustomGeometry>();
    geom->SetNumGeometries(1);
    geom->SetDynamic(false);
    geom->BeginGeometry(0, TRIANGLE_LIST);

    for (int i = 0; i < fbxMesh->GetPolygonCount(); ++i)
    {
        int polySize = fbxMesh->GetPolygonSize(i);
        if (polySize != 3) 
        {
            context->GetSubsystem<Log>()->Write(LOG_ERROR, "Can't process non-triangulated polygon");
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

    auto* cache = context->GetSubsystem<ResourceCache>();
    auto* material = cache->GetResource<Material>("Materials/Stone.xml");
    if (material)
        geom->SetMaterial(material);

    return node;
}


FbxMesh* GetFirstMeshFromScene(Log* log, FbxScene* scene)
{
    log->Write(LOG_INFO, "RUN GetFirstMeshFromScene");
    FbxNode* root = scene->GetRootNode();
    if (!root)
        return nullptr;
    for (int i = 0; i < root->GetChildCount(); ++i)
    {
        FbxNode* child = root->GetChild(i);
        if (FbxMesh* mesh = child->GetMesh()) 
        {
            return mesh;
        }
    }

    return nullptr;
}


FbxScene* ImportFBXScene(Context* context, FbxManager* manager, const String& fbxPath)
{
    
    auto* log = context->GetSubsystem<Log>();
    log->Write(LOG_INFO, "RUN ImportFBXScene");

    FbxIOSettings* ioSettings = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ioSettings);

    String path = context->GetSubsystem<FileSystem>()->GetProgramDir() + fbxPath;
    FbxImporter* importer = FbxImporter::Create(manager, "scene");
    if (!importer->Initialize(path.CString(), -1, manager->GetIOSettings()))
    {
        log->Write(LOG_ERROR, String("FBX Import Error: ") + importer->GetStatus().GetErrorString());
        manager->Destroy();
        return nullptr;
    }

    FbxScene* scene = FbxScene::Create(manager, "scene");
    log->Write(LOG_INFO, String("Load fbx file with ") + String(scene->GetNodeCount()) + String(" nodes\n"));
    importer->Import(scene);
    importer->Destroy();

    return scene;
}

SharedPtr<Node> LoadFBXToNode(Context* context, const String& fbxPath)
{
    auto* log = context->GetSubsystem<Log>();
    FbxManager* manager = FbxManager::Create();
    if (!manager)
    {
        log->Write(LOG_ERROR, "Failed to create FBX Manager");
        return nullptr;
    }
    FbxScene* scene = ImportFBXScene(context, manager, fbxPath);
    if (!scene)
    {
        log->Write(LOG_ERROR, "Failed to load FBX scene");
        manager->Destroy();
        return SharedPtr<Node>();
    }

    FbxMesh* mesh = GetFirstMeshFromScene(log, scene);
    if (!mesh)
    {
        log->Write(LOG_ERROR, "No mesh found in FBX scene");
        manager->Destroy();
        return SharedPtr<Node>();
    }

    SharedPtr<Node> node(new Node(context));
    node->SetName("FBXImpoted");

    SharedPtr<Node> resultMorhp = BuildUrhoGeometryMorphFromFBXMesh(context, mesh);
    SharedPtr<Node> resultSimple = BuildUrhoGeometryFromFBXMesh(context, mesh, Vector3(5, 0, 0));
    manager->Destroy();
    if (!resultMorhp) {
        log->Write(LOG_INFO, "BuildUrhoGeometryMorphFromFBXMesh result is null");
    } else {
        node->AddChild(resultMorhp);
    }
    if (!resultSimple) {
        log->Write(LOG_INFO, "BuildUrhoGeometryMorphFromFBXMesh result is null");
    } else {
        node->AddChild(resultSimple);
    }

    log->Write(LOG_INFO, "Complete LoadFBXToNode");
    return node;
}

