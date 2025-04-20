#pragma once

#include <Urho3D/Scene/Node.h>
#include <Urho3D/Container/Ptr.h>
#include <Urho3D/Core/Object.h>

namespace Urho3D {
    class Context;
}

Urho3D::SharedPtr<Urho3D::Node> LoadFBXToNode(Urho3D::Context* context, const Urho3D::String& path);
