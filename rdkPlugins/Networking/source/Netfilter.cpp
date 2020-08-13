/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2019 Sky UK
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
 * File:   Netfilter.cpp
 *
 */
#include "Netfilter.h"

#include <Logging.h>

#include <vector>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <iterator>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>

#define IPTABLES_SAVE_PATH "/usr/sbin/iptables-save"
#define IPTABLES_RESTORE_PATH "/usr/sbin/iptables-restore"

#if defined(DEV_VM)
    #define IP6TABLES_SAVE_PATH "/sbin/ip6tables-save"
    #define IP6TABLES_RESTORE_PATH "/sbin/ip6tables-restore"
#else
    #define IP6TABLES_SAVE_PATH "/usr/sbin/ip6tables-save"
    #define IP6TABLES_RESTORE_PATH "/usr/sbin/ip6tables-restore"
#endif

// -----------------------------------------------------------------------------
/**
 *  @class StdErrPipe
 *  @brief Utility object that creates a pipe to set as stderr.  Upon object
 *  destruction the contents of the pipe (if any) is written to the error log.
 *
 *
 */
class StdErrPipe
{
public:
    StdErrPipe()
        : mReadFd(-1)
        , mWriteFd(-1)
    {
        int fds[2];
        if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to create stderr pipe");
        }
        else
        {
            mReadFd = fds[0];
            mWriteFd = fds[1];
        }
    }

    ~StdErrPipe()
    {
        if ((mWriteFd >= 0) && (close(mWriteFd) != 0))
            AI_LOG_SYS_ERROR(errno, "failed to close write pipe");

        if (mReadFd >= 0)
        {
            dumpPipeContents();

            if (close(mReadFd) != 0)
                AI_LOG_SYS_ERROR(errno, "failed to close read pipe");
        }
    }

    int writeFd()
    {
        return mWriteFd;
    }

private:
    void dumpPipeContents()
    {
        char errBuf[256];

        ssize_t ret = TEMP_FAILURE_RETRY(read(mReadFd, errBuf, sizeof(errBuf) - 1));
        if (ret < 0)
        {
            if (ret != EAGAIN)
                AI_LOG_SYS_ERROR(errno, "failed to read from stderr pipe");
        }
        else if (ret > 0)
        {
            errBuf[ret] = '\0';
            AI_LOG_ERROR("%s", errBuf);
        }
    }

private:
    int mReadFd;
    int mWriteFd;
};

Netfilter::Netfilter()
{
}

// -----------------------------------------------------------------------------
/**
 *  @brief Performs a fork/exec operation and waits for the child to terminate
 *
 *  If any of the @a stdinFd, @a stdoutFd or @a stderrFd are less than 0 then
 *  the corresponding fd is redirected to /dev/null.
 *
 *
 *  @param[in]  execFile     The path to the file to execute
 *  @param[in]  args        The args to supply to the exec call
 *  @param[in]  stdinFd     The fd to redirect stdin to
 *  @param[in]  stdoutFd    The fd to redirect stdout to
 *  @param[in]  stderrFd    The fd to redirect stderr to
 *
 *  @return true on success, false on failure.
 */
