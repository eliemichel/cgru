#include <stdio.h>
#include <stdlib.h>

#include "../libafanasy/msgclasses/mcgeneral.h"
#include "../libafanasy/msgclasses/mclistenaddress.h"
#include "../libafanasy/msgclasses/mctaskup.h"
#include "../libafanasy/msgclasses/mctaskspos.h"
#include "../libafanasy/msg.h"
#include "../libafanasy/msgqueue.h"
#include "../libafanasy/farm.h"

#include "../libafanasy/rapidjson/rapidjson.h"
#include "../libafanasy/rapidjson/stringbuffer.h"
#include "../libafanasy/rapidjson/prettywriter.h"

#include "processmsg.h"
#include "afcommon.h"
#include "jobcontainer.h"
#include "monitorcontainer.h"
#include "rendercontainer.h"
#include "threadargs.h"
#include "usercontainer.h"
#include "action.h"


struct

af::Msg* ProcessMsg::processMsg( ThreadArgs * i_args, af::Msg * i_msg)
{
    AF_DEBUG  << "new message: " << *i_msg;
    af::Msg * o_msg_response = NULL;

    switch( i_msg->type())
    {
    case af::Msg::TVersionMismatch:
    {
        AFCommon::QueueLogError( i_msg->v_generateInfoString( false));
        o_msg_response = new af::Msg( af::Msg::TVersionMismatch, 1);
        break;
    }
    case af::Msg::TInvalid:
    {
        AFCommon::QueueLogError( std::string("Invalid message recieved: ") + i_msg->v_generateInfoString( false));
        break;
    }
    case af::Msg::TNULL:
    case af::Msg::TDATA:
    case af::Msg::TTESTDATA:
    case af::Msg::TStringList:
    {
        i_msg->stdOutData();
        break;
    }
    case af::Msg::THTTP:
    case af::Msg::TJSON:
    case af::Msg::TJSONBIN:
    {
        return ProcessMsg::processJsonMsg( i_args, i_msg);
    }
    case af::Msg::TString:
    {
        std::string str = i_msg->getString();
        if( str.empty()) break;

        AFCommon::QueueLog( str);
        AfContainerLock mLock( i_args->monitors, AfContainerLock::WRITELOCK);
        i_args->monitors->sendMessage( str);
        break;
    }
    case af::Msg::TStatRequest:
    {
        o_msg_response = new af::Msg;
        af::statwrite( o_msg_response);
        break;
    }
    case af::Msg::TConfirm:
    {
        printf("Thread process message: Msg::TConfirm: %d\n", i_msg->int32());
        ProcessMsg::processCoreMsg( i_args, new af::Msg( af::Msg::TConfirm, 1) );
        o_msg_response = new af::Msg( af::Msg::TConfirm, 1 - i_msg->int32());
        break;
    }

// ---------------------------------- Monitor ---------------------------------//
    case af::Msg::TMonitorRegister:
    {
      AfContainerLock lock( i_args->monitors, AfContainerLock::WRITELOCK);

      MonitorAf * newMonitor = new MonitorAf( i_msg);
      newMonitor->setAddressIP( i_msg->getAddress());
      o_msg_response = i_args->monitors->addMonitor( newMonitor);
      break;
    }
    case af::Msg::TMonitorUpdateId:
    {
      AfContainerLock lock( i_args->monitors, AfContainerLock::READLOCK);

      if( i_args->monitors->updateId( i_msg->int32()))
      {
         o_msg_response = new af::Msg( af::Msg::TMonitorId, i_msg->int32());
      }
      else
      {
         o_msg_response = new af::Msg( af::Msg::TMonitorId, 0);
      }
      break;
    }
    case af::Msg::TMonitorsListRequest:
    {
      AfContainerLock lock( i_args->monitors, AfContainerLock::READLOCK);

      o_msg_response = i_args->monitors->generateList( af::Msg::TMonitorsList);
      break;
    }
    case af::Msg::TMonitorsListRequestIds:
    {
      AfContainerLock lock( i_args->monitors, AfContainerLock::READLOCK);

      af::MCGeneral ids( i_msg);
      o_msg_response = i_args->monitors->generateList( af::Msg::TMonitorsList, ids);
      break;
    }
    case af::Msg::TMonitorLogRequestId:
    {
        AfContainerLock lock( i_args->monitors,  AfContainerLock::READLOCK);

        MonitorContainerIt it( i_args->monitors);
        MonitorAf* node = it.getMonitor( i_msg->int32());
        if( node == NULL )
        { // FIXME: Better to return some message in any case.
            break;
        }
        o_msg_response = new af::Msg();
        o_msg_response->setStringList( node->getLog());
        break;
    }

// ---------------------------------- Render -------------------------------//
    case af::Msg::TRenderRegister:
    {
//printf("case af::Msg::TRenderRegister:\n");
        AfContainerLock mLock( i_args->monitors, AfContainerLock::WRITELOCK);
        AfContainerLock rLock( i_args->renders,  AfContainerLock::WRITELOCK);

        RenderAf * newRender = new RenderAf( i_msg);
        newRender->setAddress( i_msg->getAddress());
        newRender->setEmittingMsgQueue( i_args->emittingMsgQueue); // dirty
        newRender->setLastMsgId( i_msg->getId());
        o_msg_response = i_args->renders->addRender( newRender, i_args->monitors);
        break;
    }
    case af::Msg::TRenderUpdate:
    {
//printf("case af::Msg::TRenderUpdate:\n");
      AfContainerLock lock( i_args->renders, AfContainerLock::READLOCK);

      af::Render render_up( i_msg);
//printf("Msg::TRenderUpdate: %s - %s\n", render_up.getName().data(), af::time2str().data());
      RenderContainerIt rendersIt( i_args->renders);
      RenderAf* render = rendersIt.getRender( render_up.getId());

      int id = 0;
      if(( NULL != render) && ( render->update( &render_up)))
      {
          render->setLastMsgId( i_msg->getId());
          id = render->getId();
      }
      o_msg_response = new af::Msg( af::Msg::TRenderId, id);
      break;
    }
    case af::Msg::TRendersListRequest:
    {
      AfContainerLock lock( i_args->renders, AfContainerLock::READLOCK);

      o_msg_response = i_args->renders->generateList( af::Msg::TRendersList);
      break;
    }
    case af::Msg::TRendersListRequestIds:
    {
      AfContainerLock lock( i_args->renders, AfContainerLock::READLOCK);

      af::MCGeneral ids( i_msg);
      o_msg_response = i_args->renders->generateList( af::Msg::TRendersList, ids);
      break;
    }
    case af::Msg::TRendersResourcesRequestIds:
    {
      AfContainerLock lock( i_args->renders, AfContainerLock::READLOCK);

      af::MCGeneral ids( i_msg);
      o_msg_response = i_args->renders->generateList( af::Msg::TRendersResources, ids);
      break;
    }
    case af::Msg::TRenderLogRequestId:
    {
      AfContainerLock lock( i_args->renders,  AfContainerLock::READLOCK);

      RenderContainerIt rendersIt( i_args->renders);
      RenderAf* render = rendersIt.getRender( i_msg->int32());
      if( render == NULL )
      { // FIXME: Better to return some message in any case.
         break;
      }
      o_msg_response = new af::Msg;
      o_msg_response->setStringList( render->getLog());
      break;
    }
    case af::Msg::TRenderTasksLogRequestId:
    {
      AfContainerLock lock( i_args->renders,  AfContainerLock::READLOCK);

      RenderContainerIt rendersIt( i_args->renders);
      RenderAf* render = rendersIt.getRender( i_msg->int32());
      if( render == NULL )
      { // FIXME: Better to return some message in any case.
         break;
      }
      o_msg_response = new af::Msg;
      if( render->getTasksLog().empty())
      {
         o_msg_response->setString("No tasks execution log.");
      }
      else
      {
         o_msg_response->setStringList( render->getTasksLog());
      }
      break;
    }
    case af::Msg::TRenderInfoRequestId:
    {
      AfContainerLock lock( i_args->renders,  AfContainerLock::READLOCK);

      RenderContainerIt rendersIt( i_args->renders);
      RenderAf* render = rendersIt.getRender( i_msg->int32());
      if( render == NULL )
      { // FIXME: Better to return some message in any case.
         break;
      }
      o_msg_response = render->writeFullInfo();
      break;
    }

// ---------------------------------- Users -------------------------------//
    case af::Msg::TUserIdRequest:
    {
      AfContainerLock lock( i_args->users, AfContainerLock::READLOCK);

      af::MsgClassUserHost usr( i_msg);
      std::string name = usr.getUserName();
      int id = 0;
      UserContainerIt usersIt( i_args->users);
      for( af::User *user = usersIt.user(); user != NULL; usersIt.next(), user = usersIt.user())
      {
         if( user->getName() == name)
         {
             id = user->getId();
         }
      }
      o_msg_response = new af::Msg( af::Msg::TUserId, id);
      break;
    }
    case af::Msg::TUsersListRequest:
    {
      AfContainerLock lock( i_args->users, AfContainerLock::READLOCK);

      o_msg_response = i_args->users->generateList( af::Msg::TUsersList);
      break;
    }
    case af::Msg::TUsersListRequestIds:
    {
      AfContainerLock lock( i_args->users, AfContainerLock::READLOCK);

      af::MCGeneral ids( i_msg);
      o_msg_response = i_args->users->generateList( af::Msg::TUsersList, ids);
      break;
    }
    case af::Msg::TUserLogRequestId:
    {
      AfContainerLock lock( i_args->users,  AfContainerLock::READLOCK);

      UserContainerIt usersIt( i_args->users);
      UserAf* user = usersIt.getUser( i_msg->int32());
      if( user == NULL )
      { // FIXME: Better to return some message in any case.
         break;
      }
      o_msg_response = new af::Msg();
      o_msg_response->setStringList( user->getLog());
      break;
    }
    case af::Msg::TUserJobsOrderRequestId:
    {
      AfContainerLock lock( i_args->users,  AfContainerLock::READLOCK);

      UserContainerIt usersIt( i_args->users);
      UserAf* user = usersIt.getUser( i_msg->int32());
      if( user == NULL )
      { // FIXME: Better to return some message in any case.
         break;
      }
      af::MCGeneral ids;
        ids.setId( user->getId());
        ids.setList( user->generateJobsIds());
      o_msg_response = new af::Msg( af::Msg::TUserJobsOrder, &ids);
      break;
    }

// ------------------------------------- Job -------------------------------//
    case af::Msg::TJobRequestId:
    {
      AfContainerLock lock( i_args->jobs,  AfContainerLock::READLOCK);

      JobContainerIt jobsIt( i_args->jobs);
      JobAf* job = jobsIt.getJob( i_msg->int32());
      if( job == NULL )
      {
         o_msg_response = new af::Msg( af::Msg::TJobRequestId, 0);
         break;
      }
      o_msg_response = new af::Msg( af::Msg::TJob, job);
      break;
    }
    case af::Msg::TJobLogRequestId:
    {
      AfContainerLock lock( i_args->jobs,  AfContainerLock::READLOCK);

      JobContainerIt jobsIt( i_args->jobs);
      JobAf* job = jobsIt.getJob( i_msg->int32());
      if( job == NULL )
      { // FIXME: Better to return some message in any case.
         break;
      }
      o_msg_response = new af::Msg();
      o_msg_response->setStringList( job->getLog());
      break;
    }
    case af::Msg::TJobErrorHostsRequestId:
    {
      AfContainerLock lock( i_args->jobs,  AfContainerLock::READLOCK);

      JobContainerIt jobsIt( i_args->jobs);
      JobAf* job = jobsIt.getJob( i_msg->int32());
      if( job == NULL )
      { // FIXME: Better to return some message in any case.
         break;
      }
      o_msg_response = new af::Msg();
      o_msg_response->setString( job->v_getErrorHostsListString());
      break;
    }
    case af::Msg::TJobProgressRequestId:
    {
      AfContainerLock lock( i_args->jobs,  AfContainerLock::READLOCK);

      JobContainerIt jobsIt( i_args->jobs);
      JobAf* job = jobsIt.getJob( i_msg->int32());
      if( job == NULL )
      {
         // FIXME: Send back the same message on error - is it good?
         o_msg_response = new af::Msg( af::Msg::TJobProgressRequestId, 0);
         break;
      }
      o_msg_response = new af::Msg;
      job->writeProgress( *o_msg_response);
      break;
    }
    case af::Msg::TJobsListRequest:
    {
      AfContainerLock lock( i_args->jobs, AfContainerLock::READLOCK);

      o_msg_response = i_args->jobs->generateList( af::Msg::TJobsList);
      break;
    }
    case af::Msg::TJobsListRequestIds:
    {
      AfContainerLock lock( i_args->jobs, AfContainerLock::READLOCK);

      af::MCGeneral ids( i_msg);
      o_msg_response = i_args->jobs->generateList( af::Msg::TJobsList, ids);
      break;
    }
    case af::Msg::TJobsListRequestUserId:
    {
      AfContainerLock jLock( i_args->jobs,  AfContainerLock::READLOCK);
      AfContainerLock uLock( i_args->users, AfContainerLock::READLOCK);

      o_msg_response = i_args->users->generateJobsList( i_msg->int32());
      if( o_msg_response == NULL )
      {
         o_msg_response = new af::Msg( af::Msg::TUserId, 0);
      }
      break;
    }
    case af::Msg::TJobsListRequestUsersIds:
    {
      AfContainerLock jLock( i_args->jobs,  AfContainerLock::READLOCK);
      AfContainerLock uLock( i_args->users, AfContainerLock::READLOCK);

      af::MCGeneral mcids( i_msg);
      std::string type_name;
      o_msg_response = i_args->users->generateJobsList( mcids.getList(), type_name);
      break;
    }
    case af::Msg::TTaskRequest:
    {
      AfContainerLock lock( i_args->jobs,  AfContainerLock::READLOCK);

      af::MCTaskPos mctaskpos( i_msg);
      JobContainerIt jobsIt( i_args->jobs);
      JobAf* job = jobsIt.getJob( mctaskpos.getJobId());
      if( job == NULL )
      {
         o_msg_response = new af::Msg();
         std::ostringstream stream;
         stream << "Msg::TTaskRequest: No job with id=" << mctaskpos.getJobId();
         o_msg_response->setString( stream.str());
         break;
      }
      af::TaskExec * task = job->generateTask( mctaskpos.getNumBlock(), mctaskpos.getNumTask());
      if( task )
      {
         o_msg_response = new af::Msg( af::Msg::TTask, task);
         delete task;
      }
      else
      {
         o_msg_response = new af::Msg();
         std::ostringstream stream;
         stream << "Msg::TTaskRequest: No such task[" << mctaskpos.getJobId() << "][" << mctaskpos.getNumBlock() << "][" << mctaskpos.getNumTask() << "]";
         o_msg_response->setString( stream.str());
      }
      break;
    }
    case af::Msg::TTaskLogRequest:
    {
      AfContainerLock lock( i_args->jobs,  AfContainerLock::READLOCK);

      af::MCTaskPos mctaskpos( i_msg);
      JobContainerIt jobsIt( i_args->jobs);
      JobAf* job = jobsIt.getJob( mctaskpos.getJobId());
      if( job == NULL )
      {
         o_msg_response = new af::Msg();
         std::ostringstream stream;
         stream << "Msg::TTaskLogRequest: No job with id=" << mctaskpos.getJobId();
         o_msg_response->setString( stream.str());
         break;
      }
      const std::list<std::string> * list = &(job->getTaskLog( mctaskpos.getNumBlock(), mctaskpos.getNumTask()));
      if( list == NULL )
      {
         // FIXME: Better to return some message in any case.
         break;
      }
      o_msg_response = new af::Msg();
      if( list->size() == 0)
      {
         std::list<std::string> list;
         list.push_back("Task log is empty.");
         o_msg_response->setStringList( list);
      }
      else
      {
         o_msg_response->setStringList( *list);
      }
      break;
    }
    case af::Msg::TTaskErrorHostsRequest:
    {
      AfContainerLock lock( i_args->jobs,  AfContainerLock::READLOCK);

      af::MCTaskPos mctaskpos( i_msg);
      JobContainerIt jobsIt( i_args->jobs);
      JobAf* job = jobsIt.getJob( mctaskpos.getJobId());
      if( job == NULL )
      {
         // FIXME: Better to return some message in any case.
         break;
      }
      o_msg_response = new af::Msg();
      o_msg_response->setString( job->v_getErrorHostsListString( mctaskpos.getNumBlock(), mctaskpos.getNumTask()));
      break;
    }
    case af::Msg::TTaskOutputRequest:
    {
        af::Msg * msg_request_render = NULL;
        std::string filename, error;
        af::MCTaskPos tp( i_msg);
//printf("ThreadReadMsg::msgCase: case af::Msg::TJobTaskOutputRequest: job=%d, block=%d, task=%d, number=%d\n", tp.getJobId(), tp.getNumBlock(), tp.getNumTask(), tp.getNumber());
        {
            AfContainerLock jLock( i_args->jobs,	 AfContainerLock::READLOCK);
            AfContainerLock rLock( i_args->renders, AfContainerLock::READLOCK);

            JobContainerIt jobsIt( i_args->jobs);
            JobAf* job = jobsIt.getJob( tp.getJobId());
            if( job == NULL )
            {
                o_msg_response = af::Msg::msgString("Error: Job is NULL.");
                AFCommon::QueueLogError("Jobs is NULL");
                break;
            }

            // Trying to set message to request output from running remote host.
            msg_request_render = job->v_getTaskStdOut( tp.getNumBlock(), tp.getNumTask(), tp.getNumber(),
                i_args->renders, filename, error);

            if( error.size())
            {
                if( msg_request_render )
                    delete msg_request_render;
                o_msg_response = af::Msg::msgString( error);
                AFCommon::QueueLogError( error);
                break;
            }
        }
        if( filename.size())
        {
        //
        //	 Retrieving output from file
        //
            int readsize = -1;
            char * data = af::fileRead( filename, &readsize, af::Msg::SizeDataMax, &error);
            if( data )
            {
                o_msg_response = new af::Msg();
                o_msg_response->setData( readsize, data);
                delete [] data;
            }
            else if( error.size())
            {
                error = std::string("Getting task output: ") + error;
                error += "\nCheck task log.";
                error += "\nIf there is 'update timeout' check firewall.";
                error += "\nClient should listen a port and server should be able to connect to it.";
                //AFCommon::QueueLogError( error);
                o_msg_response = af::Msg::msgString( error);
            }
        }
        else if( msg_request_render)
        {
        //
        //	 Retrieving output from render
        //
            msg_request_render->setReceiving();
            bool ok;
            o_msg_response = af::msgsend( msg_request_render, ok, af::VerboseOn);
            if( o_msg_response == NULL )
            {
                error = "Retrieving output from render failed. See server logs for details.";
                o_msg_response = af::Msg::msgString( error);
                //AFCommon::QueueLogError( error);
            }
            delete msg_request_render;
        }
        else
        {
            if( error.size())
            {
                o_msg_response = af::Msg::msgString( error);
                AFCommon::QueueLogError("TTaskOutputRequest: Neiter message nor filename\n" + error);
            }
            else
                AFCommon::QueueLogError("TTaskOutputRequest: Neiter message nor filename.");
        }
      break;
    }
    case af::Msg::TJobsWeightRequest:
    {
      AfContainerLock jLock( i_args->jobs,	 AfContainerLock::READLOCK);

      af::MCJobsWeight jobsWeight;
      i_args->jobs->getWeight( jobsWeight);
      o_msg_response = new af::Msg( af::Msg::TJobsWeight, &jobsWeight);
      break;
    }
    // Cases for run cycle thread:
    case af::Msg::TTaskUpdateState:
    {
      af::MCTaskUp taskup( i_msg);
      i_msg->resetWrittenSize();
      AF_DEBUG << "--> taskup: " << taskup;
      af::MCTaskPos taskpos( taskup.getNumJob(), taskup.getNumBlock(), taskup.getNumTask(), taskup.getNumber());
      o_msg_response = new af::Msg( af::Msg::TRenderCloseTask, &taskpos);
    }
    case af::Msg::TTaskUpdatePercent:
    case af::Msg::TTaskListenOutput:
    case af::Msg::TRenderDeregister:
    case af::Msg::TMonitorSubscribe:
    case af::Msg::TMonitorUnsubscribe:
    case af::Msg::TMonitorDeregister:
    case af::Msg::TMonitorUsersJobs:
    case af::Msg::TMonitorJobsIdsAdd:
    case af::Msg::TMonitorJobsIdsSet:
    case af::Msg::TMonitorJobsIdsDel:
    case af::Msg::TMonitorMessage:
    {
        // Push message for run cycle thread.
        ProcessMsg::processCoreMsg( i_args, i_msg);
        // Need to return here to not to delete input message (i_msg) later.
        return o_msg_response;
        //  ( o_msg_response is NULL in all cases except Msg::TTaskUpdateState,
        //	 in that case render should recieve an answer to close task
        //	 and finish sending any updates for the task )
    }
    // -------------------------------------------------------------------------//
    default:
    {
        AFCommon::QueueLogError( std::string("Unknown message recieved: ") + i_msg->v_generateInfoString( false));
        break;
    }
    }

    // Deleting input message as it not needed any more.
    delete i_msg;

    // Returning an answer
    return o_msg_response;
}

