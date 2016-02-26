#include "buttonmonitor.h"

#include "../libafanasy/environment.h"

#include "../libafqt/qenvironment.h"

#include "wndlist.h"

#include <QtCore/QEvent>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QMenu>
#include <QtGui/QPainter>

#define AFOUTPUT
#undef AFOUTPUT
#include "../include/macrooutput.h"

ButtonMonitor * ButtonMonitor::Buttons[Watch::WLAST] = {0,0,0,0,0};
ButtonMonitor * ButtonMonitor::Current = NULL;
int ButtonMonitor::CurrentType = Watch::WNONE;

ButtonMonitor::ButtonMonitor( int wType, QWidget *parent):
   QPushButton( parent),
   type( Watch::WNONE)
{
   if((wType <= Watch::WNONE) || (wType >= Watch::WLAST))
   {
      AFERRAR("ButtonMonitor::ButtonMonitor: Invalid type = %d", wType)
      return;
   }
   type = wType;
   if( Buttons[type] != NULL )
   {
      AFERRAR("ButtonMonitor::ButtonMonitor: Type = %d, already exists.", wType)
      return;
   }
   Buttons[type] = this;
   setText(Watch::WndName[wType]);
   QString tooltip = QString("Show %1 list.").arg(Watch::WndName[wType]);
   if( wType != Watch::WJobs ) tooltip += "\nUse RMB to open new window.";
   setToolTip(tooltip);
   setMaximumWidth(100);
}

void ButtonMonitor::contextMenuEvent( QContextMenuEvent *event)
{
   if( Watch::isConnected() == false ) return;
   if((type == CurrentType) || (type == Watch::WJobs)) return;
   QString itemname;
   if( Watch::opened[type]) itemname = "Raise";
   else itemname = "Open";

   QMenu menu(this);
   QAction *action;
   action = new QAction( itemname, this);
   connect( action, SIGNAL( triggered() ), this, SLOT( open_SLOT() ));
   menu.addAction( action);
   menu.exec( event->globalPos());
}

ButtonMonitor::~ButtonMonitor()
{
   Buttons[type] = NULL;
   if( Current == this)
   {
      Current = NULL;
      CurrentType = Watch::WNONE;
   }
}

void ButtonMonitor::open_SLOT() { openMonitor( true);}

void ButtonMonitor::openMonitor( bool inSeparateWindow)
{
   if( Watch::openMonitor( type, inSeparateWindow) && ( false == inSeparateWindow ))
   {
      unset();
      repaint();
      Current = this;
      CurrentType = type;
   }
}

void ButtonMonitor::pushButton( int wType)
{
   if( Buttons[ wType] == NULL)
   {
      AFERRAR("ButtonMonitor::pushButton: Buttons[%s] is NULL.", Watch::BtnName[wType].toUtf8().data())
      return;
   }
   Buttons[wType]->openMonitor( false);
}

void ButtonMonitor::unset()
{
   Current = NULL;
   CurrentType = Watch::WNONE;
   for( int b = 0; b < Watch::WLAST; b++) if( Buttons[b]) Buttons[b]->repaint();
}


void ButtonMonitor::mousePressEvent( QMouseEvent * event )
{
   if( event->button() == Qt::LeftButton)
   {
      openMonitor( false);
   }
}
