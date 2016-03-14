#pragma once

#include <time.h>

#include "afqueue.h"
#include "client.h"

namespace af
{
///   Message - Afanasy communication unit.
/**   Any type of client ( aftalk, afrender), afcmd and afserver communicate through messages.
***   Message can have some data or not, determined on its type.
***   Messages with type greater than or equal to \c Msg::TDATA must have some data to be valid.
***   They must have non zero data pointer and data length greater than zero.
***   Messages with type less than \c Msg::TDATA must not have any data to be valid.
***   They must have NULL data pointer.
**/
class Msg : public Af, public AfQueueItem
{
public:
    /// Default constructor.
    Msg( int msgType = 0, int msgInt = 0, bool i_receiving = false);

    /// Constructor from \c Af class.
    Msg( int msgType, Af * afClass, bool i_receiving = false );

    /// Construct a message and set an address
    Msg( const struct sockaddr_storage * ss);

    Msg( const char * rawData, int rawDataLen, bool i_receiving = false);

    ~Msg();///< Destructor.

    void v_generateInfoStream( std::ostringstream & stream, bool full = false) const;

    /// To set zero (\c Msg::TNone ) message to some non data message. Return \c true on success.
    bool set( int msgType, int msgInt = 0, bool i_receiving = false);

    /// Write \c Af class to message.
    bool set( int msgType, Af * afClass, bool i_receiving = false);

    /// To set zero (\c Msg::TNone ) message to data message. Return \c true on success.
        /// On TJSON header type will be not binary - binary header will be skipped at all.
    bool setData( int i_size, const char * i_msgData, int i_type = TDATA);

    /// To JSON data message with binary header (not for python or browser).
    void setJSONBIN();
//	bool setJSON_headerBin( const std::string & i_str);
//	inline bool setJSON_headerBin( const std::ostringstream & i_str) { return setJSON_headerBin( i_str.str());}

    /// To set zero (\c Msg::TNone ) message to QString message. Return \c true on success.
    bool setString( const std::string & qstring);

    /// To set zero (\c Msg::TNone ) message to QStringList message. Return \c true on success.
    bool setStringList( const std::list<std::string> & stringlist);

    /// Get String ( if message type is TString ).
    bool getString( std::string & string);
    const std::string getString();

