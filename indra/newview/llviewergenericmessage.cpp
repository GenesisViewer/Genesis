/** 
 * @file llviewergenericmessage.cpp
 * @brief Handle processing of "generic messages" which contain short lists of strings.
 * @author James Cook
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llviewergenericmessage.h"
#include "lldispatcher.h"
#include "lluuid.h"
#include "message.h"
#include "llagent.h"
#include "llwaterparamset.h"
#include "llwaterparammanager.h"
#include "llwlparamset.h"
#include "llwlparammanager.h"
#include "m7wlinterface.h"
#include "lluuid.h"

LLDispatcher gGenericDispatcher;
bool animatorIsRunning = true;
bool animatorUseLindenTime = true;


void send_generic_message(const std::string& method,
						  const std::vector<std::string>& strings,
						  const LLUUID& invoice)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("GenericMessage");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
	msg->nextBlock("MethodData");
	msg->addString("Method", method);
	msg->addUUID("Invoice", invoice);
	if(strings.empty())
	{
		msg->nextBlock("ParamList");
		msg->addString("Parameter", NULL);
	}
	else
	{
		std::vector<std::string>::const_iterator it = strings.begin();
		std::vector<std::string>::const_iterator end = strings.end();
		for(; it != end; ++it)
		{
			msg->nextBlock("ParamList");
			msg->addString("Parameter", *it);
		}
	}
	gAgent.sendReliableMessage();
}



void process_generic_message(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);

	std::string method;
	msg->getStringFast(_PREHASH_MethodData, _PREHASH_Method, method);
	
	//This needs to be handled by a dispatcher really, but I'm not sure where is the best place to put it
	if (method == "Windlight")
	{
		//Meta7 WindLight packet
		//We are delivering with an agentID of NULL_KEY so as to be
		//friendly and not trigger a warning for unsupporting clients.
		M7WindlightInterface::getInstance()->receiveMessage(msg);
	}
	else if (method == "WindlightReset")
	{
		M7WindlightInterface::getInstance()->receiveReset();
	}
	else if (agent_id != gAgent.getID())
	{
		LL_WARNS() << "GenericMessage for wrong agent " << agent_id << LL_ENDL;
		return;
	}
	else
	{
		std::string request;
		LLUUID invoice;
		LLDispatcher::sparam_t strings;
		LLDispatcher::unpackMessage(msg, request, invoice, strings);

		if(!gGenericDispatcher.dispatch(request, invoice, strings))
		{
			LL_WARNS() << "GenericMessage " << request << " failed to dispatch" 
				<< LL_ENDL;
		}
	}
}
void process_generic_streaming_message(LLMessageSystem* msg, void**)
{
    // placeholder to suppress packet loss reports and log spam (SL-20473)
}
