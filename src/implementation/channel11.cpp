// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <udm.hpp>
#include <sharedutils/util_uri.hpp>
#include <sharedutils/util_string.h>
#include <exprtk.hpp>

module panima;

import bezierfit;

import :channel;
import :expression;
uint32_t panima::Channel::GetSize() const { return GetTimesArray().GetSize(); }
void panima::Channel::Resize(uint32_t numValues)
{
	m_times->GetValue<udm::Array>().Resize(numValues);
	m_values->GetValue<udm::Array>().Resize(numValues);
	UpdateLookupCache();
}
void panima::Channel::Update()
{
	m_effectiveTimeFrame = m_timeFrame;
	if(m_effectiveTimeFrame.duration < 0.f)
		m_effectiveTimeFrame.duration = GetMaxTime();
}
size_t panima::Channel::Optimize()
{
	auto numTimes = GetTimeCount();
	constexpr auto EPSILON = 0.001f;
	size_t numRemoved = 0;
	if(numTimes > 2) {
		for(int32_t i = numTimes - 2; i >= 1; --i) {
			auto tPrev = *GetTime(i - 1);
			auto t = *GetTime(i);
			auto tNext = *GetTime(i + 1);

			auto shouldRemove = udm::visit_ng(GetValueType(), [this, tPrev, t, tNext, i](auto tag) -> bool {
				using T = typename decltype(tag)::type;
				if constexpr(is_animatable_type(udm::type_to_enum<T>())) {
					uint32_t pivotTimeIndex = i - 1;
					auto valPrev = GetInterpolatedValue<T>(tPrev, pivotTimeIndex);

					pivotTimeIndex = i;
					auto val = GetInterpolatedValue<T>(t, pivotTimeIndex);

					pivotTimeIndex = i + 1;
					auto valNext = GetInterpolatedValue<T>(tNext, pivotTimeIndex);

					auto f = (t - tPrev) / (tNext - tPrev);
					auto expectedVal = GetInterpolationFunction<T>()(valPrev, valNext, f);
					return uvec::is_equal(val, expectedVal, EPSILON);
				}
				return false;
			});

			if(shouldRemove) {
				// This value is just linearly interpolated between its neighbors,
				// we can remove it.
				RemoveValueAtIndex(i);
				++numRemoved;
			}
		}
	}

	numTimes = GetTimeCount();
	if(numTimes == 2) {
		// If only two values are remaining, we may be able to collapse into a single value (if they are the same)
		udm::visit_ng(GetValueType(), [this, &numRemoved](auto tag) {
			using T = typename decltype(tag)::type;
			if constexpr(is_animatable_type(udm::type_to_enum<T>())) {
				auto &val0 = GetValue<T>(0);
				auto &val1 = GetValue<T>(1);
				if(!uvec::is_equal(val0, val1, EPSILON))
					return;
				RemoveValueAtIndex(1);
				++numRemoved;
			}
		});
	}

	return numRemoved;
}
template<typename T>
bool panima::Channel::DoApplyValueExpression(double time, uint32_t timeIndex, T &inOutVal) const
{
	if(!m_valueExpression)
		return false;
	m_valueExpression->Apply<T>(time, timeIndex, m_effectiveTimeFrame, inOutVal);
	return true;
}
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Int8 &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::UInt8 &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Int16 &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::UInt16 &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Int32 &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::UInt32 &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Int64 &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::UInt64 &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Float &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Double &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Boolean &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Vector2 &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Vector3 &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Vector4 &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Quaternion &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::EulerAngles &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Mat4 &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Mat3x4 &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Vector2i &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Vector3i &) const;
template bool panima::Channel::DoApplyValueExpression(double, uint32_t, udm::Vector4i &) const;

float panima::Channel::GetMinTime() const
{
	if(GetTimeCount() == 0)
		return 0.f;
	return *GetTimesArray().GetFront<float>();
}