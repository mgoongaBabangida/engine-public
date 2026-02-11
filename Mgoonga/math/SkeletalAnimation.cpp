#include "stdafx.h"

#include "SkeletalAnimation.h"
#include <fstream>
#include <iomanip>

// Helpers
//-----------------------------------------------------------------------------------------------------------------------
Bone* FindBoneByName(const std::vector<Bone>& skeleton, const std::string& name);
glm::vec3 ExtractTranslation(const glm::mat4& mat);
float ComputeBoneLength(const Bone& bone); // distance from this bone to first child

//------------------------------------------------------------------------------------------------------------------------
Frame SkeletalAnimation::Interpolate(const Frame& _first, const Frame& _second, int64_t _animationTime, float _progression)
{
	//interpolation
	//float progression = (time - _first.timeStamp) / (_second.timeStamp - _first.timeStamp);
	/*std::cout << "Debug progression " << progression <<" "<< i << std::endl;
	std::cout << "i " << i << std::endl;*/

	std::map<std::string, Transform> newFrame;
	for (auto& entry : _first.m_pose)
	{
		auto dest = *(_second.m_pose.find(entry.first));
		Transform trans = Transform::interpolate(entry.second, dest.second, _progression);
		newFrame.insert(std::pair<std::string, Transform>(entry.first, trans));
	}
	return Frame(_animationTime * _progression, newFrame);
}

//-------------------------------------------------------------------------------------------------------------------------
SkeletalAnimation SkeletalAnimation::CreateByInterpolation(const Frame& _first, const Frame& _last, int64_t _animationTime, 
																													 int64_t _numFrames, const std::string& _name)
{
	std::vector<Frame> frames;
	for (int64_t i = 0; i < _numFrames; ++i)
	{
		// N frames, exclude both endpoints:
		float t = float(i + 1) / float(_numFrames + 1);   // i = 0..N-1  =>  t = 1/(N+1) .. N/(N+1)
		frames.push_back(Interpolate(_first, _last, _animationTime, t));
	}
	return SkeletalAnimation(_animationTime, frames, _name);
}

//-------------------------------------------------------------------------------------
SkeletalAnimation SkeletalAnimation::Retarget(const std::vector<Bone>& sourceSkeleton, const std::vector<Bone>& targetSkeleton, const SkeletalAnimation& sourceAnim)
{
	std::vector<Frame> target_frames;
	for (const auto& frame : sourceAnim.m_frames)
	{
		Frame newFrame;
		newFrame.m_timeStamp = frame.m_timeStamp;

		for (const Bone& targetBone : targetSkeleton)
		{
			Transform newTransform;
			Bone* sourceBone = FindBoneByName(sourceSkeleton, targetBone.GetName());
			if (sourceBone && frame.m_pose.find(sourceBone->GetName()) != frame.m_pose.end())
			{
				const Transform& srcTransform = frame.m_pose.at(sourceBone->GetName());

				// Compute bone length ratio
				float sourceLen = ComputeBoneLength(*sourceBone);
				float targetLen = ComputeBoneLength(targetBone);
				float ratio = (sourceLen > 0.001f) ? (targetLen / sourceLen) : 1.0f;

				newTransform.setTranslation(srcTransform.getTranslation() * ratio);
				newTransform.setRotation(srcTransform.getRotation());
				newTransform.setScale(srcTransform.getScaleAsVector());
			}
			else
			{
				// No source bone: use identity
				newTransform = Transform();
			}
			newFrame.m_pose[targetBone.GetName()] = newTransform;
		}
		target_frames.push_back(newFrame);
	}
	return SkeletalAnimation { sourceAnim.m_duration , target_frames , sourceAnim.m_name + "_retargeted" };
}