    /// Get String ( if message type is TStringList ).
    bool getStringList( std::list<std::string> & stringlist);

/**   IMPORTANT!
***   messages with (type < MText) MUST NOT have any data to be valid:
***   (( mdata == NULL) && ( data_len == 0)) ALWAYS !
***   messages with (type >= MText) MUST have some data to be valid:
***   (( mdata != NULL) && ( data_len > 0)) ALWAYS !
**/
   enum Type{
/*------------ NONDATA MESSAGES ----------------------*/
/// Default message with default type - zero. Only this type can be changed by \c set function.
/**/TNULL/**/,
/// Message set to this type itself, when reading.
/**/TVersionMismatch/**/,
/// Invalid message. This message type generated by constructors if wrong arguments provieded.
/**/TInvalid/**/,

/**/TConfirm/**/,                   ///< Simple answer with no data to confirm something.

/// Request messages, sizes, quantities statistics. Can be requested displayed by anatoly.
/**/TStatRequest/**/,
/// NEW VERSION
/**/DEPRECATED_TConfigLoad/**/,                ///< Reload config file
/**/DEPRECATED_TFarmLoad/**/,                  ///< Reload farm file


/**/TClientExitRequest/**/,         ///< Request to client to exit,
/**/TClientRestartRequest/**/,      ///< Restart client application,
/**/TClientWOLSleepRequest/**/,     ///< Request to client to fall a sleep,
/**/TClientRebootRequest/**/,       ///< Reboot client host computer,
/**/TClientShutdownRequest/**/,     ///< Shutdown client host computer,

/*- Talk messages -*/
/**/DEPRECATED_TTalkId/**/,                    ///< Id for new Talk. Server sends it back when new Talk registered.
/**/DEPRECATED_TTalkUpdateId/**/,              ///< Update Talk with given id ( No information for updating Talk needed).
/**/DEPRECATED_TTalksListRequest/**/,          ///< Request online Talks list.
/**/DEPRECATED_TTalkDeregister/**/,            ///< Deregister talk with given id.


/*- Monitor messages -*/
/**/TMonitorId/**/,                 ///< Id for new Monitor. Server sends it back when new Talk registered.
/**/TMonitorUpdateId/**/,           ///< Update Monitor with given id ( No information for updating Monitor needed).
/**/TMonitorsListRequest/**/,       ///< Request online Monitors list.
/**/TMonitorDeregister/**/,         ///< Deregister monitor with given id.
/**/TMonitorLogRequestId/**/,       ///< Request a log of a Monitor with given id.

/*- Render messages -*/
/** When Server successfully registered new Render it's send back it's id.**/
/**/TRenderId/**/,
/**/TRendersListRequest/**/,        ///< Request online Renders list message.
/**/TRenderLogRequestId/**/,        ///< Request a log of Render with given id.
/**/TRenderTasksLogRequestId/**/,   ///< Request a log of Render with given id.
/**/TRenderInfoRequestId/**/,       ///< Request a string information about a Render with given id.
/**/TRenderDeregister/**/,          ///< Deregister Render with given id.


/*- Users messages -*/
/**/TUsersListRequest/**/,          ///< Active users information.
/// Uset id. Afanasy sends it back as an answer on \c TUserIdRequest , which contains user name.
/**/TUserId/**/,
/**/TUserLogRequestId/**/,          ///< Request a log of User with given id.
/**/TUserJobsOrderRequestId/**/,    ///< Request User(id) jobs ids in server list order.


/*- Job messages -*/
/**/TJobsListRequest/**/,           ///< Request brief of jobs.
/**/TJobsListRequestUserId/**/,     ///< Request brief of jobs of user with given id.
/**/TJobLogRequestId/**/,           ///< Request a log of a job with given id.
/**/TJobErrorHostsRequestId/**/,    ///< Request a list of hosts produced tasks with errors.
/**/TJobsWeightRequest/**/,         ///< Request all jobs weight.

/// Request a job with given id. The answer is TJob. If there is no job with such id the answer is TJobRequestId.
/**/TJobRequestId/**/,
/// Request a job progress with given id. The answer is TJobProgress. If there is no job with such id the answer is TJobProgressRequestId.
/**/TJobProgressRequestId/**/,

TRESERVED00,
TRESERVED01,
TRESERVED02,
TRESERVED03,
TRESERVED04,
TRESERVED05,
TRESERVED06,
TRESERVED07,
TRESERVED08,
TRESERVED09,

/*---------------------------------------------------------------------------------------------------------*/
/*--------------------------------- DATA MESSAGES ---------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/


/**/TDATA/**/,                      ///< Some data.
/**/TTESTDATA/**/,                  ///< Test some data transfer.
/**/TJSON/**/,                      ///< JSON
/**/THTTP/**/,                      ///< HTTP - with JSON POST data.
/**/THTTPGET/**/,                   ///< HTTP Get request.
/**/TString/**/,                    ///< String message.
/**/TStringList/**/,                ///< Strings list message.

/**/TStatData/**/,                  ///< Statistics data.

/*- Client messages -*/

/*- Talk messages -*/
/// Register Talk. Send by Talk client to register. Server sends back its id \c TTalkId.
/// NEW VERSION
/**/DEPRECATED_TTalkRegister/**/,
/**/DEPRECATED_TTalksListRequestIds/**/,       ///< Request a list of Talks with given ids.
/**/DEPRECATED_TTalksList/**/,                 ///< Message with a list of online Talks.
/**/DEPRECATED_TTalkDistributeData/**/,        ///< Message with a list Talk's users and a text to send to them.
/**/DEPRECATED_TTalkData/**/,                  ///< Message to Talk with text.


/*- Monitor messages -*/
/// Register Monitor. Send by Monitor client to register. Server sends back its id \c TMonitorId.
/**/TMonitorRegister/**/,
/**/TMonitorsListRequestIds/**/,    ///< Request a list of Monitors with given ids.
/**/TMonitorsList/**/,              ///< Message with a list of online Monitors.
/**/TMonitorSubscribe/**/,          ///< Subscribe monitor on some events.
/**/TMonitorUnsubscribe/**/,        ///< Unsubscribe monitor from some events.
/**/TMonitorUsersJobs/**/,          ///< Set users ids to monitor their jobs.
/**/TMonitorJobsIdsAdd/**/,         ///< Add jobs ids for monitoring.
/**/TMonitorJobsIdsSet/**/,         ///< Set jobs ids for monitoring.
/**/TMonitorJobsIdsDel/**/,         ///< Delete monitoring jobs ids.
/**/TMonitorMessage/**/,            ///< Send a message (TQString) to monitors with provieded ids (MCGeneral).

/**/TMonitorEvents_BEGIN/**/,       ///< Events types start.

/**/TMonitorJobEvents_BEGIN/**/,    ///< Job events types start.
/**/TMonitorJobsAdd/**/,            ///< IDs of new jobs.
/**/TMonitorJobsChanged/**/,        ///< IDs of changed jobs.
/**/TMonitorJobsDel/**/,            ///< IDs of deleted jobs.
/**/TMonitorJobEvents_END/**/,      ///< Job events types end.

/**/TMonitorCommonEvents_BEGIN/**/, ///< Common events types start.
/**/TMonitorUsersAdd/**/,           ///< IDs of new users.
/**/TMonitorUsersChanged/**/,       ///< IDs of changed users.
/**/TMonitorUsersDel/**/,           ///< IDs of deleted users.
/**/TMonitorRendersAdd/**/,         ///< IDs of new renders.
/**/TMonitorRendersChanged/**/,     ///< IDs of changed renders.
/**/TMonitorRendersDel/**/,         ///< IDs of deleted renders.
/**/TMonitorMonitorsAdd/**/,        ///< IDs of new monitors.
/**/TMonitorMonitorsChanged/**/,    ///< IDs of changed monitors.
/**/TMonitorMonitorsDel/**/,        ///< IDs of deleted monitors.
/**/TMonitorTalksAdd/**/,           ///< IDs of new talks.
/**/TMonitorTalksDel/**/,           ///< IDs of deleted talks.
/**/TMonitorCommonEvents_END/**/,   ///< Common events types end.

/**/TMonitorEvents_END/**/,         ///< Events types end.


/*- Render messages -*/
/** Sent by Render on start, when it's server begin to listen port.
And when Render can't connect to Afanasy. Afanasy register new Render and send back it's id \c TRenderId. **/
/**/TRenderRegister/**/,
/**/TRenderUpdate/**/,              ///< Update Render, message contains its resources.
/**/TRendersListRequestIds/**/,     ///< Request a list of Renders with given ids.
/**/TRendersResourcesRequestIds/**/,///< Request a list of resources of Renders with given ids.
/**/TRendersList/**/,               ///< Message with a list of Renders.
/**/TRendersResources/**/,          ///< Message with a list of resources of Renders.
/**/TRenderStopTask/**/,            ///< Signal from Afanasy to Render to stop task.
/**/TRenderCloseTask/**/,           ///< Signal from Afanasy to Render to close (delete) finished (stopped) task.


/*- Users messages -*/
/**/TUsersListRequestIds/**/,       ///< Request a list of Users with given ids.
/**/TUsersList/**/,                 ///< Active users information.
/// NEW VERSION
/**/DEPRECATED_TUserAdd/**/,                   ///< Add a permatent user.
/**/TUserIdRequest/**/,             ///< Request an id of user with given name.
/**/TUserJobsOrder/**/,             ///< Jobs ids in server list order.


/*- Job messages -*/
/// NEW VERSION
/**/DEPRECATED_TJobRegister/**/,               ///< Register job.
/**/TJobsListRequestIds/**/,        ///< Request a list of Jobs with given ids.
/**/TJobsListRequestUsersIds/**/,   ///< Request brief of jobs od users with given ids.
/**/TJobsList/**/,                  ///< Jobs list information.
/**/TJobProgress/**/,               ///< Jobs progress.
/**/TJobsWeight/**/,                ///< All jobs weight data.
/**/TJob/**/,                       ///< Job (all job data).

/**/TBlocksProgress/**/,            ///< Blocks running progress data.
/**/TBlocksProperties/**/,          ///< Blocks progress and properties data.
/**/TBlocks/**/,                    ///< Blocks data.

/**/TTask/**/,                      ///< A task of some job.
/**/TTaskRequest/**/,               ///< Get task information.
/**/TTaskLogRequest/**/,            ///< Get task information log.
/**/TTaskErrorHostsRequest/**/,     ///< Get task error hosts list.
/**/TTaskOutputRequest/**/,         ///< Job task output request.
/**/TTaskUpdatePercent/**/,         ///< New progress percentage for task.
/**/TTaskUpdateState/**/,           ///< New state for task.
/**/TTaskListenOutput/**/,          ///< Request to send task output to provided address.
/**/TTaskFiles/**/,                 ///< Task (or entire job) files
/**/TTasksRun/**/,                  ///< Job tasks run data.

/**/TTaskOutput/**/,                ///< Job task output data (for task listening: from afrender directly to afwatch).
/**/TJSONBIN/**/,
/**/TRESERVED12/**/,
/**/TRESERVED13/**/,
/**/TRESERVED14/**/,
/**/TRESERVED15/**/,
/**/TRESERVED16/**/,
/**/TRESERVED17/**/,
/**/TRESERVED18/**/,
/**/TRESERVED19/**/,

/**/TLAST/**/                       ///< The last type number.
};

