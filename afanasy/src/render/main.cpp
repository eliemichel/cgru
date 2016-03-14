#ifndef WINNT
#include <sys/wait.h>
#endif

#include "../libafanasy/logger.h"
#include "../libafanasy/environment.h"
#include "../libafanasy/host.h"
#include "../libafanasy/render.h"

#include "res.h"
#include "renderhost.h"

#define AFOUTPUT
#undef AFOUTPUT
#include "../include/macrooutput.h"

extern bool AFRunning;

//####################### interrupt signal handler ####################################
#include <signal.h>
void sig_pipe(int signum)
{
	AFERROR("AFRender SIGPIPE");
}
void sig_int(int signum)
{
	if( AFRunning )
		fprintf( stderr,"\nAFRender: Interrupt signal catched.\n");
	AFRunning = false;
}
//#####################################################################################

// Functions:
void set_signal_handlers();
void msgCase(af::Msg * msg, RenderHost &render);

int main(int argc, char *argv[])
{
    Py_InitializeEx(0);

    set_signal_handlers();

	// Initialize environment and try to append python path:
	af::Environment ENV( af::Environment::AppendPythonPath | af::Environment::SolveServerName, argc, argv);
	if( !ENV.isValid())
	{
        AF_ERR << "Environment initialization failed.";
		exit(1);
	}

	// Fill command arguments:
	ENV.addUsage("-nimby", "Set initial state to 'nimby'.");
	ENV.addUsage("-NIMBY", "Set initial state to 'NIMBY'.");
	ENV.addUsage( std::string("-cmd") + " [command]", "Run command only, do not connect to server.");
	ENV.addUsage("-res", "Check host resources only and quit.");
	ENV.addUsage("-nor", "No output redirection.");
	// Help mode, usage is alredy printed, exiting:
	if( ENV.isHelpMode() )
		return 0;

	// Check resources and exit:
	if( ENV.hasArgument("-res"))
	{
		af::Host host;
		af::HostRes hostres;
		GetResources( host, hostres, true);
		af::sleep_msec(100);
		GetResources( host, hostres);
		printf("\n");
		host.v_stdOut( true);
		hostres.v_stdOut( true);
        Py_Finalize();
		return 0;
	}

	// Run command and exit
	if( ENV.hasArgument("-cmd"))
	{
		std::string command;
		ENV.getArgument("-cmd", command);
		printf("Test command mode:\n%s\n", command.c_str());

		pid_t m_pid;
		int status;
		pid_t pid = 0;
		#ifdef WINNT
		PROCESS_INFORMATION m_pinfo;
    	if( af::launchProgram( &m_pinfo, command, "", 0, 0, 0))
			m_pid = m_pinfo.dwProcessId;
		DWORD result = WaitForSingleObject( m_pinfo.hProcess, 0);
		if ( result == WAIT_OBJECT_0)
		{
			GetExitCodeProcess( m_pinfo.hProcess, &result);
			status = result;
			pid = m_pid;
		}
		else if ( result == WAIT_FAILED )
		{
			pid = -1;
		}
		#else
		m_pid = af::launchProgram( command, "", 0, 0, 0);
		pid = waitpid( m_pid, &status, 0);
		#endif

        Py_Finalize();
		return 0;
	}

	// Create temp directory, if it does not exist:
	if( af::pathMakePath( ENV.getTempDir(), af::VerboseOn ) == false) return 1;

    RenderHost * render = RenderHost::getInstance();

	uint64_t cycle = 0;
	while( AFRunning)
	{
        // Collect all available incomming messages:
        while( af::Msg * msg = render->acceptTry() )
            msgCase( msg, *render);

        // Let tasks to do their work:
        if( cycle % af::Environment::getRenderUpdateSec() == 0){
            render->refreshTasks();
        }

		// Update render resources:
        if( cycle % af::Environment::getRenderUpdateSec() == 0){
            render->update();
        }

        cycle++;

        if( AFRunning)
			af::sleep_sec(1);
    }

    AF_LOG << "Exiting render.";

    delete render;

    Py_Finalize();

	return 0;
}

