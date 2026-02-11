#pragma once
#include <game_assets/ObjectFactory.h>

class eMainContextBase;
class CharacterScript;

//---------------------------------------------------------------
class RPGFactory : public ObjectFactoryBase
{
public:
  RPGFactory(eMainContextBase*, AnimationManagerYAML* _animationManager = nullptr);
  std::shared_ptr<eObject> CreateObjectMain(std::shared_ptr<IModel>,
                                            eObject::RenderType _render_type,
                                            const std::string& _name,
                                            const std::string& _rigger_path,
                                            const std::string& _collider_path,
                                            ColliderType = ColliderType::BOX);

  std::shared_ptr<eObject> CreateObjectStairsScript(std::shared_ptr<IModel>,
                                                    eObject::RenderType _render_type,
                                                    const std::string& _name);

  std::shared_ptr<eObject> CreateObjectChairScript(std::shared_ptr<IModel>,
                                                    eObject::RenderType _render_type,
                                                    const std::string& _name);

  std::shared_ptr<eObject> CreateObjectTakebleScript(std::shared_ptr<IModel>,
                                                    eObject::RenderType _render_type,
                                                    const std::string& _name);

  std::shared_ptr<eObject> CreateObjectAnimationScript(std::shared_ptr<IModel>,
                                                     eObject::RenderType _render_type,
                                                     const std::string& _name);

  IScript* GetMainCharacterScript();

protected:
  eMainContextBase* m_game = nullptr;
  IScript* m_crt_script = nullptr;
};