void ProcessMsg::processCoreMsg( ThreadArgs * i_args, af::Msg * i_msg)
{
    AF_DEBUG  << "new message: " << *i_msg;
    switch ( i_msg->type())
    {
    case af::Msg::THTTP:
    case af::Msg::TJSON:
    case af::Msg::TJSONBIN:
    {
        Action action( i_msg, i_args);
        if( action.isInvalid())
            return;

        if( action.type == "monitors")
            i_args->monitors->action( action);
        else if( action.type == "jobs")
            i_args->jobs->action( action);
        else if( action.type == "renders")
            i_args->renders->action( action);
        else if( action.type == "users")
            i_args->users->action( action);
        else
            AFCommon::QueueLogError(std::string("JSON action has unknown type - \"") + action.type + "\"");

        break;
    }
    case af::Msg::TMonitorDeregister:
    {
        MonitorContainerIt it( i_args->monitors);
        MonitorAf* node = it.getMonitor( i_msg->int32());
        if( node ) node->deregister();
        break;
    }
    case af::Msg::TMonitorMessage:
    {
        af::MCGeneral mcgeneral( i_msg);
        i_args->monitors->sendMessage( mcgeneral);
        break;
    }
    case af::Msg::TMonitorSubscribe:
    case af::Msg::TMonitorUnsubscribe:
    case af::Msg::TMonitorUsersJobs:
    case af::Msg::TMonitorJobsIdsAdd:
    case af::Msg::TMonitorJobsIdsSet:
    case af::Msg::TMonitorJobsIdsDel:
    {
        af::MCGeneral ids( i_msg);
        i_args->monitors->setInterest( i_msg->type(), ids);
        break;
    }
    case af::Msg::TRenderDeregister:
    {
        RenderContainerIt rendersIt( i_args->renders);
        RenderAf* render = rendersIt.getRender( i_msg->int32());
        if( render != NULL) render->deregister( i_args->jobs, i_args->monitors);
        break;
    }
    case af::Msg::TTaskListenOutput:
    {
        af::MCListenAddress mclass( i_msg);
        JobContainerIt jobsIt( i_args->jobs);
        JobAf* job = jobsIt.getJob( mclass.getJobId());
        if( mclass.fromRender() == false ) mclass.setIP( i_msg->getAddress());
        mclass.v_stdOut();
        if( job ) job->listenOutput( mclass, i_args->renders);
        break;
    }
    case af::Msg::TTaskUpdatePercent:
    case af::Msg::TTaskUpdateState:
    {
        af::MCTaskUp taskup( i_msg);
        i_args->jobs->updateTaskState( taskup, i_args->renders, i_args->monitors);
        break;
    }
    case af::Msg::TConfirm:
    {
        AFCommon::QueueLog( std::string("af::Msg::TConfirm: ") + af::itos( i_msg->int32()));
        break;
    }
    default:
    {
        AFCommon::QueueLogError( std::string("Run: Unknown message recieved: ") + i_msg->v_generateInfoString( false));
        break;
    }
    }
    delete i_msg;
}


