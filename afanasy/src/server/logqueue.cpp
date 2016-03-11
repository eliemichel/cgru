#include "logqueue.h"

#include <errno.h>
#include <iostream>

#include "../libafanasy/logger.h"
#include "../libafanasy/name_af.h"

#define AFOUTPUT
#undef AFOUTPUT
#include "../include/macrooutput.h"

LogData::LogData( const std::string & str, int flags)
    : text(str)
    , m_flags((Flags)flags)
{}

void LogData::output()
{
    switch(m_flags)
    {
    case Info:
        AF_LOG << text;
        break;
    case Error:
        AF_ERR << text;
        break;
    case Errno:
        AF_ERR << text << " (system returned: " << strerror(errno) << ")";
        break;
    }
}

LogQueue::LogQueue( const std::string & QueueName):
    AfQueue( QueueName, af::AfQueue::e_start_thread)
{}

LogQueue::~LogQueue() {}

void LogQueue::processItem( af::AfQueueItem* item)
{
   LogData * data = (LogData*)item;
   data->output();
   delete data;
}
