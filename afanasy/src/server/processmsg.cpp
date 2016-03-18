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

/* ************************************************************************* */
/*                            BaseMsgHandler                                 */
/* ************************************************************************* */

void BaseMsgHandler::emitMsg(af::Msg *msg, af::Address *dest)
{
    if (NULL == m_emitting_queue || NULL == msg)
        return;

    if (NULL != dest)
        msg->setAddress(dest);

    m_emitting_queue->pushMsg(msg);
}

/* ************************************************************************* */
/*                            JsonMsgHandler                                 */
/* ************************************************************************* */

JsonMsgHandler::JsonMsgHandler(af::EmittingMsgQueue *emitting_queue, ThreadArgs *i_args)
    : m_thread_args(i_args)
{
    this->setEmittingMsgQueue(*emitting_queue);
}

bool JsonMsgHandler::processMsg(af::Msg *msg)
{
    // Return address
    af::Address addr = msg->getAddress();

    switch( msg->type())
    {
    case af::Msg::THTTP:
    case af::Msg::TJSON:
    case af::Msg::TJSONBIN:
    {
        emitMsg( ProcessMsg::processJsonMsg( m_thread_args, msg), &addr);
        return true;
    }
    default:
        return false;
    }
}

/* ************************************************************************* */
/*                            FrctMsgHandler                                 */
/* ************************************************************************* */

FrctMsgHandler::FrctMsgHandler(af::EmittingMsgQueue *emitting_queue, ThreadArgs *i_args)
    : m_thread_args(i_args)
{
    this->setEmittingMsgQueue(*emitting_queue);
}

