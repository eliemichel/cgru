#ifndef EMITTINGMSGQUEUE_H
#define EMITTINGMSGQUEUE_H

#include "logger.h"
#include "afqueue.h"
#include "msg.h"

namespace af
{

/**
 * @brief EmittingMsgQueue is a queue in which you can only push messages.
 * Pushed messages are emitted to their destination in an independent thread.
 */
class EmittingMsgQueue : public AfQueue
{
public:
    EmittingMsgQueue( const std::string & QueueName, StartTread i_start_thread);

    /// Push message to emitting queue.
    inline bool pushMsg( Msg* msg) { AF_DEBUG << "Queuing Msg: " << msg; return push( msg); }

protected:
    void processItem( AfQueueItem* item);

};

} // namespace af

#endif // EMITTINGMSGQUEUE_H
