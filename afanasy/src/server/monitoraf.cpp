#include "monitoraf.h"

#include "../libafanasy/common/dlScopeLocker.h"

#include "../libafanasy/environment.h"
#include "../libafanasy/monitorevents.h"

#include "action.h"
#include "afcommon.h"
#include "jobcontainer.h"
#include "monitorcontainer.h"
#include "usercontainer.h"

#define AFOUTPUT
#undef AFOUTPUT
#include "../include/macrooutput.h"

MonitorContainer * MonitorAf::m_monitors = NULL;

MonitorAf::MonitorAf( af::Msg * msg, UserContainer * i_users):
	af::Monitor( msg),
	AfNodeSrv( this)
{
	UserAf * user = i_users->getUser( m_user_name);
	if( user )
	{
		m_uid = user->getId();
		user->updateTimeActivity();
	}
}

MonitorAf::MonitorAf( const JSON & i_obj, UserContainer * i_users):
	AfNodeSrv( this),
	af::Monitor( i_obj)
{
	UserAf * user = i_users->getUser( m_user_name);
	if( user )
	{
		m_uid = user->getId();
		user->updateTimeActivity();
	}
}

MonitorAf::~MonitorAf()
{
}

void MonitorAf::v_refresh( time_t currentTime, AfContainer * pointer, MonitorContainer * monitoring)
{
	if( getTimeUpdate() < (currentTime - af::Environment::getMonitorZombieTime()))
	{
		if( monitoring) monitoring->addEvent( af::Monitor::EVT_monitors_del, m_id);
		{
			AFCommon::QueueLog("Monitor zombie: " + v_generateInfoString( false));
			setZombie();
		}
	}
}

void MonitorAf::deregister()
{
	AFCommon::QueueLog("Monitor deregister: " + v_generateInfoString( false));
	setZombie();
	m_monitors->addEvent( af::Monitor::EVT_monitors_del, getId());
}

void MonitorAf::v_action( Action & i_action)
{
	const JSON & operation = (*i_action.data)["operation"];
	if( operation.IsObject())
	{
		std::string optype;
		af::jr_string("type", optype, operation);
		if( optype == "exit")
		{
			m_e.m_instruction = "exit";
			return;
		}
		if( optype == "deregister")
		{
			deregister();
		}
		if( optype == "watch")
		{
			std::string opclass, opstatus;
			af::jr_string("class", opclass, operation);
			af::jr_string("status", opstatus, operation);
			bool subscribe = false;
			std::vector<int32_t> eids;
			std::vector<int32_t> uids;
			if( opstatus == "subscribe")
				subscribe = true;

			if( opclass == "perm")
			{
				int32_t new_uid = -1;
				af::jr_int32("uid", new_uid, operation);
				if( new_uid >= 0 )
					m_uid = new_uid;
			}
			else if( opclass == "jobs")
			{
				eids.push_back( af::Monitor::EVT_jobs_add);
				eids.push_back( af::Monitor::EVT_jobs_change);
				eids.push_back( af::Monitor::EVT_jobs_del);
			}
			else if( opclass == "tasks")
			{
				std::vector<int32_t> jids;
				af::jr_int32vec("ids", jids, operation);
				if( subscribe )
					addJobIds( jids);
				else
					delJobIds( jids);
			}
			else if( opclass == "renders")
			{
				eids.push_back( af::Monitor::EVT_renders_add);
				eids.push_back( af::Monitor::EVT_renders_change);
				eids.push_back( af::Monitor::EVT_renders_del);
			}
			else if( opclass == "users")
			{
				eids.push_back( af::Monitor::EVT_users_add);
				eids.push_back( af::Monitor::EVT_users_change);
				eids.push_back( af::Monitor::EVT_users_del);
			}
			else if( opclass == "monitors")
			{
				eids.push_back( af::Monitor::EVT_monitors_add);
				eids.push_back( af::Monitor::EVT_monitors_change);
				eids.push_back( af::Monitor::EVT_monitors_del);
			}
			else if( opclass == "listen")
			{
				int32_t job_id = -1, block = -1, task = -1;
				af::jr_int32("job",   job_id, operation);
				af::jr_int32("block", block,  operation);
				af::jr_int32("task",  task,   operation);

				JobContainerIt jobsIt( i_action.jobs);
				JobAf * job = jobsIt.getJob( job_id);
				if( job )
					if( setListening( job_id, block, task, subscribe))
						job->listenOutput( i_action.renders, subscribe, block, task);
			}
			else
			{
				appendLog("Unknown operation \"" + optype + "\" class \"" + opclass + "\" status \"" + opstatus + "\" by " + i_action.author);
				return;
			}
			if( eids.size())
			{
				// Subscribe or unsubscibe common events:
				setEvents( eids, subscribe);
			}
			m_monitors->addEvent( af::Monitor::EVT_monitors_change, getId());
			appendLog("Operation \"" + optype + "\" class \"" + opclass + "\" status \"" + opstatus + "\" by " + i_action.author);
			m_time_activity = time( NULL);
			return;
		}
		else
		{
			appendLog("Unknown operation \"" + optype + "\" by " + i_action.author);
			return;
		}
		appendLog("Operation \"" + optype + "\" by " + i_action.author);
	}
}

