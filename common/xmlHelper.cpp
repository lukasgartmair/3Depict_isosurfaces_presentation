/* 
 * XmlHelper.cpp : libXML2 wrapper code
 * Copyright (C) 2015  D Haley
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "xmlHelper.h"

#include <cstring>
using std::string;
//Convert a normal string sequence into an XML escaped sequence
std::string escapeXML(const std::string &input)
{
	size_t strLen= input.size();
	std::string output;
	for (size_t ui = 0; ui < strLen; ui++)
	{
		char c;
		c= input[ui];
		if (c == '&') 
			output+=("&amp;");
		else if (c == '<') 
			output+=("&lt;");
		else if (c == '>') 
			output+=("&gt;");
		else if (c == '"') 
			output+=("&quot;");
		else if (c == '\'') 
			output+=("&apos;");
		else 
			output+=c;
	}
	return output;
}


//Convert an xml escaped sequence into a normal string sequence
//Re-used under GPL v3+ From:
//http://svn.lsdcas.engineering.uiowa.edu/repos/lsdcas/trunk/cas2/libcas/xml.cc
//accessed 3 Mar 2012
std::string unescapeXML(const std::string &input)
{
	const char* chars = "<>'\"&" ;
	const char* refs[] =
	{
		"&lt;",
		"&gt;",
		"&apos;",
		"&quot;",
		"&amp;",
		0
	} ;

	std::string data=input;
	for( size_t i = 0 ; refs[i] != NULL ; i++ )
	{
		std::string::size_type pos = data.find( refs[i] ) ;

		while( pos != std::string::npos )
		{
			std::stringstream unescaped ;
			unescaped	<< data.substr( 0, pos )
			<< chars[i]
			<< data.substr( pos + strlen( refs[i] ) ) ;

			data = unescaped.str() ;
			pos = data.find( refs[i], pos + strlen( refs[i] ) ) ;
		}
	}

	return data ;
}

template<> unsigned int XMLHelpGetProp(std::string  &prop,xmlNodePtr node, string propName)
{
	xmlChar *xmlString;

	//grab the xml property
	xmlString = xmlGetProp(node,(const xmlChar *)propName.c_str());

	//Check string contents	
	if(!xmlString)
		return PROP_PARSE_ERR;

	prop=(char *)xmlString;
			
	xmlFree(xmlString);

	return 0;
}


unsigned int XMLHelpNextType(xmlNodePtr &node, int nodeType)
{
	do
	{
		node= node->next;
		if(!node)
			return 1;
	}
	while(node->type != nodeType);
	return 0;
}

//returns zero on success, nonzero on fail
unsigned int XMLHelpFwdToElem(xmlNodePtr &node, const char *nodeName)
{
	do
	{	
		node=node->next;
	}while(node != NULL &&  
		xmlStrcmp(node->name,(const xmlChar *) nodeName));
	return (!node);
}


