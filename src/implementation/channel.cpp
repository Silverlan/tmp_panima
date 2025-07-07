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


panima::Channel::Channel() : m_times {::udm::Property::Create(udm::Type::ArrayLz4)}, m_values {::udm::Property::Create(udm::Type::ArrayLz4)}
{
	UpdateLookupCache();
	GetTimesArray().SetValueType(udm::Type::Float);
}
panima::Channel::Channel(const udm::PProperty &times, const udm::PProperty &values) : m_times {times}, m_values {values} { UpdateLookupCache(); }
panima::Channel::Channel(Channel &other) : Channel {} { operator=(other); }
panima::Channel::~Channel() {}
panima::Channel &panima::Channel::operator=(Channel &other)
{
	interpolation = other.interpolation;
	targetPath = other.targetPath;
	m_times = other.m_times->Copy(true);
	m_values = other.m_values->Copy(true);
	m_valueExpression = nullptr;
	if(other.m_valueExpression)
		m_valueExpression = std::make_unique<expression::ValueExpression>(*other.m_valueExpression);
	m_timeFrame = other.m_timeFrame;
	m_effectiveTimeFrame = other.m_effectiveTimeFrame;
	UpdateLookupCache();
	return *this;
}
bool panima::Channel::Save(udm::LinkedPropertyWrapper &prop) const
{
	prop["interpolation"] = interpolation;
	prop["targetPath"] = targetPath.ToUri();
	if(m_valueExpression)
		prop["expression"] = m_valueExpression->expression;

	prop["times"] = m_times;
	prop["values"] = m_values;
	return true;
}
bool panima::Channel::Load(udm::LinkedPropertyWrapper &prop)
{
	prop["interpolation"](interpolation);
	std::string targetPath;
	prop["targetPath"](targetPath);
	this->targetPath = std::move(targetPath);

	auto *el = prop.GetValuePtr<udm::Element>();
	if(!el)
		return false;
	auto itTimes = el->children.find("times");
	auto itValues = el->children.find("values");
	if(itTimes == el->children.end() || itValues == el->children.end())
		return false;
	m_times = itTimes->second;
	m_values = itValues->second;
	UpdateLookupCache();

	// Note: Expression has to be loaded *after* the values, because
	// it's dependent on the value type
	auto udmExpression = prop["expression"];
	if(udmExpression) {
		std::string expr;
		udmExpression(expr);
		std::string err;
		if(SetValueExpression(expr, err) == false)
			; // TODO: Print warning?
	}
	return true;
}









