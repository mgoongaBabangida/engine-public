#pragma once
#include "game_assets.h"
#include <base/Object.h>

class IModel;
class AnimationManagerYAML;

enum class ColliderType
{
  BOX,
  DYNAMIC_BOX,
  MESH
};

//--------------------------------------------------------------------------------------------
class DLL_GAME_ASSETS ObjectFactoryBase
{
public:
  explicit ObjectFactoryBase(AnimationManagerYAML* _animationManager = nullptr); //@ game instead of anim manager ?

  std::unique_ptr<eObject> CreateObject(std::shared_ptr<IModel>,
                                        eObject::RenderType _render_type,
                                        const std::string& _name = std::string("default"));

  std::unique_ptr<eObject> CreateObject(std::shared_ptr<IModel>,
                                        eObject::RenderType _render_type,
                                        const std::string& _name,
                                        const std::string& _rigger_path,
                                        const std::string& _collider_path,
                                        ColliderType = ColliderType::BOX);

  void SaveDynamicCollider(shObject, const std::string& _collider_path);
protected:
  AnimationManagerYAML* m_animationManager = nullptr;
};
