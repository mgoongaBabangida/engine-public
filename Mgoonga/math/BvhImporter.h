#pragma once
#include "stdafx.h"

#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/quaternion.hpp>
#include <glm/glm/gtx/quaternion.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>

#include "Transform.h"
#include "SkeletalAnimation.h"

namespace dbb 
{
//------------------------------------------------------------
struct ChannelInfo
{
  std::string jointName;
  std::string property; // "Xposition", "Yrotation", etc.
};

//------------------------------------------------------------
struct SkeletalAnimationInfo
{
  int64_t m_duration;
  std::vector<Frame> m_frames;
};

//--------------------------------------------------------------------------------------------------------
void ParseJoint(std::ifstream& file, std::string& currentLine,
  std::vector<ChannelInfo>& channelOrder,
  std::map<std::string, glm::vec3>& jointOffsets)
{
  std::istringstream ss(currentLine);
  std::string jointType, jointName;
  ss >> jointType >> jointName;

  //// Remove colon prefix if needed
  //if (jointName.find(':') != std::string::npos) {
  //  jointName = jointName.substr(jointName.find(':') + 1);
  //}

  // Expect next line to be "{"
  std::getline(file, currentLine);
  if (currentLine.find("{") == std::string::npos) return;

  // Read until matching "}"
  while (std::getline(file, currentLine)) {
    if (currentLine.find("OFFSET") != std::string::npos) {
      std::istringstream os(currentLine);
      std::string label;
      float x, y, z;
      os >> label >> x >> y >> z;
      jointOffsets[jointName] = glm::vec3(x, y, z);
    }
    else if (currentLine.find("CHANNELS") != std::string::npos) {
      std::istringstream cs(currentLine);
      std::string temp;
      int channelCount;
      cs >> temp >> channelCount;
      for (int i = 0; i < channelCount; ++i) {
        std::string ch;
        cs >> ch;
        channelOrder.push_back({ jointName, ch });
      }
    }
    else if (currentLine.find("JOINT") != std::string::npos || currentLine.find("End Site") != std::string::npos) {
      ParseJoint(file, currentLine, channelOrder, jointOffsets); // Recursive call
    }
    else if (currentLine.find("}") != std::string::npos) {
      break;
    }
  }
}

//--------------------------------------------------------------
int GetAxisIndex(const std::string& property)
{
  if (property[0] == 'X') return 0;
  if (property[0] == 'Y') return 1;
  if (property[0] == 'Z') return 2;
  return -1;
}

// Simple parser assuming correct BVH format:
//------------------------------------------------------------
SkeletalAnimation ImportBvh(const std::string& path, const std::string& name)
{
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open BVH file.");
  }

  SkeletalAnimationInfo animation;
  std::vector<ChannelInfo> channelOrder;
  std::map<std::string, glm::vec3> jointOffsets;

  std::string line;
  bool inMotionSection = false;

  // Parsing hierarchy
  while (std::getline(file, line))
  {
    if (line.find("ROOT") != std::string::npos)
      ParseJoint(file, line, channelOrder, jointOffsets);
    else if (line.find("MOTION") != std::string::npos)
      break;
  }

  // Parse motion header
  int frameCount = 0;
  float frameTime = 0.0f;
  while (std::getline(file, line))
  {
    if (line.find("Frames:") != std::string::npos)
    {
      std::istringstream ss(line);
      std::string temp;
      ss >> temp >> frameCount;
    }
    else if (line.find("Frame Time:") != std::string::npos)
    {
      std::istringstream ss(line);
      std::string temp;
      ss >> temp >> temp >> frameTime;
      break;
    }
  }

  // Parse each frame
  for (int i = 0; i < frameCount; ++i)
  {
    std::getline(file, line);
    std::istringstream ss(line);

    Frame frame;
    frame.m_timeStamp = static_cast<int64_t>(i * frameTime * 1000);

    std::map<std::string, Transform> jointTransforms;
    std::map<std::string, glm::vec3> positions;
    std::map<std::string, glm::vec3> rotations;

    for (const auto& ch : channelOrder)
    {
      float value;
      ss >> value;

      int axis = GetAxisIndex(ch.property);
      if (axis != -1)
      {
        if (ch.property.find("position") != std::string::npos) {
          positions[ch.jointName][ch.property[0] - 'X'] = value;
        }
        else if (ch.property.find("rotation") != std::string::npos) {
          rotations[ch.jointName][ch.property[0] - 'X'] = glm::radians(value);
        }
      }
    }

    std::set<std::string> allJoints;
    for (const auto& [name, _] : positions) allJoints.insert(name);
    for (const auto& [name, _] : rotations) allJoints.insert(name);

    for (const auto& jointName : allJoints)
    {
      Transform t;
      glm::vec3 finalPos;
      if (positions.count(jointName))
        finalPos = positions[jointName]; // Use animated translation only
      else
        finalPos = jointOffsets.count(jointName) ? jointOffsets[jointName] : glm::vec3(0.0f);
      t.setTranslation(finalPos);

      glm::vec3 rot = rotations.count(jointName) ? rotations[jointName] : glm::vec3(0.0f);
      glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), rot.z, glm::vec3(0, 0, 1));
      glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), rot.x, glm::vec3(1, 0, 0));
      glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), rot.y, glm::vec3(0, 1, 0));
      glm::mat4 rotationMat = rotZ * rotX * rotY;
      glm::quat correctedQuat = glm::quat_cast(rotationMat);
      t.setRotation(correctedQuat);

      frame.m_pose[jointName] = t;
    }
    animation.m_frames.push_back(frame);
  }

  // @todo name normalization
  for (auto& frame : animation.m_frames)
  {
    std::map<std::string, Transform> updated;
    for (const auto& [key, value] : frame.m_pose)
    {
      std::string new_key = key;
      std::replace(new_key.begin(), new_key.end(), ':', '_');
      updated[new_key] = value;
    }
    frame.m_pose = updated;
  }

  animation.m_duration = static_cast<int>(frameCount * frameTime * 1000);
  return { animation.m_duration, animation.m_frames, name };
}

}