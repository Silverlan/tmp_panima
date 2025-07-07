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

uint32_t panima::Channel::AddValue(float t, const void *value)
{
	float interpFactor;
	auto indices = FindInterpolationIndices(t, interpFactor);
	if(indices.first == std::numeric_limits<decltype(indices.first)>::max()) {
		auto size = GetSize() + 1;
		Resize(size);
		auto idx = size - 1;
		GetTimesArray()[idx] = t;
		GetValueArray()[idx] = value;
		return idx;
	}
	if(umath::abs(t - *GetTime(indices.first)) < VALUE_EPSILON) {
		// Replace value at first index with new value
		auto idx = indices.first;
		GetTimesArray()[idx] = t;
		GetValueArray()[idx] = value;
		return idx;
	}
	if(umath::abs(t - *GetTime(indices.second)) < VALUE_EPSILON) {
		// Replace value at second index with new value
		auto idx = indices.second;
		GetTimesArray()[idx] = t;
		GetValueArray()[idx] = value;
		return idx;
	}
	auto &times = GetTimesArray();
	auto &values = GetValueArray();
	if(indices.first == indices.second) {
		if(indices.first == 0 && t < times.GetValue<float>(0)) {
			// New time value preceeds first time value in time array, push front
			auto idx = indices.first;
			times.InsertValue(idx, t);
			udm::visit_ng(GetValueType(), [&values, idx, value](auto tag) {
				using T = typename decltype(tag)::type;
				values.InsertValue(idx, *static_cast<const T *>(value));
			});
			UpdateLookupCache();
			return idx;
		}
		// New time value exceeds last time value in time array, push back
		auto idx = indices.second + 1;
		times.InsertValue(idx, t);
		udm::visit_ng(GetValueType(), [&values, idx, value](auto tag) {
			using T = typename decltype(tag)::type;
			values.InsertValue(idx, *static_cast<const T *>(value));
		});
		UpdateLookupCache();
		return idx;
	}
	// Insert new value between the two indices
	auto idx = indices.second;
	times.InsertValue(idx, t);
	udm::visit_ng(GetValueType(), [&values, idx, value](auto tag) {
		using T = typename decltype(tag)::type;
		values.InsertValue(idx, *static_cast<const T *>(value));
	});
	UpdateLookupCache();
	return idx;
}