bool FrctMsgHandler::processMsg(af::Msg *msg)
{
    af::Address addr = msg->getAddress();

    switch( msg->type())
    {
    case af::Msg::TRenderDeregister:
    {
        // Mixing renders, jobs and monitors
        RenderContainerIt rendersIt( m_thread_args->renders);
        RenderAf* render = rendersIt.getRender( msg->int32());
        if( render != NULL) render->deregister( m_thread_args->jobs, m_thread_args->monitors);
        return true;
    }
    case af::Msg::TTaskListenOutput:
    {
        // Mixing renders and jobs
        af::MCListenAddress mclass( msg);
        JobContainerIt jobsIt( m_thread_args->jobs);
        JobAf* job = jobsIt.getJob( mclass.getJobId());
        if( mclass.fromRender() == false ) mclass.setIP( msg->getAddress());
        AF_LOG << mclass;
        if( job ) job->listenOutput( mclass, m_thread_args->renders);
        return true;
    }
    case af::Msg::TTaskUpdatePercent:
    {
        // Mixing renders, jobs and monitors
        af::MCTaskUp taskup( msg);
        m_thread_args->jobs->updateTaskState( taskup, m_thread_args->renders, m_thread_args->monitors);
        return true;
    }
    case af::Msg::TTaskUpdateState:
    {
        // Mixing renders, jobs and monitors
        af::MCTaskUp taskup( msg);
        m_thread_args->jobs->updateTaskState( taskup, m_thread_args->renders, m_thread_args->monitors);
        af::MCTaskPos taskpos( taskup.getNumJob(), taskup.getNumBlock(), taskup.getNumTask(), taskup.getNumber());
        emitMsg( new af::Msg( af::Msg::TRenderCloseTask, &taskpos), &addr);
        return true;
    }
    case af::Msg::TJobsListRequestUserId:
    {
        // Mixes jobs and users
        AfContainerLock jLock( m_thread_args->jobs,  AfContainerLock::READLOCK);
        AfContainerLock uLock( m_thread_args->users, AfContainerLock::READLOCK);

        af::Msg *res = m_thread_args->users->generateJobsList( msg->int32());
        if( NULL == res )
            emitMsg( new af::Msg( af::Msg::TUserId, 0), &addr);
        else
            emitMsg( res, &addr);
        return true;
    }
    case af::Msg::TJobsListRequestUsersIds:
    {
        // Mixes jobs and users
        AfContainerLock jLock( m_thread_args->jobs,  AfContainerLock::READLOCK);
        AfContainerLock uLock( m_thread_args->users, AfContainerLock::READLOCK);

        af::MCGeneral mcids( msg);
        std::string type_name;
        emitMsg( m_thread_args->users->generateJobsList( mcids.getList(), type_name), &addr);
        return true;
    }
    case af::Msg::TTaskOutputRequest:
    {
        // Mixes jobs and renders
        {
            af::MCTaskPos tp( msg);

            AfContainerLock jLock( m_thread_args->jobs,	 AfContainerLock::READLOCK);
            AfContainerLock rLock( m_thread_args->renders, AfContainerLock::READLOCK);

            JobContainerIt jobsIt( m_thread_args->jobs);
            JobAf* job = jobsIt.getJob( tp.getJobId());
            if( job == NULL )
            {
                AF_ERR << "Jobs is NULL";
                emitMsg( af::Msg::msgString("Error: Job is NULL."), &addr);
                return true;
            }

            // Trying to set message to request output from running remote host.
            af::Msg * msg_request_render = NULL;
            std::string filename, error;
            msg_request_render = job->v_getTaskStdOut( tp.getNumBlock(), tp.getNumTask(), tp.getNumber(),
                                                       m_thread_args->renders, filename, error);

            if( filename.size())
            {
                // Log has already been retreived from render
                // Retrieving output from file
                int readsize = -1;
                char * data = af::fileRead( filename, &readsize, af::Msg::SizeDataMax, &error);
                if( data )
                {
                    af::Msg *res = new af::Msg();
                    res->setData( readsize, data);
                    delete [] data;
                    emitMsg( res, &addr);
                }
                else if( error.size())
                {
                    std::stringstream ss;
                    ss << "Getting task output: " << error << "\n"
                       << "Check task log.";
                    emitMsg( af::Msg::msgString( ss.str()), &addr);
                }
            }
            else if( msg_request_render)
            {
                // Retrieving output from render
                msg_request_render->setReceiving();
                bool ok;
                // argl! this is blocking!
                // TODO: Make this whole mecanism asynchronous
                af::Msg *res = af::msgsend( msg_request_render, ok, af::VerboseOn);
                if( res == NULL )
                {
                    std::string err = "Retrieving output from render failed. See server logs for details.";
                    emitMsg( af::Msg::msgString( err), &addr);
                }
                else
                {
                    emitMsg( res, &addr);
                }
            }
            else if( error.size())
            {
                emitMsg( af::Msg::msgString( error), &addr);
                AF_ERR << "TTaskOutputRequest: Neiter message nor filename. Error: " << error;
            }
            else
            {
                AF_ERR << "TTaskOutputRequest: Neiter message nor filename.";
            }

            if (msg_request_render)
                delete msg_request_render;
            return true;
        }
    }
    case af::Msg::TRenderRegister:
    {
        // This one mixes both renders and monitors
        // Let's wait until the architecture redesign resolved this
        AfContainerLock mLock( m_thread_args->monitors, AfContainerLock::WRITELOCK);
        AfContainerLock rLock( m_thread_args->renders,  AfContainerLock::WRITELOCK);

        RenderAf * newRender = new RenderAf( msg);
        newRender->setAddress( msg->getAddress());
        newRender->setEmittingMsgQueue( m_thread_args->emittingMsgQueue); // dirty
        newRender->setLastMsgId( msg->getId());
        emitMsg( m_thread_args->renders->addRender( newRender, m_thread_args->monitors), &addr);
        return true;
    }
    default:
        return false;
    }
}

/* ************************************************************************* */
/*                            MainMsgHandler                                 */
/* ************************************************************************* */

MainMsgHandler::MainMsgHandler(af::EmittingMsgQueue *emitting_queue, ThreadArgs *i_args)
    : m_thread_args(i_args)
    , m_json_msgh(emitting_queue, i_args)
    , m_frct_msgh(emitting_queue, i_args)
{
    this->setEmittingMsgQueue(*emitting_queue);
}

bool MainMsgHandler::preFilterMsg(af::Msg *msg)
{
    AF_DEBUG  << "new message: " << *msg;

    switch( msg->type())
    {
    case af::Msg::TString:
    {
        // Log TString messages
        std::string str = msg->getString();
        if( str.empty()) return false;
        AF_LOG << str;
        return true;
    }
    default:
        return true;
    }
}

