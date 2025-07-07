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
float panima::Channel::GetMaxTime() const
{
	if(GetTimeCount() == 0)
		return 0.f;
	return *GetTimesArray().GetBack<float>();
}
void panima::Channel::MergeValues(const Channel &other)
{
	if(!udm::is_convertible(other.GetValueType(), GetValueType()))
		return;
	auto startTime = other.GetMinTime();
	auto endTime = other.GetMaxTime();
	if(!ClearRange(startTime, endTime))
		return;
	auto indicesStart = FindInterpolationIndices(startTime, startTime);
	auto startIdx = indicesStart.second;
	if(startIdx == std::numeric_limits<uint32_t>::max()) {
		if(!GetTimesArray().IsEmpty())
			return;
		startIdx = 0;
	}
	auto &times = GetTimesArray();
	auto &timesOther = other.GetTimesArray();
	auto &values = GetValueArray();
	auto &valuesOther = other.GetValueArray();
	times.AddValueRange(startIdx, other.GetValueCount());
	values.AddValueRange(startIdx, other.GetValueCount());
	UpdateLookupCache();
	memcpy(times.GetValuePtr(startIdx), const_cast<udm::Array &>(other.GetTimesArray()).GetValuePtr(0), timesOther.GetSize() * timesOther.GetValueSize());
	if(other.GetValueType() == GetValueType()) {
		// Same value type, just copy
		memcpy(values.GetValuePtr(startIdx), const_cast<udm::Array &>(other.GetValueArray()).GetValuePtr(0), valuesOther.GetSize() * valuesOther.GetValueSize());
		return;
	}
	// Values have to be converted
	udm::visit_ng(GetValueType(), [this, &other, startIdx, &values, &valuesOther](auto tag) {
		using T = typename decltype(tag)::type;
		udm::visit_ng(other.GetValueType(), [this, &other, startIdx, &values, &valuesOther](auto tag) {
			using TOther = typename decltype(tag)::type;
			if constexpr(udm::is_convertible<TOther, T>()) {
				auto n = other.GetValueCount();
				for(auto i = decltype(n) {0u}; i < n; ++i)
					values.GetValue<T>(startIdx + i) = udm::convert<TOther, T>(valuesOther.GetValue<TOther>(i));
			}
		});
	});
}
void panima::Channel::ClearAnimationData()
{
	GetTimesArray().Resize(0);
	GetValueArray().Resize(0);
	UpdateLookupCache();
}
bool panima::Channel::ClearRange(float startTime, float endTime, bool addCaps)
{
	if(GetTimesArray().IsEmpty())
		return true;
	auto minTime = *GetTime(0);
	auto maxTime = *GetTime(GetTimeCount() - 1);
	if(endTime < startTime || (startTime < minTime - TIME_EPSILON && endTime < minTime - TIME_EPSILON) || (startTime > maxTime + TIME_EPSILON && endTime > maxTime + TIME_EPSILON))
		return false;
	startTime = umath::clamp(startTime, minTime, maxTime);
	endTime = umath::clamp(endTime, minTime, maxTime);
	float t;
	auto indicesStart = FindInterpolationIndices(startTime, t);
	auto startIdx = (t < TIME_EPSILON) ? indicesStart.first : indicesStart.second;
	auto indicesEnd = FindInterpolationIndices(endTime, t);
	auto endIdx = (t > (1.f - TIME_EPSILON)) ? indicesEnd.second : indicesEnd.first;
	if(startIdx == std::numeric_limits<uint32_t>::max() || endIdx == std::numeric_limits<uint32_t>::max() || endIdx < startIdx)
		return false;
	udm::visit_ng(GetValueType(), [this, startTime, endTime, startIdx, endIdx, addCaps](auto tag) {
		using T = typename decltype(tag)::type;
		if constexpr(is_animatable_type(udm::type_to_enum<T>())) {
			auto startVal = GetInterpolatedValue<T>(startTime);
			auto endVal = GetInterpolatedValue<T>(endTime);

			auto &times = GetTimesArray();
			auto &values = GetValueArray();
			times.RemoveValueRange(startIdx, (endIdx - startIdx) + 1);
			values.RemoveValueRange(startIdx, (endIdx - startIdx) + 1);
			UpdateLookupCache();

			if(addCaps) {
				AddValue<T>(startTime, startVal);
				AddValue<T>(endTime, endVal);
				UpdateLookupCache();
			}
		}
	});
	return true;
}
void panima::Channel::ClearValueExpression() { m_valueExpression = nullptr; }
bool panima::Channel::TestValueExpression(std::string expression, std::string &outErr)
{
	m_valueExpression = nullptr;

	auto expr = std::make_unique<expression::ValueExpression>(*this);
	expr->expression = std::move(expression);
	if(!expr->Initialize(GetValueType(), outErr))
		return false;
	return true;
}
bool panima::Channel::SetValueExpression(std::string expression, std::string &outErr)
{
	m_valueExpression = nullptr;

	auto expr = std::make_unique<expression::ValueExpression>(*this);
	expr->expression = std::move(expression);
	if(!expr->Initialize(GetValueType(), outErr))
		return false;
	m_valueExpression = std::move(expr);
	return true;
}
const std::string *panima::Channel::GetValueExpression() const
{
	if(m_valueExpression)
		return &m_valueExpression->expression;
	return nullptr;
}
void panima::Channel::MergeDataArrays(uint32_t n0, const float *times0, const uint8_t *values0, uint32_t n1, const float *times1, const uint8_t *values1, std::vector<float> &outTimes, const std::function<uint8_t *(size_t)> &fAllocateValueData, size_t valueStride)
{
	outTimes.resize(n0 + n1);
	auto *times = outTimes.data();
	auto *values = fAllocateValueData(outTimes.size());
	size_t idx0 = 0;
	size_t idx1 = 0;
	size_t outIdx = 0;
	while(idx0 < n0 || idx1 < n1) {
		if(idx0 < n0) {
			if(idx1 >= n1 || times0[idx0] < times1[idx1]) {
				if(outIdx == 0 || umath::abs(times0[idx0] - times[outIdx - 1]) > TIME_EPSILON) {
					times[outIdx] = times0[idx0];
					memcpy(values + outIdx * valueStride, values0 + idx0 * valueStride, valueStride);
					++outIdx;
				}
				++idx0;
				continue;
			}
		}

		assert(idx1 < n1);
		if(outIdx == 0 || umath::abs(times1[idx1] - times[outIdx - 1]) > TIME_EPSILON) {
			times[outIdx] = times1[idx1];
			memcpy(values + outIdx * valueStride, values1 + idx1 * valueStride, valueStride);
			++outIdx;
		}
		++idx1;
	}
	outTimes.resize(outIdx);
	fAllocateValueData(outIdx);
}