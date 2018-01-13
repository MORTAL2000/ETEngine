#include "stdafx.hpp"
#include "JSONdom.h"

JSONstring* JSONvalue::str()
{
	if (GetType() == JSON_String)return static_cast<JSONstring*>(this); return nullptr;
}

JSONnumber* JSONvalue::num()
{
	if (GetType() == JSON_Number)return static_cast<JSONnumber*>(this); return nullptr;
}

JSONobject* JSONvalue::obj()
{
	if (GetType() == JSON_Object)return static_cast<JSONobject*>(this); return nullptr;
}

JSONarray* JSONvalue::arr()
{
	if (GetType() == JSON_Array)return static_cast<JSONarray*>(this); return nullptr;
}

JSONbool* JSONvalue::b()
{
	if (GetType() == JSON_Bool)return static_cast<JSONbool*>(this); return nullptr;
}

std::vector<std::string> JSONarray::StrArr()
{
	std::vector<std::string>ret;
	for (auto el : value)
	{
		JSONstring* jstr = el->str();
		if (jstr)ret.push_back(jstr->value);
	}
	return ret;
}

std::vector<double> JSONarray::NumArr()
{
	std::vector<double>ret;
	for (auto el : value)
	{
		JSONnumber* jnum = el->num();
		if (jnum)ret.push_back(jnum->value);
	}
	return ret;
}