void set_signal_handlers()
{
   // Set signals handlers:
#ifdef WINNT
    signal( SIGINT,  sig_int);
    signal( SIGTERM, sig_int);
    signal( SIGSEGV, sig_int);
#else
    struct sigaction actint;
    bzero( &actint, sizeof(actint));
    actint.sa_handler = sig_int;
    sigaction( SIGINT,  &actint, NULL);
    sigaction( SIGTERM, &actint, NULL);
    sigaction( SIGSEGV, &actint, NULL);
    // SIGPIPE signal catch:
    struct sigaction actpipe;
    bzero( &actpipe, sizeof(actpipe));
    actpipe.sa_handler = sig_pipe;
    sigaction( SIGPIPE, &actpipe, NULL);
#endif
}

void msgCase( af::Msg * msg, RenderHost &render)
{
	if( false == AFRunning )
		return;

    if( NULL == msg)
		return;

    int32_t rid = msg->getRid();
    AF_DEBUG << "RID=" << rid;
    if( rid >= 0 && rid < render.getFirstValidMsgId())
    {
        AF_WARN << "Ignoring obsolete message: " << *msg << " (rid=" << rid << ")";
        delete msg;
        return;
    }

    // Check not sended messages first, they were pushed back in accept queue:
	if( msg->wasSendFailed())
	{
        AF_WARN << "Message sending failed: " << *msg;
		if( msg->getAddress().equal( af::Environment::getServerAddress()))
		{
            AF_DEBUG << "Message was failed to send to server";
            render.connectionLost();
		}
		else if( msg->type() == af::Msg::TTaskOutput )
		{
            // Stop sending updates to this listener
            render.listenFailed( msg->getAddress());
		}
		delete msg;
		return;
	}

    AF_DEBUG  << "msgCase: " << *msg;

	switch( msg->type())
	{
	case af::Msg::TRenderId:
	{
		int new_id = msg->int32();
		// Server sends back -1 id if a render with the same hostname already exists:
		if( new_id == -1)
		{
            AF_ERR << "Render with this hostname '" << af::Environment::getHostName() << "' already registered.";
			AFRunning = false;
		}
		// Render was trying to register (its id==0) and server has send id>0
		// This is the situation when client was sucessfully registered
        else if( (new_id > 0) && (render.getId() == 0))
		{
            render.setRegistered( new_id);
		}
		// Server sends back zero id on any error
		else if ( new_id == 0 )
		{
            AF_LOG << "Zero ID recieved, no such online render, re-connecting...";
            render.connectionLost();
		}
		// Bad case, should not ever happen, try to re-register.
        else if ( render.getId() != new_id )
		{
            AF_ERR << "IDs mistatch: this " << render.getId() << " != " << new_id << " new, re-connecting...";
            render.connectionLost();
        }
		break;
	}
	case af::Msg::TVersionMismatch:
	case af::Msg::TClientExitRequest:
	{
        AF_LOG << "Render exit request received.";
		AFRunning = false;
		break;
	}
	case af::Msg::TTask:
	{
        render.runTask( msg);
		break;
	}
	case af::Msg::TClientRestartRequest:
	{
        AF_LOG << "Restart client request, executing command:\n" << af::Environment::getRenderExec();
		af::launchProgram(af::Environment::getRenderExec());
		AFRunning = false;
		break;
	}
	case af::Msg::TClientRebootRequest:
	{
		AFRunning = false;
        AF_LOG << "Reboot request, executing command:\n" << af::Environment::getRenderCmdReboot();
		af::launchProgram( af::Environment::getRenderCmdReboot());
		break;
	}
	case af::Msg::TClientShutdownRequest:
	{
		AFRunning = false;
        AF_LOG << "Shutdown request, executing command:\n" << af::Environment::getRenderCmdShutdown();
		af::launchProgram( af::Environment::getRenderCmdShutdown());
		break;
	}
	case af::Msg::TClientWOLSleepRequest:
	{
        AF_LOG << "Sleep request, executing command:\n" << af::Environment::getRenderCmdWolSleep();
		af::launchProgram( af::Environment::getRenderCmdWolSleep());
		break;
	}
	case af::Msg::TRenderStopTask:
	{
		af::MCTaskPos taskpos( msg);
        render.stopTask( taskpos);
		break;
	}
	case af::Msg::TRenderCloseTask:
	{
		af::MCTaskPos taskpos( msg);
        render.closeTask( taskpos);
		break;
	}
	case af::Msg::TTaskListenOutput:
	{
		af::MCListenAddress mcaddr( msg);
        render.listenTasks( mcaddr);
		break;
	}
	default:
	{
        AF_ERR << "Unknown message recieved: " << *msg;
		break;
	}
	}

	delete msg;
}
