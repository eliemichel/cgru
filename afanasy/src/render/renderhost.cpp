#include "renderhost.h"

#ifdef WINNT
#include <fstream>
#endif

#include "../libafanasy/common/dlScopeLocker.h"

#include "../libafanasy/logger.h"
#include "../libafanasy/environment.h"
#include "../libafanasy/msg.h"
#include "../libafanasy/receivingmsgqueue.h"
#include "../libafanasy/emittingmsgqueue.h"

#include "pyres.h"
#include "res.h"

#define AFOUTPUT
#undef AFOUTPUT
#include "../include/macrooutput.h"

extern bool AFRunning;

RenderHost::RenderHost():
    af::Render( Client::GetEnvironment),
    m_msgAcceptQueue(NULL),
    m_msgDispatchQueue(NULL),
    m_updateMsgType(af::Msg::TRenderRegister),
    m_connected(false),
    m_no_output_redirection(false),
    m_first_time(true),
    m_first_valid_msg_id(0)
{
    if( af::Environment::hasArgument("-nor")) m_no_output_redirection = true;

    m_msgAcceptQueue   = new af::ReceivingMsgQueue("Messages Accept Queue",  af::AfQueue::e_start_thread );
    m_msgDispatchQueue = new af::EmittingMsgQueue("Messages Dispatch Queue", af::AfQueue::e_start_thread );
    af::Environment::getSocketPool().subscribe(m_msgAcceptQueue);

    setOnline();

    m_host.m_os = af::strJoin( af::Environment::getPlatform(), " ");
    GetResources( m_host, m_hres);

    std::vector<std::string> resclasses = af::Environment::getRenderResClasses();
    for( std::vector<std::string>::const_iterator it = resclasses.begin(); it != resclasses.end(); it++)
    {
        if( (*it).empty() ) continue;
        AF_LOG << "Adding custom resource meter '" << *it << "'";
        m_pyres.push_back( new PyRes( *it, &m_hres));
    }

#ifdef WINNT
    // Windows Must Die:
    m_windowsmustdie = af::Environment::getRenderWindowsMustDie();
    if( m_windowsmustdie.size())
    {
        printf("Windows Must Die:\n");
        for( int i = 0; i < m_windowsmustdie.size(); i++)
            printf("   %s\n", m_windowsmustdie[i].c_str());
    }
#endif

	af::sleep_msec( 100);

    GetResources( m_host, m_hres);
    for( int i = 0; i < m_pyres.size(); i++) m_pyres[i]->update();

    AF_LOG << this;
    AF_LOG << m_host;
    AF_LOG << m_hres;
}

RenderHost::~RenderHost()
{
    // Delete custom python resources:
    for( int i = 0; i < m_pyres.size(); i++)
        if( m_pyres[i])
            delete m_pyres[i];

    // Send deregister message if connected:
    if( m_connected )
    {
        af::Msg *msg = new af::Msg( af::Msg::TRenderDeregister, this->getId());
        msg->setAddress( af::Environment::getServerAddress());
        dispatchMessage( msg);
        m_connected = false;
    }

    // Delete all tasks:
    for( std::vector<TaskProcess*>::iterator it = m_tasks.begin(); it != m_tasks.end(); )
    {
        delete *it;
        it = m_tasks.erase( it);
    }

    // Delete queues:
    delete m_msgAcceptQueue;
    delete m_msgDispatchQueue;
}

RenderHost * RenderHost::getInstance()
{
    // Does not return a reference, although sometimes recommanded for a
    // singleton, because we need to control when it is destroyed.
    static RenderHost * ms_obj = new RenderHost();
    return ms_obj;
}

void RenderHost::dispatchMessage( af::Msg * i_msg)
{
    if( false == AFRunning ) return;

    if( i_msg->addressIsEmpty() && ( i_msg->addressesCount() == 0 ))
    {
        // Assuming that message should be send to server if no address specified.
        i_msg->setAddress( af::Environment::getServerAddress());
    }
    m_msgDispatchQueue->pushMsg( i_msg);
}

void RenderHost::setRegistered( int i_id)
{
    m_connected = true;
    m_id = i_id;
    setUpdateMsgType( af::Msg::TRenderUpdate);
    AF_LOG << "Render registered.";
}


void RenderHost::connectionLost()
{
    if( m_connected == false ) return;

    m_connected = false;

    m_id = 0;

    // Stop all tasks:
    for( int t = 0; t < m_tasks.size(); t++) m_tasks[t]->stop();

    // Begin to try to register again:
    setUpdateMsgType( af::Msg::TRenderRegister);

    AF_LOG << "Render connection lost, connecting...";
}

void RenderHost::setUpdateMsgType( int i_type)
{
    m_updateMsgType = i_type;
}

