/*
 * Social Club UI / login endpoint stubs (loopback).
 *
 * Ported from legacy ros-patches-five (SCUIStub.cpp). The Rockstar-bridging
 * LoginHandler2 (real /ros/login + /ros/validate against prod.ros) is
 * replaced by a purely local fabricated implementation - nothing in this
 * component ever contacts Rockstar.
 */

#include "StdInc.h"
#include <ros/EndpointMapper.h>

#include <fstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <tinyxml2.h>

#include <Local.h>

class SCUIHandler : public net::HttpHandler
{
private:
	std::string m_scuiData;

public:
	SCUIHandler()
	{
	}

	bool HandleRequest(fwRefContainer<net::HttpRequest> request, fwRefContainer<net::HttpResponse> response) override
	{
		response->SetStatusCode(200);
		response->SetHeader("Content-Type", "text/html; charset=utf-8");

		response->End(m_scuiData);

		return true;
	}
};

namespace
{
	template<typename TValue>
	void GetJsonValue(TValue value, rapidjson::Document& document, rapidjson::Value& outValue)
	{
		outValue.CopyFrom(rapidjson::Value(value), document.GetAllocator());
	}

	template<>
	void GetJsonValue<const char*>(const char* value, rapidjson::Document& document, rapidjson::Value& outValue)
	{
		outValue.CopyFrom(rapidjson::Value(value, document.GetAllocator()), document.GetAllocator());
	}

	template<>
	void GetJsonValue<std::nullptr_t>(std::nullptr_t value, rapidjson::Document& document, rapidjson::Value& outValue)
	{
		rapidjson::Value val;
		val.SetNull();

		outValue.CopyFrom(val, document.GetAllocator());
	}
}

std::string GetRockstarTicketXml()
{
	// generate initial XML to be contained by JSON
	tinyxml2::XMLDocument document;

	auto rootElement = document.NewElement("Response");
	document.InsertFirstChild(rootElement);

	// set root attributes
	rootElement->SetAttribute("ms", 30.0);
	rootElement->SetAttribute("xmlns", "CreateTicketResponse");

	// elements
	auto appendChildElement = [&](tinyxml2::XMLNode* node, const char* key, auto value)
	{
		auto element = document.NewElement(key);
		element->SetText(value);

		node->InsertEndChild(element);

		return element;
	};

	auto appendElement = [&](const char* key, auto value)
	{
		return appendChildElement(rootElement, key, value);
	};

	// create the document
	appendElement("Status", 1);
	appendElement("Ticket", "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"); // 'a' repeated
	appendElement("PosixTime", static_cast<unsigned int>(time(nullptr)));
	appendElement("SecsUntilExpiration", 86399);
	appendElement("PlayerAccountId", va("%lld", ROS_DUMMY_ACCOUNT_ID));
	appendElement("PublicIp", "127.0.0.1");
	appendElement("SessionId", 5);
	appendElement("SessionKey", "MDEyMzQ1Njc4OWFiY2RlZg=="); // '0123456789abcdef'
	appendElement("SessionTicket", "vhASmPR0NnA7MZsdVCTCV/3XFABWGa9duCEscmAM0kcCDVEa7YR/rQ4kfHs2HIPIttq08TcxIzuwyPWbaEllvQ==");
	appendElement("CloudKey", "8G8S9JuEPa3kp74FNQWxnJ5BXJXZN1NFCiaRRNWaAUR=");

	// services
	auto servicesElement = appendElement("Services", "");
	servicesElement->SetAttribute("Count", 0);

	// Rockstar account
	tinyxml2::XMLNode* rockstarElement = appendElement("RockstarAccount", "");
	appendChildElement(rockstarElement, "RockstarId", va("%lld", ROS_DUMMY_ACCOUNT_ID));
	appendChildElement(rockstarElement, "Age", 18);
	appendChildElement(rockstarElement, "AvatarUrl", "Bully/b20.png");
	appendChildElement(rockstarElement, "CountryCode", "CA");
	appendChildElement(rockstarElement, "Email", "onlineservices@fivem.net");
	appendChildElement(rockstarElement, "LanguageCode", "en");
	appendChildElement(rockstarElement, "Nickname", fmt::sprintf("R%08x", ROS_DUMMY_ACCOUNT_ID).c_str());

	appendElement("Privileges", "1,2,3,4,5,6,8,9,10,11,14,15,16,17,18,19,21,22,27,29,30");

	auto privsElement = appendElement("Privs", "");
	auto privElement = appendChildElement(privsElement, "p", "");
	privElement->SetAttribute("id", "27");
	privElement->SetAttribute("g", "True");

	// format as string
	tinyxml2::XMLPrinter printer;
	document.Print(&printer);

	return printer.CStr();
}