bool MainMsgHandler::processMsg(af::Msg *msg)
{
    // Return address
    af::Address addr = msg->getAddress();

    if (false == this->preFilterMsg(msg))
        return true; // The Msg was for us, but has been filtered out from processing

    switch( msg->type())
    {

// ---------------------------------- JSON ---------------------------------//
    case af::Msg::THTTP:
    case af::Msg::TJSON:
    case af::Msg::TJSONBIN:
    {
        return m_json_msgh.processMsg(msg);
    }

// ---------------------------------- Monitor ---------------------------------//
    case af::Msg::TMonitorRegister:
    case af::Msg::TMonitorUpdateId:
    case af::Msg::TMonitorsListRequest:
    case af::Msg::TMonitorsListRequestIds:
    case af::Msg::TMonitorLogRequestId:
    case af::Msg::TString:
    case af::Msg::TMonitorDeregister:
    case af::Msg::TMonitorMessage:
    case af::Msg::TMonitorSubscribe:
    case af::Msg::TMonitorUnsubscribe:
    case af::Msg::TMonitorUsersJobs:
    case af::Msg::TMonitorJobsIdsAdd:
    case af::Msg::TMonitorJobsIdsSet:
    case af::Msg::TMonitorJobsIdsDel:
    {
        return m_thread_args->monitors->processMsg( msg);
    }

// ---------------------------------- Render -------------------------------//
    case af::Msg::TRenderUpdate:
    case af::Msg::TRendersListRequest:
    case af::Msg::TRendersListRequestIds:
    case af::Msg::TRendersResourcesRequestIds:
    case af::Msg::TRenderLogRequestId:
    case af::Msg::TRenderTasksLogRequestId:
    case af::Msg::TRenderInfoRequestId:
    {
        return m_thread_args->renders->processMsg( msg);
    }

// ---------------------------------- Users -------------------------------//
    case af::Msg::TUserIdRequest:
    case af::Msg::TUsersListRequest:
    case af::Msg::TUsersListRequestIds:
    case af::Msg::TUserLogRequestId:
    case af::Msg::TUserJobsOrderRequestId:
    {
        return m_thread_args->users->processMsg( msg);
    }

// ---------------------------------- Jobs -------------------------------//
    case af::Msg::TJobRequestId:
    case af::Msg::TJobLogRequestId:
    case af::Msg::TJobErrorHostsRequestId:
    case af::Msg::TJobProgressRequestId:
    case af::Msg::TJobsListRequest:
    case af::Msg::TJobsListRequestIds:
    case af::Msg::TJobsWeightRequest:
    case af::Msg::TTaskRequest:
    case af::Msg::TTaskLogRequest:
    case af::Msg::TTaskErrorHostsRequest:
    {
        return m_thread_args->jobs->processMsg( msg);
    }

// ---------------------------------- Cases mixing many containers -------------------------------//
    case af::Msg::TTaskUpdatePercent:
    case af::Msg::TTaskListenOutput:
    case af::Msg::TRenderDeregister:
    case af::Msg::TJobsListRequestUserId:
    case af::Msg::TJobsListRequestUsersIds:
    case af::Msg::TTaskOutputRequest:
    case af::Msg::TRenderRegister:
    {
        // We kept here all the messages whose processing involves many of the
        // containers at the same time.
        return m_frct_msgh.processMsg(msg);
    }

// ---------------------------------- Misc -------------------------------//
    case af::Msg::TVersionMismatch:
    {
        AF_ERR << msg;
        emitMsg( new af::Msg( af::Msg::TVersionMismatch, 1), &addr);
        return true;
    }
    case af::Msg::TInvalid:
    {
        AF_ERR << "Invalid message recieved: " << msg;
        return true;
    }
    case af::Msg::TNULL:
    case af::Msg::TDATA:
    case af::Msg::TTESTDATA:
    case af::Msg::TStringList:
    {
        msg->stdOutData();
        return true;
    }
    case af::Msg::TStatRequest:
    {
        af::Msg *res = new af::Msg;
        af::statwrite( res);
        emitMsg( res, &addr);
        return true;
    }
    case af::Msg::TConfirm:
    {
        AF_LOG << "Thread process message: Msg::TConfirm: " << msg->int32();
        emitMsg( new af::Msg( af::Msg::TConfirm, 1 - msg->int32()), &addr);
        return true;
    }

    default:
        AF_ERR << "Unknown message recieved: " << msg;
        return false;
    }
}


/* ************************************************************************* */
/*                                ProcessMsg                                 */
/* ************************************************************************* */

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
        Action action( i_msg, i_args);
        if( action.isInvalid())
            return NULL;

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