bool Netfilter::forkExec(const std::string &execFile,
                         const std::list<std::string> &args,
                         int stdinFd, int stdoutFd, int stderrFd) const
{
    AI_LOG_FN_ENTRY();

    // get the executable name
    char *execFileCopy = strdup(execFile.c_str());
    char *execFileName = strdup(basename(execFileCopy));
    free(execFileCopy);

    // create the args vector (the first arg is always the exe name the last
    // is always nullptr)
    std::vector<char*> execArgs;
    execArgs.reserve(args.size() + 2);
    execArgs.push_back(execFileName);

    for (const std::string &arg : args)
        execArgs.push_back(strdup(arg.c_str()));

    execArgs.push_back(nullptr);

    // set an empty environment list so we don't leak info
    std::vector<char*> execEnvs(1, nullptr);


    // fork off to execute the iptables-save tool
    pid_t pid = vfork();
    if (pid == 0)
    {
        // within forked child

        // open /dev/null so can redirect stdout and stderr to that
        int devNull = open("/dev/null", O_RDWR);
        if (devNull < 0)
            _exit(EXIT_FAILURE);

        if (stdinFd < 0)
            stdinFd = devNull;
        if (stdoutFd < 0)
            stdoutFd = devNull;
        if (stderrFd < 0)
            stderrFd = devNull;

        // remap the standard descriptors to either /dev/null or one of the
        // supplied descriptors (nb: dup2 removes the O_CLOEXEC flag which is
        // what we want)
        if (dup2(stdinFd, STDIN_FILENO) != STDIN_FILENO)
            _exit(EXIT_FAILURE);

        if (dup2(stdoutFd, STDOUT_FILENO) != STDOUT_FILENO)
            _exit(EXIT_FAILURE);

        if (dup2(stderrFd, STDERR_FILENO) != STDERR_FILENO)
            _exit(EXIT_FAILURE);

        // don't need /dev/null anymore
        if (devNull > STDERR_FILENO)
            close(devNull);

        // reset the file mode mask to defaults
        umask(0);

        // reset the signal mask, we need to do this because signal mask are
        // inherited and we've explicitly blocked SIGCHLD as we're monitoring
        // that using sigwaitinfo
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGCHLD);
        if (sigprocmask(SIG_UNBLOCK, &set, nullptr) != 0)
            _exit(EXIT_FAILURE);

        // change the current working directory
        if ((chdir("/")) < 0)
            _exit(EXIT_FAILURE);

        // and finally execute the iptables-restore command
        execvpe(execFile.c_str(), execArgs.data(), execEnvs.data());

        // iptables-restore failed, but don't bother trying to print an error as
        // we've already redirected stdout & stderr to /dev/null
        _exit(EXIT_FAILURE);
    }

    // clean up dup'ed args
    for (char *arg : execArgs)
    {
        free(arg);
    }

    // sanity check the fork worked
    if (pid < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "vfork failed");
        return false;
    }


    // in the parent so wait till the iptables-save process completes
    int status;
    if (TEMP_FAILURE_RETRY(waitpid(pid, &status, 0)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "waitpid failed");
        return false;
    }
    else if (!WIFEXITED(status))
    {
        AI_LOG_ERROR_EXIT("%s didn't exit? (status: 0x%04x)", execFile.c_str(),
                          status);
        return false;
    }
    else if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        AI_LOG_ERROR_EXIT("%s failed with exit code %d", execFile.c_str(),
                          WEXITSTATUS(status));
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Uses the iptables-save tool to get the current rules
 *
 *  A ruleset is just a list of strings containing the iptables rule formatted
 *  in the same form the iptables-save reports.
 *
 *  Each list of rules is grouped by the table they belong to.
 *
 *  @param[in]  ipVersion       iptables version to use.
 *
 *  @return an list of all the rules read (ruleset)
 */