void MonitorAf::setEvents( const std::vector<int32_t> & i_ids, bool value)
{
	for( int i = 0; i < i_ids.size(); i++)
	{
		int eventNum = i_ids[i];
		if(( eventNum >= 0) && ( eventNum < EVT_COUNT))
		{
			m_events[eventNum] = value;
		}
		else
		{
			AFERRAR("MonitorAf::addEvent: Invalid event number: %d\n", eventNum);
		}
	}
//printf("MonitorAf::setEvents:\n"); v_stdOut(true);
}

bool MonitorAf::hasJobEvent( int type, int uid) const
{
//printf("MonitorAf::hasJobEvent: hasEvent=%d, uid=%d, m_uid=%d\n", hasEvent(type), uid, m_uid);
	if( hasEvent( type))
	{
		if( uid == m_uid ) return true;
		if( m_uid == 0 ) return true;
	}
	return false;
}

bool MonitorAf::hasJobId( int id) const
{
	for( std::list<int32_t>::const_iterator it = m_jobsIds.begin(); it != m_jobsIds.end(); it++)
	{
		if( *it == id) return true;
	}
	return false;
}

void MonitorAf::addJobIds( const std::vector<int32_t> & i_ids)
{
//printf("MonitorAf::addJobIds:[%d]",getId());for(int i=0;i<i_ids.size();i++)printf(" %d",i_ids[i]);printf("\n");
	for( int i = 0; i < i_ids.size(); i++)
	{
		if( hasJobId( i_ids[i]) == false)
		{
			m_jobsIds.push_back( i_ids[i]);
		}
	}
}

void MonitorAf::delJobIds( const std::vector<int32_t> & i_ids)
{
//printf("MonitorAf::delJobIds:[%d]",getId());for(int i=0;i<i_ids.size();i++)printf(" %d",i_ids[i]);printf("\n");
	for( int i = 0; i < i_ids.size(); i++) m_jobsIds.remove( i_ids[i]);
}

void MonitorAf::addEvents( int i_type, const std::list<int32_t> i_ids)
{
	if(( i_type >= af::Monitor::EVT_COUNT ) || ( i_type < 0 ))
	{
		AFERRAR("MonitorAf::addEvents: Event %d is invalid.", i_type)
		return;
	}

	std::list<int32_t>::const_iterator it = i_ids.begin();
	while( it != i_ids.end())
	{
		af::addUniqueToVect( m_e.m_events[i_type], *it);
		it++;
	}

//printf("MonitorAf::addEvents: i_ids.size()=%lu\n", i_ids.size());
}

void MonitorAf::addTaskProgress( int i_j, int i_b, int i_t, const af::TaskProgress * i_tp)
{
//std::ostringstream str;af::jw_state( i_tp->state, str);printf("MonitorAf::addTaskProgress():j=%d b=%d t=%d s='%s'\n", i_j, i_b, i_t, str.str().c_str());

	for( int j = 0; j < m_e.m_tp.size(); j++)
	{
		if( m_e.m_tp[j].job_id != i_j ) continue;

		for( int t = 0; t < m_e.m_tp[j].tp.size(); t++)
		{
			if(( m_e.m_tp[j].blocks[t] == i_t ) && ( m_e.m_tp[j].tasks[t] == i_b ))
			{
				m_e.m_tp[j].tp[t] = *i_tp;
//printf("MonitorAf::addTaskProgress(): Task progress updated.\n");
				return;
			}
		}

		m_e.m_tp[j].blocks.push_back( i_b);
		m_e.m_tp[j].tasks.push_back(  i_t);
		m_e.m_tp[j].tp.push_back( *i_tp);

//printf("MonitorAf::addTaskProgress(): Task progress of the same job pushed.\n");
		return;
	}

	const int last = m_e.m_tp.size();
	m_e.m_tp.push_back( af::MonitorEvents::MTaskProgresses());
	m_e.m_tp[last].job_id = i_j;
	m_e.m_tp[last].blocks.push_back( i_b);
	m_e.m_tp[last].tasks.push_back( i_t);
	m_e.m_tp[last].tp.push_back( *i_tp);
//printf("MonitorAf::addTaskProgress(): New job task progress pushed.\n");
}

