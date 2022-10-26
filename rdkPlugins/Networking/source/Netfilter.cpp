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
#include "StdStreamPipe.h"

#if defined (DOBBY_BUILD)
    #include <Logging.h>
#else
    #include <Dobby/Logging.h>
#endif

#include <vector>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <iterator>
#include <libgen.h>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <ext/stdio_filebuf.h>
#include <regex>

#define IPTABLES_SAVE_PATH "/usr/sbin/iptables-save"
#define IPTABLES_RESTORE_PATH "/usr/sbin/iptables-restore"

#if defined(DEV_VM)
    #define IPTABLES_PATH "/sbin/iptables"
    #define IP6TABLES_SAVE_PATH "/sbin/ip6tables-save"
    #define IP6TABLES_RESTORE_PATH "/sbin/ip6tables-restore"
#else
    #define IPTABLES_PATH "/usr/sbin/iptables"
    #define IP6TABLES_SAVE_PATH "/usr/sbin/ip6tables-save"
    #define IP6TABLES_RESTORE_PATH "/usr/sbin/ip6tables-restore"
#endif


// for some reason the XiOne toolchain is build against old kernel headers
// which doesn't have the memfd syscall
#if !defined(SYS_memfd_create) && defined(__arm__)
#   define SYS_memfd_create    385
#endif

// glibc prior to version 2.27 didn't have a syscall wrapper for memfd_create(...)
#if defined(__GLIBC__) && ((__GLIBC__ < 2) || ((__GLIBC__ >= 2) && (__GLIBC_MINOR__ < 27)))
#include <syscall.h>
static inline int memfd_create(const char *name, unsigned int flags)
{
    return syscall(SYS_memfd_create, name, flags);
}
#endif

#  if !defined(MFD_CLOEXEC)
#    define MFD_CLOEXEC         0x0001U
#  endif

Netfilter::Netfilter()
    : mIptablesVersion(getIptablesVersion())
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

    // create a memfd for storing the iptables-save output
    int rulesMemFd = memfd_create("iptables-save-buf", MFD_CLOEXEC);
    if (rulesMemFd < 0)
     {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create memfd buffer");
        return RuleSet();
    }

    // create a pipe to store the stderr output (it's destructor prints the
    // content of the pipe if not empty)
    StdStreamPipe stdErrPipe(true);

    // exec the iptables-save function, passing in the pipe for stdout
    if (ipVersion == AF_INET)
    {
        if (!forkExec(IPTABLES_SAVE_PATH, { }, -1, rulesMemFd, stdErrPipe.writeFd()))
        {
            close(rulesMemFd);
            return RuleSet();
        }
    }
    else if (ipVersion == AF_INET6)
    {
        if (!forkExec(IP6TABLES_SAVE_PATH, { }, -1, rulesMemFd, stdErrPipe.writeFd()))
        {
            close(rulesMemFd);
            return RuleSet();
        }
    }
    else
    {
        close(rulesMemFd);
        AI_LOG_ERROR_EXIT("netfilter only supports AF_INET or AF_INET6");
        return RuleSet();
    }

    AI_LOG_DEBUG("iptables-save wrote %ld bytes into the buffer",
                 lseek(rulesMemFd, 0, SEEK_CUR));

    // seek back to the start of the file
    if (lseek(rulesMemFd, 0, SEEK_SET) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to seek to the beginning of the memfd file");
    }

    // and wrap in a ifstream object (nb: stdio_filebuf takes ownership of the
    // fd and will close it when done)
    __gnu_cxx::stdio_filebuf<char> rulesBuf(rulesMemFd, std::ios::in);
    std::istream rulesStream(&rulesBuf);


    // create a ruleset object with empty initial fields
    RuleSet ruleSet;
    ruleSet.insert(std::make_pair(TableType::Raw,    std::list<std::string>()));
    ruleSet.insert(std::make_pair(TableType::Nat,    std::list<std::string>()));
    ruleSet.insert(std::make_pair(TableType::Mangle, std::list<std::string>()));
    ruleSet.insert(std::make_pair(TableType::Filter, std::list<std::string>()));

    // parse the data read from the iptables-save tool
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

    if (ruleSet.size() == 0)
    {
        AI_LOG_WARN("iptables-save returned no rules - suspicious");
    }

    AI_LOG_FN_EXIT();
    return ruleSet;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Trims duplicates from mRuleSets based on the operation.
 *
 *  Rules with 'Delete' operation will be removed from mRuleSets if the rule is
 *  not found in iptables-save, so we avoid deleting rules that aren't there.
 *
 *  Conversely, any other rules are removed from mRuleSets if they are found in
 *  iptables-save, so we avoid adding duplicate rules.
 *
 *  @param[in]  existing        existing rules from iptables-save.
 *  @param[in]  newRuleSet      new ruleset to add from mRuleSets.
 *  @param[in]  operation       operation intended to be added for the ruleset.
 */
