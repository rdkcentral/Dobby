/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
/*
 *  File:   InitMain.cpp
 *  Author:
 *
 *
 *  This file creates a very simple 'init' process for the container.  The main
 *  motivation for this is described here:
 *
 *  https://blog.phusion.nl/2015/01/20/docker-and-the-pid-1-zombie-reaping-problem/
 *
 *  It boils down to ensuring we have an 'init' process that does at least the
 *  following two things:
 *
 *      1. Reaps adopted child processes.
 *      2. Forwards on signals to child processes.
 *
 *  In addition to the above it provides some basic logging to indicate why a
 *  child process died.
 *
 *  It's worth pointing out that runC does implement a sub-reaper which
 *  is enabled by default - it can be disabled by specifying the
 *  '--no-subreaper' option on the start command line.  However it doesn't
 *  solve the signal problems, and I've found without this code in place the
 *  only way to kill a process inside a container is with SIGKILL, which is
 *  a bit anti-social.
 *
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <vector>
#include <string>
#include <cstdlib>


#if defined(USE_ETHANLOG)

    #include <ethanlog.h>

    #define LOG_ERR(fmt,...) \
        do { \
            ethanlog(ETHAN_LOG_ERROR, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__); \
            fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
        } while(0)

    #define LOG_NFO(fmt,...) \
        do { \
            ethanlog(ETHAN_LOG_INFO, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__); \
            fprintf(stdout, fmt "\n", ##__VA_ARGS__); \
        } while(0)

#else

    #define LOG_ERR(fmt,...) \
        do { \
            fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
        } while(0)

    #define LOG_NFO(fmt,...) \
        do { \
            fprintf(stdout, fmt "\n", ##__VA_ARGS__); \
        } while(0)

#endif



static void closeAllFileDescriptors(int logPipeFd)
{
    // the two options for this are to loop over every possible file descriptor
    // (usually 1024), or read /proc/self/fd/ directory.  I've gone for the
    // later as think it's slightly nicer although more cumbersome to implement.

    // get the fd rlimit
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) != 0)
    {
        LOG_ERR("failed to get the fd rlimit, defaulting to 1024 (%d - %s)",
                errno, strerror(errno));
        rlim.rlim_cur = 1024;
    }

    // iterate through all the fd sym links
    int dirFd = open("/proc/self/fd/", O_DIRECTORY | O_CLOEXEC);
    if (dirFd < 0)
    {
        LOG_ERR("failed to open '/proc/self/fd/' directory (%d - %s)",
                errno, strerror(errno));
        return;
    }

    DIR *dir = fdopendir(dirFd);
    if (!dir)
    {
        LOG_ERR("failed to open '/proc/self/fd/' directory");
        return;
    }

    std::vector<int> openFds;
    openFds.reserve(8);

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (entry->d_type != DT_LNK)
            continue;

        // get the fd and sanity check it's in the valid range
        long fd = std::strtol(entry->d_name, nullptr, 10);
        if ((fd < 3) || (fd > (long)rlim.rlim_cur))
            continue;

        // skip the fd opened for iterating the directory
        if (fd == dirFd)
            continue;

        openFds.push_back(int(fd));
    }

    closedir(dir);

    // close all the open fds (except stdin, stdout or stderr)
    for (int fd : openFds)
    {
        // don't close the logging pipe
        if ((logPipeFd >= 0) && (fd == logPipeFd))
            continue;

        // close all the other descriptors
        if (close(fd) != 0)
            LOG_ERR("failed to close fd %d (%d - %s)", fd, errno, strerror(errno));
    }
}

#if (AI_BUILD_TYPE == AI_DEBUG)

static bool readCgroup(const std::string &cgroup, unsigned long *val)
{
    static const std::string base = "/sys/fs/cgroup/";
    std::string path = base + cgroup;

    FILE *fp = fopen(path.c_str(), "r");
    if (!fp)
    {
        if (errno != ENOENT)
            LOG_ERR("failed to open '%s' (%d - %s)", path.c_str(), errno, strerror(errno));

        return false;
    }

    char* line = nullptr;
    size_t len = 0;
    ssize_t rd;

    if ((rd = getline(&line, &len, fp)) < 0)
    {
        if (line)
            free(line);
        fclose(fp);
        LOG_ERR("failed to read cgroup file line (%d - %s)", errno, strerror(errno));
        return false;
    }

    *val = strtoul(line, nullptr, 0);

    fclose(fp);
    free(line);

    return true;
}

static void checkForOOM(void)
{
    unsigned long failCnt;

    if (readCgroup("memory/memory.failcnt", &failCnt) && (failCnt > 0))
    {
        LOG_ERR("memory allocation failure detected in container, likely OOM (failcnt = %lu)", failCnt);
    }

    if (readCgroup("gpu/gpu.failcnt", &failCnt) && (failCnt > 0))
    {
        LOG_NFO("GPU memory allocation failure detected in container (failcnt = %lu)", failCnt);
    }
}

#if defined(USE_ETHANLOG)

static void reportLoggingPipeInode(int logPipeFd)
{
    // this code to log pipe inode number is to provide extra info in the
    // case of debug context name is incorrect. It should be removed
    // as soon as we are sure a pipe mis-connection is not the cause of this
    // problem
    if (logPipeFd >= 0)
    {
        struct stat pipeStat;
        if (fstat(logPipeFd, &pipeStat) < 0)
        {
            fprintf(stderr, "Couldn't fstat ethanlog pipe (%d - %s)",
                    errno, strerror(errno));
        }
        else
        {
            ethanlog(ETHAN_LOG_MILESTONE, NULL, NULL, 0,
                     "Logging pipe inode is %d",
                     int(pipeStat.st_ino));
        }
    }
}

#else // defined(USE_ETHANLOG)

static void reportLoggingPipeInode(int logPipeFd)
{
    (void) logPipeFd;
}

#endif // defined(USE_ETHANLOG)

#endif // (AI_BUILD_TYPE == AI_DEBUG)

static int doForkExec(int argc, char * argv[])
{
    // if a ETHAN_LOG pipe was supplied then we don't want to close that as we
    // use it to log the exit status of the thing we launched
    int logPipeFd = -1;

#if defined(USE_ETHANLOG)
    const char *logPipeEnv = getenv("ETHAN_LOGGING_PIPE");
    if (logPipeEnv)
    {
        long fd = std::strtol(logPipeEnv, nullptr, 10);
        if ((fd >= 3) && (fd <= 1024))
            logPipeFd = static_cast<int>(fd);
    }
#endif

    // print the logging pipe indode number to make sure that proper app
    // name is shown in logs
#if (AI_BUILD_TYPE == AI_DEBUG)
    reportLoggingPipeInode(logPipeFd);
#endif


    const int maxArgs = 64;

    if ((argc < 2) || (argc > maxArgs))
    {
        LOG_ERR("to many or too few args (%d)", argc);
        return EXIT_FAILURE;
    }

    pid_t exePid = vfork();
    if (exePid < 0)
    {
        LOG_ERR("failed to fork and launch app (%d - %s)", errno, strerror(errno));
        return EXIT_FAILURE;
    }

    if (exePid == 0)
    {
        // the args supplied to the init process are what we supply to the
        // child exec'd process, i.e.
        //
        //   argv[] = { "DobbyInit", <arg1>, <arg2>, ... <argN> }
        //                             /       /           /
        //   args[] = {   basename(<arg1>), <arg2>, ... <argN> }
        //

        char* args[maxArgs];
        char* execBinary = argv[1];

        // the first arg is always the name of the exec being run
        args[0] = basename(execBinary);

        // copy the rest of the args verbatium
        for (int i = 2; i < argc; i++)
            args[i - 1] = argv[i];

        // terminate with a null
        args[(argc - 1)] = nullptr;

        // within forked client so exec the main process
        execvp(argv[1], args);

        // if we reached here then the above has failed
        LOG_ERR("failed exec '%s' (%d - %s)", argv[1], errno, strerror(errno));
        _exit(EXIT_FAILURE);
    }

    // we should now close any file descriptors we have open except for
    // stdin, stdout or stderr.  If we don't do this it's a minor security hole
    // as we'll be holding the file descriptors open for the lifetime of the
    // container ... whereas it's the app that we run that should manage the
    // lifetime of any supplied descriptors (except stdin, stdout and stderr)
    closeAllFileDescriptors(logPipeFd);


    int ret = EXIT_FAILURE;

    // wait for all children to finish
    pid_t pid;
    int status;
    while ((pid = TEMP_FAILURE_RETRY(wait(&status))) != -1)
    {
        if (pid > 0)
        {
            char msg[128];
            int msglen;

            msglen = snprintf(msg, sizeof(msg), "pid %d has terminated ", pid);

            if (WIFSIGNALED(status))
            {
                msglen += snprintf(msg + msglen, sizeof(msg) - msglen,
                                   "by signal %d ", WTERMSIG(status));

                if (WCOREDUMP(status))
                {
                    msglen += snprintf(msg + msglen, sizeof(msg) - msglen,
                                       "and produced a core dump ");
                }
            }

            if (WIFEXITED(status))
            {
                msglen += snprintf(msg + msglen, sizeof(msg) - msglen,
                                   "(return code %d)", WEXITSTATUS(status));

                if (pid == exePid)
                {
                    ret = WEXITSTATUS(status);
                }
            }

            // if the process died because of a signal, or it didn't exit with
            // success then log as an error, otherwise it's just info
            if (WIFSIGNALED(status) ||
                (WIFEXITED(status) && (WEXITSTATUS(status) != EXIT_SUCCESS)))
            {
                LOG_ERR("%s", msg);
            }
            else
            {
                LOG_NFO("%s", msg);
            }
        }
    }

#if (AI_BUILD_TYPE == AI_DEBUG)

    // check the memory cgroups memory status for allocation failures, this is
    // an indication of OOMs
    checkForOOM();

#endif

    return ret;
}

static void signalHandler(int sigNum)
{
    // consume the signal but passes it onto all processes in the container
    kill(-1, sigNum);
}

int main(int argc, char * argv[])
{
    // install a signal handler for SIGTERM and friends, dobby sends a SIGTERM
    // first to ask the container to die, then "after a reasonable timeout"
    // sends a SIGKILL.
    const int sigNums[] = { SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGFPE,
                            SIGSEGV, SIGALRM, SIGTERM, SIGUSR1, SIGUSR2 };
    for (int sigNum : sigNums)
    {
        if (signal(sigNum, signalHandler) != nullptr)
        {
            LOG_ERR("failed to install handler for signal %d (%d - %s)",
                    sigNum, errno, strerror(errno));
            // should this be fatal ?
        }
    }

    return doForkExec(argc, argv);
}

