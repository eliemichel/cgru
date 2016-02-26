#pragma once

#include "watch.h"

#include <QtGui/QImage>
#include <QtGui/QWidget>
#include <QtGui/QPushButton>

class ButtonMonitor : public QPushButton
{
   Q_OBJECT

public:
   ButtonMonitor( int wType, QWidget *parent);
   ~ButtonMonitor();

   static void pushButton( int wType);

   static void unset();

protected:
   void contextMenuEvent( QContextMenuEvent *event);

   void mousePressEvent( QMouseEvent * event );

private slots:
   void open_SLOT();

private:
   void openMonitor( bool inSeparateWindow);

private:
   bool hovered;
   bool pressed;
   int type;

   int width;
   int height;

   bool useimages;
   QImage img;
   QImage img_h;
   QImage img_p;
   QImage img_t;

private:
   static ButtonMonitor *Buttons[Watch::WLAST];
   static ButtonMonitor *Current;
   static int CurrentType;
};
