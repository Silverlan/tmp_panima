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
#pragma clang optimize off
void panima::Channel::ScaleTimeInRange(float tStart, float tEnd, float tPivot, double scale, bool retainBoundaryValues)
{
	auto [idxStart, idxEnd] = GetBoundaryIndices(tStart, tEnd, retainBoundaryValues);
	if(!idxStart || !idxEnd)
		return;
	tStart = *GetTime(*idxStart);
	tEnd = *GetTime(*idxEnd);

	// After scaling, we need to restore the value at the boundaries where we are scaling
	// away from the rest of the animation
	std::shared_ptr<void> boundaryValueStart = nullptr;
	std::shared_ptr<void> boundaryValueEnd = nullptr;
	if(retainBoundaryValues) {
		auto valIdxStart = *idxStart;
		auto valIdxEnd = *idxEnd;
		udm::visit_ng(GetValueType(), [this, valIdxStart, valIdxEnd, &boundaryValueStart, &boundaryValueEnd](auto tag) {
			using T = typename decltype(tag)::type;
			boundaryValueStart = std::make_shared<T>(GetValue<T>(valIdxStart));
			boundaryValueEnd = std::make_shared<T>(GetValue<T>(valIdxEnd));
		});
	}

	auto rescale = [tPivot, scale](double t) {
		t -= tPivot;
		t *= scale;
		t += tPivot;
		return t;
	};
	if(retainBoundaryValues) {
		auto scaledStart = rescale(tStart);
		if(scaledStart < tStart)
			ClearRange(scaledStart, tStart - TIME_EPSILON * 1.5f, false);

		auto scaledEnd = rescale(tEnd);
		if(scaledEnd > tEnd)
			ClearRange(tEnd + TIME_EPSILON * 1.5f, scaledEnd, false);

		// Indices may have changed due to cleared ranges
		idxStart = FindValueIndex(tStart);
		idxEnd = FindValueIndex(tEnd);
		assert(idxStart.has_value() && idxEnd.has_value());
	}

	auto &times = GetTimesArray();
	if(!idxStart || !idxEnd)
		return;
	// Scale all times within the range [tStart,tEnd]
	for(auto idx = *idxStart; idx <= *idxEnd; ++idx) {
		auto &t = times.GetValue<float>(idx);
		t = rescale(t);
	}
	ResolveDuplicates(*GetTime(*idxStart));
	ResolveDuplicates(*GetTime(*idxEnd));

	if(retainBoundaryValues) {
		// Restore boundary values
		udm::visit_ng(GetValueType(), [this, &boundaryValueStart, boundaryValueEnd, &times, tStart, tEnd, tPivot, scale](auto tag) {
			using T = typename decltype(tag)::type;
			// If the scale is smaller than 1, we'll be pulled towards the pivot, otherwise we will be
			// pushed away from it. In some cases this will create a 'hole' near the boundary that we have to plug.
			// (e.g. If the pivot lies to the right of the start time, and the start time is being pulled towards it (i.e. pivot < 1.0),
			// a hole will be left to the left of the start time.)
			if((scale < 1.0 && tPivot >= tStart) || (scale > 1.0 && tPivot <= tStart))
				AddValue<T>(tStart, *static_cast<T *>(boundaryValueStart.get()));
			if((scale < 1.0 && tPivot <= tEnd) || (scale > 1.0 && tPivot >= tEnd))
				AddValue<T>(tEnd, *static_cast<T *>(boundaryValueEnd.get()));
		});
		ResolveDuplicates(tStart);
		ResolveDuplicates(tEnd);
	}
}
void panima::Channel::Decimate(float tStart, float tEnd, float error)
{
	return udm::visit_ng(GetValueType(), [this, tStart, tEnd, error](auto tag) {
		using T = typename decltype(tag)::type;
		using TValue = std::conditional_t<std::is_same_v<T, bool>, uint8_t, T>;
		if constexpr(is_animatable_type(udm::type_to_enum<TValue>())) {
			std::vector<float> times;
			
			std::vector<TValue> values;
			/*if constexpr(std::is_same_v<TValue, Vector2>)
				values.resize(5, TValue {0.f, 0.f});
			else if constexpr(std::is_same_v<TValue, Vector4>)
				values.resize(5, TValue {0.f, 0.f, 0.f, 0.f});
			else
				values.resize(5, TValue {});*/
			
			GetDataInRange<TValue>(tStart, tEnd, times, values);

			// We need to decimate each component of the value separately, then merge the reduced values
			auto valueType = GetValueType();
			auto numComp = udm::get_numeric_component_count(valueType);
			std::vector<std::vector<float>> newTimes;
			std::vector<std::vector<TValue>> newValues;
			newTimes.resize(numComp);
			if constexpr(std::is_same_v<TValue, Vector2>)
				newValues.resize(numComp, Vector2{0.f,0.f});
			else if constexpr(std::is_same_v<TValue, Vector4>)
				newValues.resize(numComp, Vector4{0.f,0.f});
			else
				newValues.resize(numComp);
			for(auto c = decltype(numComp) {0u}; c < numComp; ++c) {
				std::vector<bezierfit::VECTOR> tmpValues;
				tmpValues.reserve(times.size());
				for(auto i = decltype(times.size()) {0u}; i < times.size(); ++i)
					tmpValues.push_back({times[i], udm::get_numeric_component(values[i], c)});
				auto reduced = bezierfit::reduce(tmpValues, error);

				// Calculate interpolated values for the reduced timestamps
				auto &cValues = newValues[c];
				auto &cTimes = newTimes[c];
				cValues.reserve(reduced.size());
				cTimes.reserve(reduced.size());
				for(auto &v : reduced) {
					auto value = GetInterpolatedValue<TValue>(v.x);
					udm::set_numeric_component(value, c, v.y);
					cTimes.push_back(v.x);
					cValues.push_back(value);
				}
			}

			// Clear values in the target range
			ClearRange(tStart, tEnd, true);

			// Merge components back together
			for(auto c = decltype(numComp) {0u}; c < numComp; ++c) {
				auto &cValues = newValues[c];
				auto &cTimes = newTimes[c];
				InsertValues<TValue>(cTimes.size(), cTimes.data(), cValues.data(), 0.f, InsertFlags::None);
			}
		}
	});
}
