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

panima::ChannelPath::ChannelPath(const std::string &ppath)
{
	auto pathWithoutScheme = ppath;
	auto colon = pathWithoutScheme.find(':');
	if(colon != std::string::npos) {
		auto scheme = pathWithoutScheme.substr(0, colon);
		if(scheme != "panima")
			return; // Invalid panima URI
		pathWithoutScheme = pathWithoutScheme.substr(colon + 1);
	}
	auto escapedPath = uriparser::escape(pathWithoutScheme);
	uriparser::Uri uri {escapedPath};
	auto scheme = uri.scheme();
	if(!scheme.empty() && scheme != "panima")
		return; // Invalid panima URI
	auto strPath = uri.path();
	if(!strPath.empty() && strPath.front() == '/')
		strPath.erase(strPath.begin());
	ustring::replace(strPath, "%20", " ");
	path = std::move(uriparser::unescape(strPath));
	auto strQueries = uri.query();
	std::vector<std::string> queries;
	ustring::explode(strQueries, "&", queries);
	for(auto &queryParam : queries) {
		std::vector<std::string> query;
		ustring::explode(queryParam, "=", query);
		if(query.size() < 2)
			continue;
		if(query.front() == "components") {
			m_components = std::make_unique<std::vector<std::string>>();
			ustring::explode(query[1], ",", *m_components);
			for(auto &str : *m_components)
				str = uriparser::unescape(str);
		}
	}
}
panima::ChannelPath::ChannelPath(const ChannelPath &other) { operator=(other); }
bool panima::ChannelPath::operator==(const ChannelPath &other) const { return path == other.path && ((!m_components && !other.m_components) || (m_components && other.m_components && *m_components == *other.m_components)); }
panima::ChannelPath &panima::ChannelPath::operator=(const ChannelPath &other)
{
	path = other.path;
	m_components = nullptr;
	if(other.m_components)
		m_components = std::make_unique<std::vector<std::string>>(*other.m_components);
	return *this;
}
std::string panima::ChannelPath::ToUri(bool includeScheme) const
{
	std::string uri;
	if(includeScheme)
		uri = "panima:";
	uri += path.GetString();
	if(m_components) {
		std::string strComponents;
		for(auto first = true; auto &c : *m_components) {
			if(first)
				first = false;
			else
				strComponents += ",";
			strComponents += c;
		}
		if(!strComponents.empty())
			uri += "?" + strComponents;
	}
	return uri;
}
