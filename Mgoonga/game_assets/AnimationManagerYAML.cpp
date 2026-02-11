#include "AnimationManagerYAML.h"
#include "YamlTyps.h"

#include <math/RigAnimator.h>
#include <math/BvhImporter.h>

#include <algorithm>
#include <fstream>

#include <opengl_assets/AssimpLoader.h>

//--------------------------------------------------------------------------------------------------------------
static void SerializeAnimation(YAML::Emitter& _out, const SkeletalAnimation& _animation)
{
  _out << YAML::BeginMap; //Animation
  _out << YAML::Key << "Name" << YAML::Value << _animation.GetName();
  _out << YAML::Key << "Duration" << YAML::Value << const_cast<SkeletalAnimation&>(_animation).GetDuration();
  _out << YAML::Key << "FixHips" << YAML::Value << const_cast<SkeletalAnimation&>(_animation).GetFixHipsZ();
  _out << YAML::Key << "FixHipsY" << YAML::Value << const_cast<SkeletalAnimation&>(_animation).GetFixHipsY();

  _out << YAML::Key << "Frames" << YAML::BeginSeq;
  for (unsigned int i = 0; i < _animation.GetNumFrames(); ++i)
  {
    Frame frame = _animation.GetFrameByNumber(i);
    _out << YAML::BeginMap; // Frames
    _out << YAML::Key << "TimeStamp" << YAML::Value << frame.m_timeStamp;
    _out << YAML::Key << "Positions" << YAML::BeginSeq;
    for (auto& p : frame.m_pose)
    {
      _out << YAML::BeginMap; //Pos
      _out << YAML::Key << "BoneName" << YAML::Value << p.first;
      _out << YAML::Key << "Translation" << YAML::Value << p.second.getTranslation();
      _out << YAML::Key << "Rotation" << YAML::Value << p.second.getRotation();
      _out << YAML::Key << "Scale" << YAML::Value << p.second.getScaleAsVector();
      _out << YAML::EndMap; //Pos
    }
    _out << YAML::EndSeq;
    _out << YAML::EndMap;// Frames
  }

  _out << YAML::EndSeq;
  _out << YAML::EndMap; //Animation
}

//-----------------------------------------------------------
AnimationManagerYAML::AnimationManagerYAML(const std::string& _asset_path)
  : m_asset_path(_asset_path)
{
}

//-----------------------------------------------------------
void AnimationManagerYAML::AddAnimation(const SkeletalAnimation& _animation)
{
  m_animations.push_back(_animation);
}

//-----------------------------------------------------------
void AnimationManagerYAML::RemoveAnimation(const std::string& _name)
{
  auto it = std::find_if(m_animations.begin(), m_animations.end(), [_name](const SkeletalAnimation& _animation) { return _animation.GetName() == _name; });
    if(it != m_animations.end())
      m_animations.erase(std::remove(m_animations.begin(), m_animations.end(), *it));
}

//-----------------------------------------------------------
std::optional<SkeletalAnimation> AnimationManagerYAML::GetAnimation(const std::string& _name) const
{
  if (auto it = std::find_if(m_animations.begin(), m_animations.end(), [_name](const SkeletalAnimation& _anim) { return _anim.GetName() == _name; });
    it != m_animations.end())
    return *it;
  else
    return std::nullopt;
}

//-----------------------------------------------------------
bool AnimationManagerYAML::LoadBvh(const std::string& _filepath, const std::string& _name)
{
  m_animations.push_back(dbb::ImportBvh(m_asset_path + _filepath, _name));

  std::vector<std::string> difs;
  for (const auto& pos : m_animations.back().GetCurrentFrame().m_pose)
  {
    if (m_animations.front().GetCurrentFrame().m_pose.find(pos.first) == m_animations.front().GetCurrentFrame().m_pose.end())
      difs.push_back(pos.first);
  }
  return true; //@improve add checks
}

