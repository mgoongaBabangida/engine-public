#include "AnimationUtils.h"
#include <regex>
#include <set>

//-------------------------------------------------------------------------------------------
std::string NormalizeBoneName(const std::string& name)
{
  std::string result = name;

  // Remove known prefixes like "mixamorig_" or "mixamorig:"
  static const std::regex prefixRegex(R"(^(?:mixamorig[:_]))", std::regex::icase);
  result = std::regex_replace(result, prefixRegex, "");

  // Remove suffixes like "$Rotation", "$Position", etc.
  static const std::regex suffixRegex(R"(\$(Rotation|Position|Scale)$)", std::regex::icase);
  result = std::regex_replace(result, suffixRegex, "");

  return result;
}

//-------------------------------------------------------------------------
bool IsStandardSkeleton(const std::vector<Bone>& skeleton)
{
  // check if it has the minimum set of bones
  std::set<std::string> requiredJoints = {
      "Hips", "Spine", "Spine1", "Neck", "Head",
      "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand",
      "RightShoulder", "RightArm", "RightForeArm", "RightHand",
      "LeftUpLeg", "LeftLeg", "LeftFoot",
      "RightUpLeg", "RightLeg", "RightFoot"
  };

  std::set<std::string> skeletonJoints;

  std::function<void(const IBone&)> collect;
  collect = [&](const IBone& bone) {
    skeletonJoints.insert(bone.GetName());
    for (const IBone* child : bone.GetChildren()) {
      if (child) collect(*child);
    }
  };

  for (const Bone& b : skeleton)
    collect(b);

  for (const std::string& joint : requiredJoints) {
    if (skeletonJoints.find(joint) == skeletonJoints.end())
      return false;
  }
  return true;
}