    static const char * TNAMES[]; ///< Type names.

    inline int   type()    const { return m_type;  }///< Get message type.
    inline char* data()    const { return m_data;  }///< Get data pointer.
    inline int   dataLen() const { return m_int32; }///< Get data length.
    inline int   int32()   const { return m_int32; }///< Get 32-bit integer, data lenght for data messages.

    inline void setRid( int32_t rid) { m_rid = rid; rw_header( true); }
    inline int32_t getId()  const { return m_id; }
    inline int32_t getRid() const { return m_rid; }

    inline char* buffer() const { return m_buffer;}///< Get buffer pointer.

    /// Get message full size (with data).
    inline int writeSize() const { return m_type<TDATA ? Msg::SizeHeader : Msg::SizeHeader+m_int32;}

    /// Get buffer at already written postition to write \c size bytes in it.
    char * writtenBuffer( int size);
    inline bool isWriting() const { return  m_writing; } ///< Writing or reading data in message.
    inline bool isReading() const { return !m_writing; } ///< Writing or reading data in message.

    void setInvalid();             ///< Set message invalidness.
    void readHeader( int bytes);   ///< Read header from message buffer, \c bytes - number of already written bytes in it's buffer.

    // Set header to specified values, that are get from text data
    // i_offset and i_bytes to shift message data, to remove HTTP header for example
    void setHeader( int i_type, int i_size, int i_offset = 0, int i_bytes = 0);

