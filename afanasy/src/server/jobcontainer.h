#pragma once

#include "../include/afjob.h"

#include "../libafanasy/msgclasses/mcgeneral.h"
#include "../libafanasy/msgclasses/mcjobsweight.h"

#include "../libafsql/name_afsql.h"

#include "afcontainer.h"
#include "afcontainerit.h"
#include "jobaf.h"
#include "processmsg.h"

class MsgAf;
class UserContainer;

/// All Afanasy jobs store in this container.
class JobContainer: public AfContainer, public BaseMsgHandler
{
public:
	JobContainer();
	~JobContainer();

	/// Register a new job, new id returned on success, else return 0.
	int job_register( JobAf *job, UserContainer *users, MonitorContainer * monitoring);

	/// Update some task state of some job.
	void updateTaskState( af::MCTaskUp &taskup, RenderContainer * renders, MonitorContainer * monitoring);

	void getWeight( af::MCJobsWeight & jobsWeight );

    bool solve( RenderAf * i_render, MonitorContainer * i_monitoring);

    /// Inherited from MsgHandlerItf
    virtual bool processMsg(af::Msg *msg);
};

//########################## Iterator ##############################

/// Afanasy jobs interator.
class JobContainerIt: public AfContainerIt
{
public:
	JobContainerIt( JobContainer* jobContainer, bool skipZombies = true);
	~JobContainerIt();

	inline JobAf * job() { return (JobAf*)(getNode()); }
	inline JobAf * getJob( int id) { return (JobAf*)(get( id)); }

private:
};