af::Msg * ProcessMsg::processJsonMsg( ThreadArgs * i_args, af::Msg * i_msg)
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
            if( getObj.HasMember("uids"))
            {
                std::vector<int32_t> uids;
                af::jr_int32vec("uids", uids, getObj);
                if( uids.size())
                {
                    AfContainerLock jLock( i_args->jobs,  AfContainerLock::READLOCK);
                    AfContainerLock uLock( i_args->users, AfContainerLock::READLOCK);
                    o_msg_response = i_args->users->generateJobsList( uids, type, json);
                }
            }
            if( getObj.HasMember("users"))
            {
                std::vector<std::string> users;
                af::jr_stringvec("users", users, getObj);
                if( users.size())
                {
                    AfContainerLock jLock( i_args->jobs,  AfContainerLock::READLOCK);
                    AfContainerLock uLock( i_args->users, AfContainerLock::READLOCK);
                    o_msg_response = i_args->users->generateJobsList( users, type, json);
                }
            }
            else if( mode == "output")
            {
                std::vector<int32_t> block_ids;
                std::vector<int32_t> task_ids;
                int number = 0;
                af::jr_int32vec("block_ids", block_ids, getObj);
                af::jr_int32vec("task_ids", task_ids, getObj);
                af::jr_int("number", number, getObj);
                if(( ids.size() == 1 ) && ( block_ids.size() == 1 ) && ( task_ids.size() == 1 ))
                {
                    af::Msg * msg_request_render = NULL;
                    std::string filename, error, name;

                    // Get output from job, it can return a request message for render or a filename
                    {
                        AfContainerLock jlock( i_args->jobs,    AfContainerLock::READLOCK);
                        AfContainerLock rLock( i_args->renders, AfContainerLock::READLOCK);

                        JobContainerIt it( i_args->jobs);
                        JobAf * job = it.getJob( ids[0]);
                        if( job == NULL )
                            o_msg_response = af::jsonMsgError("Invalid ID");
                        else
                        {
                            msg_request_render = job->v_getTaskStdOut( block_ids[0], task_ids[0], number,
                                    i_args->renders, filename, error);
                            name = job->generateTaskName( block_ids[0], task_ids[0]);
                            if( number > 0 )
                                name += "["+af::itos(number)+"]";
                        }
                    }

                    if( filename.size()) // Reading output from file
                    {
                        int readsize = -1;
                        char * data = af::fileRead( filename, &readsize, af::Msg::SizeDataMax, &error);
                        if( data )
                        {
                            o_msg_response = af::jsonMsg( mode, name, data, readsize);
                            delete [] data;
                        }
                    }
                    else if( msg_request_render) // Retrieving output from render
                    {
                        msg_request_render->setReceiving();
                        bool ok;
                        af::Msg * response = af::msgsend( msg_request_render, ok, af::VerboseOn);
                        if( response )
                        {
                            o_msg_response = af::jsonMsg( mode, name, response->data(), response->dataLen());
                            delete response;
                        }
                        else
                            error = "Retrieving output from render failed. See server logs for details.";
                        delete msg_request_render;
                    }

                    if( error.size())
                    {
                        error += "\nCheck task log.";
                        error += "\nIf there is 'update timeout' check firewall.";
                        error += "\nClient should listen a port and server should be able to connect to it.";
                        if( o_msg_response == NULL )
                            o_msg_response = af::jsonMsgError( error);
                        //AFCommon::QueueLogError("TTaskOutputRequest: " + error);
                    }
                }
            }
            else
            {
                AfContainerLock lock( i_args->jobs, AfContainerLock::READLOCK);
                JobAf * job = NULL;
                bool was_error = false;
                if( ids.size() == 1 )
                {
                    JobContainerIt it( i_args->jobs);
                    job = it.getJob( ids[0]);
                    if( job == NULL )
                        o_msg_response = af::jsonMsgError( "Invalid ID");
                }

                if( job )
                {
                    std::vector<int32_t> block_ids;
                    af::jr_int32vec("block_ids", block_ids, getObj);
                    if( block_ids.size() && ( block_ids[0] != -1 ))
                    {
                        std::vector<int32_t> task_ids;
                        af::jr_int32vec("task_ids", task_ids, getObj);
                        if( task_ids.size() && ( task_ids[0] != -1))
                            o_msg_response = job->writeTask( block_ids[0], task_ids[0], mode, binary);
                        else
                        {
                            std::vector<std::string> modes;
                            af::jr_stringvec("mode", modes, getObj);
                            o_msg_response = job->writeBlocks( block_ids, modes);
                        }
                    }
                    else if( mode.size())
                    {
                        if( mode == "thumbnail" )
                            o_msg_response = job->writeThumbnail( binary);
                        else if( mode == "progress" )
                            o_msg_response = job->writeProgress( json);
                        else if( mode == "error_hosts" )
                            o_msg_response = job->writeErrorHosts();
                        else if( mode == "log" )
                            o_msg_response = job->writeLog();
                    }
                }

                if( o_msg_response == NULL )
                    o_msg_response = i_args->jobs->generateList(
                                full ? af::Msg::TJob : af::Msg::TJobsList, type, ids, mask, json);
            }
        }
        else if( type == "renders")
        {
            AfContainerLock lock( i_args->renders, AfContainerLock::READLOCK);
            if( mode.size())
            {
                RenderAf * render = NULL;
                if( ids.size() == 1 )
                {
                    RenderContainerIt it( i_args->renders);
                    render = it.getRender( ids[0]);
                    if( render == NULL )
                        o_msg_response = af::jsonMsgError( "Invalid ID");
                }
                if( render )
                {
                    if( full )
                        o_msg_response = render->jsonWriteSrvFarm();
                    else if( mode == "log" )
                        o_msg_response = render->writeLog();
                    else if( mode == "tasks_log" )
                        o_msg_response = af::jsonMsg("tasks_log", render->getName(), render->getTasksLog());
                }
            }
            if( o_msg_response == NULL )
            {
                if( mode == "resources" )
                    o_msg_response = i_args->renders->generateList( af::Msg::TRendersResources, type, ids, mask, json);
                else
                    o_msg_response = i_args->renders->generateList( af::Msg::TRendersList, type, ids, mask, json);
            }
        }
        else if( type == "users")
        {
            AfContainerLock lock( i_args->users, AfContainerLock::READLOCK);
            if( mode.size())
            {
                UserAf * user = NULL;
                if( ids.size() == 1 )
                {
                    UserContainerIt it( i_args->users);
                    user = it.getUser( ids[0]);
                    if( user == NULL )
                        o_msg_response = af::jsonMsgError( "Invalid ID");
                }
                if( user )
                {
                    if( mode == "jobs_order" )
                        o_msg_response = user->writeJobdsOrder();
                    else if( mode == "log" )
                        o_msg_response = user->writeLog();
                }
            }
            if( o_msg_response == NULL )
                o_msg_response = i_args->users->generateList( af::Msg::TUsersList, type, ids, mask, json);
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
                        o_msg_response = af::jsonMsg("{\"monitor\":{\"id\":0}}");
                    }
                }
                else
                {
                    o_msg_response = af::jsonMsgError("id is not specified");
                }
            }
            else
                o_msg_response = i_args->monitors->generateList( af::Msg::TMonitorsList, type, ids, mask, json);
        }
        else if( type == "files")
        {
            std::string path;
            std::ostringstream files;
            af::jr_string("path", path, getObj);
            std::vector<std::string> list = af::getFilesListSafe( path);
            files << "{\"path\":\"" << path << "\",\n";
            files << "\"files\":[";
            for( int i = 0; i < list.size(); i++)
            {
                if( i )
                    files << ',';
                files << '"' << list[i] << '"';
            }
            files << "]}";
            o_msg_response = af::jsonMsg( files);
        }
        else if( type == "config" )
        {
            o_msg_response = af::jsonMsg( af::Environment::getConfigData());
        }
        else if( type == "farm" )
        {
            o_msg_response = af::jsonMsg( af::farm()->getText());
        }
    }
    else if( document.HasMember("action"))
    {
        ProcessMsg::processCoreMsg( i_args, i_msg );
        // To not to detele it, set to NULL, as it pushed to another queue
        i_msg = NULL;
        o_msg_response = af::jsonMsg("{\"status\":\"OK\"}");
    }
    else if( document.HasMember("job"))
    {
        if( af::Environment::isDemoMode() )
        {
            AFCommon::QueueLogError("Job registration is not allowed: Server demo mode.");
        }
        else
        {
            // No containers locks needed here.
            // Job registration is a complex procedure.
            // It locks and unlocks needed containers itself.
            int id = i_args->jobs->job_register( new JobAf( document["job"]), i_args->users, i_args->monitors);
            std::string str = "{\"id\":";
            str += af::itos(id) + "}";
            o_msg_response = af::jsonMsg( str);
        }
    }
    else if( document.HasMember("monitor"))
    {
        AfContainerLock mlock( i_args->monitors, AfContainerLock::WRITELOCK);
        AfContainerLock ulock( i_args->users,    AfContainerLock::READLOCK);
        MonitorAf * newMonitor = new MonitorAf( document["monitor"], i_args->users);
        newMonitor->setAddressIP( i_msg->getAddress());
        o_msg_response = i_args->monitors->addMonitor( newMonitor, /*JSON = */ true);
    }
    else if( document.HasMember("user"))
    {
        AfContainerLock ulock( i_args->users, AfContainerLock::WRITELOCK);
        o_msg_response = i_args->users->addUser( new UserAf( document["user"]), i_args->monitors);
    }
    else if( document.HasMember("reload_farm"))
    {
        AfContainerLock mLock( i_args->monitors, AfContainerLock::WRITELOCK);
        AfContainerLock rlock( i_args->renders,  AfContainerLock::WRITELOCK);

        printf("\n	========= RELOADING FARM =========\n\n");
        if( af::loadFarm( true))
        {
            RenderContainerIt rendersIt( i_args->renders);
            for( RenderAf *render = rendersIt.render(); render != NULL; rendersIt.next(), render = rendersIt.render())
            {
                render->getFarmHost();
                i_args->monitors->addEvent( af::Msg::TMonitorRendersChanged, render->getId());
            }
            printf("\n	========= FARM RELOADED SUCCESSFULLY =========\n\n");
            o_msg_response = af::jsonMsgStatus( true, "reload_farm",
                                                "Reloaded successfully.");
        }
        else
        {
            printf("\n	========= FARM RELOADING FAILED =========\n\n");
            o_msg_response = af::jsonMsgStatus( false, "reload_farm",
                                                "Failed, see server logs fo details. Check farm with \"afcmd fcheck\" at first.");
        }
    }
    else if( document.HasMember("reload_config"))
    {
        AfContainerLock jlock( i_args->jobs,	AfContainerLock::WRITELOCK);
        AfContainerLock rlock( i_args->renders, AfContainerLock::WRITELOCK);
        AfContainerLock ulock( i_args->users,	AfContainerLock::WRITELOCK);
        printf("\n	========= RELOADING CONFIG =========\n\n");
        std::string message;
        if( af::Environment::reload())
        {
            printf("\n	========= CONFIG RELOADED SUCCESSFULLY =========\n\n");
            o_msg_response = af::jsonMsgStatus( true, "reload_config",
                                                "Reloaded successfully.");
        }
        else
        {
            printf("\n	========= CONFIG RELOADING FAILED =========\n\n");
            o_msg_response = af::jsonMsgStatus( false, "reload_config",
                                                "Failed, see server logs fo details.");
        }
    }
    else if( document.HasMember("save"))
    {
        o_msg_response = ProcessMsg::jsonSaveObject( document);
    }

    delete [] data;
    if( i_msg ) delete i_msg;

    return o_msg_response;
}