    inline bool      isNull() const { return m_type == TNULL;    }///< Whether message is null.
    inline bool   isInvalid() const { return m_type == TInvalid; }///< Whether message is invalid.

    void stdOutData( bool withHeader = true);

    static const int SizeHeader;     ///< size of message header.
    static const int SizeBuffer;     ///< message reading buffer size.
    static const int SizeBufferLimit;///< message buffer maximum size.
    static const int SizeDataMax;    ///< maximum data size that can handle a message.

    static const int Version;    ///< Current afanasy version.

    inline int version() const { return m_version; } ///< Get message afanasy version.

    inline void resetWrittenSize() { m_writtensize = 0; }

    inline bool addressIsEmpty() const { return m_address.isEmpty();}

    inline const size_t addressesCount() const { return m_addresses.size();}

    /// Set to recieve an answer from the same socket after send
    void setReceiving( bool i_value = true ) { m_receive = i_value; }

    /// Set to recieve an answer from the same socket after send
    bool isReceiving() const { return m_receive; }

    /// Set to recieve an answer from the same socket after send
    void setSendFailed( bool i_value = true ) { ++m_sendfailedattempts; m_sendfailed = i_value; }

    /// Set to recieve an answer from the same socket after send
    bool wasSendFailed() { return m_sendfailed; }