//-------------------------------------------------------------------------------------
const Frame& SkeletalAnimation::GetCurrentFrame()
{
	if (m_freeze_frame != std::numeric_limits<size_t>::max())
	{
		return m_freeze_frame < m_frames.size() ? m_frames[m_freeze_frame] : m_frames[m_frames.size()-1];
	}

	if (m_clock.timeElapsedMsc() > m_duration * m_speed)
	{
		if (m_play_once)
			m_clock.pause();
		else
			m_clock.restart();
	}

	int64_t time = m_clock.timeElapsedMsc();
	size_t i = 0;

	for (; i < m_frames.size() - 1; ++i)
	{
		if(m_frames[i].m_timeStamp * m_speed > time) //should be sorted
			break;
	}
	m_cur_frame_index = i;
	return m_frames[i];

	//interpolation
	//float progression = (time - frames[i - 1].timeStamp) / (frames[i].timeStamp - frames[i - 1].timeStamp);	
	/*std::cout << "Debug progression " << progression <<" "<< i << std::endl;
	std::cout << "i " << i << std::endl;*/

	//std::map<std::string, Transform> newFrame;
	////names !?
	//for (auto & entry : frames[i - 1].pose)
	//{
	//	auto dest = *(frames[i].pose.Find(entry.first));
	//	Transform trans = Transform::interpolate(entry.second, dest.second, progression);
	//	newFrame.insert(std::pair<std::string, Transform>(entry.first, trans));
	//}

	//return Frame(time, newFrame);
}

//-------------------------------------------------------------------------------------
const Frame& SkeletalAnimation::GetFrameByNumber(size_t _num) const
{
	if (_num < m_frames.size())
		return m_frames[_num];
	else
		return m_frames[m_frames.size() - 1];
}

//-------------------------------------------------------------------------------------
size_t SkeletalAnimation::GetCurFrameIndex() const
{
	return m_freeze_frame != std::numeric_limits<size_t>::max() ? m_freeze_frame : m_cur_frame_index;
}

//-------------------------------------------------------------------------------------
void SkeletalAnimation::Start()
{
	m_play_once = false;
	/*if (m_cur_frame_index == std::numeric_limits<size_t>::max())*/
	m_cur_frame_index = 0; // start always from o frame
	m_clock.restart();
}

//-------------------------------------------------------------------------------------
void SkeletalAnimation::Stop()
{
	m_clock.pause();
}

//-------------------------------------------------------------------------------------
void SkeletalAnimation::Continue()
{
	m_clock.goOn();
}

//-------------------------------------------------------------------------------------
bool SkeletalAnimation::IsPaused()
{
	return m_clock.isPaused();
}

//-------------------------------------------------------------------------------------
void SkeletalAnimation::PlayOnce()
{
	m_play_once = true;
	m_clock.restart();
}

//-------------------------------------------------------------------------------------
void SkeletalAnimation::FreezeFrame(size_t _frame)
{
	m_freeze_frame = _frame;
}

//-------------------------------------------------------------------------------------
const std::string& SkeletalAnimation::GetName() const
{
	return m_name;
}

//-------------------------------------------------------------------------------------
void SkeletalAnimation::SetName(const std::string& n)
{
	m_name = n;
}

//-------------------------------------------------------------------------------------
void SkeletalAnimation::Debug()
{
	std::cout << "--------Time-----" << " " << std::endl;
	for (auto &fr : m_frames)
	{
		for (auto& ps : fr.m_pose)
		{
			std::cout << ps.first << " " << std::endl;
			std::cout << ps.second.getModelMatrix()[0][0] <<" " << ps.second.getModelMatrix()[0][1]<< " " << ps.second.getModelMatrix()[0][2] << " " << ps.second.getModelMatrix()[0][3] <<std::endl;
			std::cout << ps.second.getModelMatrix()[1][0] << " " << ps.second.getModelMatrix()[1][1] << " " << ps.second.getModelMatrix()[1][2] << " " << ps.second.getModelMatrix()[1][3] << std::endl;
			std::cout << ps.second.getModelMatrix()[2][0] << " " << ps.second.getModelMatrix()[2][1] << " " << ps.second.getModelMatrix()[2][2] << " " << ps.second.getModelMatrix()[2][3] << std::endl;
		}
	}
}

//------------------------------------------------------------------------------
Bone* FindBoneByName(const std::vector<Bone>& skeleton, const std::string& name)
{
	for (const auto& b : skeleton)
		if (b.GetName() == name) return const_cast<Bone*>(&b);
	return nullptr;
}