void Netfilter::trimDuplicates(RuleSet &existing, RuleSet &newRuleSet, Operation operation) const
{
    // iterate through all tables in new ruleset
    for (std::pair<const TableType, std::list<std::string>> &newRules : newRuleSet)
    {
        const TableType &table = newRules.first;
        AI_LOG_DEBUG("Trimming duplicates for rule type %d", int(table));
        std::list<std::string> &tableRules = newRules.second;

        // get the existing table
        auto existingIt = existing.find(table);
        if (existingIt == existing.end())
        {
            AI_LOG_WARN("Could not find any existing rules for table type %d", int(table));
            continue;
        }
        const std::list<std::string> &existingRules = (*(existingIt)).second;

        // iterate through all rules in the table to check for duplicates
        auto it = tableRules.begin();
        while (it != tableRules.end())
        {
            const std::string &rule = *it;
            if (operation == Operation::Delete && !ruleInList(rule, existingRules))
            {
                // didn't find rule to delete, remove from ruleset
                AI_LOG_DEBUG("failed to find rule '%s' to delete", rule.c_str());
                it = tableRules.erase(it);
            }
            else if (ruleInList(rule, existingRules))
            {
                // found duplicate rule, remove from ruleset
                AI_LOG_DEBUG("skipping duplicate rule '%s'", rule.c_str());
                it = tableRules.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Checks all rulesets in a rule cache for duplicates to check which
 *  rules need to be applied.
 *
 *  @param[in]  ruleCache       cache of rules to check for duplicates.
 *  @param[in]  ipVersion       iptables version to use.
 *
 *  @return true if there are any new rules to write, otherwise false.
 */
bool Netfilter::checkDuplicates(RuleSets ruleCache, const int ipVersion) const
{
    AI_LOG_FN_ENTRY();

    // get the existing iptables rules
    RuleSet existing = getRuleSet(ipVersion);

    if (existing.size() == 0)
    {
        AI_LOG_ERROR("Failed to get existing iptables rules - cannot determine which rules to write");
        return false;
    }

    // trim duplicates rules from the rulesets we want to add to iptables
    trimDuplicates(existing, ruleCache.appendRuleSet, Operation::Append);
    trimDuplicates(existing, ruleCache.insertRuleSet, Operation::Insert);
    trimDuplicates(existing, ruleCache.deleteRuleSet, Operation::Delete);
    trimDuplicates(existing, ruleCache.unchangedRuleSet, Operation::Unchanged);

    // check if we actually have any rules left to apply
    if (ruleCache.appendRuleSet.empty() && ruleCache.insertRuleSet.empty() &&
        ruleCache.deleteRuleSet.empty() && ruleCache.unchangedRuleSet.empty())
    {
        AI_LOG_INFO("All container iptables rules are duplicates - no new rules to write");
        AI_LOG_FN_EXIT();
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Uses the iptables-restore tool to apply the rules stored in
 *  mRulesets.
 *
 *  The method creates a pipe to feed in the rules to the iptables-restore
 *  cmdline app, it then writes all the rules from the ruleset correctly
 *  formatted.
 *
 *  Its publicly stated that iptables doesn't provide a stable C/C++ API
 *  for adding / removing rules, hence the reason we go to the extra pain
 *  of fork/exec. Running benchmark tests with an implementation of a libiptc
 *  wrapper resulted in slower results compared to using fork/exec [RDK-29283].
 *
 *  @param[in]  ipVersion       iptables version to use.
 *
 *  @return true on success, false on failure.
 */
bool Netfilter::applyRules(const int ipVersion)
{
    AI_LOG_FN_ENTRY();

    // we simply need to pipe the fixed iptables rules into iptables-restore
    // without flushing the existing rules

    // create a memfd for feeding the iptables-restore monster
    int rulesMemFd = memfd_create("iptables-restore-buf", MFD_CLOEXEC);
    if (rulesMemFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create memfd buffer");
        return false;
    }

    // and wrap in a ifstream object (nb: stdio_filebuf takes ownership of the
    // fd and will close it when done)
    __gnu_cxx::stdio_filebuf<char> rulesBuf(rulesMemFd, std::ios::out);
    std::ostream rulesStream(&rulesBuf);

    // positive attitude
    bool success = true;

    // get reference to the correct cache
    RuleSets &ruleCache = (ipVersion == AF_INET) ? mIpv4RuleCache : mIpv6RuleCache;

    // before doing anything to the rules, check for duplicates in iptables and
    // remove the duplicates from our cache
    if (!checkDuplicates(ruleCache, ipVersion))
    {
        // all of the rules were duplicate, none left to write
        AI_LOG_FN_EXIT();
        return true;
    }

    // fill the pipe with the iptables rules
    const TableType tableTypes[] = { TableType::Raw,     TableType::Nat,
                                     TableType::Mangle,  TableType::Filter,
                                     TableType::Security };

    // iterate through all tables
    for (TableType tableType : tableTypes)
    {
        // check if there are any rules to be applied for this table
        RuleSet::iterator tableUnchanged = ruleCache.unchangedRuleSet.find(tableType);
        RuleSet::iterator tableAppend = ruleCache.appendRuleSet.find(tableType);
        RuleSet::iterator tableInsert = ruleCache.insertRuleSet.find(tableType);
        RuleSet::iterator tableDelete = ruleCache.deleteRuleSet.find(tableType);

        // add all rules with their matching operation to the table rules
        std::list<std::pair<Operation, std::list<std::string>>> tableRules;
        // Unchanged = new chain, which will have to go first
        if (tableUnchanged != ruleCache.unchangedRuleSet.end())
        {
            tableRules.emplace_back(std::pair<Operation, std::list<std::string>>({ Operation::Unchanged, tableUnchanged->second }));
        }
        if (tableAppend != ruleCache.appendRuleSet.end())
        {
            tableRules.emplace_back(std::pair<Operation, std::list<std::string>>({ Operation::Append, tableAppend->second }));
        }
        if (tableInsert != ruleCache.insertRuleSet.end())
        {
            tableRules.emplace_back(std::pair<Operation, std::list<std::string>>({ Operation::Insert, tableInsert->second }));
        }
        if (tableDelete != ruleCache.deleteRuleSet.end())
        {
            tableRules.emplace_back(std::pair<Operation, std::list<std::string>>({ Operation::Delete, tableDelete->second }));
        }

        // if there are no rules to install, try the next table
        if (tableRules.empty())
        {
            continue;
        }

        // write the table name first
        switch (tableType)
        {
            case TableType::Raw:        rulesStream << "*raw\n";        break;
            case TableType::Nat:        rulesStream << "*nat\n";        break;
            case TableType::Mangle:     rulesStream << "*mangle\n";     break;
            case TableType::Filter:     rulesStream << "*filter\n";     break;
            case TableType::Security:   rulesStream << "*security\n";   break;
            default:
                continue;
        }

        // iterate through rule operations
        for (auto &ruleGroup : tableRules)
        {
            std::string operationStr;
            switch (ruleGroup.first)
            {
                case Operation::Append:
                    operationStr = "-A ";
                    break;
                case Operation::Insert:
                    operationStr = "-I ";
                    break;
                case Operation::Delete:
                    operationStr = "-D ";
                    break;
                case Operation::Unchanged:
                    operationStr = "";
                    break;
            }

            // and then the actual rules
            for (const std::string &rule : ruleGroup.second)
            {
                rulesStream << operationStr;
                rulesStream << rule;
                rulesStream << '\n';
            }
        }

        // finish each table with a 'COMMIT'
        rulesStream << "COMMIT\n";
        if (rulesStream.bad() || rulesStream.fail())
        {
            AI_LOG_ERROR("failed to write into memfd");
            success = false;
            break;
        }
    }

    // exec the iptables-restore function, passing in the pipe for stdin
    if (success)
    {
        // set the args
        std::list<std::string> args;
        args.emplace_back("--noflush");

        // Prevent race condition with iptables locking during bootup
        // Need version 1.6.2 or higher. Latest RDK should ship with 1.8.x series
        if ((mIptablesVersion.major >= 1 && mIptablesVersion.minor > 6) ||
            (mIptablesVersion.major >= 1 && mIptablesVersion.minor == 6 && mIptablesVersion.patch >= 2))
        {
            // wait up to 2 seconds to aquire lock
            args.emplace_back("-w");
            args.emplace_back("2");
            // poll lock status every 0.1 secs
            args.emplace_back("-W");
            args.emplace_back("100000");
        }
        else
        {
            AI_LOG_DEBUG("iptables-restore too old to support waiting");
        }

        // create a pipe to store the stderr output (it's destructor prints the
        // content of the pipe if not empty)
        StdStreamPipe stdErrPipe(true);

        // ensure all rules flushed to the memfd
        rulesStream.flush();

        // seek back to the beginning of the memfd
        int rulesFd = rulesBuf.fd();
        if (lseek(rulesFd, 0, SEEK_SET) < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to seek to the beginning of the memfd");
        }

        if (ipVersion == AF_INET)
        {
            success = forkExec(IPTABLES_RESTORE_PATH, args,
                               rulesFd, -1, stdErrPipe.writeFd());
        }
        else if (ipVersion == AF_INET6)
        {
            success = forkExec(IP6TABLES_RESTORE_PATH, args,
                               rulesFd, -1, stdErrPipe.writeFd());
        }
        else
        {
            AI_LOG_ERROR_EXIT("netfilter only supports AF_INET or AF_INET6");
            return false;
        }

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
 *  @brief Adds rules to the internal rule caches.
 *
 *  The rules are added to the correct cache depending on the input ipVersion
 *  and operation type.
 *
 *  ipVersion is set to either AF_INET or AF_INET6 depending on whether the
 *  rule is an IPv4 rule for iptables or IPv6 rule for ip6tables.
 *
 *  The operation types match the following iptables/ip6tables options:
 *
 *      Netfilter::Append: -A
 *      Netfilter::Insert: -I
 *      Netfilter::Delete: -D
 *      Netfilter::Unchanged: not used in this method, @see createNewChain()
 *
 *  NB: The rules are not written into iptables until the
 *  Netfilter::applyRules() method is called.
 *
 *  @param[in]  ruleSet         The ruleset to apply.
 *  @param[in]  ipVersion       iptables version to use.
 *  @param[in]  operation       iptables operation to use for rules.
 *
 *  @return returns true on success, otherwise false.
 */
bool Netfilter::addRules(RuleSet &ruleSet, const int ipVersion, Operation operation)
{
    AI_LOG_FN_ENTRY();

    if (ipVersion != AF_INET && ipVersion != AF_INET6)
    {
        AI_LOG_ERROR_EXIT("incorrect ip version %d, use AF_INET or AF_INET6", ipVersion);
        return false;
    }

    // get pointer to the correct operation's ruleset in the cache
    RuleSets *ruleCache = (ipVersion == AF_INET) ? &mIpv4RuleCache : &mIpv6RuleCache;
    RuleSet *cacheRuleSet;
    switch(operation) {
        case Operation::Append:
            cacheRuleSet = &ruleCache->appendRuleSet;
            break;
        case Operation::Insert:
            cacheRuleSet = &ruleCache->insertRuleSet;
            break;
        case Operation::Delete:
            cacheRuleSet = &ruleCache->deleteRuleSet;
            break;
        case Operation::Unchanged:
            AI_LOG_ERROR_EXIT("operation type 'Unchanged' not allowed, use Append, "
                              "Insert or Delete");
            return false;
    }

    for (auto &it : ruleSet)
    {
        // find the ruleset's Netfilter::TableType rules table from cache
        auto cacheRuleSetTable = cacheRuleSet->find(it.first);
        if (cacheRuleSetTable == cacheRuleSet->end())
        {
            // the table doesn't exist, so we can just emplace ours
            cacheRuleSet->emplace(it);
        }
        else
        {
            // table exists, merge new rules to the end of it
            cacheRuleSetTable->second.splice(cacheRuleSetTable->second.end(), it.second);
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates a new IPTables chain with the given name and put it in the
 *  rule cache to write later.
 *
 *  The Netfilter::Unchanged operation type is used to add new chains.
 *
 *  This is equivalent to:
 *     iptables -t <table> -N <name>
 *
 *  @param[in]  table           The table to add the new chain to.
 *  @param[in]  name            Name of the chain to add.
 *  @param[in]  ipVersion       iptables version to use.
 *
 *  @return always returns true.
 */
bool Netfilter::createNewChain(TableType table, const std::string &name, const int ipVersion)
{
    AI_LOG_FN_ENTRY();

    // get reference to the correct cache
    RuleSets &ruleCache = (ipVersion == AF_INET) ? mIpv4RuleCache : mIpv6RuleCache;

    // create the rule to add the new chain
    const std::string chainRule = ":" + name + " - [0:0]";

    // find rules table from cache
    auto cacheRuleset = ruleCache.unchangedRuleSet.find(table);
    if (cacheRuleset == ruleCache.unchangedRuleSet.end())
    {
        // the table doesn't exist, so add it as a new table
        ruleCache.unchangedRuleSet.emplace(
            std::pair<TableType, std::list<std::string>>({ table, { chainRule }})
        );
    }
    else
    {
        // table exists, merge new rules to the end of it
        cacheRuleset->second.merge({ chainRule });
    }

    AI_LOG_FN_EXIT();
    return true;
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

// -----------------------------------------------------------------------------
/**
 * Gets the version of iptables that's installed
 *
 * @returns iptables version
 */
Netfilter::IptablesVersion Netfilter::getIptablesVersion() const
{
    AI_LOG_FN_ENTRY();

    Netfilter::IptablesVersion version{
        0, // Major
        0, // Minor
        0  // Patch
    };

    // create a pipe for reading the stderr
    StdStreamPipe stdOutPipe(false);
    StdStreamPipe stdErrPipe(true);

    std::list<std::string> args;
    args.emplace_back("--version");

    if (!forkExec(IPTABLES_PATH, args, -1, stdOutPipe.writeFd(), stdErrPipe.writeFd()))
    {
        AI_LOG_ERROR_EXIT("Failed to get iptables version");
        return version;
    }

    std::string output = stdOutPipe.getPipeContents();

    // Got version string, parse
    static const std::regex versionMatch(R"(v([0-9]+)\.([0-9]+)\.([0-9]+))", std::regex::ECMAScript | std::regex::icase);

    std::cmatch matches;
    if (!std::regex_search(output.c_str(), matches, versionMatch) && matches.size() != 3)
    {
        AI_LOG_ERROR_EXIT("Failed to parse iptables version");
        return version;
    }

    version.major = std::stoi(matches.str(1));
    version.minor = std::stoi(matches.str(2));
    version.patch = std::stoi(matches.str(3));

    AI_LOG_DEBUG("Running iptables version %d.%d.%d",
                version.major, version.minor, version.patch);

    AI_LOG_FN_EXIT();

    return version;
}