    /// Set message address
    inline void setAddress( const Address & i_address)
        { m_address = i_address;}

    /// Set message address to \c client
    inline void setAddress( const Client* i_client)
        { m_address = i_client->getAddress();}

    /// Set message address to \c client
    inline void setAddresses( const std::list<Address> & i_addresses)
        { m_addresses = i_addresses;}

    /// Add dispatch address
    inline void addAddress( const Client* client)
        { m_addresses.push_back( client->getAddress());}

    /// Get address constant pointer
    inline const Address & getAddress() const { return m_address;}

    /// Get addresses constant list pointer
    inline const std::list<Address> * getAddresses() const { return &m_addresses;}

    /// Whether one should keep on trying to send the message
    inline bool canRetrySending() const { return m_sendfailedattempts < m_maxsendfailedattempts; }

    /// Returns the number of attempts to send the message
    inline int getSendingAttempts() const { return m_sendfailedattempts; }

    void setTypeHTTP();
//	void createHTTPHeader();

    inline int getHeaderOffset() const { return m_header_offset;}

public:
    /// Convenient utility to built string message
    static Msg * msgString( const std::string & i_str);

    static inline int32_t getNextId() { return ms_nextId; }

private:

// header:
    int32_t m_version;   ///< Afanasy network protocol version.
    int32_t m_type;      ///< Message type.
    int32_t m_int32;     ///< Some 32-bit integer, data length for data messages.

// id system
    int32_t m_id;   ///< Message ID, sender side, that should be provided back in any answer as rid
    int32_t m_rid;  ///< Response ID, destination, identifing the message to which this is an answer, or -1

// data poiters:
    char * m_buffer;     ///< Internal buffer pointer, for header and data
    char * m_data;       ///< Message data pointer = buffer + header_size.

// buffering parameters:
    bool m_writing;                  ///< Writing or reading data in message.
    int  m_buffer_size;              ///< Buffer size.
    int  m_data_maxsize;             ///< Data maximum size ( = buffer size - header size).
    int  m_writtensize;              ///< Number of bytes already written in message buffer.
    int  m_header_offset;            ///< From where begin to write, for exampl to send json to browser
                                     ///< we should skip header at all ( m_header_offset = Msg::SizeHeader )

// communication parameters:
    Address m_address;                ///< Address, where message came from or will be send.
    std::list<Address> m_addresses;   ///< Addresses to dispatch message to.
    bool m_receive;                   ///< Whether to recieve an answer on message request.
    bool m_sendfailed;                ///< Message was failed to send.
    int m_sendfailedattempts;         ///< number of attempts to sent the message that already failed
    int m_maxsendfailedattempts;      ///< maximum number of attempts to send. Beyond this, canRetrySending returns false

private:
    static int32_t ms_nextId;

private:

    void construct();                ///< Called from constuctors.
    bool checkZero( bool outerror ); ///< Check Zero type, data length and pointer.
    bool checkValidness();           ///< Check message header validness and magic number;

    /// Allocate memory for buffer, copy \c to_copy_len bytes in new buffer if any
    bool allocateBuffer( int i_size, int i_copy_len = 0, int i_copy_offset = Msg::SizeHeader);

    void rw_header( bool write); ///< Read or write message header.
    void v_readwrite( Msg * msg);
};
}