void MonitorAf::addBlock( int i_j, int i_b, int i_mode)
{
	for( int i = 0; i < m_e.m_bids.size(); i++)
	{
		if(( m_e.m_bids[i].job_id == i_j ) && ( m_e.m_bids[i].block_num == i_b ))
		{
			if( m_e.m_bids[i].mode < i_mode )
				m_e.m_bids[i].mode = i_mode;
			return;
		}
	}

	int i = m_e.m_bids.size();
	m_e.m_bids.push_back( af::MonitorEvents::MBlocksIds());
	m_e.m_bids[i].job_id = i_j;
	m_e.m_bids[i].block_num = i_b;
	m_e.m_bids[i].mode = i_mode;
}

bool MonitorAf::setListening( int i_j, int i_b, int i_t, bool i_subscribe)
{
	std::list<int>::iterator jIt = m_lis_j.begin();
	std::list<int>::iterator bIt = m_lis_b.begin();
	std::list<int>::iterator tIt = m_lis_t.begin();

	for( ; jIt != m_lis_j.end(); jIt++, bIt++, tIt++)
	{
		if(( *jIt == i_j ) && ( *bIt == i_b ) && ( *tIt == i_t ))
		{
			if( i_subscribe )
				return false;

			m_lis_j.erase( jIt);
			m_lis_b.erase( bIt);
			m_lis_t.erase( tIt);

			return true;
		}
	}

	if( false == i_subscribe )
		return false;

	m_lis_j.push_back( i_j);
	m_lis_b.push_back( i_b);
	m_lis_t.push_back( i_t);

	return true;
}

void MonitorAf::waitOutput( const af::MCTaskPos & i_tp)
{
	if( isWaintingOutput( i_tp))
		return;

	m_wait_output.push_back( i_tp);
	//printf("MonitorAf::waitOutput: "); i_tp.v_stdOut();
}

void MonitorAf::addOutput( const af::MCTaskPos & i_tp, const std::string & i_output)
{
	m_e.addOutput( i_tp, i_output);

	m_wait_output.clear();
	//printf("MonitorAf::addOutput: "); i_tp.v_stdOut();
}

bool MonitorAf::isWaintingOutput( const af::MCTaskPos & i_tp)
{
	for( int i = 0; i < m_wait_output.size(); i++)
		if( m_wait_output[i].equal( i_tp))
			return true;

	return false;
}

bool MonitorAf::isListening( const af::MonitorEvents::MListen & i_listen) const
{
	std::list<int>::const_iterator jIt = m_lis_j.begin();
	std::list<int>::const_iterator bIt = m_lis_b.begin();
	std::list<int>::const_iterator tIt = m_lis_t.begin();

	for( ; jIt != m_lis_j.end(); jIt++, bIt++, tIt++)
		if( *jIt == i_listen.job_id )
		{
			if(( *bIt == -1 ) && ( *tIt == -1 ))
				return true;
			if(( *bIt == i_listen.block ) && ( *tIt == i_listen.task ))
				return true;
		}

	return false;
}

af::Msg * MonitorAf::getEventsBin()
{
	updateTime();

	DlScopeLocker mutex( &m_mutex);

	af::Msg * msg = NULL;

	if( m_e.isEmpty())
		msg = new af::Msg( af::Msg::TMonitorId, getId());
	else
	{
		prepareEvents();
		msg = new af::Msg( af::Msg::TMonitorEvents, &m_e);
	}

	m_e.clear();

	return msg;
}

af::Msg * MonitorAf::getEventsJSON()
{
	updateTime();

	af::Msg * msg = new af::Msg();

	std::ostringstream stream;
	stream << "{\"events\":";

	DlScopeLocker mutex( &m_mutex);

	prepareEvents();
	m_e.jsonWrite( stream);

	stream << "\n}";

//if( hasevents ) printf("MonitorAf::getEvents():\n%s\n", stream.str().c_str());

	msg->setData( stream.str().size(), stream.str().c_str(), af::Msg::TJSON);

	m_e.clear();

	return msg;
}

void MonitorAf::prepareEvents()
{
	//
	// Removing deleted jobs from changed:
	//
	{
	std::vector<int32_t>::const_iterator delIt = m_e.m_events[af::Monitor::EVT_jobs_del].begin();
	for( ; delIt != m_e.m_events[af::Monitor::EVT_jobs_del].end(); delIt++)
	{
		std::vector<int32_t>::iterator it = m_e.m_events[af::Monitor::EVT_jobs_change].begin();
		while( it != m_e.m_events[af::Monitor::EVT_jobs_change].end())
		{
			if( *it == *delIt )
				it = m_e.m_events[af::Monitor::EVT_jobs_change].erase( it);
			else
				it++;
		}
	}
	}
}

