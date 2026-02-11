#include "stdafx.h"
#include "AnimationSocketScript.h"

#include <base/Object.h>
#include <math/RigAnimator.h>

#include "MainContextBase.h"

//---------------------------------------------------
AnimationSocketScript::AnimationSocketScript(eMainContextBase* _game)
  :m_game(_game)
{
}

//---------------------------------------------------
void AnimationSocketScript::Initialize()
{
}

//---------------------------------------------------
void AnimationSocketScript::Update(float _tick)
{
  if (shObject object = m_object.lock(); object && object->GetRigger() != nullptr)
  {
    const auto& sockets = object->GetRigger()->GetSockets();
    if (sockets.size() > sockets_count) //socket added
    {
      for (int i = 0; i < sockets.size() - sockets_count; ++i)
      {
        const AnimationSocket& socket = sockets[sockets.size()-1-i];
        for (auto& obj : m_game->GetObjects())
        {
          if (socket.m_socket_object == obj.get())
          {
            object->AddChildObject(obj);
            m_game->DeleteObject(obj);
          }
        }
      }
    }
    else if (object->GetRigger()->GetSockets().size() < sockets_count) //socket removed
    {
      sockets_count = object->GetRigger()->GetSockets().size();
    }

    if (!sockets.empty())
    {
      for (auto& obj : object->GetChildrenObjects())
      {
        auto socket = std::find_if(sockets.begin(), sockets.end(), [obj](const AnimationSocket& _socket)
          { return _socket.m_socket_object == obj.get(); });
        if (socket != sockets.end())
        {
          glm::mat4 cur_bone_matrix;
          if (socket->m_map_global)
            cur_bone_matrix = dynamic_cast<RigAnimator*>(object->GetRigger())->GetGlobalMatrixForBone(socket->m_bone_name);
          else
            cur_bone_matrix = object->GetRigger()->GetMatrices()[socket->m_bone_id];
          
          glm::mat4 socket_matrix = object->GetTransform()->getModelMatrix() * cur_bone_matrix * socket->m_pre_transform;
          obj->GetTransform()->setModelMatrix(socket_matrix);
        }
      }
    }
  }
}

// to test-> map from global
//const glm::mat4 boneGlobal =
//static_cast<RigAnimator*>(object->GetRigger())->GetGlobalMatrixForBone(socket->m_bone_name);
//
//glm::mat4 socket_matrix = object->GetTransform()->getModelMatrix() * boneGlobal * socket->m_pre_transform;
//obj->GetTransform()->setModelMatrix(socket_matrix);