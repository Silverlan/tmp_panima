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
bool panima::Channel::Validate() const
{
	std::vector<float> times;
	auto numTimes = GetTimeCount();
	times.reserve(numTimes);
	for(auto i = decltype(numTimes) {0u}; i < numTimes; ++i)
		times.push_back(*GetTime(i));
	if(times.size() <= 1)
		return true;
	for(auto i = decltype(times.size()) {1u}; i < times.size(); ++i) {
		auto t0 = times[i - 1];
		auto t1 = times[i];
		if(t0 >= t1) {
			throw std::runtime_error {"Time values are not in order!"};
			const_cast<panima::Channel *>(this)->ResolveDuplicates(t0);
			return false;
		}
		auto diff = t1 - t0;
		if(fabsf(diff) < TIME_EPSILON * 0.5f) {
			throw std::runtime_error {"Time values are too close!"};
			return false;
		}
	}
	return true;
}
void panima::Channel::TimeToLocalTimeFrame(float &inOutT) const
{
	inOutT -= m_timeFrame.startOffset;
	if(m_timeFrame.duration >= 0.f)
		inOutT = umath::min(inOutT, m_timeFrame.duration);
	inOutT *= m_timeFrame.scale;
}
std::pair<uint32_t, uint32_t> panima::Channel::FindInterpolationIndices(float t, float &interpFactor, uint32_t pivotIndex, uint32_t recursionDepth) const
{
	constexpr uint32_t MAX_RECURSION_DEPTH = 2;
	auto &times = GetTimesArray();
	if(pivotIndex >= times.GetSize() || times.GetSize() < 2 || recursionDepth == MAX_RECURSION_DEPTH)
		return FindInterpolationIndices(t, interpFactor);
	// We'll use the pivot index as the starting point of our search and check out the times immediately surrounding it.
	// If we have a match, we can return immediately. If not, we'll slightly broaden the search until we've reached the max recursion depth or found a match.
	// If we hit the max recusion depth, we'll just do a regular binary search instead.
	auto tPivot = times.GetValue<float>(pivotIndex);
	auto tLocal = t;
	TimeToLocalTimeFrame(tLocal);
	if(tLocal >= tPivot) {
		if(pivotIndex == times.GetSize() - 1) {
			interpFactor = 0.f;
			return {static_cast<uint32_t>(GetValueArray().GetSize() - 1), static_cast<uint32_t>(GetValueArray().GetSize() - 1)};
		}
		auto tPivotNext = times.GetValue<float>(pivotIndex + 1);
		if(tLocal < tPivotNext) {
			// Most common case
			interpFactor = (tLocal - tPivot) / (tPivotNext - tPivot);
			return {pivotIndex, pivotIndex + 1};
		}
		return FindInterpolationIndices(t, interpFactor, pivotIndex + 1, recursionDepth + 1);
	}
	if(pivotIndex == 0) {
		interpFactor = 0.f;
		return {0u, 0u};
	}
	return FindInterpolationIndices(t, interpFactor, pivotIndex - 1, recursionDepth + 1);
}
std::pair<uint32_t, uint32_t> panima::Channel::FindInterpolationIndices(float t, float &interpFactor, uint32_t pivotIndex) const { return FindInterpolationIndices(t, interpFactor, pivotIndex, 0u); }

std::pair<uint32_t, uint32_t> panima::Channel::FindInterpolationIndices(float t, float &interpFactor) const
{
	auto &times = GetTimesArray();
	auto numTimes = times.GetSize();
	if(numTimes == 0) {
		interpFactor = 0.f;
		return {std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()};
	}
	// Binary search
	TimeToLocalTimeFrame(t);
	auto it = std::upper_bound(begin(times), end(times), t);
	if(it == end(times)) {
		interpFactor = 0.f;
		return {static_cast<uint32_t>(numTimes - 1), static_cast<uint32_t>(numTimes - 1)};
	}
	if(it == begin(times)) {
		interpFactor = 0.f;
		return {0u, 0u};
	}
	auto itPrev = it - 1;
	interpFactor = (t - *itPrev) / (*it - *itPrev);
	return {static_cast<uint32_t>(itPrev - begin(times)), static_cast<uint32_t>(it - begin(times))};
}

std::optional<size_t> panima::Channel::FindValueIndex(float time, float epsilon) const
{
	auto &t = GetTimesArray();
	auto size = t.GetSize();
	if(size == 0)
		return {};
	float interpFactor;
	auto indices = FindInterpolationIndices(time, interpFactor);
	if(indices.first == std::numeric_limits<decltype(indices.first)>::max())
		return {};
	if(interpFactor == 0.f && indices.first == indices.second) {
		if(indices.first == 0 && umath::abs(t.GetValue<float>(0) - time) >= epsilon)
			return {};
		if(indices.first == size - 1 && umath::abs(t.GetValue<float>(size - 1) - time) >= epsilon)
			return {};
	}
	interpFactor *= (*GetTime(indices.second) - *GetTime(indices.first));
	if(interpFactor < epsilon)
		return indices.first;
	if(interpFactor > 1.f - epsilon)
		return indices.second;
	return {};
}

uint32_t panima::Channel::GetTimeCount() const { return GetTimesArray().GetSize(); }
uint32_t panima::Channel::GetValueCount() const { return GetValueArray().GetSize(); }
std::optional<float> panima::Channel::GetTime(uint32_t idx) const
{
	auto &times = GetTimesArray();
	auto n = times.GetSize();
	return (idx < n) ? times.GetValue<float>(idx) : std::optional<float> {};
}