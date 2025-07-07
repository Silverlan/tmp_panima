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
uint32_t panima::Channel::InsertValues(uint32_t n, const float *times, const void *values, size_t valueStride, float offset, InsertFlags flags)
{
	if(n == 0)
		return std::numeric_limits<uint32_t>::max();
	if(offset != 0.f) {
		std::vector<float> timesWithOffset;
		timesWithOffset.resize(n);
		for(auto i = decltype(n) {0u}; i < n; ++i)
			timesWithOffset[i] = times[i] + offset;
		return InsertValues(n, timesWithOffset.data(), values, valueStride, 0.f);
	}
	if(umath::is_flag_set(flags, InsertFlags::ClearExistingDataInRange) == false) {
		auto tStart = times[0];
		auto tEnd = times[n - 1];
		return udm::visit_ng(GetValueType(), [this, tStart, tEnd, n, times, values, valueStride, offset, flags](auto tag) {
			using T = typename decltype(tag)::type;
			using TValue = std::conditional_t<std::is_same_v<T, bool>, uint8_t, T>;
			std::vector<float> newTimes;
			std::vector<TValue> newValues;
			GetDataInRange(tStart, tEnd, newTimes, newValues);

			std::vector<float> mergedTimes;
			std::vector<TValue> mergedValues;
			MergeDataArrays(
			  newTimes.size(), newTimes.data(), reinterpret_cast<uint8_t *>(newValues.data()), n, times, static_cast<const uint8_t *>(values), mergedTimes,
			  [&mergedValues](size_t size) -> uint8_t * {
				  mergedValues.resize(size);
				  return reinterpret_cast<uint8_t *>(mergedValues.data());
			  },
			  sizeof(TValue));

			auto newFlags = flags;
			umath::set_flag(newFlags, InsertFlags::ClearExistingDataInRange);
			return InsertValues(mergedTimes.size(), mergedTimes.data(), mergedValues.data(), valueStride, offset, newFlags);
		});
	}
	auto startTime = times[0];
	auto endTime = times[n - 1];
	ClearRange(startTime - TIME_EPSILON, endTime + TIME_EPSILON, false);
	float f;
	auto indices = FindInterpolationIndices(times[0], f);
	auto startIndex = indices.second;
	if(startIndex == std::numeric_limits<decltype(startIndex)>::max() || startTime > *GetTime(indices.second))
		startIndex = GetValueCount();
	auto numCurValues = GetValueCount();
	auto numNewValues = numCurValues + n;
	Resize(numCurValues + n);

	auto &timesArray = GetTimesArray();
	auto &valueArray = GetValueArray();
	auto *pValues = static_cast<const uint8_t *>(values);

	// Move up current values
	for(auto i = static_cast<int64_t>(numNewValues - 1); i >= startIndex + n; --i) {
		auto iSrc = i - n;
		auto iDst = i;
		timesArray.SetValue(iDst, static_cast<const void *>(timesArray.GetValuePtr(iSrc)));
		valueArray.SetValue(iDst, static_cast<const void *>(valueArray.GetValuePtr(iSrc)));
	}

	// Assign new values
	for(auto i = startIndex; i < (startIndex + n); ++i) {
		auto iSrc = i - startIndex;
		auto iDst = i;
		timesArray.SetValue(iDst, times[i - startIndex]);
		valueArray.SetValue(iDst, static_cast<const void *>(pValues));
		pValues += valueStride;
	}

	if(umath::is_flag_set(flags, InsertFlags::DecimateInsertedData))
		Decimate(startTime, endTime);
	return startIndex;
}
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
void panima::Channel::UpdateLookupCache()
{
	m_timesArray = m_times->GetValuePtr<udm::Array>();
	m_valueArray = m_values->GetValuePtr<udm::Array>();
	m_timesData = !m_timesArray->IsEmpty() ? m_timesArray->GetValuePtr<float>(0) : nullptr;
	m_valueData = !m_valueArray->IsEmpty() ? m_valueArray->GetValuePtr(0) : nullptr;

	if(m_timesArray->GetArrayType() == udm::ArrayType::Compressed)
		static_cast<udm::ArrayLz4 *>(m_timesArray)->SetUncompressedMemoryPersistent(true);
	if(m_valueArray->GetArrayType() == udm::ArrayType::Compressed)
		static_cast<udm::ArrayLz4 *>(m_valueArray)->SetUncompressedMemoryPersistent(true);
}
udm::Array &panima::Channel::GetTimesArray() { return *m_timesArray; }
udm::Array &panima::Channel::GetValueArray() { return *m_valueArray; }
udm::Type panima::Channel::GetValueType() const { return GetValueArray().GetValueType(); }
void panima::Channel::SetValueType(udm::Type type) { GetValueArray().SetValueType(type); }