#pragma once

#include "../libafanasy/client.h"
#include "../libafanasy/emittingmsgqueue.h"

#include "afcontainer.h"
#include "afcontainerit.h"
#include "processmsg.h"

/// Clients container (abstact class, clients are Render, Talk, Monitor).
class ClientContainer : public AfContainer, public BaseMsgHandler
{
public:
   ClientContainer( std::string ContainerName, int MaximumSize);
   ~ClientContainer();

   bool updateId( int id);

   /// Inherited from MsgHandlerItf
   virtual bool processMsg(af::Msg *msg) { return false; }

protected:
    /// Add new Client to container, new id returned on success, else return 0.
	int addClient( AfNodeSrv * i_nodesrv, bool deleteSameAddress = false, MonitorContainer * monitoring = NULL, int msgEventType = 0);

protected:
	static af::MsgQueue * ms_msg_queue;
};
