#pragma once

#include "../libafanasy/common/dlRWLock.h"

#include "../libafanasy/msgclasses/mclistenaddress.h"

#include "../libafanasy/msgqueue.h"
#include "../libafanasy/render.h"

#include "taskprocess.h"

class Parser;
class PyRes;

class RenderHost: public af::Render
{
private:  // This is a singleton class
    RenderHost();

public:
    ~RenderHost();
    /// Get singleton instance
    static RenderHost * getInstance();

    inline void acceptMessage( af::Msg * i_msg) { m_msgAcceptQueue->pushMsg( i_msg);}
    void dispatchMessage( af::Msg * i_msg);

    /// Get incomming messages (blocking and not blocking versions)
    inline af::Msg * acceptWait() { return m_msgAcceptQueue->popMsg( af::AfQueue::e_wait);    }
    inline af::Msg * acceptTry()  { return m_msgAcceptQueue->popMsg( af::AfQueue::e_no_wait); }

    /// Some getters and setters
    inline bool noOutputRedirection() { return m_no_output_redirection; }
    bool isConnected() { return m_connected;  }
    void setRegistered( int i_id);
    void setUpdateMsgType( int i_type);

    /**
     * @brief Switch the render host to a "connection lost" state to try to reconnect.
     */
    void connectionLost();

    /**
     * @brief Task monitoring cycle, checking how task processes are doing
     */
    void refreshTasks();

    /**
     * @brief Main cycle function, measuring host ressources and sending heartbeat to the server
     */
    void update();

    /**
     * @brief Get one of the render's task processes by its taskpos or its job/block/task/num
     */
    TaskProcess * getTask( const af::MCTaskPos & i_taskpos);
    TaskProcess * getTask( int i_jobId, int i_blockNum, int i_taskNum, int i_Number);

    /**
     * @brief Create a new TaskExec and then a TaskProcess from the provided
     * message and add it to the render's tasks
     * @param i_msg TTask message received from server
     */
    void runTask( af::Msg * i_msg);

    /**
     * @brief Stop a task process
     * @param i_taskpos Index of the task process to stop
     */
    void stopTask( const af::MCTaskPos & i_taskpos);

    /**
     * @brief Close a task process
     * @param i_taskpos Index of the task process to close
     */
    void closeTask( const af::MCTaskPos & i_taskpos);

    /**
     * @brief Write task output into a message
     * @param i_taskpos Index of the task process to get output from
     * @param o_msg Message into which writing the task output
     */
    void getTaskOutput( const af::MCTaskPos & i_taskpos, af::Msg * o_msg);

    /**
     * @brief Subscribe or unsubscribe to task updates, depending on what the
     * `i_mcaddr` Msg Content asks for.
     * @param i_mcaddr ListenAddress message content specifying what to listen to
     */
    void listenTasks( const af::MCListenAddress & i_mcaddr);

    /**
     * @brief Unsubscribe the given address from all task processes and send
     * message to the subscriber to say so
     * (not sure actually, but it sends something to the subscriber...)
     * @param i_addr Address to unsubscribe
     */
    void listenFailed( const af::Address & i_addr);

#ifdef WINNT
    void windowsMustDie();
#endif

private:
    std::vector<std::string> m_windowsmustdie;

    std::vector<PyRes*> m_pyres;

    /// Queue of incoming messages
    af::MsgQueue * m_msgAcceptQueue;
    /// Active queue sending messages. Spawns another thread.
    af::MsgQueue * m_msgDispatchQueue;

    /// Whether the render is connected or not
    bool m_connected;

    /// Heartbeat message to sent at each update.
    /// It is initially a `TRenderRegister` and as soon as the server
    /// registered the render, it becomes a `TRenderUpdate`.
    int m_updateMsgType;

    /// List of task processed being currently ran by the render
    std::vector<TaskProcess*> m_tasks;

    /// Whether the task outputs must be redirected. Used essentially by TaskProcess
    bool m_no_output_redirection;

    /// Bool used to avoid measuring ressources at the first update (dirty hack, should be avoided)
    bool m_first_time;
};
