#include "itemmonitor.h"

#include "../libafanasy/msg.h"

#include "../libafqt/qenvironment.h"

#include "ctrlsortfilter.h"

#include <QtCore/QEvent>
#include <QtNetwork/QHostAddress>
#include <QtGui/QPainter>

#define AFOUTPUT
#undef AFOUTPUT
#include "../include/macrooutput.h"
#include "../libafanasy/logger.h"

const QString EventsName     = "Events[%1]:";
const QString UserIdName     = "User Id: %1";
const QString JobsIdsName    = "Jobs Ids[%1]:";
const QString TimeLaunch     = "L: %1";
const QString TimeRegister   = "R: %1";
const QString TimeActivity   = "A: %1";
const QString Address        = "IP=%1";

ItemMonitor::ItemMonitor( af::Monitor * i_monitor, const CtrlSortFilter * i_ctrl_sf):
	ItemNode( (af::Monitor*)i_monitor, i_ctrl_sf)
{
	time_launch   = i_monitor->getTimeLaunch();
	time_register = i_monitor->getTimeRegister();

	time_launch_str   = TimeLaunch.arg(   afqt::time2Qstr( time_launch  ));
	time_register_str = TimeRegister.arg( afqt::time2Qstr( time_register));

	address_str = Address.arg( i_monitor->getAddress().generateIPString( true).c_str());
	engine = i_monitor->getEngine().c_str();

	updateValues( i_monitor, 0);
}

ItemMonitor::~ItemMonitor()
{
}

void ItemMonitor::updateValues( af::Node * i_node, int i_type)
{
	af::Monitor *monitor = (af::Monitor*)i_node;

	time_activity = monitor->getTimeActivity();
	if( time_activity )
		time_activity_str = TimeActivity.arg( afqt::time2Qstr( time_activity ));
	else
		time_activity_str = TimeActivity.arg( "no activity");

	events.clear();
	eventscount = 0;
	for( int e = 0; e < af::Monitor::EVT_COUNT; e++)
	{
		if( monitor->hasEvent(e))
		{
			events << af::Monitor::EVT_NAMES[e];
			eventscount ++;
		}
	}
	eventstitle = EventsName.arg( eventscount);
	m_height = 25 + 12*eventscount;
	if( m_height < 75) m_height = 75;

	m_user_id = monitor->getUid();
	m_user_id_str = UserIdName.arg( m_user_id);

	jobsids.clear();
	const std::list<int32_t> * jlist = monitor->getJobsIds();
	int jobsidscount = int( jlist->size());
	for( std::list<int32_t>::const_iterator it = jlist->begin(); it != jlist->end(); it++)
		jobsids += QString(" %1").arg( *it);
	jobsidstitle = JobsIdsName.arg( jobsidscount);

	m_tooltip = afqt::stoq( monitor->v_generateInfoString( true));
}

void ItemMonitor::paint( QPainter *painter, const QStyleOptionViewItem &option) const
{
	drawBack( painter, option, isSuperUser() ? &(afqt::QEnvironment::clr_LinkVisited.c) : NULL);

	int x = option.rect.x(); int y = option.rect.y(); int w = option.rect.width(); int h = option.rect.height();

	painter->setPen(   clrTextMain( option) );
	painter->setFont(  afqt::QEnvironment::f_name);
	painter->drawText( option.rect, Qt::AlignTop | Qt::AlignHCenter, m_name );

	painter->setPen(   clrTextInfo( option) );
	painter->setFont(  afqt::QEnvironment::f_info);
	painter->drawText( x+10, y+15, eventstitle );
	for( int e = 0; e < eventscount; e++)
		painter->drawText( x+5, y+30+12*e, events[e] );

	painter->setPen(   clrTextInfo( option) );
	painter->setFont(  afqt::QEnvironment::f_info);
	int i = y+2; int dy = 15;
	painter->drawText( x, i+=dy, w-5, h, Qt::AlignTop | Qt::AlignRight, time_launch_str );
	painter->drawText( x, i+=dy, w-5, h, Qt::AlignTop | Qt::AlignRight, time_register_str );
	painter->drawText( x, i+=dy, w-5, h, Qt::AlignTop | Qt::AlignRight, time_activity_str );

	painter->drawText( x, y, w-5, h, Qt::AlignBottom | Qt::AlignRight, address_str );

	i = y+2;
	painter->drawText( x, i+=dy, w-5, h, Qt::AlignTop | Qt::AlignHCenter, m_user_id_str );
	painter->drawText( x, i+=dy, w-5, h, Qt::AlignTop | Qt::AlignHCenter, jobsidstitle );
	painter->drawText( x, i+=dy, w-5, h, Qt::AlignTop | Qt::AlignHCenter, jobsids );

	painter->drawText( x, y+2, w-5, h, Qt::AlignTop | Qt::AlignRight, engine );
}

void ItemMonitor::setSortType( int i_type1, int i_type2 )
{
	resetSorting();

	switch( i_type1 )
	{
		case CtrlSortFilter::TNONE:
			break;
		case CtrlSortFilter::TNAME:
			m_sort_str1 = m_name;
			break;
		case CtrlSortFilter::TTIMELAUNCHED:
			m_sort_int1 = time_launch;
			break;
		case CtrlSortFilter::TTIMEREGISTERED:
			m_sort_int1 = time_register;
			break;
		case CtrlSortFilter::TTIMEACTIVITY:
			m_sort_int1 = time_activity;
			break;
		case CtrlSortFilter::TENGINE:
			m_sort_str1 = engine;
			break;
		case CtrlSortFilter::TADDRESS:
			m_sort_str1 = address_str;
			break;
		default:
			AF_ERR << "Invalid type1 number = " << i_type1;
	}

	switch( i_type2 )
	{
		case CtrlSortFilter::TNONE:
			break;
		case CtrlSortFilter::TNAME:
			m_sort_str2 = m_name;
			break;
		case CtrlSortFilter::TTIMELAUNCHED:
			m_sort_int2 = time_launch;
			break;
		case CtrlSortFilter::TTIMEREGISTERED:
			m_sort_int2 = time_register;
			break;
		case CtrlSortFilter::TTIMEACTIVITY:
			m_sort_int2 = time_activity;
			break;
		case CtrlSortFilter::TENGINE:
			m_sort_str2 = engine;
			break;
		case CtrlSortFilter::TADDRESS:
			m_sort_str2 = address_str;
			break;
		default:
			AF_ERR << "Invalid type2 number = " << i_type2;
	}
}

void ItemMonitor::setFilterType( int i_type )
{
	resetFiltering();

	switch( i_type )
	{
		case CtrlSortFilter::TNONE:
			break;
		case CtrlSortFilter::TNAME:
			m_filter_str = m_name;
			break;
		case CtrlSortFilter::TENGINE:
			m_filter_str = engine;
			break;
		case CtrlSortFilter::TADDRESS:
			m_filter_str = address_str;
			break;
		default:
			AF_ERR << "Invalid type number = " << i_type;
	}
}
