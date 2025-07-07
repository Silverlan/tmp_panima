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
void panima::Channel::GetDataInRange(float tStart, float tEnd, std::vector<float> *optOutTimes, const std::function<void *(size_t)> &optAllocateValueData) const
{
	if(tEnd < tStart)
		return;
	::udm::visit_ng(GetValueType(), [this, tStart, tEnd, optOutTimes, &optAllocateValueData](auto tag) {
		using T = typename decltype(tag)::type;

		auto n = GetValueCount();
		auto getInterpolatedValue = [this, n](float t, uint32_t &outIdx, bool prefix) -> std::optional<std::pair<float, T>> {
			float f;
			auto indices = FindInterpolationIndices(t, f);
			if(indices.first == std::numeric_limits<decltype(indices.first)>::max()) {
				if(t <= *GetTime(0)) {
					indices = {0, 0};
					f = 0.f;
				}
				else {
					indices = {n - 1, n - 1};
					f = 0.f;
				}
			}

			if(f == 0.f)
				outIdx = indices.first;
			else if(f == 1.f)
				outIdx = indices.second;
			else {
				outIdx = prefix ? indices.second : indices.first;

				auto time0 = *GetTime(indices.first);
				auto time1 = *GetTime(indices.second);
				auto &value0 = GetValue<T>(indices.first);
				auto &value1 = GetValue<T>(indices.second);
				// We have to explicitely construct Vector2 and Vector4 here due to
				// an unresolved symbol compiler bug with clang
				if constexpr(std::is_same_v<T, Vector2>) {
					Vector2 result {0.f, 0.f};
					udm::lerp_value(value0, value1, f, result, udm::type_to_enum<T>());
					return std::pair<float, T> {umath::lerp(time0, time1, f), result};
				}
				else if constexpr(std::is_same_v<T, Vector4>) {
					Vector4 result {0.f, 0.f, 0.f, 0.f};
					udm::lerp_value(value0, value1, f, result, udm::type_to_enum<T>());
					return std::pair<float, T> {umath::lerp(time0, time1, f), result};
				}
				else {
					T result;
					udm::lerp_value(value0, value1, f, result, udm::type_to_enum<T>());
					return std::pair<float, T> {umath::lerp(time0, time1, f), result};
				}
			}
			return {};
		};

		if(n > 0) {
			uint32_t idxStart;
			auto prefixValue = getInterpolatedValue(tStart, idxStart, true);

			uint32_t idxEnd;
			auto postfixValue = getInterpolatedValue(tEnd, idxEnd, false);

			auto count = idxEnd - idxStart + 1;
			if(prefixValue)
				++count;
			if(postfixValue)
				++count;

			float *times = nullptr;
			if(optOutTimes) {
				optOutTimes->resize(count);
				times = optOutTimes->data();
			}
			T *values = nullptr;
			if(optAllocateValueData)
				values = static_cast<T *>(optAllocateValueData(count));
			size_t idx = 0;
			if(prefixValue) {
				if(times)
					times[idx] = prefixValue->first;
				if(values)
					values[idx] = prefixValue->second;
				++idx;
			}
			for(auto i = idxStart; i <= idxEnd; ++i) {
				if(times)
					times[idx] = *GetTime(i);
				if(values)
					values[idx] = GetValue<T>(i);
				++idx;
			}
			if(postfixValue) {
				if(times)
					times[idx] = postfixValue->first;
				if(values)
					values[idx] = postfixValue->second;
				++idx;
			}
		}
	});
}
void panima::Channel::GetTimesInRange(float tStart, float tEnd, std::vector<float> &outTimes) const { GetDataInRange(tStart, tEnd, &outTimes, nullptr); }
void panima::Channel::Decimate(float error)
{
	auto n = GetTimeCount();
	if(n < 2)
		return;
	Decimate(*GetTime(0), *GetTime(n - 1), error);
}
std::optional<uint32_t> panima::Channel::InsertSample(float t)
{
	if(GetTimeCount() == 0)
		return {};
	float f;
	auto idx = FindValueIndex(t);
	if(idx)
		return *idx; // There already is a sample at this timestamp
	return udm::visit_ng(GetValueType(), [this, t](auto tag) -> std::optional<uint32_t> {
		using T = typename decltype(tag)::type;
		if constexpr(is_animatable_type(udm::type_to_enum<T>())) {
			auto val = GetInterpolatedValue<T>(t);
			return AddValue<T>(t, val);
		}
		return {};
	});
}
void panima::Channel::TransformGlobal(const umath::ScaledTransform &transform)
{
	auto valueType = GetValueType();
	auto numTimes = GetTimeCount();
	switch(valueType) {
	case udm::Type::Vector3:
		{
			for(size_t i = 0; i < numTimes; ++i) {
				auto t = *GetTime(i);
				auto &val = GetValue<Vector3>(i);
				val = transform * val;
			}
			break;
		}
	case udm::Type::Quaternion:
		{
			for(size_t i = 0; i < numTimes; ++i) {
				auto t = *GetTime(i);
				auto &val = GetValue<Quat>(i);
				val = transform * val;
			}
			break;
		}
	default:
		break;
	}
}
void panima::Channel::RemoveValueAtIndex(uint32_t idx)
{
	auto &times = GetTimesArray();
	times.RemoveValue(idx);

	auto &values = GetValueArray();
	values.RemoveValue(idx);
	UpdateLookupCache();
}
void panima::Channel::ResolveDuplicates(float t)
{
	for(;;) {
		auto idx = FindValueIndex(t);
		if(!idx)
			break;
		auto t = *GetTime(*idx);
		auto numTimes = GetTimeCount();
		if(*idx > 0) {
			auto tPrev = *GetTime(*idx - 1);
			if(umath::abs(t - tPrev) <= TIME_EPSILON) {
				RemoveValueAtIndex(*idx - 1);
				continue;
			}
		}
		if(numTimes > 1 && *idx < numTimes - 1) {
			auto tNext = *GetTime(*idx + 1);
			if(umath::abs(t - tNext) <= TIME_EPSILON) {
				RemoveValueAtIndex(*idx + 1);
				continue;
			}
		}
		break;
	}
}