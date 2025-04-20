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

inline Vector3 ToVector3(const FbxVector4& v)
{
    return Vector3(
        static_cast<float>(v[0]),
        static_cast<float>(v[1]),
        static_cast<float>(v[2])
    );
}

inline void applyNormal(Log* log, const FbxMesh* fbxMesh, bool& brokeNormal, Vector3& n, const int i, const char* nodeName) {
    FbxVector4 fn;
    if (fbxMesh->GetPolygonVertexNormal(i, 0, fn))
    {
        n = Vector3((float)fn[0], (float)fn[1], (float)fn[2]);
    } else {
        if (!brokeNormal)
        {
            log->Write(LOG_WARNING, "Can't found some normal for " + String(nodeName));
            brokeNormal = true;
        }
    }
}

bool LoadFBXRecursive(Context* context, Node* parent, FbxNode* fbxNode)
{
    auto* log = context->GetSubsystem<Log>();
    Node* thisNode = parent->CreateChild(fbxNode->GetName());
    thisNode->SetPosition(Vector3::ZERO);
    const char* nodeName = fbxNode->GetName();
    thisNode->SetName(nodeName && nodeName[0] ? nodeName : "UnnamedNode");

    FbxMesh* fbxMesh = fbxNode->GetMesh();
    if (fbxMesh)
    {
        CustomGeometry* geom = thisNode->CreateComponent<CustomGeometry>();
        geom->SetNumGeometries(1);
        geom->BeginGeometry(0, TRIANGLE_LIST);

        int polygonCount = fbxMesh->GetPolygonCount();
        FbxVector4* controlPoints = fbxMesh->GetControlPoints();
        
        bool brokeNormal = false;
        for (int i = 0; i < polygonCount; ++i)
        {
            int polySize = fbxMesh->GetPolygonSize(i);
            if (polySize != 3) 
            {
                log->Write(LOG_ERROR, "Can't process not triangulate ploygon");
                return false;
            }
            for (int j = 1; j < polySize - 1; ++j)
            {
                // Мы обходим один полигон, так что j = 1 
                // Получается 0, 1, 2 - три вершины
                // Если полгино задается четрыехугольников все продолжит работать корректно, мы проведем триангуляцию
                int i0 = fbxMesh->GetPolygonVertex(i, 0);
                int i1 = fbxMesh->GetPolygonVertex(i, j);
                int i2 = fbxMesh->GetPolygonVertex(i, j + 1);
            
                Vector3 v0 = ToVector3(controlPoints[i0]);
                Vector3 v1 = ToVector3(controlPoints[i1]);
                Vector3 v2 = ToVector3(controlPoints[i2]);
            
                Vector3 n0 = Vector3::UP, n1 = Vector3::UP, n2 = Vector3::UP;
                applyNormal(log, fbxMesh, brokeNormal, n0, i, nodeName);
                applyNormal(log, fbxMesh, brokeNormal, n1, i, nodeName);
                applyNormal(log, fbxMesh, brokeNormal, n2, i, nodeName);
            
                Vector2 uv0 = Vector2::ZERO, uv1 = Vector2::ZERO, uv2 = Vector2::ZERO;
                FbxStringList uvSets;
                fbxMesh->GetUVSetNames(uvSets);
                if (uvSets.GetCount() > 0)
                {
                    FbxVector2 uv;
                    bool unmapped;
                    fbxMesh->GetPolygonVertexUV(i, 0, uvSets[0], uv, unmapped);
                    uv0 = Vector2((float)uv[0], 1.0f - (float)uv[1]);
                    fbxMesh->GetPolygonVertexUV(i, j, uvSets[0], uv, unmapped);
                    uv1 = Vector2((float)uv[0], 1.0f - (float)uv[1]);
                    fbxMesh->GetPolygonVertexUV(i, j + 1, uvSets[0], uv, unmapped);
                    uv2 = Vector2((float)uv[0], 1.0f - (float)uv[1]);
                }
            
                geom->DefineVertex(v0);
                geom->DefineNormal(n0);
                geom->DefineTexCoord(uv0);
            
                geom->DefineVertex(v1);
                geom->DefineNormal(n1);
                geom->DefineTexCoord(uv1);
            
                geom->DefineVertex(v2);
                geom->DefineNormal(n2);
                geom->DefineTexCoord(uv2);
            
                geom->DefineTangent(Vector4(1, 0, 0, 1));
            }
        }
        FbxGeometry* geometry = dynamic_cast<FbxGeometry*>(fbxMesh);
        if (geometry) {
            int blendShapeCount = geometry->GetDeformerCount(FbxDeformer::eBlendShape);
            
            for (int i = 0; i < blendShapeCount; ++i)
            {
                FbxBlendShape* blendShape = (FbxBlendShape*)geometry->GetDeformer(i, FbxDeformer::eBlendShape);
            
                String shapeInfo = "BlendShape: ";
                shapeInfo += blendShape->GetName();
                log->Write(LOG_INFO, shapeInfo);
            
                for (int j = 0; j < blendShape->GetBlendShapeChannelCount(); ++j)
                {
                    FbxBlendShapeChannel* channel = blendShape->GetBlendShapeChannel(j);
                    if (!channel) continue;
            
                    String channelInfo = "  Channel: ";
                    channelInfo += channel->GetName();
                    log->Write(LOG_INFO, channelInfo);
            
                    for (int k = 0; k < channel->GetTargetShapeCount(); ++k)
                    {
                        FbxShape* shape = channel->GetTargetShape(k);
                        String targetInfo = "    Target Shape: ";
                        targetInfo += shape->GetName();
                        targetInfo += " (Vertices: ";
                        targetInfo += String(shape->GetControlPointsCount());
                        targetInfo += ")";
                        log->Write(LOG_INFO, targetInfo);
            
                        // Здесь можно получить вершины shape->GetControlPoints()
                    }
                }
            }
        }

        auto* cache = context->GetSubsystem<ResourceCache>();
        auto* material = cache->GetResource<Material>("Materials/Stone.xml");
        if (material) 
        {
            geom->SetMaterial(material);
        } else 
        {
            log->Write(LOG_ERROR, "Failed to load material!");
        }
        geom->Commit();
        if (!geom->GetNumVertices(0)) log->Write(LOG_WARNING, "CustomGeometry has 0 vertices!");
    }

    // Рекурсивно обходим дочерние узлы
    for (int i = 0; i < fbxNode->GetChildCount(); ++i)
    {
        if (!LoadFBXRecursive(context, thisNode, fbxNode->GetChild(i)))
        {
            return false;
        }
    }
    return true;
}


SharedPtr<Node> LoadFBXToNode(Context* context, const String& fbxPath)
{
    Log* log = context->GetSubsystem<Log>();
    FbxManager* manager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);

    FbxScene* scene = FbxScene::Create(manager, "scene");

    String path = context->GetSubsystem<FileSystem>()->GetProgramDir() + fbxPath;
    FbxImporter* importer = FbxImporter::Create(manager, "");
    if (!importer->Initialize(path.CString(), -1, manager->GetIOSettings()))
    {
        log->Write(LOG_ERROR, String("FBX Import Error: ") + importer->GetStatus().GetErrorString());
        return SharedPtr<Node>();
    }

    importer->Import(scene);
    importer->Destroy();

    FbxGeometryConverter converter(manager);
    converter.Triangulate(scene, true);

    SharedPtr<Node> rootNode(new Node(context));

    FbxNode* fbxRoot = scene->GetRootNode();
    if (fbxRoot)
    {
        if (!LoadFBXRecursive(context, rootNode, fbxRoot))
        {
            return SharedPtr<Node>();
        }
    }

    scene->Destroy();
    manager->Destroy();

    return rootNode;
}

