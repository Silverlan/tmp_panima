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
