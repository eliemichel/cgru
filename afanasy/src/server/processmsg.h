#ifndef PROCESSMSG_H
#define PROCESSMSG_H

#include "afcommon.h"

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
 * @brief processCoreMsg
 * @param i_args To be changed into what we actually use from those args?
 * @param i_msg Message to process
 */
void processCoreMsg( ThreadArgs * i_args, af::Msg * i_msg);

/**
 * @brief processJsonMsg
 * @param i_args
 * @param i_msg Message to process
 * @return Message to return
 */
af::Msg * processJsonMsg( ThreadArgs * i_args, af::Msg * i_msg);

/**
 * @brief processMsg
 * @param i_args
 * @param i_msg Message to process
 * @return Message to return
 */
af::Msg * processMsg( ThreadArgs * i_args, af::Msg * i_msg);

/**
 * @brief Save an object to JSON. Utility function, should not be here but is
 * used only in `processMsg`.
 * @param i_obj Object to save
 * @return A JSON Msg, saying where the data has been saved
 */
af::Msg * jsonSaveObject( rapidjson::Document & i_obj);

} // namespace ProcessMsg

#endif // PROCESSMSG_H
