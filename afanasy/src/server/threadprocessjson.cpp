#include "afcommon.h"
#include "jobcontainer.h"
#include "monitoraf.h"
#include "monitorcontainer.h"
#include "rendercontainer.h"
#include "threadargs.h"
#include "usercontainer.h"

#define AFOUTPUT
#undef AFOUTPUT
#include "../include/macrooutput.h"

af::Msg * threadProcessJSON( ThreadArgs * i_args, af::Msg * i_msg)
{
	rapidjson::Document document;
	std::string error;
	char * data = af::jsonParseMsg( document, i_msg, &error);
	if( data == NULL )
	{
		AFCommon::QueueLogError( error);
		delete i_msg;
		return NULL;
	}

    af::Msg * o_msg_response = NULL;

	JSON & getObj = document["get"];
	if( getObj.IsObject())
	{
		std::string type, mode;
		bool binary = false;
		af::jr_string("type", type, getObj);
		af::jr_string("mode", mode, getObj);
		af::jr_bool("binary", binary, getObj);

		bool json = true;
		if( binary )
			json = false;
		bool full = false;
		if( mode == "full")
			full = true;

		std::vector<int32_t> ids;
		af::jr_int32vec("ids", ids, getObj);

		std::string mask;
		af::jr_string("mask", mask, getObj);

		if( type == "jobs" )
		{
			std::vector<int32_t> uids;
			af::jr_int32vec("uids", uids, getObj);
			if( uids.size())
			{
				AfContainerLock jLock( i_args->jobs,  AfContainerLock::READLOCK);
				AfContainerLock uLock( i_args->users, AfContainerLock::READLOCK);
				o_msg_response = i_args->users->generateJobsList( uids, type, json);
			}
			else
			{
				AfContainerLock lock( i_args->jobs, AfContainerLock::READLOCK);
				o_msg_response = i_args->jobs->generateList(
					full ? af::Msg::TJob : af::Msg::TJobsList, type, ids, mask, json);
			}
		}
		else if( type == "users")
		{
			AfContainerLock lock( i_args->users, AfContainerLock::READLOCK);
			o_msg_response = i_args->users->generateList( af::Msg::TUsersList, type, ids, mask, json);
		}
		else if( type == "renders")
		{
			AfContainerLock lock( i_args->renders, AfContainerLock::READLOCK);
			o_msg_response = i_args->renders->generateList( af::Msg::TRendersList, type, ids, mask, json);
		}
		else if( type == "monitors")
		{
			AfContainerLock lock( i_args->monitors, AfContainerLock::READLOCK);
			if( mode == "events")
			{
				MonitorContainerIt it( i_args->monitors);
				if( ids.size() )
				{
					MonitorAf* node = it.getMonitor( ids[0]);
					if( node != NULL )
					{
						o_msg_response = node->getEvents();
					}
					else
					{
						o_msg_response = af::jsonMsg("{\"id\":0}");
					}
				}
				else
				{
					o_msg_response = af::jsonMsg("{\"error\":\"id not specified\"}");
				}
			}
			else
				o_msg_response = i_args->monitors->generateList( af::Msg::TMonitorsList, type, ids, mask, json);
		}
	}
	else if( document.HasMember("action"))
	{
		i_args->msgQueue->pushMsg( i_msg);
		// To not to detele it, set to NULL, as it pushed to another queue
		i_msg = NULL; //< To not to detele it, as it pushed to another queue
	}
	else if( document.HasMember("job"))
	{
		// No containers locks needed here.
		// Job registration is a complex procedure.
		// It locks and unlocks needed containers itself.
		i_args->jobs->job_register( new JobAf( document["job"]), i_args->users, i_args->monitors);
	}
	else if( document.HasMember("monitor"))
	{
		AfContainerLock lock( i_args->monitors, AfContainerLock::WRITELOCK);
		MonitorAf * newMonitor = new MonitorAf( document["monitor"]);
		newMonitor->setAddressIP( i_msg->getAddress());
		o_msg_response = i_args->monitors->addMonitor( newMonitor, true);
	}


	delete [] data;
	if( i_msg ) delete i_msg;

	return o_msg_response;
}