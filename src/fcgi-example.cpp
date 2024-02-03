#include <iostream>
#include <thread>
#include <mutex>
#include <string>
#include <csignal>

#ifdef _MSC_VER
#include <WinSock2.h>
#include <direct.h>
#define PATH_MAX MAX_PATH
#define getcwd _getcwd
#define OPEN_SYSLOG(NAME)
#define sleep Sleep
#define DEF_WAIT 1000
#else
#define DEF_WAIT 1
#endif

#include <fastcgi/fcgi_config.h>
#include <fastcgi/fcgiapp.h>

#include "daemonize.h"

const char *programName = "fcgi-example";

#define THREAD_COUNT 8 
// #define SOCKET_PATH "127.0.0.1:9000" 
#define SOCKET_PATH ":9000" 
#define MSG_INTERRUPTED 				"Interrupted "
#define MSG_GRACEFULLY_STOPPED			"Stopped gracefully"
#define MSG_ABRT                        "Aborted"

#define ERR_OPEN_SOCKET                 (-5001)

//хранит дескриптор открытого сокета 
static int socketId;

class AppEnv {
public:
    bool stopRequest;
    bool runAsDaemon;
    int running;
    int code;
    std::mutex m;
    AppEnv() 
        : stopRequest(false), runAsDaemon(false), running(0), code(0)
    {

    }
};

static void* serve(void* a)
{
    FCGX_Request request;
    if (FCGX_InitRequest(&request, socketId, 0) != 0) {
        std::cerr << "Can not init request" << std::endl;
        return nullptr;
    }

    AppEnv* env = (AppEnv*) a;

    env->m.lock();
    env->running++;
    env->m.unlock();
    
    while (!env->stopRequest) {
        env->m.lock();
        int rc = FCGX_Accept_r(&request);
        env->m.unlock();

        if (rc < 0)
            break;

        char* server_name = FCGX_GetParam("SERVER_NAME", request.envp);

        FCGX_PutS("Content-type: text/html\r\n\r\n", request.out);
        
        FCGX_PutS("<html>\r\n", request.out);
        FCGX_PutS("<head>\r\n", request.out);
        FCGX_PutS("<title>FastCGI Hello! (multi-threaded C, fcgiapp library)</title>\r\n", request.out);
        FCGX_PutS("</head>\r\n", request.out);
        FCGX_PutS("<body>\r\n", request.out);
        FCGX_PutS("<h1>FastCGI Hello! (multi-threaded C, fcgiapp library)</h1>\r\n", request.out);
        FCGX_PutS("<p>Request accepted from host <i>", request.out);
        FCGX_PutS(server_name ? server_name : "?", request.out);
        FCGX_PutS("</i></p>\r\n", request.out);
        FCGX_PutS("</body>\r\n", request.out);
        FCGX_PutS("</html>\r\n", request.out);

        FCGX_Finish_r(&request);
    }
    FCGX_Free(&request, true);

    env->m.lock();
    env->running--;
    env->m.unlock();

    return nullptr;
}

AppEnv env;

static void done() {
    while (env.running > 0) {
#ifdef _MSC_VER
    Sleep(1000);
#else
    sleep(1);    
#endif
    }
#ifdef _MSC_VER
    WSACleanup();
#endif
    std::cerr << MSG_GRACEFULLY_STOPPED << std::endl;
    exit(env.code);
}

static void stop() {
	env.stopRequest = true;
}

#ifndef _MSC_VER
void signalHandler(int signal)
{
    switch (signal) {
        case SIGINT:
            std::cerr << MSG_INTERRUPTED << std::endl;
            stop();
            done();
            break;
        case SIGABRT:
            std::cerr << MSG_ABRT << std::endl;
            exit(signal);
        default:
            break;
    }
}

#else

BOOL WINAPI winSignallHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType) {
        // Handle the CTRL-C signal.
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
        std::cerr << MSG_INTERRUPTED << std::endl;
        stop();
        // done();
        return FALSE;
    case CTRL_BREAK_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        std::cerr << MSG_INTERRUPTED << std::endl;
        stop();
        // done();
        return FALSE;
    default:
        return FALSE;
    }
}

#endif

void setSignalHandler()
{
#ifdef _MSC_VER
    SetConsoleCtrlHandler(winSignallHandler, TRUE);
#else
	struct sigaction action = {};
	action.sa_handler = &signalHandler;
	sigaction(SIGINT, &action, nullptr);
	sigaction(SIGHUP, &action, nullptr);
#endif
}

void run() {
#ifdef _MSC_VER
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    FCGX_Init();
    socketId = FCGX_OpenSocket(SOCKET_PATH, 20);
    if (socketId < 0) {
        env.code = ERR_OPEN_SOCKET;
        return;
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        std::thread t(serve, &env);
        std::cerr << i << std::endl;
        t.detach();
    }
    std::string l;
    while (!env.stopRequest && env.running > 0) {
        sleep(DEF_WAIT);
    }

}

int main(void)
{
    if (env.runAsDaemon) {
		char workDir[PATH_MAX];
		std::string programPath = getcwd(workDir, PATH_MAX);
		OPEN_SYSLOG(programName)
        Daemonize daemon(programName, programPath, run, stop, done);
	} else {
		setSignalHandler();
		run();
		done();
	}
}
