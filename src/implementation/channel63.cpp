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

udm::Array &panima::Channel::GetTimesArray() { return *m_timesArray; }
udm::Array &panima::Channel::GetValueArray() { return *m_valueArray; }
udm::Type panima::Channel::GetValueType() const { return GetValueArray().GetValueType(); }
void panima::Channel::SetValueType(udm::Type type) { GetValueArray().SetValueType(type); }