std::string HandleCfxLogin()
{
	auto rockstarTicket = GetRockstarTicketXml();

	// JSON document
	rapidjson::Document json;

	// this is an object
	json.SetObject();

	// append data
	auto appendJson = [&](const char* key, auto value)
	{
		rapidjson::Value jsonKey(key, json.GetAllocator());

		rapidjson::Value jsonValue;
		GetJsonValue(value, json, jsonValue);

		json.AddMember(jsonKey, jsonValue, json.GetAllocator());
	};

	appendJson("SessionKey", "MDEyMzQ1Njc4OWFiY2RlZg==");
	appendJson("Ticket", "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh");
	appendJson("Email", "onlineservices@fivem.net");
	appendJson("SaveEmail", true);
	appendJson("SavePassword", true);
	appendJson("Password", "DetCon1");
	appendJson("Nickname", fmt::sprintf("R%08x", ROS_DUMMY_ACCOUNT_ID).c_str());
	appendJson("RockstarId", va("%lld", ROS_DUMMY_ACCOUNT_ID));
	appendJson("Lang", "en-US");
	appendJson("CountryCode", "CA");
	appendJson("CallbackData", 2);
	appendJson("Local", false);
	appendJson("SignedIn", true);
	appendJson("SignedOnline", true);
	appendJson("AutoSignIn", false);
	appendJson("Expiration", 86399);
	appendJson("AccountId", va("%lld", ROS_DUMMY_ACCOUNT_ID));
	appendJson("Age", 18);
	appendJson("AvatarUrl", "Bully/b20.png");
	appendJson("XMLResponse", rockstarTicket.c_str());

	// serialize json
	rapidjson::StringBuffer buffer;

	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	json.Accept(writer);

	return { buffer.GetString(), buffer.GetSize() };
}

class LoginHandler : public net::HttpHandler
{
public:
	bool HandleRequest(fwRefContainer<net::HttpRequest> request, fwRefContainer<net::HttpResponse> response) override
	{
		auto buffer = HandleCfxLogin();

		// and write HTTP response
		response->SetStatusCode(200);
		response->SetHeader("Content-Type", "application/json; charset=utf-8");
		response->End(std::move(buffer));

		return true;
	}
};

// local-only replacement for the old Rockstar-bridging /ros/login + /ros/validate
class LoginHandler2 : public net::HttpHandler
{
public:
	bool HandleRequest(fwRefContainer<net::HttpRequest> request, fwRefContainer<net::HttpResponse> response) override
	{
		request->SetDataHandler([=](const std::vector<uint8_t>& data)
		{
			response->SetStatusCode(200);

			if (request->GetPath() == "/ros/validate")
			{
				// ownership is already proven locally by this point; return a
				// successful-looking (dummy) entitlement block
				response->SetHeader("Content-Type", "text/plain; charset=utf-8");
				response->End("AAAA");
			}
			else
			{
				response->SetHeader("Content-Type", "application/json; charset=utf-8");
				response->End(HandleCfxLogin());
			}
		});

		return true;
	}
};

static InitFunction initFunction([]()
{
	EndpointMapper* endpointMapper = Instance<EndpointMapper>::Get();
	endpointMapper->AddPrefix("/scui/v2/desktop", new SCUIHandler());
	endpointMapper->AddPrefix("/cfx/login", new LoginHandler());
	endpointMapper->AddPrefix("/ros/login", new LoginHandler2());
	endpointMapper->AddPrefix("/ros/validate", new LoginHandler2());

	// somehow launcher likes using two slashes - this should be handled better tbh
	endpointMapper->AddPrefix("//scui/v2/desktop", new SCUIHandler());

	// MTL
	endpointMapper->AddPrefix("/scui/mtl/launcher", new SCUIHandler());
	endpointMapper->AddPrefix("//scui/mtl/launcher", new SCUIHandler());
});