//------------------------------------------------------------------------------
float ComputeBoneLength(const Bone& bone)
{
	if (!bone.GetChildren().empty())
	{
		glm::vec3 parentPos = ExtractTranslation(bone.GetLocalBindTransform());
		glm::vec3 childPos = ExtractTranslation(bone.GetChildren()[0]->GetLocalBindTransform());
		return glm::length(childPos - parentPos);
	}
	return 1.0f; // default length
}

//------------------------------------------------------------------------------
glm::vec3 ExtractTranslation(const glm::mat4& mat) { return glm::vec3(mat[3]); }

//----------------------------------------------------------------------------------
void SkeletalAnimation::GetOffsetsFromSkeleton(const std::vector<Bone>& skeleton)
{
	std::map<std::string, glm::vec3> offsets;

	// Step 1: Build name → offset map from skeleton
	std::function<void(const IBone&)> collect;
	collect = [&](const IBone& bone) {
		offsets[bone.GetName()] = glm::vec3(bone.GetLocalBindTransform()[3]);
		for (const IBone* child : bone.GetChildren()) {
			if (child) collect(*child);
		}
	};

	for (const Bone& b : skeleton)
		collect(b);

	// Step 2: Apply fallback translation to each frame
	for (Frame& frame : m_frames)
	{
		for (auto& [joint, t] : frame.m_pose)
		{
			glm::vec3 pos = t.getTranslation();

			// Apply offset only if translation is zero or very small
			if (glm::length(pos) < 0.001f && offsets.count(joint))
				t.setTranslation(offsets[joint]);
		}
	}
}

//-----------------------------------------------
void SkeletalAnimation::ResetPlaybackState()
{
	m_play_once = false;
	m_cur_frame_index = 0;
	m_freeze_frame = std::numeric_limits<size_t>::max();
	m_clock.reset();
}

//Frame CrossFadeFrames(const Frame& from, const Frame& to, float alpha) {
//	Frame result;
//	result.m_timeStamp = glm::mix(float(from.m_timeStamp), float(to.m_timeStamp), alpha);
//
//	std::set<std::string> allJoints;
//	for (const auto& [name, _] : from.m_pose) allJoints.insert(name);
//	for (const auto& [name, _] : to.m_pose) allJoints.insert(name);
//
//	for (const auto& joint : allJoints) {
//		Transform ta = from.m_pose.count(joint) ? from.m_pose.at(joint) : Transform{};
//		Transform tb = to.m_pose.count(joint) ? to.m_pose.at(joint) : Transform{};
//
//		Transform blended;
//		blended.m_translation = glm::mix(ta.m_translation, tb.m_translation, alpha);
//		blended.q_rotation = glm::slerp(ta.q_rotation, tb.q_rotation, alpha);
//		blended.m_scale = glm::mix(ta.m_scale, tb.m_scale, alpha);
//
//		blended.totalTransform = glm::translate(glm::mat4(1.0f), blended.m_translation) *
//			glm::toMat4(blended.q_rotation) *
//			glm::scale(glm::mat4(1.0f), blended.m_scale);
//
//		result.m_pose[joint] = blended;
//	}
//
//	return result;
//}

// Assume frames are sorted by m_timeStamp
//Frame GetInterpolatedFrame(const SkeletalAnimation& anim, int64_t timeMs) {
//	if (anim.m_frames.empty()) return {};
//
//	// Loop to find frame pair (a, b) such that: a.time <= timeMs <= b.time
//	for (size_t i = 0; i < anim.m_frames.size() - 1; ++i) {
//		const Frame& a = anim.m_frames[i];
//		const Frame& b = anim.m_frames[i + 1];
//
//		if (timeMs >= a.m_timeStamp && timeMs <= b.m_timeStamp) {
//			float alpha = float(timeMs - a.m_timeStamp) / float(b.m_timeStamp - a.m_timeStamp);
//			return InterpolateFrame(a, b, alpha);
//		}
//	}
//
//	// If out of bounds, return last frame
//	return anim.m_frames.back();
//}