//-----------------------------------------------------------
bool AnimationManagerYAML::LoadAnimation(const std::string& _filepath, const std::string& _name)
{
  AssimpLoader loader;
  m_animations.push_back(loader.ImportAnimation(m_asset_path + _filepath));
  if(_name!= "")
    m_animations.back().SetName(_name);
  return true; //@improve add checks
}

//-----------------------------------------------------------
void AnimationManagerYAML::Serialize(const std::string& _filepath)
{
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << "AnimationData" << YAML::Value << "Unnamed";
  out << YAML::Key << "Animations" << YAML::Value << YAML::BeginSeq;
  for (auto& animation : m_animations)
  {
    SerializeAnimation(out, animation);
  }
  out << YAML::EndSeq;
  out << YAML::EndMap;

  std::ofstream fout(_filepath);
  fout << out.c_str();
}

//-----------------------------------------------------------
bool AnimationManagerYAML::Deserialize(const std::string& _filepath)
{
  std::ifstream stream(_filepath);
  std::stringstream strstream;
  strstream << stream.rdbuf();

  YAML::Node data = YAML::Load(strstream.str());
  if (!data["AnimationData"])
    return false;

  auto serialized_animations = data["Animations"];
  if (serialized_animations)
  {
    for (auto serialized_animation : serialized_animations)
    {
      int64_t duration; std::vector<Frame> frames; std::string name; bool fix_hips_z; bool fix_hips_y;
      name = serialized_animation["Name"].as<std::string>();
      duration = serialized_animation["Duration"].as<int64_t>();
      fix_hips_z = serialized_animation["FixHips"].as<bool>();
      fix_hips_y = serialized_animation["FixHipsY"].as<bool>();
      auto framesComponent = serialized_animation["Frames"];
      if (framesComponent)
      {
        for (auto frame : framesComponent)
        {
          int64_t timeStamp = frame["TimeStamp"].as<int64_t>();
          std::map<std::string, Transform> pose;
          for (auto pos : frame["Positions"])
          {
            std::string bone = pos["BoneName"].as<std::string>();
            Transform transform;
            transform.setTranslation(pos["Translation"].as<glm::vec3>());
            transform.setRotation(pos["Rotation"].as<glm::quat>());
            transform.setScale(pos["Scale"].as<glm::vec3>());
            pose.insert({ bone, transform });
          }
          frames.push_back(Frame{ timeStamp, pose });
        }
      }
      m_animations.emplace_back(duration, frames, name);
      m_animations.back().GetFixHipsZ() = fix_hips_z;
      m_animations.back().GetFixHipsY() = fix_hips_y;
    }
  }
  return true;
}

//------------------------------------------------------------------------
static void _SerializeBone(YAML::Emitter& _out, const IBone* _bone)
{
  _out << YAML::BeginMap; //Bone
  _out << YAML::Key << "BoneName" << YAML::Value << _bone->GetName();
  _out << YAML::Key << "BoneID" << YAML::Value << _bone->GetID();
  _out << YAML::Key << "BindTransform" << YAML::Value << _bone->GetLocalBindTransform();
  _out << YAML::Key << "MTransform" << YAML::Value << _bone->GetMTransform();
  _out << YAML::Key << "IsRealBone" << YAML::Value << _bone->IsRealBone();
  _out << YAML::Key << "HasExplicitInverseBind" << YAML::Value << _bone->HasExplicitInverseBind();
  _out << YAML::Key << "InverseBindTransform" << YAML::Value << _bone->GetInverseBindTransform();

  if (!_bone->GetChildren().empty())
  {
    _out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
    for (auto* child : _bone->GetChildren())
    {
      _out << YAML::BeginMap;
      _out << YAML::Key << "BoneName" << YAML::Value << child->GetName();
      _out << YAML::Key << "BoneID" << YAML::Value << child->GetID();
      _out << YAML::EndMap;
    }
    _out << YAML::EndSeq;
  }
  _out << YAML::EndMap; //Bone
}