af::Msg * ProcessMsg::jsonSaveObject( rapidjson::Document & i_obj)
{
    JSON & jSave = i_obj["save"];
    if( false == jSave.IsObject())
        return af::jsonMsgError("\"save\" is not an object.");

    JSON & jPath = jSave["path"];
    if( false == jPath.IsString())
        return af::jsonMsgError("\"path\" is not an string.");

    JSON & jObj = jSave["object"];
    if( false == jObj.IsObject())
        return af::jsonMsgError("\"object\" is not an object.");

    std::string path((char*)jPath.GetString());
    while(( path[0] == '/' ) || ( path[0] == '/' ) || ( path[0] == '.'))
        path = path.substr(1);
    path = af::Environment::getCGRULocation() + '/' + path + ".json";

    std::string info;
    info = std::string("Created by afserver at ") + af::time2str();
    jObj.AddMember("__cgru__", info.c_str(), i_obj.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent('\t',1);
    jObj.Accept(writer);
    std::string text( buffer.GetString());

    std::ostringstream str;
    str << "{\"save\":\n";
    str << "\t\"path\":\"" << path << "\",\n";
    if( af::pathFileExists( path)) str << "\t\"overwrite\":true,\n";
    str << "\t\"size\":" << text.size() << "\n";
    str << "}}";

    if( false == AFCommon::writeFile( text, path))
        return af::jsonMsgError( std::string("Unable to write to \"") + path + "\", see server log for details.");

    return af::jsonMsg( str);
}

