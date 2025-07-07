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
panima::ArrayFloatIterator::ArrayFloatIterator(float *data) : m_data {data} {}
panima::ArrayFloatIterator &panima::ArrayFloatIterator::operator++()
{
	m_data++;
	return *this;
}
panima::ArrayFloatIterator panima::ArrayFloatIterator::operator++(int)
{
	++m_data;
	return *this;
}
panima::ArrayFloatIterator panima::ArrayFloatIterator::operator+(uint32_t n)
{
	m_data += n;
	return *this;
}
int32_t panima::ArrayFloatIterator::operator-(const ArrayFloatIterator &other) const { return m_data - other.m_data; }
panima::ArrayFloatIterator panima::ArrayFloatIterator::operator-(int32_t idx) const { return ArrayFloatIterator {m_data - idx}; }
panima::ArrayFloatIterator::reference panima::ArrayFloatIterator::operator*() { return *m_data; }
panima::ArrayFloatIterator::const_reference panima::ArrayFloatIterator::operator*() const { return const_cast<ArrayFloatIterator *>(this)->operator*(); }
panima::ArrayFloatIterator::pointer panima::ArrayFloatIterator::operator->() { return m_data; }
const panima::ArrayFloatIterator::pointer panima::ArrayFloatIterator::operator->() const { return const_cast<ArrayFloatIterator *>(this)->operator->(); }
bool panima::ArrayFloatIterator::operator==(const ArrayFloatIterator &other) const { return m_data == other.m_data; }
bool panima::ArrayFloatIterator::operator!=(const ArrayFloatIterator &other) const { return !operator==(other); }

panima::ArrayFloatIterator panima::begin(const udm::Array &a) { return ArrayFloatIterator {static_cast<float *>(&const_cast<udm::Array &>(a).GetValue<float>(0))}; }
panima::ArrayFloatIterator panima::end(const udm::Array &a) { return ArrayFloatIterator {static_cast<float *>(&const_cast<udm::Array &>(a).GetValue<float>(0)) + a.GetSize()}; }

////////////////