//-----------------------------------------------------------
void AnimationManagerYAML::SerializeRigger(const RigAnimator* _riger, const std::string& _filepath)
{
  YAML::Emitter out;
  out << YAML::BeginMap; // Rigger
  out << YAML::Key << "Rigger" << YAML::Value << "232311132";
  out << YAML::Key << "RootBoneName" << YAML::Value << _riger->GetNameRootBone();
  out << YAML::Key << "GlobalModelTransform" << YAML::Value << _riger->GetGlobalModelTransform();

  out << YAML::Key << "Bones" << YAML::Value << YAML::BeginSeq;
  for (auto& bone : _riger->GetBones())
  {
    _SerializeBone(out, &bone);
  }
  out << YAML::EndSeq;

  out << YAML::Key << "Animations" << YAML::Value << YAML::BeginSeq;
  for (auto& animation : _riger->GetAnimations())
  {
    out << animation.second->GetName();
  }
  out << YAML::EndSeq;

  out << YAML::EndMap; // Rigger
  std::ofstream fout(_filepath);
  fout << out.c_str();
}

//-----------------------------------------------------------
IRigger* AnimationManagerYAML::DeserializeRigger(const std::string& _filepath)
{
  std::ifstream stream(_filepath);
  std::stringstream strstream;
  strstream << stream.rdbuf();

  YAML::Node data = YAML::Load(strstream.str());
  if (!data["Rigger"])
    return nullptr;

  std::string rootBoneName = data["RootBoneName"].as<std::string>();
  glm::mat4 globaltransform = data["GlobalModelTransform"].as<glm::mat4>();

  std::vector<Bone> bones;
  bones.reserve(100); //@todo!
  std::map<Bone*, std::vector<unsigned int>> bone_children_map;
  auto serialized_bones = data["Bones"];
  for (auto serialized_bone : serialized_bones)
  {
    auto name = serialized_bone["BoneName"].as<std::string>();
    auto id = serialized_bone["BoneID"].as<unsigned int>();
    glm::mat4 bindTransform = serialized_bone["BindTransform"].as<glm::mat4>();
    glm::mat4 mTransform = serialized_bone["MTransform"].as<glm::mat4>();
    bool isReal = serialized_bone["IsRealBone"].as<bool>();
    bool hasExplicit = false;
    glm::mat4 invBind = glm::mat4(1.0f);

    if (serialized_bone["HasExplicitInverseBind"])
      hasExplicit = serialized_bone["HasExplicitInverseBind"].as<bool>();

    if (serialized_bone["InverseBindTransform"])
      invBind = serialized_bone["InverseBindTransform"].as<glm::mat4>();

    std::vector<unsigned int> children;
    auto childrenComponent = serialized_bone["Children"];
    if (childrenComponent)
    {
      for (auto child : childrenComponent)
        children.push_back(child["BoneID"].as<unsigned int>());
    }
    bones.push_back(Bone(id, name, bindTransform, isReal));
    bone_children_map.insert({ &bones.back(), children });
    bones.back().SetMTransform(mTransform);
    if (hasExplicit)
      bones.back().SetInverseBindTransform(invBind);
  }
  for (auto& bone_node : bone_children_map)
  {
    for (auto& child_index : bone_node.second)
    {
      auto childIter = std::find_if(bones.begin(), bones.end(), [child_index](const Bone& bone)
        { return bone.GetID() == child_index; });
      bone_node.first->AddChild(&(*childIter));
    }
  }
  std::vector<SkeletalAnimation> animations;
  auto animationsComponent = data["Animations"];
  if (animationsComponent)
  {
    for (auto animation : animationsComponent)
    {
      std::optional<SkeletalAnimation> anim = this->GetAnimation(animation.as<std::string>());
      if(anim.has_value())
        animations.push_back(*anim);
    }
  }
  IRigger* rigger = new RigAnimator(animations, bones, rootBoneName, globaltransform, _filepath);
  return rigger;
}
