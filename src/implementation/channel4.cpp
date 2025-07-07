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
panima::ChannelValueSubmitter::ChannelValueSubmitter(const std::function<void(Channel &, uint32_t &, double)> &submitter) : submitter {submitter} {}
panima::ChannelValueSubmitter::operator bool() const { return submitter && enabled; }
bool panima::ChannelValueSubmitter::operator==(const std::nullptr_t &t) const { return submitter == nullptr; }
bool panima::ChannelValueSubmitter::operator!=(const std::nullptr_t &t) const { return submitter != nullptr; }
void panima::ChannelValueSubmitter::operator()(Channel &channel, uint32_t &timestampIndex, double t) { submitter(channel, timestampIndex, t); }
std::ostream &operator<<(std::ostream &out, const panima::Channel &o)
{
	out << "Channel";
	out << "[Path:" << o.targetPath << "]";
	out << "[Interp:" << magic_enum::enum_name(o.interpolation) << "]";
	out << "[Values:" << o.GetValueArray().GetSize() << "]";

	std::pair<float, float> timeRange {0.f, 0.f};
	auto n = o.GetTimeCount();
	if(n > 0)
		out << "[TimeRange:" << *o.GetTime(0) << "," << *o.GetTime(n - 1) << "]";
	return out;
}
std::ostream &operator<<(std::ostream &out, const panima::TimeFrame &o)
{
	out << "TimeFrame";
	out << "[StartOffset:" << o.startOffset << "]";
	out << "[Scale:" << o.scale << "]";
	out << "[Duration:" << o.duration << "]";
	return out;
}
std::ostream &operator<<(std::ostream &out, const panima::ChannelPath &o)
{
	out << "ChannelPath";
	out << "[" << o.ToUri() << "]";
	return out;
}