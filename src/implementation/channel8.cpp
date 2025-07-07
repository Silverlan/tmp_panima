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
std::pair<std::optional<uint32_t>, std::optional<uint32_t>> panima::Channel::GetBoundaryIndices(float tStart, float tEnd, bool retainBoundaries)
{
	if(!retainBoundaries) {
		float f;
		auto indicesStart = FindInterpolationIndices(tStart, f, 0);
		if(indicesStart.first == std::numeric_limits<decltype(indicesStart.first)>::max())
			return {};
		f *= (*GetTime(indicesStart.second) - *GetTime(indicesStart.first));
		auto startIdx = (f < TIME_EPSILON) ? indicesStart.first : indicesStart.second;

		auto indicesEnd = FindInterpolationIndices(tEnd, f, startIdx);
		if(indicesEnd.first == std::numeric_limits<decltype(indicesEnd.first)>::max())
			return {};
		f *= (*GetTime(indicesEnd.second) - *GetTime(indicesEnd.first));
		auto endIdx = (umath::abs(f - *GetTime(indicesEnd.second)) < TIME_EPSILON) ? indicesEnd.second : indicesEnd.first;
		auto tStartTs = *GetTime(startIdx);
		auto tEndTs = *GetTime(endIdx);
		auto tMin = tStart - TIME_EPSILON;
		auto tMax = tEnd + TIME_EPSILON;
		if(tStartTs < tMin || tStartTs > tMax || tEndTs < tMin || tEndTs > tMax)
			return {};
		return {startIdx, endIdx};
	}
	std::optional<uint32_t> idxStart = 0;
	auto t0 = GetTime(*idxStart);
	if(!t0)
		return {};
	if(*t0 < tStart - TIME_EPSILON)
		idxStart = InsertSample(tStart);

	std::optional<uint32_t> idxEnd = GetTimeCount() - 1;
	auto t1 = GetTime(*idxEnd);
	if(!t1)
		return {};
	if(*t1 > tEnd + TIME_EPSILON)
		idxEnd = InsertSample(tEnd);
	return {idxStart, idxEnd};
}
void panima::Channel::ShiftTimeInRange(float tStart, float tEnd, float shiftAmount, bool retainBoundaryValues)
{
	if(umath::abs(shiftAmount) <= TIME_EPSILON * 1.5f)
		return;
	auto [idxStart, idxEnd] = GetBoundaryIndices(tStart, tEnd, retainBoundaryValues);
	if(!idxStart || !idxEnd)
		return;
	tStart = *GetTime(*idxStart);
	tEnd = *GetTime(*idxEnd);
	// After shifting, we need to restore the value at the opposite boundary of the shift
	// (e.g. if we're shifting left, we have to restore the right-most value and vice versa.)
	std::shared_ptr<void> boundaryValue = nullptr;
	if(retainBoundaryValues) {
		auto valIdxStart = *idxStart;
		auto valIdxEnd = *idxEnd;
		udm::visit_ng(GetValueType(), [this, &boundaryValue, shiftAmount, valIdxStart, valIdxEnd](auto tag) {
			using T = typename decltype(tag)::type;
			auto val = GetValue<T>((shiftAmount < 0.f) ? valIdxEnd : valIdxStart);
			boundaryValue = std::make_shared<T>(val);
		});
		if(shiftAmount < 0) {
			// Clear the shift range, but keep the value at tStart. We use epsilon *1.5 to prevent
			// potential edge cases because of precision errors
			ClearRange(tStart + shiftAmount - TIME_EPSILON * 1.5f, tStart - TIME_EPSILON * 1.5f, false);
		}
		else
			ClearRange(tEnd + TIME_EPSILON * 1.5f, tEnd + shiftAmount + TIME_EPSILON * 1.5f, false);

		// Indices may have changed due to cleared ranges
		idxStart = FindValueIndex(tStart);
		idxEnd = FindValueIndex(tEnd);
		assert(idxStart.has_value() && idxEnd.has_value());
	}

	auto &times = GetTimesArray();
	if(!idxStart || !idxEnd)
		return;
	for(auto idx = *idxStart; idx <= *idxEnd; ++idx) {
		auto &t = times.GetValue<float>(idx);
		t += shiftAmount;
	}
	ResolveDuplicates(*GetTime(*idxStart));
	ResolveDuplicates(*GetTime(*idxEnd));
	if(retainBoundaryValues) {
		// Restore boundary value
		udm::visit_ng(GetValueType(), [this, &boundaryValue, shiftAmount, tStart, tEnd](auto tag) {
			using T = typename decltype(tag)::type;
			AddValue<T>((shiftAmount < 0.f) ? tEnd : tStart, *static_cast<T *>(boundaryValue.get()));
		});
		ResolveDuplicates(tStart);
		ResolveDuplicates(tEnd);
	}
}