void RenderHost::refreshTasks()
{
    if( false == AFRunning )
        return;

    // Refresh tasks:
    for( int t = 0; t < m_tasks.size(); t++)
    {
        m_tasks[t]->refresh();
    }

    // Remove zombies:
    for( std::vector<TaskProcess*>::iterator it = m_tasks.begin(); it != m_tasks.end(); )
    {
        if((*it)->isZombie())
        {
            delete *it;
            it = m_tasks.erase( it);
        }
        else
            it++;
    }
}

void RenderHost::update()
{
    if( false == AFRunning )
        return;

    // Do this every update time, but not the first time, as at the begininng resources are already updated
    if( false == m_first_time )
    {
        GetResources( m_host, m_hres);
        for( int i = 0; i < m_pyres.size(); i++) m_pyres[i]->update();
    }
    else
        m_first_time = false;

#ifdef WINNT
    windowsMustDie();
#endif

    af::Msg * msg = new af::Msg( m_updateMsgType, this);
    msg->setReceiving();
    dispatchMessage( msg);
}

#ifdef WINNT
void RenderHost::windowsMustDie()
{
// Windows Must Die:
    AF_DEBUG << "RenderHost::windowsMustDie():";
    for( int i = 0; i < m_windowsmustdie.size(); i++)
    {
        HWND WINAPI hw = FindWindow( NULL, TEXT( m_windowsmustdie[i].c_str()));
        if( hw != NULL )
        {
            AF_LOG << "Window must die found:\n" << m_windowsmustdie[i];
            SendMessage( hw, WM_CLOSE, 0, 0);
        }
    }
}
#endif

TaskProcess * RenderHost::getTask( const af::MCTaskPos & i_taskpos)
{
    for( int t = 0; t < m_tasks.size(); t++)
    {
        if( m_tasks[t]->is( i_taskpos))
        {
            return m_tasks[t];
        }
    }
    return NULL;
}

TaskProcess * RenderHost::getTask( int i_jobId, int i_blockNum, int i_taskNum, int i_Number)
{
    for( int t = 0; t < m_tasks.size(); t++)
    {
        if( m_tasks[t]->is( i_jobId, i_blockNum, i_taskNum, i_Number))
        {
            return m_tasks[t];
        }
    }
    return NULL;
}

void RenderHost::runTask( af::Msg * i_msg)
{
    m_tasks.push_back( new TaskProcess( new af::TaskExec( i_msg), this));
}

void RenderHost::stopTask( const af::MCTaskPos & i_taskpos)
{
    TaskProcess * task = getTask( i_taskpos);
    if ( NULL == task)
        AF_ERR << "RenderHost::stopTask: " << m_tasks.size() << " tasks, no such task: " << i_taskpos.v_generateInfoString();
    else
        task->stop();
}

void RenderHost::closeTask( const af::MCTaskPos & i_taskpos)
{
    TaskProcess * task = getTask( i_taskpos);
    if ( NULL == task)
        AF_ERR << "RenderHost::closeTask: " << m_tasks.size() << " tasks, no such task: " << i_taskpos.v_generateInfoString();
    else
        task->close();
}

void RenderHost::getTaskOutput( const af::MCTaskPos & i_taskpos, af::Msg * o_msg)
{
    TaskProcess * task = getTask( i_taskpos);
    if ( NULL == task)
        AF_ERR << "RenderHost::closeTask: No such task: " << i_taskpos.v_generateInfoString();
    else
        task->getOutput( o_msg);
}

void RenderHost::listenTasks( const af::MCListenAddress & i_mcaddr)
{
    for( int t = 0; t < m_tasks.size(); t++)
    {
        if( i_mcaddr.justTask())
        {
            // (Un)subscribe to a single task
            if( m_tasks[t]->is( i_mcaddr.getJobId(), i_mcaddr.getNumBlock(), i_mcaddr.getNumTask(), 0))
            {
                if( i_mcaddr.toListen()) m_tasks[t]->addListenAddress(    i_mcaddr.getAddress());
                else                     m_tasks[t]->removeListenAddress( i_mcaddr.getAddress());
                AF_LOG << i_mcaddr.v_generateInfoString();
            }
        }
        else
        {
            // (Un)subscribe to a whole job
            if( m_tasks[t]->is( i_mcaddr.getJobId()))
            {
                if( i_mcaddr.toListen()) m_tasks[t]->addListenAddress(    i_mcaddr.getAddress());
                else                     m_tasks[t]->removeListenAddress( i_mcaddr.getAddress());
                AF_LOG << i_mcaddr.v_generateInfoString();
            }
        }
    }
}

void RenderHost::listenFailed( const af::Address & i_addr)
{
    int lasttasknum = -1;
    for( int t = 0; t < m_tasks.size(); t++) if( (m_tasks[t])->removeListenAddress( i_addr)) lasttasknum = t;
    if( lasttasknum != -1)
    {
        af::MCListenAddress mclass( af::MCListenAddress::FROMRENDER, i_addr, m_tasks[lasttasknum]->exec()->getJobId());
        dispatchMessage( new af::Msg( af::Msg::TTaskListenOutput, &mclass));
    }
}
