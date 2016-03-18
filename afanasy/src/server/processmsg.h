#ifndef PROCESSMSG_H
#define PROCESSMSG_H

#include "afcommon.h"
#include "../libafanasy/emittingmsgqueue.h"

/**
 * @brief Message Handler Interface
 * This interface is implemented by any class that can potentially process messages.
 */
class MsgHandlerItf
{
public:
    /**
     * @brief Process a message if its type is supported. Otherwise, return
     * `false`.
     * When providing a message to this method, you give up any responsibility
     * about it, which means that in particular you must not free it.
     * @param msg Pointer to the message to process.
     * @return Whether the message can be handled by this object or not.
     * This is NOT a status, since a return value of `true` only means that
     * the object CAN handle the message, not that the processing went good.
     * This can be used to chain different handlers of different priorities,
     * feeding a message to a Msg handler iff the first one did not recognize
     * it. (e.g. for a plugin system)
     */
    virtual bool processMsg(af::Msg *msg) = 0;

public:
    /// Virtual destructor is part of the Interface C++ Design Pattern
    virtual ~MsgHandlerItf() {}
};

/**
 * @brief Basic implementation of the Message Handler Interface
 * Notably adds the emitMsg() method and associated emitting queue attribute.
 */
class BaseMsgHandler
{
public:
    /// Inherited from MsgHandlerItf
    virtual bool processMsg(af::Msg *msg) = 0;

    /**
     * @brief Set emitting Msg queue
     * @param emitting_queue Pointer to the queue through which emitting
     * messages
     */
    void setEmittingMsgQueue(af::EmittingMsgQueue &q) { m_emitting_queue = &q; }

protected:
    /**
     * @brief Emit a message
     * This is an optional method of the Message Handler interface not directly
     * related to handling but because usually when handling a message, one
     * also send some Msg back
     * @param msg Message to emit (will be deleted when sent)
     * @param dest Destination to which sending the message. If not provided,
     * the message itself can specify a destination.
     */
    virtual void emitMsg(af::Msg *msg, af::Address *dest=NULL);

private:
    /// Emitting queue used by emitMsg()
    af::EmittingMsgQueue *m_emitting_queue;
};

/**
 * @brief Message Handler for JSON message
 */
class JsonMsgHandler : public BaseMsgHandler
{
public:
    /**
     * @brief JsonMsgHandler ctor
     * @param emitting_queue if provided, this queue will be used to emit messages
     */
    JsonMsgHandler(af::EmittingMsgQueue *emitting_queue=NULL, ThreadArgs *i_args=NULL);

    /// Inherited from MsgHandlerItf
    virtual bool processMsg(af::Msg *msg);

private:
    // To be removed
    ThreadArgs *m_thread_args;
};


/**
 * @brief Former Run Cycle Thread Message Handler
 */
class FrctMsgHandler : public BaseMsgHandler
{
public:
    /**
     * @brief FrctMsgHandler ctor
     * @param emitting_queue if provided, this queue will be used to emit messages
     */
    FrctMsgHandler(af::EmittingMsgQueue *emitting_queue=NULL, ThreadArgs *i_args=NULL);

    /// Inherited from MsgHandlerItf
    virtual bool processMsg(af::Msg *msg);

private:
    // To be removed
    ThreadArgs *m_thread_args;
};

/**
 * @brief Main Message Handler, or an attempt to make ProcessMsg cleaner
 */
class MainMsgHandler : public BaseMsgHandler
{
public:
    /**
     * @brief MainMsgHandler ctor
     * @param emitting_queue if provided, this queue will be used to emit messages
     */
    MainMsgHandler(af::EmittingMsgQueue *emitting_queue=NULL, ThreadArgs *i_args=NULL);

    /// Inherited from MsgHandlerItf
    virtual bool processMsg(af::Msg *msg);

private:
    /**
     * @brief Filter messages before the main switch
     * This is used to trigger some action or log some information about every
     * message before they are processed. It can return `false` to prevent the
     * main switch to handle the message. Be aware that this would mean that
     * `processMsg()` returns `true` as the Msg type has been recognized,
     * although the Msg itself is rejected.
     * @param msg Incoming Message to analyse
     * @return Whether the message has been filtered out
     */
    bool preFilterMsg(af::Msg *msg);

private:
    // To be removed
    ThreadArgs *m_thread_args;
    JsonMsgHandler m_json_msgh;
    FrctMsgHandler m_frct_msgh;
};

/**
 * Message processing routines.
 *
 * Ok, we should clean that up eventually, but for now let's consolidate all
 * the Msg processing functions at the same place.
 * We must determine a clear role for each of those, and stop prefix them with
 * `thread` when they are not actually ran within different threads.
 *
 * Something already clear is that we don't need message processing routines to
 * return a message when they want to reply since our messaging system is not
 * based on synchron request/response mechanism any more.
 * Maybe this was the only difference between `processCoreMsg` and `processMsg`
 * that actually matters.
 *
 * Then, we need to isolate more cleanly the different message formats. Let's
 * use only one reference system, namely the binary messages, and convert
 * enything else into this before any processing. Thus processing routines
 * should only have to handle binary messages.
 *
 * Different messaging format (binary, HTTP, JSON) should then be abstracted
 * into a general "transport" API.
 */
namespace ProcessMsg
{

/**
 * @brief processJsonMsg
 * @param i_args
 * @param i_msg Message to process
 * @return Message to return
 */
af::Msg * processJsonMsg( ThreadArgs * i_args, af::Msg * i_msg);

/**
 * @brief Save an object to JSON. Utility function, should not be here but is
 * used only in `processMsg`.
 * @param i_obj Object to save
 * @return A JSON Msg, saying where the data has been saved
 */
af::Msg * jsonSaveObject( rapidjson::Document & i_obj);

} // namespace ProcessMsg

#endif // PROCESSMSG_H
