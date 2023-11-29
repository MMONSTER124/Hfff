#include "fastfetch.h"
#include "common/processing.h"
#include "common/io/io.h"
#include "common/time.h"

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

#define FF_PIPE_BUFSIZ 4096

static inline void waitpid_wrapper(const pid_t* pid)
{
    // remove zombie processes
    if (*pid > 0)
        waitpid(*pid, NULL, 0);
}

static inline int ffPipe2(int *fds, int flags)
{
    #ifdef __APPLE__
        if(pipe(fds) == -1)
            return -1;
        fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | flags);
        fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | flags);
        return 0;
    #else
        return pipe2(fds, flags);
    #endif
}

const char* ffProcessAppendOutput(FFstrbuf* buffer, char* const argv[], bool useStdErr)
{
    int pipes[2];
    const int timeout = instance.config.general.processingTimeout;

    if(ffPipe2(pipes, O_CLOEXEC) == -1)
        return "pipe() failed";

    __attribute__((__cleanup__(waitpid_wrapper))) pid_t childPid = fork();
    if(childPid == -1)
    {
        close(pipes[0]);
        close(pipes[1]);
        return "fork() failed";
    }

    //Child
    if(childPid == 0)
    {
        int nullFile = open("/dev/null", O_WRONLY|O_CLOEXEC);
        dup2(pipes[1], useStdErr ? STDERR_FILENO : STDOUT_FILENO);
        dup2(nullFile, useStdErr ? STDOUT_FILENO : STDERR_FILENO);
        setenv("LANG", "C", 1);
        execvp(argv[0], argv);
        _exit(127);
    }

    //Parent
    close(pipes[1]);

    int FF_AUTO_CLOSE_FD childPipeFd = pipes[0];
    char str[FF_PIPE_BUFSIZ];

    while(1)
    {
        if (timeout >= 0)
        {
            struct pollfd pollfd = { childPipeFd, POLLIN, 0 };
            if (poll(&pollfd, 1, timeout) == 0)
            {
                kill(childPid, SIGTERM);
                return "poll(&pollfd, 1, timeout) timeout";
            }
            else if (pollfd.revents & POLLERR)
            {
                kill(childPid, SIGTERM);
                return "poll(&pollfd, 1, timeout) error";
            }
        }

        ssize_t nRead = read(childPipeFd, str, FF_PIPE_BUFSIZ);
        if (nRead > 0)
            ffStrbufAppendNS(buffer, (uint32_t) nRead, str);
        else if (nRead == 0)
            return NULL;
        else if (nRead < 0)
            break;
    };

    return "read(childPipeFd, str, FF_PIPE_BUFSIZ) failed";
}
