#include "FBXLoader.h"
#include "MorphGeometry.h"
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Graphics/CustomGeometry.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Technique.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Core/Context.h>
#include <fbxsdk.h>
#include <Urho3D/Container/Vector.h>

using namespace Urho3D;

struct ControlPointsMorph {
    // Если 0 - изменний не требуется
    // Имеет размер равный количеству контрольных точек
    Vector<Vector3> diff;
    // Название морфа из fbx
    String name;
    // Положение центра для сферической и цилиндрической интерполяции
    Vector3 center;
    // Ось вращения для цилиндрической интерполяции. Если цилиндрическая инетрполяция не используеься - 0
    Vector3 axis;
};

struct ControlPoints {
    // Контрольные точки, используемые fbx
    FbxVector4* points;
    // Количество контрольных точек
    int count;
    // Активыне морфы контрольных точек в fbx
    Vector<ControlPointsMorph> morphs;
};

Vector3 toUrho(FbxVector4 v) {
    return Vector3((float)v[0], (float)v[1], (float)v[2]);
}

ControlPointsMorph LoadPointsMorph(Context* context, FbxBlendShapeChannel* channel, FbxVector4* controlPoints, i32 totalPoints) {
    auto* log = context->GetSubsystem<Log>();
    log->Write(LOG_DEBUG, "RUN LoadPointsMorph");
    ControlPointsMorph morph{
        Vector<Vector3>(totalPoints, Vector3::ZERO),
        channel->GetName(),
        Vector3::ZERO,
        Vector3(100.0f, 100.0f, 100.0f)
    };

    FbxShape* shape = channel->GetTargetShape(0);
    int numVertices = shape->GetControlPointsCount();
    int* indexes = shape->GetControlPointIndices();
    int indexesNum = shape->GetControlPointIndicesCount();
    FbxVector4* shapePoints = shape->GetControlPoints();
    log->Write(LOG_DEBUG, String("Start loading morp \"") + String(channel->GetName()) + String("\" with numVertex=") + String(numVertices) + String(" and num indexes=") + String(indexesNum));
    for (int i = 0; i < numVertices && (i < indexesNum || indexesNum == 0); ++i)
    {
        i32 index;
        if (indexes) {
            index = indexes[i];
        } else {
            index = i;
        }
        FbxVector4 delta = shapePoints[index] - controlPoints[index];
        morph.diff[index] = toUrho(delta);

    }
    log->Write(LOG_DEBUG, "END LoadPointsMorph");
    return morph;
}

ControlPoints LoadControlPointsWithMorphs(Context* context, FbxMesh* fbxMesh) {
    auto* log = context->GetSubsystem<Log>();
    log->Write(LOG_DEBUG, "RUN LoadControlPointsWithMorphs");
    ControlPoints points{
        fbxMesh->GetControlPoints(),
        fbxMesh->GetControlPointsCount(),
        Vector<ControlPointsMorph>()
    };

    for (int deformerIndex = 0; deformerIndex < fbxMesh->GetDeformerCount(); ++deformerIndex)
    {
        FbxDeformer* deformer = fbxMesh->GetDeformer(deformerIndex);
        if (!deformer || deformer->GetDeformerType() != FbxDeformer::EDeformerType::eBlendShape) continue;
        
        FbxBlendShape* blendShape = static_cast<FbxBlendShape*>(deformer);
        for (int channelIndex = 0; channelIndex < blendShape->GetBlendShapeChannelCount(); ++channelIndex)
        {    
            FbxBlendShapeChannel* channel = blendShape->GetBlendShapeChannel(channelIndex);
            if (!channel || channel->GetTargetShapeCount() == 0)
                continue;
            ControlPointsMorph morph = LoadPointsMorph(context, channel, points.points, points.count);
            FbxProperty prop = fbxMesh->FindProperty(("Center_" + String(channel->GetName())).CString());
            if (prop.IsValid()) {
                FbxDouble3 value = prop.Get<FbxDouble3>();
                morph.center = Vector3((float) value[0], (float) value[1], (float) value[2]);
                log->Write(LOG_INFO, "Fund property  " + String(prop.GetName().Buffer()) + " with vector " + String(morph.center));
                prop = fbxMesh->FindProperty(("Axis_" + String(channel->GetName())).CString());
                if (prop.IsValid()) {
                    FbxDouble3 value = prop.Get<FbxDouble3>();
                    morph.axis = Vector3((float) value[0], (float) value[1], (float) value[2]);
                    log->Write(LOG_INFO, "Fund property  " + String(prop.GetName().Buffer()) + " with vector " + String(morph.center));
                } else {
                    morph.axis = Vector3::ZERO;
                }
            }
            points.morphs.Push(morph);
        }
    }
    log->Write(LOG_DEBUG, "End LoadControlPointsWithMorphs");
    return points;
}

