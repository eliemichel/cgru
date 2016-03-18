#include "usercontainer.h"

#include <stdio.h>
#include <string.h>

#include "../include/afanasy.h"

#include "../libafanasy/logger.h"
#include "../libafanasy/msgqueue.h"

#include "../libafsql/dbconnection.h"

#include "action.h"
#include "afcommon.h"
#include "renderaf.h"
#include "monitorcontainer.h"

#define AFOUTPUT
#undef AFOUTPUT
#include "../include/macrooutput.h"

using namespace af;

UserContainer::UserContainer():
	AfContainer( "Users", AFUSER::MAXCOUNT)
{
	UserAf::setUserContainer( this);
}

UserContainer::~UserContainer()
{
}

UserAf* UserContainer::addUser( const std::string & i_usernmae, const std::string & i_hostname, MonitorContainer * i_monitoring)
{
	{
		UserContainerIt usersIt( this);
		for(UserAf *user = usersIt.user(); user != NULL; usersIt.next(), user = usersIt.user())
		{
			if( i_usernmae == user->getName())
			{
				if( user->getHostName() != i_hostname)
				{
					user->setHostName( i_hostname);
					if( i_monitoring) i_monitoring->addEvent( af::Msg::TMonitorUsersChanged, user->getId());
				}
				return user;
			}
		}
	}

	UserAf *user = new UserAf( i_usernmae, i_hostname);
	if( addUser(user) == 0)
	{
        AF_ERR << "Can't add user to container." << user;
		delete user;
		return NULL;
	}

	if( i_monitoring) i_monitoring->addEvent( af::Msg::TMonitorUsersAdd, user->getId());

	AFCommon::QueueLog("New job user registered: " + user->v_generateInfoString( false));
	return user;
}

UserAf * UserContainer::getUser( const std::string & i_name )
{
	UserContainerIt usersIt( this);
	for( UserAf * user = usersIt.user(); user != NULL; usersIt.next(), user = usersIt.user())
	{
		if( user->getName() == i_name )
		return user;
	}
	return NULL;
}

void UserContainer::updateTimeActivity( const std::string & i_name)
{
	UserAf * user = getUser( i_name);
	if( user )
		user->updateTimeActivity();
}

void UserContainer::logAction( const Action & i_action, const std::string & i_node_name)
{
	UserAf * user = getUser( i_action.user_name);
	if( user )
        user->logAction( i_action, i_node_name);
}

af::User * UserContainer::getUserByName(std::string name)
{
    UserContainerIt usersIt( this);
    for( af::User *user = usersIt.user(); user != NULL; usersIt.next(), user = usersIt.user())
        if( user->getName() == name)
            return user;
    return NULL;
}

bool UserContainer::processMsg(Msg *msg)
{
    // Return address
    af::Address addr = msg->getAddress();

    switch( msg->type())
    {
    case af::Msg::TUserIdRequest:
    {
        AfContainerLock lock( this, AfContainerLock::READLOCK);

        af::MsgClassUserHost usr( msg);
        af::User *user = getUserByName( usr.getUserName());
        int id = 0;
        if( NULL != user )
            id = user->getId();
        emitMsg( new af::Msg( af::Msg::TUserId, id), &addr);
        return true;
    }
    case af::Msg::TUsersListRequest:
    {
        AfContainerLock lock( this, AfContainerLock::READLOCK);

        emitMsg( generateList( af::Msg::TUsersList), &addr);
        return true;
    }
    case af::Msg::TUsersListRequestIds:
    {
        AfContainerLock lock( this, AfContainerLock::READLOCK);

        af::MCGeneral ids( msg);
        emitMsg( generateList( af::Msg::TUsersList, ids), &addr);
        return true;
    }
    case af::Msg::TUserLogRequestId:
    {
        AfContainerLock lock( this,  AfContainerLock::READLOCK);

        UserContainerIt usersIt( this);
        UserAf* user = usersIt.getUser( msg->int32());
        // FIXME: Better to return some message in any case.
        if( user == NULL )
            return true;
        emitMsg( af::Msg::msgStringList( user->getLog()), &addr);
        return true;
    }
    case af::Msg::TUserJobsOrderRequestId:
    {
        AfContainerLock lock( this,  AfContainerLock::READLOCK);

        UserContainerIt usersIt( this);
        UserAf* user = usersIt.getUser( msg->int32());
        // FIXME: Better to return some message in any case.
        if( user == NULL )
            return true;
        af::MCGeneral ids;
        ids.setId( user->getId());
        ids.setList( user->generateJobsIds());
        emitMsg( new af::Msg( af::Msg::TUserJobsOrder, &ids), &addr);
        return true;
    }
    default:
        return false;
    }
}

