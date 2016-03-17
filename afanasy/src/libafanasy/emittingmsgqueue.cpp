#include "emittingmsgqueue.h"
#include "logger.h"

using namespace af;

EmittingMsgQueue::EmittingMsgQueue(const std::string &QueueName, AfQueue::StartTread i_start_thread)
    : AfQueue::AfQueue( QueueName, i_start_thread)
{
}

void EmittingMsgQueue::processItem( AfQueueItem* item)
{
    Msg * msg = (Msg*)item;

    if( msg == NULL )
    {
        AF_WARN << "'" << name << "': NULL Message received.";
        return;
    }

    if( msg->addressIsEmpty() && ( msg->addressesCount() == 0 ))
    {
        AF_WARN << "'" << name << "': Message has no addresses to send to: " << msg;
        return;
    }

    AF_DEBUG << "<--- " << msg << " to " << msg->getAddress() << " (payload: " << msg->dataLen() << " bytes)";

    if (false == af::msgsendonly( msg))
    {
         AF_WARN << "Unable to send message: " << msg;
         msg->setSendFailed();
         if( msg->canRetrySending())
         {
             this->push( msg, true /* front */);
             return;
         }
         else
         {
             AF_ERR << "Giving up on sending message: " << msg << " (after " << msg->getSendingAttempts() << " attempts)";
         }
    }

    delete msg;
}
