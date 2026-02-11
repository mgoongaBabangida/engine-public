#pragma once
#include "stdafx.h"

#include "game_assets.h"

#include <math/SkeletalAnimation.h>

#include <optional>

class RigAnimator;

//--------------------------------------------------------------
class DLL_GAME_ASSETS AnimationManagerYAML
{
public:
  explicit AnimationManagerYAML(const std::string& _asset_path);

  void AddAnimation(const SkeletalAnimation&);
  void RemoveAnimation(const std::string& _name);

  std::optional<SkeletalAnimation> GetAnimation(const std::string& _name) const;

  bool LoadBvh(const std::string& _filepath, const std::string& _name);
  bool LoadAnimation(const std::string& _filepath, const std::string& _name = "");

  void Serialize(const std::string& _filepath);
  bool Deserialize(const std::string& _filepath);

  void SerializeRigger(const RigAnimator* _riger, const std::string& _filepath);
  IRigger* DeserializeRigger(const std::string& _filepath);

protected:
  std::vector<SkeletalAnimation> m_animations;
  std::string m_asset_path;
};
