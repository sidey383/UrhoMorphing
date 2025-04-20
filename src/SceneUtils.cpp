#include "SceneUtils.h"

#include "MorphGeometry.h"

#include <Urho3D/IO/Log.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Graphics/CustomGeometry.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Octree.h>

using namespace Urho3D;

void traverse(Urho3D::Log* log, Node* node, int depth) 
{
    String indent(' ', depth * 2);
    String info = indent + "Node: " + node->GetName();
    log->Write(LOG_INFO, info);

    // Проверка на наличие компонентов CustomGeometry или StaticModel
    do {
        for (Component* component : node->GetComponents()) {
            if (component->GetTypeName() == String("StaticModel")) {
                StaticModel* model = static_cast<StaticModel*>(component);
                auto* res = model->GetModel();
                String meshInfo = indent + "  StaticModel attached: ";
                meshInfo += res ? res->GetName() : "null model";
                log->Write(LOG_INFO, meshInfo);
            } else {
                log->Write(LOG_INFO, indent + "  Component " + component->GetTypeName() + " attached.");
            }
        }
    } while (false);


    // Рекурсивно обходим детей
    for (Node* child : node->GetChildren())
        traverse(log, child, depth + 1);
}

void LogSceneContents(Urho3D::Log* log, Urho3D::Scene* scene) 
{
    if (!scene)
    {
        log->Write(LOG_WARNING, "Scene is not initialized.");
        return;
    }

    log->Write(LOG_INFO, "== Urho3D Scene Contents ==");
    traverse(log, scene, 0);
    log->Write(LOG_INFO, "== End of Scene ==");
}