int UserContainer::addUser( UserAf * i_user)
{
	// Add node to container:
	if( false == add( i_user))
		return 0;

	// Add user to solving list:
	m_userslist.add( i_user);

	// Initialize user:
	if( false == i_user->initialize())
		return 0;

	return i_user->getId();
}

af::Msg * UserContainer::addUser( UserAf * i_user, MonitorContainer * i_monitoring)
{
	UserContainerIt usersIt( this);
	for(UserAf *user = usersIt.user(); user != NULL; usersIt.next(), user = usersIt.user())
	{
		if( user->getName() == i_user->getName())
		{
            AF_ERR << "User '" << i_user->getName() << "' already exists.";
			delete i_user;
			std::ostringstream str;
			str << "{\"error\":\"exists\"";
			str << ",\n\"user\":\n";
			user->v_jsonWrite( str, /*type no matter*/ 0);
			str << "\n}";
			return af::jsonMsg( str);
		}
	}

	if( addUser( i_user) == 0)
	{
        AF_ERR << "Can't add user '" << i_user->getName() << "' node to container.";
		delete i_user;
		return af::jsonMsgError("Unable to add node to container.");
	}

//AFCommon::QueueDBAddItem( i_user);
	i_user->store();
	if( i_monitoring) i_monitoring->addEvent( af::Msg::TMonitorUsersAdd, i_user->getId());

	AFCommon::QueueLog("User registered: " + i_user->v_generateInfoString( false));

	std::ostringstream str;
	str << "{\"user\":\n";
	i_user->v_jsonWrite( str, /*type no matter*/ 0);
	str << "\n}";
	return af::jsonMsg( str);
}

bool UserContainer::solve( RenderAf * i_render, MonitorContainer * i_monitoring)
{
	return m_userslist.solve( af::Node::SolveByPriority, i_render, i_monitoring);
}

af::Msg* UserContainer::generateJobsList( int id)
{
	UserContainerIt usersIt( this);
	UserAf* user = usersIt.getUser( id);
	if( user == NULL) return NULL;

	MCAfNodes mcjobs;
	user->jobsinfo( mcjobs);

	return new af::Msg( af::Msg::TJobsList, &mcjobs);
}

af::Msg * UserContainer::generateJobsList( const std::vector<int32_t> & ids, const std::string & i_type_name, bool i_json)
{
	return generateJobsList( ids, std::vector<std::string>(), i_type_name, i_json);
}

af::Msg * UserContainer::generateJobsList( const std::vector<std::string> & i_names, const std::string & i_type_name, bool i_json)
{
	return generateJobsList( std::vector<int32_t>(), i_names, i_type_name, i_json);
}

af::Msg * UserContainer::generateJobsList( const std::vector<int32_t> & ids, const std::vector<std::string> & i_names,
	const std::string & i_type_name, bool i_json)
{
	UserContainerIt usersIt( this);
	MCAfNodes mcjobs;
	std::ostringstream stream;
	bool has_jobs = false;

	if( i_json )
	{
		stream << "{\"" << i_type_name << "\":[\n";
	}

	std::vector<UserAf*> users;
	for( int i = 0; i < ids.size(); i++)
		users.push_back( usersIt.getUser( ids[i]));
	for( int i = 0; i < i_names.size(); i++)
		users.push_back( getUser( i_names[i]));

	for( int i = 0; i < users.size(); i++)
	{
		if( users[i] == NULL) continue;
		if( i_json )
		{
			if(( i != 0 ) && ( has_jobs ))
				stream << ",\n";
			has_jobs = users[i]->getJobs( stream);
		}
		else
			users[i]->jobsinfo( mcjobs);
	}

	if( i_json )
	{
		stream << "\n]}";
		return af::jsonMsg( stream);
	}

	af::Msg * msg = new af::Msg();
	msg->set( af::Msg::TJobsList, &mcjobs);
	return msg;
}

//############################################################################
//                               UserContainerIt
//############################################################################

UserContainerIt::UserContainerIt( UserContainer* userslist, bool skipZombies):
	AfContainerIt( (AfContainer*)userslist, skipZombies)
{
}

UserContainerIt::~UserContainerIt()
{
}
