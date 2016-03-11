#ifndef EMITTINGMSGQUEUE_H
#define EMITTINGMSGQUEUE_H

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
    /// Push message to emitting queue.
    inline bool pushMsg( Msg* msg) { return push( msg); }

protected:
    void processItem( AfQueueItem* item);

};

} // namespace af

#endif // EMITTINGMSGQUEUE_H