Netfilter::RuleSet Netfilter::getRuleSet(const int ipVersion) const
{
    AI_LOG_FN_ENTRY();

    // create a pipe for reading the iptables-save output
    int pipeFds[2];
    if (pipe2(pipeFds, O_CLOEXEC) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create pipe");
        return RuleSet();
    }

    // create a pipe to store the stderr output (it's destructor prints the
    // content of the pipe if not empty)
    StdErrPipe stdErrPipe;

    // exec the iptables-save function, passing in the pipe for stdout
    if (ipVersion == AF_INET)
    {
        if (!forkExec(IPTABLES_SAVE_PATH, { }, -1, pipeFds[1], stdErrPipe.writeFd()))
        {
            close(pipeFds[0]);
            close(pipeFds[1]);
            return RuleSet();
        }
    }
    else if (ipVersion == AF_INET6)
    {
        if (!forkExec(IP6TABLES_SAVE_PATH, { }, -1, pipeFds[1], stdErrPipe.writeFd()))
        {
            close(pipeFds[0]);
            close(pipeFds[1]);
            return RuleSet();
        }
    }
    else
    {
        close(pipeFds[0]);
        close(pipeFds[1]);
        AI_LOG_ERROR_EXIT("netfilter only supports AF_INET or AF_INET6");
        return RuleSet();
    }

    // close the write side of the pipe
    if (close(pipeFds[1]) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close write side of pipe");
    }

    // read everything out of the pipe
    std::string output;
    char buf[256];
    while (true)
    {
        ssize_t rd = TEMP_FAILURE_RETRY(read(pipeFds[0], buf, sizeof(buf)));
        if (rd < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to read from pipe");
            break;
        }
        else if (rd == 0)
        {
            break;
        }
        else
        {
            output.append(buf, rd);
        }
    }

    // can now close the read side of the pipe
    if (close(pipeFds[0]) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close read side of pipe");
    }


    // create a ruleset object with empty initial fields
    RuleSet ruleSet;
    ruleSet.insert(std::make_pair(TableType::Raw,    std::list<std::string>()));
    ruleSet.insert(std::make_pair(TableType::Nat,    std::list<std::string>()));
    ruleSet.insert(std::make_pair(TableType::Mangle, std::list<std::string>()));
    ruleSet.insert(std::make_pair(TableType::Filter, std::list<std::string>()));

    // parse the data read from the iptables-save tool
    std::istringstream rulesStream(output);
    std::string ruleLine;

    // the first character on a line indicates what follows, a '*' represents
    // a table name, a ':' is the chain name and default policy with packet
    // counts, and a '-' represents a rule to add. We only care about tables and
    // rules.
    TableType ruleTable = TableType::Invalid;
    while (std::getline(rulesStream, ruleLine))
    {
        if (ruleLine.empty())
            continue;

        if (ruleLine[0] == '*')
        {
            if (ruleLine == "*raw")
            {
                ruleTable = TableType::Raw;
            }
            else if (ruleLine == "*nat")
            {
                ruleTable = TableType::Nat;
            }
            else if (ruleLine == "*mangle")
            {
                ruleTable = TableType::Mangle;
            }
            else if (ruleLine == "*filter")
            {
                ruleTable = TableType::Filter;
            }
            else if (ruleLine == "*security")
            {
                ruleTable = TableType::Security;
            }
            else
            {
                AI_LOG_ERROR_EXIT("unknown rule line '%s'", ruleLine.c_str());
                return RuleSet();
            }
        }
        else if ((ruleLine.size() >= 3) && (ruleLine.compare(0, 3, "-A ") == 0))
        {
            if (ruleTable == TableType::Invalid)
            {
                AI_LOG_ERROR_EXIT("found rule without a table");
                return RuleSet();
            }
            else
            {
                // store the rule stripping off the "-A " characters at the front
                ruleSet[ruleTable].emplace_back(ruleLine.substr(3));
            }
        }
    }

    AI_LOG_FN_EXIT();
    return ruleSet;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Uses the iptables-restore tool to apply the given rules
 *
 *  The method creates a pipe to feed in the rules to the iptables-restore
 *  cmdline app, it then writes all the rules from the ruleset correctly
 *  formatted.
 *
 *  All rules are appended, so care must be taken to ensure that you don't
 *  end up with duplicated iptables rules.
 *
 *  @param[in]  operation       Operation to set (set/append/insert/delete)
 *  @param[in]  ruleSet         The ruleset to apply.
 *  @param[in]  ipVersion       iptables version to use.
 *
 *  @return true on success, false on failure.
 */
bool Netfilter::applyRuleSet(Operation operation, const RuleSet &ruleSet, const int ipVersion)
{
    AI_LOG_FN_ENTRY();

    // we simply need to pipe the fixed iptables rules into iptables-restore
    // without flushing the existing rules

    // create a pipe for feeding the iptables-restore monster
    int pipeFds[2];
    if (pipe2(pipeFds, O_CLOEXEC) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create pipe");
        return false;
    }

    // positive attitude
    bool success = true;

    // fill the pipe with the iptables rules
    const TableType tables[] = { TableType::Raw,     TableType::Nat,
                                 TableType::Mangle,  TableType::Filter,
                                 TableType::Security };
    const size_t nTables = sizeof(tables) / sizeof(tables[0]);

    for (size_t i = 0; success && (i < nTables); i++)
    {
        RuleSet::const_iterator table = ruleSet.find(tables[i]);
        if (table == ruleSet.end())
            continue;

        const std::list<std::string>& tableRules = table->second;
        if (tableRules.empty())
            continue;

        // write the table name first
        std::string line;
        switch (tables[i])
        {
            case TableType::Raw:        line = "*raw\n";        break;
            case TableType::Nat:        line = "*nat\n";        break;
            case TableType::Mangle:     line = "*mangle\n";     break;
            case TableType::Filter:     line = "*filter\n";     break;
            case TableType::Security:   line = "*security\n";   break;
            default:
                continue;
        }

        if (!writeString(pipeFds[1], line))
        {
            AI_LOG_SYS_ERROR(errno, "failed to write into pipe");
            success = false;
            break;
        }

        // then iterate over the rules to add
        for (const std::string &rule : tableRules)
        {
            switch (operation)
            {
                case Operation::Set:
                case Operation::Append:
                    line = "-A ";
                    break;
                case Operation::Insert:
                    line = "-I ";
                    break;
                case Operation::Delete:
                    line = "-D ";
                    break;

                case Operation::Unchanged:
                    line = "";
                    break;
            }

            line += rule;
            line += '\n';

            AI_LOG_DEBUG("applying rule '%s'", line.c_str());

            if (!writeString(pipeFds[1], line))
            {
                AI_LOG_SYS_ERROR(errno, "failed to write into pipe");
                success = false;
                break;
            }
        }

        // finish each table with a 'COMMIT'
        line = "COMMIT\n";
        if (!writeString(pipeFds[1], line))
        {
            AI_LOG_SYS_ERROR(errno, "failed to write into pipe");
            success = false;
            break;
        }

    }

    // close the write side of the pipe before calling iptables-restore
    if (close(pipeFds[1]) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close write side of pipe");
    }

    // exec the iptables-restore function, passing in the pipe for stdin
    if (success)
    {
        // set the args
        std::list<std::string> args;
        if (operation != Operation::Set)
        {
            args.emplace_back("--noflush");
        }

        // create a pipe to store the stderr output (it's destructor prints the
        // content of the pipe if not empty)
        StdErrPipe stdErrPipe;

        if (ipVersion == AF_INET)
        {
            success = forkExec(IPTABLES_RESTORE_PATH, args,
                               pipeFds[0], -1, stdErrPipe.writeFd());
        }
        else if (ipVersion == AF_INET6)
        {
            success = forkExec(IP6TABLES_RESTORE_PATH, args,
                               pipeFds[0], -1, stdErrPipe.writeFd());
        }
        else
        {
            AI_LOG_ERROR_EXIT("netfilter only supports AF_INET or AF_INET6");
            return false;
        }

    }

    // can now close the read side of the pipe
    if (close(pipeFds[0]) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close read side of pipe");
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Writes the string into the supplied file descriptor
 *
 *  Attempts to write the entire string until an error occurs.
 *
 *  @param[in]  fd      The fd to write to
 *  @param[in]  str     The string to write into the fd
 *
 *  @return true if the entire string was written, otherwise false.
 */
bool Netfilter::writeString(int fd, const std::string &str) const
{
    const char* s = str.data();
    ssize_t n = str.size();

    while (n > 0)
    {
        ssize_t wr = TEMP_FAILURE_RETRY(write(fd, s, n));
        if (wr < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to write to file");
            break;
        }
        else if (wr == 0)
        {
            break;
        }

        s += wr;
        n -= wr;
    }

    return (n == 0);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns the current iptables ruleset.
 *
 *  @param[in]  ipVersion       iptables version to use.
 *
 *  @return an list of all the rules read (ruleset)
 */
Netfilter::RuleSet Netfilter::rules(const int ipVersion) const
{
    return getRuleSet(ipVersion);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Replaces all installed iptables rules with one from the ruleset.
 *
 *  This will flush out all existing rules and then append the new ruleset.
 *  Beware this is probably not what you want as it will clear other rules setup
 *  by the system.
 *
 *  @param[in]  ruleSet         The ruleset to apply.
 *  @param[in]  ipVersion       iptables version to use.
 *
 *  @return true on success, false on failure.
 */
bool Netfilter::setRules(const RuleSet &ruleSet, const int ipVersion)
{
    AI_LOG_FN_ENTRY();

    // finally apply the rules
    bool success = applyRuleSet(Operation::Set, ruleSet, ipVersion);
    if (!success)
    {
        AI_LOG_ERROR("failed to set all iptables rules");
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns true if the rule is in the rulesList.
 *
 *  Both rule and the contents of rulesList are strings, however they are
 *  parsed as if they are command line args to the iptables tool.
 *
 *  @return true if the rule exists in the list, otherwise false.
 */
bool Netfilter::ruleInList(const std::string &rule,
                           const std::list<std::string> &rulesList) const
{
    return std::find(rulesList.begin(), rulesList.end(), rule) != rulesList.end();

    // FIXME: use parsed ruleset rather command line string compare
}

// -----------------------------------------------------------------------------
/**
 *  @brief Uses the iptables-restore tool to atomically add a set of rules
 *
 *  The function pipes the rules in @a ruleSet into the iptables-restore tool,
 *  this will append the rules onto the end ... beware.
 *
 *  This is equivalent to running the following for all the rules
 *     iptables -t <table> -A <rule>
 *
 *  Its publicly stated that iptables doesn't provide a stable C/C++ API
 *  for adding / removing rules, hence the reason we go to the extra pain
 *  of fork/exec.  All that said, we do have the libiptc.so on the box, but
 *  it's fair from an easy API to use.
 *
 *  @see https://bani.com.br/2012/05/programmatically-managing-iptables-rules-in-c-iptc
 *
 *  @param[in]  ruleSet         The ruleset to apply.
 *  @param[in]  ipVersion       iptables version to use.
 *
 *  @return true on success, false on failure.
 */
bool Netfilter::appendRules(const RuleSet &ruleSet, const int ipVersion)
{
    AI_LOG_FN_ENTRY();

    // get the existing iptables rules
    RuleSet existing = getRuleSet(ipVersion);

    // create a rule set from entries in the supplied argument that are not
    // not in the existing rules
    RuleSet actual;

    // check if we already have any of the rules
    for (const std::pair<const TableType, std::list<std::string>> &newRule : ruleSet)
    {
        const TableType &table = newRule.first;
        const std::list<std::string> &tableRules = newRule.second;

        // get the existing table
        const std::list<std::string> &existingRules = existing[table];

        // add the rule to the actual list if it exists
        for (const std::string &rule : tableRules)
        {
            if (!ruleInList(rule, existingRules))
            {
                actual[table].emplace_back(rule);
            }
        }
    }

    if (actual.empty())
    {
        AI_LOG_INFO("all iptables rules are already set");
        AI_LOG_FN_EXIT();
        return true;
    }

    // finally apply the rules
    bool success = applyRuleSet(Operation::Append, actual, ipVersion);
    if (!success)
    {
        AI_LOG_ERROR("failed to append all iptables rules");
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Uses the iptables-restore tool to atomically add a set of rules
 *
 *  The function pipes the rules in @a ruleSet into the iptables-restore tool,
 *  this will insert the rules at the begining of the  table
 *
 *  This is equivalent to running the following for all the rules
 *     iptables -t <table> -I <rule>
 *
 *  @warning This doesn't re-insert already existing rules.  If the rule already
 *  existed in the table then it's position is left unchanged.
 *
 *  @param[in]  ruleSet         The ruleset to apply.
 *  @param[in]  ipVersion       iptables version to use.
 *
 *  @return true on success, false on failure.
 */
bool Netfilter::insertRules(const RuleSet &ruleSet, const int ipVersion)
{
    AI_LOG_FN_ENTRY();

    // get the existing iptables rules
    RuleSet existing = getRuleSet(ipVersion);

    // create a rule set from entries in the supplied argument that are not
    // not in the existing rules
    RuleSet actual;

    // check if we already have any of the rules
    for (const std::pair<const TableType, std::list<std::string>> &newRule : ruleSet)
    {
        const TableType& table = newRule.first;
        const std::list<std::string>& tableRules = newRule.second;

        // get the existing table
        const std::list<std::string> &existingRules = existing[table];

        // add the rule to the actual list if it exists
        for (const std::string &rule : tableRules)
        {
            if (!ruleInList(rule, existingRules))
            {
                actual[table].emplace_back(rule);
            }
        }
    }

    if (actual.empty())
    {
        AI_LOG_INFO("all iptables rules are already set");
        AI_LOG_FN_EXIT();
        return true;
    }

    // finally apply the rules
    bool success = applyRuleSet(Operation::Insert, actual, ipVersion);
    if (!success)
    {
        AI_LOG_ERROR("failed to insert all iptables rules");
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Uses the iptables-restore tool to atomically delete a set of rules
 *
 *  The function pipes the rules in @a ruleSet into the iptables-restore tool,
 *  prefixed with the -D option.
 *
 *  This is equivalent to running the following for all the rules
 *     iptables -t <table> -D <rule>
 *
 *  @param[in]  ruleSet         The ruleset to apply.
 *  @param[in]  ipVersion       iptables version to use.
 *
 *  @return true on success, false on failure.
 */
bool Netfilter::deleteRules(const RuleSet &ruleSet, const int ipVersion)
{
    AI_LOG_FN_ENTRY();

    // get the existing iptables rules
    RuleSet existing = getRuleSet(ipVersion);

    // create a rule set from entries in the supplied argument that are in the
    // existing rules, we need this because iptables-restore throw an error
    // if trying to remove a rule that doesn't exist
    RuleSet actual;

    // check if we already have any of the rules
    for (const std::pair<const TableType, std::list<std::string>> &newRule : ruleSet)
    {
        const TableType& table = newRule.first;
        const std::list<std::string>& tableRules = newRule.second;

        // get the existing table
        const std::list<std::string>& existingRules = existing[table];

        // add the rule to the actual list if it exists
        for (const std::string& rule : tableRules)
        {
            if (ruleInList(rule, existingRules))
            {
                actual[table].emplace_back(rule);
            }
            else
            {
                AI_LOG_DEBUG("failed to find rule '%s' to delete", rule.c_str());
            }
        }
    }

    if (actual.empty())
    {
        AI_LOG_INFO("none of the rules to remove are in the table");
        AI_LOG_FN_EXIT();
        return true;
    }

    // finally apply the rules
    bool success = applyRuleSet(Operation::Delete, actual, ipVersion);
    if (!success)
    {
        AI_LOG_ERROR("failed to delete all iptables rules");
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates a new IPTables chain with the given name.
 *
 *  If @a withDropRule is true then a catch-all rule is added to the newly
 *  created change to drop everything.
 *
 *  This is equivalent to running the following for all the rules
 *     iptables -t <table> -N <name>
 *
 *  @param[in]  table           The ruleset to apply.
 *  @param[in]  name            Name of the chain to add.
 *  @param[in]  withDropRule    Add drop rule if true.
 *  @param[in]  ipVersion       iptables version to use.
 *
 *  @return true on success, false on failure.
 */
bool Netfilter::createNewChain(TableType table, const std::string &name,
                               bool withDropRule, const int ipVersion)
{
    AI_LOG_FN_ENTRY();

    // create the initial ruleset to create the table
    RuleSet newChainRuleSet =
        {
            {   table,
                {
                    // create the table
                    ":" + name + " - [0:0]"
                }
            },
        };

    // if a drop rule was requested append that to the end of the newly
    // created table
    if (withDropRule)
    {
        newChainRuleSet[table].emplace_back("-A " + name + " -j DROP");
    }

    // finally apply the rules
    bool success = applyRuleSet(Operation::Unchanged, newChainRuleSet, ipVersion);
    if (!success)
    {
        AI_LOG_ERROR("failed to append all iptables rules");
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Debugging function to print out the supplied ruleset.
 *
 *
 */
void Netfilter::dump(const RuleSet &ruleSet, const char *title) const
{
    AI_LOG_INFO("======== %s ==========", title ? title : "");
    for (const auto &ruleTable : ruleSet)
    {
        switch (ruleTable.first)
        {
            case TableType::Invalid:    AI_LOG_INFO("INVALID");         break;
            case TableType::Raw:        AI_LOG_INFO("*raw");            break;
            case TableType::Nat:        AI_LOG_INFO("*nat");            break;
            case TableType::Mangle:     AI_LOG_INFO("*mangle");         break;
            case TableType::Filter:     AI_LOG_INFO("*filter");         break;
            case TableType::Security:   AI_LOG_INFO("*security");       break;
        }

        for (const auto &ruleLine : ruleTable.second)
        {
            AI_LOG_INFO("%s", ruleLine.c_str());
        }
    }
    AI_LOG_INFO("======== %s ==========", title ? title : "");
}

