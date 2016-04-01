#pragma once

#include "../libafanasy/monitor.h"
#include "../libafanasy/msg.h"

#include <QtCore/QList>

class MonitorHost: public af::Monitor
{
public:
	MonitorHost();
	~MonitorHost();

	static af::Msg * genRegisterMsg();

	static void subscribe( const std::string & i_class, bool i_subscribe);

	static const af::Address & getClientAddress() { return m_->getAddress();}

	static inline void addJobId( int i_jid ) { setJobId( i_jid, true );}
	static inline void delJobId( int i_jid ) { setJobId( i_jid, false);}

	static void setUid( int i_uid);
	static int getUid() { return ms_uid ;}

	static void connectionLost();
	static void connectionEstablished( int i_id, int i_uid);

private:
	static MonitorHost * m_;

	static int ms_uid;

private:
	static void setJobId( int i_jid, bool i_add);
};