void LoadMorphGeometry(Context* context, ControlPoints points, FbxMesh* fbxMesh, MorphGeometry* morphGeometry) {
    auto* log = context->GetSubsystem<Log>();
    log->Write(LOG_INFO, "RUN LoadMorphGeometry");
    Vector<Morpher> morphers{};
    for (auto m : points.morphs) {
        morphers.Push({
            m.name,
            Vector<i32>(),
            Vector<Vector3>(),
            m.center,
            m.axis
        });
    }
    Vector<MorphVertex> vertices;
    Vector<i32> indices;
    for (int i = 0; i < fbxMesh->GetPolygonCount(); ++i)
    {
        int polySize = fbxMesh->GetPolygonSize(i);
        if (polySize != 3)
        {
            log->Write(LOG_ERROR, "Can't process non-triangulated polygon");
            continue;
        }

        for (int j = 0; j < polySize; ++j)
        {
            int ctrlPointIndex = fbxMesh->GetPolygonVertex(i, j);
            FbxVector4 pos = points.points[ctrlPointIndex];
            Vector3 position = toUrho(pos);

            // Нормаль
            Vector3 normal = Vector3::UP;
            FbxVector4 fbxNormal;
            if (fbxMesh->GetPolygonVertexNormal(i, j, fbxNormal))
                normal = toUrho(pos);

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

            vertices.Push(vertex);
            indices.Push(vertices.Size() - 1);
            for (int k = 0; k < points.morphs.Size(); ++k) {
                auto m = points.morphs[k];
                Vector3 diffV = m.diff[ctrlPointIndex];
                if (diffV != Vector3::ZERO) {
                    morphers[k].morphDeltas.Push(diffV);
                    morphers[k].indexes.Push(vertices.Size() - 1);
                }
            }
        }
    }
    morphGeometry->SetVertices(vertices);
    morphGeometry->SetIndices(indices);
    for (auto m : morphers) {
        morphGeometry->AddMorpher(m);
    }

    log->Write(LOG_INFO, "END LoadMorphGeometry");
}

SharedPtr<Node> BuildUrhoGeometryMorphFromFBXMeshNew(Context* context, FbxNode* fbxNode) 
{
    auto* log = context->GetSubsystem<Log>();
    log->Write(LOG_INFO, "RUN BuildUrhoGeometryMorphFromFBXMeshNew");
    FbxMesh* fbxMesh = fbxNode->GetMesh();
    SharedPtr<Node> node(new Node(context));

    ControlPoints controlPoints = LoadControlPointsWithMorphs(context, fbxMesh);
    auto* morphGeometry = node->CreateComponent<MorphGeometry>();
    node->SetName(fbxMesh->GetName());
    LoadMorphGeometry(context, controlPoints, fbxMesh, morphGeometry);

    auto* cache = context->GetSubsystem<ResourceCache>();
    auto* material = cache->GetResource<Material>("Materials/Morph.xml");
    if (material) {
        Technique* tech = material->GetTechnique(0);
        Pass* pass = tech->GetPass(0);
        pass->SetVertexShaderDefines("MORPH_ENABLED");
        morphGeometry->SetMaterial(material);
    }

    morphGeometry->Commit();
    log->Write(LOG_INFO, "END BuildUrhoGeometryMorphFromFBXMesh");
    return node;
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

    log->Write(LOG_INFO, "END ImportFBXScene");
    return scene;
}

void LoadFBXNodeRecursive(Context* context, Node* parentNode, FbxNode* fbxNode, SharedPtr<Node> nodeLoader (Context* context, FbxNode* fbxMesh))
{
    auto* log = context->GetSubsystem<Log>();
    log->Write(LOG_INFO, "RUN LoadFBXNodeRecursive");
    // Создаём новый Urho3D Node с именем из FBX
    Node* node = parentNode->CreateChild(fbxNode->GetName());

    // Загружаем геометрию, если есть FbxMesh
    FbxMesh* fbxMesh = fbxNode->GetMesh();
    if (fbxMesh)
    {
        // Заменяешь на MorphGeometry, если нужно
        SharedPtr<Node> morphGeom = nodeLoader(context, fbxNode);
        if (morphGeom)
            node->AddChild(morphGeom);
    }

    // Рекурсивно обходим дочерние узлы
    for (int i = 0; i < fbxNode->GetChildCount(); ++i)
    {
        FbxNode* childFBX = fbxNode->GetChild(i);
        LoadFBXNodeRecursive(context, node, childFBX, nodeLoader);
    }
    log->Write(LOG_INFO, "END LoadFBXNodeRecursive");
}


SharedPtr<Node> LoadFBXToNode(Context* context, const String& fbxPath)
{
    auto* log = context->GetSubsystem<Log>();
    log->Write(LOG_INFO, "RUN LoadFBXToNode");
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

    SharedPtr<Node> node(new Node(context));
    node->SetName("FBXImpoted");


    SharedPtr<Node> resultMorhp = SharedPtr<Node>(node->CreateChild("Morph"));
    LoadFBXNodeRecursive(context, resultMorhp, scene->GetRootNode(), [](Context* ctx, FbxNode* node) {
        return BuildUrhoGeometryMorphFromFBXMeshNew(ctx, node);
    });
    resultMorhp->SetPosition(Vector3(0, 0, 0));
    manager->Destroy();
    log->Write(LOG_INFO, "END LoadFBXToNode");
    return node;
}

