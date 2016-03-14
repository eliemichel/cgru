#pragma once

#ifdef WINNT
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include "../libafanasy/name_af.h"
#include "../libafanasy/emittingmsgqueue.h"
#include "../libafanasy/receivingmsgqueue.h"

class MonitorContainer;
class RenderContainer;
class JobContainer;
class UserContainer;

struct ThreadArgs
{
	MonitorContainer  * monitors;
	RenderContainer   * renders;
	JobContainer      * jobs;
	UserContainer     * users;
	af::MsgQueue      * msgQueue;
    af::EmittingMsgQueue   * emittingMsgQueue;
    af::ReceivingMsgQueue  * receivingMsgQueue;

	int sd;
	struct sockaddr_storage ss;
};
