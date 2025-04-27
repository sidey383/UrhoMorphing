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

const float MODEL_MULTIPLIER = 1.0f;

struct ControlPointsMorph {
    // Если 0 - изменний не требуется
    // Имеет размер равный количеству контрольных точек
    Vector<Vector3> diff;
    // Название морфа из fbx
    String name;
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
    log->Write(LOG_DEBUG, "Start LoadPointsMorph");
    ControlPointsMorph morph{
        Vector<Vector3>(totalPoints, Vector3::ZERO),
        channel->GetName()
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
    log->Write(LOG_DEBUG, "End LoadPointsMorph");
    return morph;
}

ControlPoints LoadControlPointsWithMorphs(Context* context, FbxMesh* fbxMesh) {
    auto* log = context->GetSubsystem<Log>();
    log->Write(LOG_DEBUG, "Start LoadControlPointsWithMorphs");
    ControlPoints points{
        fbxMesh->GetControlPoints(),
        fbxMesh->GetControlPointsCount(),
        Vector<ControlPointsMorph>()
    };

    for (int deformerIndex = 0; deformerIndex < fbxMesh->GetDeformerCount(); ++deformerIndex)
    {
        FbxDeformer* deformer = fbxMesh->GetDeformer(deformerIndex);
        if (!deformer)
            continue;
        FbxBlendShape* blendShape = static_cast<FbxBlendShape*>(deformer);
        for (int channelIndex = 0; channelIndex < blendShape->GetBlendShapeChannelCount(); ++channelIndex)
        {    
            FbxBlendShapeChannel* channel = blendShape->GetBlendShapeChannel(channelIndex);
            if (!channel || channel->GetTargetShapeCount() == 0)
                continue;
            ControlPointsMorph morph = LoadPointsMorph(context, channel, points.points, points.count);
            points.morphs.Push(morph);
        }
    }
    log->Write(LOG_DEBUG, "End LoadControlPointsWithMorphs");
    return points;
}

void LoadMorphGeometry(Context* context, ControlPoints points, FbxMesh* fbxMesh, MorphGeometry* morphGeometry) {
    auto* log = context->GetSubsystem<Log>();
    Vector<Morpher> morphers{};
    for (auto m : points.morphs) {
        morphers.Push({
            m.name,
            Vector<i32>(),
            Vector<Vector3>()
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
            position *= MODEL_MULTIPLIER;

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
}

SharedPtr<Node> BuildUrhoGeometryMorphFromFBXMeshNew(Context* context, FbxMesh* fbxMesh) 
{
    auto* log = context->GetSubsystem<Log>();
    log->Write(LOG_INFO, "RUN BuildUrhoGeometryMorphFromFBXMeshNew");
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
    log->Write(LOG_INFO, "Success compete BuildUrhoGeometryMorphFromFBXMesh");

    return node;
}

SharedPtr<Node> BuildUrhoGeometryMorphFromFBXMesh(Context* context, FbxMesh* fbxMesh)
{
    auto* log = context->GetSubsystem<Log>();
    log->Write(LOG_INFO, "RUN BuildUrhoGeometryMorphFromFBXMesh");

    SharedPtr<Node> node(new Node(context));
    auto* morphGeometry = node->CreateComponent<MorphGeometry>();

    // Извлечение контрольных точек
    FbxVector4* controlPoints = fbxMesh->GetControlPoints();
    int controlPointCount = fbxMesh->GetControlPointsCount();
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
            position *= MODEL_MULTIPLIER;

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

            vertices.Push(vertex);
            indices.Push(vertices.Size() - 1);
        }
    }

        // Извлечение морф-таргета
    log->Write(LOG_INFO, String("Found ") + String(fbxMesh->GetDeformerCount()) + String(" Deformets for ") + fbxMesh->GetName());
    for (int deformerIndex = 0; deformerIndex < fbxMesh->GetDeformerCount(); ++deformerIndex)
    {
        FbxDeformer* deformer = fbxMesh->GetDeformer(deformerIndex);
        if (!deformer)
            continue;

        log->Write(LOG_INFO, String("Load deformer ") + String(deformerIndex) + String(" Name: ") + String(deformer->GetName()));
        FbxBlendShape* blendShape = static_cast<FbxBlendShape*>(deformer);
        

        for (int channelIndex = 0; channelIndex < blendShape->GetBlendShapeChannelCount(); ++channelIndex)
        {    
            FbxBlendShapeChannel* channel = blendShape->GetBlendShapeChannel(channelIndex);
            if (!channel || channel->GetTargetShapeCount() == 0)
                continue;
            log->Write(LOG_INFO, String("Load channel ") + String(channelIndex) + String(" With name: ") + String(channel->GetName()));

            FbxShape* shape = channel->GetTargetShape(0);
            int numVertices = shape->GetControlPointsCount();
            int* indexes = shape->GetControlPointIndices();
            FbxVector4* shapePoints = shape->GetControlPoints();

            if (!indexes) {
                log->Write(LOG_WARNING, String("Can't found indexes for channel ") + String(channelIndex) + String(" With name: ") + String(channel->GetName()));
                break;
            }

            Morpher morpher{
                channel->GetName(),
                Vector<i32>(numVertices, 0),
                Vector<Vector3>(numVertices, Vector3::ZERO)
            };
            log->Write(LOG_DEBUG, String("numVertices ") + String(numVertices));
            for (int i = 0; i < numVertices; ++i)
            {
                FbxVector4 base = controlPoints[i];
                FbxVector4 delta = shapePoints[i] - base;
                morpher.morphDeltas[i] = Vector3((float)delta[0], (float)delta[1], (float)delta[2]);
                morpher.indexes[i] = i;
                // indexes[i];
            }
            morphGeometry->AddMorpher(morpher);
        }
    }

    // Создание узла и компонента MorphGeometry
    morphGeometry->SetVertices(vertices);
    morphGeometry->SetIndices(indices);

    auto* cache = context->GetSubsystem<ResourceCache>();
    auto* material = cache->GetResource<Material>("Materials/Morph.xml");
    if (material) {
        Technique* tech = material->GetTechnique(0);
        Pass* pass = tech->GetPass(0);
        pass->SetVertexShaderDefines("MORPH_ENABLED");
        morphGeometry->SetMaterial(material);
    }

    morphGeometry->SetMorphWeight(0.0f); // Начальный вес морфинга
    morphGeometry->Commit();
    log->Write(LOG_INFO, "Success compete BuildUrhoGeometryMorphFromFBXMesh");

    return node;
}


SharedPtr<Node> BuildUrhoGeometryFromFBXMesh(Context* context, FbxMesh* fbxMesh)
{
    auto* log = context->GetSubsystem<Log>();
    log->Write(LOG_INFO, "RUN BuildUrhoGeometryFromFBXMesh");
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
            context->GetSubsystem<Log>()->Write(LOG_ERROR, "Can't process non-triangulated polygon");
            return SharedPtr<Node>();
        }

        for (int j = 0; j < polySize; ++j)
        {
            int ctrlPointIndex = fbxMesh->GetPolygonVertex(i, j);
            FbxVector4 pos = controlPoints[ctrlPointIndex];
            Vector3 position((float)pos[0], (float)pos[1], (float)pos[2]);
            position *= MODEL_MULTIPLIER;

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

void LoadFBXNodeRecursive(Context* context, Node* parentNode, FbxNode* fbxNode, SharedPtr<Node>  nodeLoader (Context* context, FbxMesh* fbxMesh))
{
    // Создаём новый Urho3D Node с именем из FBX
    Node* node = parentNode->CreateChild(fbxNode->GetName());

    // Загружаем геометрию, если есть FbxMesh
    FbxMesh* fbxMesh = fbxNode->GetMesh();
    if (fbxMesh)
    {
        // Заменяешь на MorphGeometry, если нужно
        SharedPtr<Node> morphGeom = nodeLoader(context, fbxMesh);
        if (morphGeom)
            node->AddChild(morphGeom);
    }

    // Рекурсивно обходим дочерние узлы
    for (int i = 0; i < fbxNode->GetChildCount(); ++i)
    {
        FbxNode* childFBX = fbxNode->GetChild(i);
        LoadFBXNodeRecursive(context, node, childFBX, nodeLoader);
    }
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

    SharedPtr<Node> node(new Node(context));
    node->SetName("FBXImpoted");


    SharedPtr<Node> resultMorhp = SharedPtr<Node>(node->CreateChild("Morph"));
    LoadFBXNodeRecursive(context, resultMorhp, scene->GetRootNode(), [](Context* ctx, FbxMesh* mesh) {
        return BuildUrhoGeometryMorphFromFBXMeshNew(ctx, mesh);
    });
    resultMorhp->SetPosition(Vector3(0, 0, 0));
    node->AddChild(resultMorhp);

    // SharedPtr<Node> resultSimple = SharedPtr<Node>(node->CreateChild("Simple"));
    // LoadFBXNodeRecursive(context, resultSimple, scene->GetRootNode(), [](Context* ctx, FbxMesh* mesh) {
    //     return BuildUrhoGeometryFromFBXMesh(ctx, mesh);
    // });
    // node->AddChild(resultSimple);
    manager->Destroy();
    log->Write(LOG_INFO, "Complete LoadFBXToNode");
    return node;
}

