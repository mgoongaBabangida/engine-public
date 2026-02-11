#ifndef SCELETAL_ANIMATION_H 
#define SCELETAL_ANIMATION_H

#include "stdafx.h"
#include <base/interfaces.h>

#include "Transform.h"
#include "Clock.h"
#include "Bone.h"
#include "Frame.h"

//---------------------------------------------------------------------
class DLL_MATH SkeletalAnimation : public IAnimation
{
public:
	friend class RigAnimator;

	SkeletalAnimation(int64_t dur, const std::vector<Frame> &  frames ,const std::string _name)
		: m_duration(static_cast<int>(dur)),
		  m_frames(frames),
		  m_name(_name)
	{ Start(); } //Start?
	
	SkeletalAnimation(const SkeletalAnimation& _other)
	{
		*this = _other;
	}

	SkeletalAnimation& operator=(const SkeletalAnimation& _other)
	{ 
		if (&_other != this)
		{
			m_duration = _other.m_duration;
			m_frames = _other.m_frames;
			m_name = _other.m_name;
			m_freeze_frame = _other.m_freeze_frame;
			m_cur_frame_index = _other.m_cur_frame_index;
			m_fix_hips_z_movement = _other.m_fix_hips_z_movement;
			m_fix_hips_y_movement = _other.m_fix_hips_y_movement;
		}
		return *this;
	}
	bool operator==(const SkeletalAnimation& other) { return m_name == other.m_name && m_duration == other.m_duration; }
	
	static Frame							Interpolate(const Frame&, const Frame&, int64_t _animationTime, float _progression);
	static SkeletalAnimation	CreateByInterpolation(const Frame& _first, const Frame& _last, int64_t _animationTime, int64_t _numFrames, const std::string& _name);
	static SkeletalAnimation	Retarget(const std::vector<Bone>& sourceSkeleton, const std::vector<Bone>& targetSkeleton, const SkeletalAnimation& sourceAnim);

	const Frame&	GetCurrentFrame();
	const Frame&	GetFrameByNumber(size_t _num) const;

	size_t				GetCurFrameIndex() const ;
	bool&					GetFixHipsZ() { return m_fix_hips_z_movement; }
	bool&					GetFixHipsY() { return m_fix_hips_y_movement; }

	virtual void Start() override;
	virtual void Stop() override;
	virtual void Continue() override;
	virtual bool IsPaused() override;

	virtual int64_t		GetNumFrames() const  override { return m_frames.size(); }
	virtual int&			GetDuration() override { return m_duration; }
	virtual float&		GetSpeed() override { return m_speed; }

	void PlayOnce();
	void FreezeFrame(size_t);

	virtual const std::string&	GetName() const		override;
	void												SetName(const std::string& n);

	void GetOffsetsFromSkeleton(const std::vector<Bone>& skeleton);

	void ResetPlaybackState();

	void Debug();

protected:
  math::eClock					m_clock;
  std::vector<Frame>		m_frames;
	int										m_duration; // msc
  std::string						m_name;

	bool									m_play_once = false;
	size_t								m_freeze_frame = std::numeric_limits<size_t>::max();
	size_t								m_cur_frame_index = std::numeric_limits<size_t>::max();
	bool									m_fix_hips_z_movement = false;
	bool									m_fix_hips_y_movement = false;
	float									m_speed = 1.0f;
};

#endif 

