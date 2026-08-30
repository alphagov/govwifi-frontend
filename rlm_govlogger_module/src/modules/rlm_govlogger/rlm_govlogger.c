/*
 *   This program is is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or (at
 *   your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/**
 * $Id: 9ff2b48e02eb0ca0b6b13b4c1111f0334fadf6fe $
 * @file rlm_govlogger.c
 * @brief Example module code.
 *
 * @copyright 2013 The FreeRADIUS server project
 * @copyright 2013 your name \<your address\>
 */
// RCSID("$Id: 9ff2b48e02eb0ca0b6b13b4c1111f0334fadf6fe $")

#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/modules.h>
#include <freeradius-devel/rad_assert.h>
#include <freeradius-devel/modpriv.h>
#include <freeradius-devel/modcall.h>
#include <freeradius-devel/parser.h>

#include <pthread.h>
#include <freeradius-devel/log.h>
#include <include/modpriv.h>

#include <rlm_eap.h>
#include "conf_parse.h"
#include "json.h"

#include <linux/close_range.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * If we have no threads then we need not lock...
 */
#ifdef HAVE_PTHREAD_H
#define PTHREAD_MUTEX_LOCK pthread_mutex_lock
#define PTHREAD_MUTEX_UNLOCK pthread_mutex_unlock
#else
#define PTHREAD_MUTEX_LOCK(_x)
#define PTHREAD_MUTEX_UNLOCK(_x)
#endif


// The number of seconds a retry is done on opening the logfile - we dont give up
#define LOGFILE_OPEN_RETRY_SECONDS (60)

#undef LOTS_OF_DEBUG

#ifdef LOTS_OF_DEBUG
#define MYDEBUG(...) DEBUG(__VA_ARGS__)
#else
#define MYDEBUG(...) do { } while(0)
#endif


static CONF_PARSER const json_format_attr_config[] = {
    { "prefix", FR_CONF_OFFSET(PW_TYPE_STRING, rlm_govlogger_t, attr_prefix), NULL },

    CONF_PARSER_TERMINATOR
};

static CONF_PARSER const json_format_value_config[] = {
    { "single_value_as_array", FR_CONF_OFFSET(PW_TYPE_BOOLEAN, rlm_govlogger_t, value_as_array), "no" },
    { "enum_as_integer", FR_CONF_OFFSET(PW_TYPE_BOOLEAN, rlm_govlogger_t, enum_as_int), "no" },
    { "dates_as_integer", FR_CONF_OFFSET(PW_TYPE_BOOLEAN, rlm_govlogger_t, dates_as_int), "no" },
    { "always_string", FR_CONF_OFFSET(PW_TYPE_BOOLEAN, rlm_govlogger_t, always_string), "no" },

    CONF_PARSER_TERMINATOR
};

static CONF_PARSER const fr_json_format_config[] = {
    { "output_mode", FR_CONF_OFFSET(PW_TYPE_STRING, rlm_govlogger_t, output_mode_str), "object" },
    { "attribute", FR_CONF_POINTER(PW_TYPE_SUBSECTION, NULL), (void const *) json_format_attr_config },
    { "value", FR_CONF_POINTER(PW_TYPE_SUBSECTION, NULL), (void const *) json_format_value_config },
    CONF_PARSER_TERMINATOR
};


/*
 *    A mapping of config file variables to entries in the rlm_govlogger_t for the config parser
 */
static const CONF_PARSER module_config[] = {
    { "log_file",              FR_CONF_OFFSET(PW_TYPE_STRING | PW_TYPE_REQUIRED, rlm_govlogger_t, log_filename), NULL },
    { "rotated_log_file",      FR_CONF_OFFSET(PW_TYPE_STRING, rlm_govlogger_t, rotated_log_filename), NULL },
    { "rotate_interval",       FR_CONF_OFFSET(PW_TYPE_INTEGER,                   rlm_govlogger_t, rotate_wait_seconds), "20" },
    { "talloc_debug_filename", FR_CONF_OFFSET(PW_TYPE_STRING,                    rlm_govlogger_t, talloc_debug_filename), NULL },
    { "max_mmapped_mem_size",  FR_CONF_OFFSET(PW_TYPE_INTEGER,                   rlm_govlogger_t, max_mmapped_mem_size), "104857600" }, // 100MB default
    { "encode", FR_CONF_POINTER(PW_TYPE_SUBSECTION, NULL), (void const *) fr_json_format_config },
    { "log_prog_command",      FR_CONF_OFFSET(PW_TYPE_STRING,                    rlm_govlogger_t, log_prog_command), NULL },
    { "log_prog_nice",         FR_CONF_OFFSET(PW_TYPE_INTEGER,                   rlm_govlogger_t, log_prog_nice), "0" },
    { "warn_if_rotated_file_present", FR_CONF_OFFSET(PW_TYPE_BOOLEAN,            rlm_govlogger_t, warn_if_rotated_file_present), "no" },
    { "log_prog_keep_stderr",  FR_CONF_OFFSET(PW_TYPE_BOOLEAN,                   rlm_govlogger_t, log_prog_keep_stderr), "no" },
    CONF_PARSER_TERMINATOR
};

/*
 *    A mapping of configuration items per section to variables in the rlm_govlogger_section struct for the config parser
 *
 *    Note that the string is dynamically allocated, so it MUST
 *    be freed.  When the configuration file parse re-reads the string,
 *    it free's the old one, and strdup's the new one, placing the pointer
 *    to the strdup'd string into 'config.string'.  This gets around
 *    buffer over-flows.
 */
static const CONF_PARSER rlm_govlogger_section_config[] = {
    { "govlogger_line",           FR_CONF_OFFSET(PW_TYPE_STRING | PW_TYPE_XLAT | PW_TYPE_REQUIRED, rlm_govlogger_section, log_line), "" },
    { "govlogger_line_reference", FR_CONF_OFFSET(PW_TYPE_STRING | PW_TYPE_XLAT,                    rlm_govlogger_section, log_line_reference), NULL },
    CONF_PARSER_TERMINATOR
};

static bool rotate_logfile(rlm_govlogger_t *instance) {

    if (instance->rotated_log_filename == NULL) {
        return false;
    }

    /* If warn_if_rotated_file_present set check for rotated log file */
    if (instance->warn_if_rotated_file_present) {
        FILE *present = fopen(instance->rotated_log_filename,"r");
        if (present != NULL) {
            ERROR("rlm_govlogger: Rotated file %s exists on rotate", instance->rotated_log_filename);
            fclose(present);
        }
    }

    if (rename(instance->log_filename, instance->rotated_log_filename) != 0) {
        char buffer[256];
        strerror_r(errno, buffer, sizeof(buffer));
        ERROR("rlm_govlogger: Failed to rotate log file by renaming \"%s\" to \"%s\": %s",
            instance->log_filename, instance->rotated_log_filename, buffer);
        return false;
    }

    DEBUG("rlm_govlogger: Rotated log file by renaming \"%s\" to \"%s\"",
        instance->log_filename, instance->rotated_log_filename);
    return true;
}


/*
 * Helper function to run a command in a child process, detaching from the parent so it can continue to run independently,
 * returning true on success, flase on failure.
 * It should really return the child pid, but as two forks are required considerable code to open a named pipe and send the
 * pid back through the named pipe, or shared memory is required.
 * The command is run in a shell to allow for redirections etc, so should be used with care if the command includes any user input.
 * Stdin, Stdout and Stderr are closed in child.
 * This is traditional code to daemonize a process, worth reading W. Richard Stevens
 * "Advanced Programming in the Unix Environment" for more details on this pattern
 */
static bool send_file_to_post_processor(rlm_govlogger_t *instance)
{
    char const *command  = instance->log_prog_command;
    int nice_level = instance->log_prog_nice;
    char buffer[256];

    if (command == NULL) {
        return false;
    }

    // First fork - creating a child process....
    pid_t pid = fork();
    if (pid == 0) {

#ifdef DETATCH_CHILD_PROCESS
        // OK we are a child... now detach from the parent before execve
        // by creating a new program group
        pid = setsid();

        if (pid == (pid_t)-1) {
            strerror_r(errno, buffer, sizeof(buffer));
            DEBUG("rlm_govlogger: Failed to detach child to run shell \"%s\": %s", command, buffer);
            exit(1);
        }

        // Now we fork again and exit in parent
        pid = fork();
        if (pid == 0) {
#endif

            /* Redirect stdin, stdout and stderr to /dev/null so they dont hold open any files or pipes
             * that the parent process has open, and to avoid any output from the child process going anywhere unexpected */
            int devnull = open("/dev/null",O_RDWR, 0);
            if (devnull < 0) {
                strerror_r(errno, buffer, sizeof(buffer));
                DEBUG("Failed opening /dev/null: %s\n", buffer);
                exit(1);
            }

            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);

            /*
             *      If we're not debugging, then we can't do
             *      anything with the error messages, so we throw
             *      them away.
             *
             *      If we are debugging or log_prog_keep_stderr then we want the error
             *      messages to go to the STDERR of the server.
             */
            if (!instance->log_prog_keep_stderr && (rad_debug_lvl == 0)) {
                dup2(devnull, STDERR_FILENO);
            }
            close(devnull);

            /*
             * close all file descriptors, I would rather use close_range but the ruby distro does
             * not have this system call?
             */
            closefrom(3);

            /*
             * Now if we've set the nice level lets change here before we exec
             */
            if (nice_level != 0) {
                errno = 0;
                nice(nice_level);
                if (errno) {
                    strerror_r(errno, buffer, sizeof(buffer));
                    DEBUG("Failed to set nice level to %d: %s\n", nice_level, buffer);
                }
            }

            // Now exec the command in shell (this permits redirections etc...
            execl("/bin/sh", "sh", "-c", command, (char *)NULL);

            // We should not get here unless the execl fails
            strerror_r(errno, buffer, sizeof(buffer));
            DEBUG("rlm_govlogger: Failed to execl to run log processing command \"%s\": %s", command, buffer);
            exit(1);

#ifdef DETATCH_CHILD_PROCESS
        }

        /* Check the grandchild child fork succeeded - as our exit status is collected by parent below
         * and we can still send erros up STDOUT
         */
        if (pid == (pid_t)-1) {
            strerror_r(errno, buffer, sizeof(buffer));
            DEBUG("rlm_govlogger: Failed to fork to run shell \"%s\": %s", command, buffer);
            exit(1);
        }

        /* All good grandchild has detached and we can return success to parent */
        DEBUG("rlm_govlogger: Started child process with pid %d to run log processing command \"%s\"", pid, command);
        exit(0);
#endif
    }

    if (pid == -1) {
        strerror_r(errno, buffer, sizeof(buffer));
        DEBUG("rlm_govlogger: Failed to fork to run log processing command \"%s\": %s", command, buffer);
        return false;
    }
    else {
        DEBUG("rlm_govlogger: Running post processing command \"%s\" pid %d", command, pid);

    }

#ifdef DETATCH_CHILD_PROCESS
    // Collect child - which should exit quickly and successfully
    int wait_status;
    waitpid(pid, &wait_status, 0);
    if (!WIFEXITED(wait_status) || (WEXITSTATUS(wait_status) != 0)) {
        DEBUG("rlm_govlogger: Child process to run log processing command \"%s\" did not exit successfully", command);
        return false;
    }
#else
    instance->log_prog_pid = pid;
#endif

    return true;
}

/**
 *    Helper function to open the log file, and update the rotation check values.
 *    Returns a pointer to the opened log file, or NULL if we failed to open it.
 */
static FILE* open_logfile(rlm_govlogger_t *instance, time_t now) {

    // If we have recently failed to open the log file, don't try again for a while to avoid spamming the logs with errors
    if (instance->last_failed_open_time) {
        if (instance->last_failed_open_time + LOGFILE_OPEN_RETRY_SECONDS > now) {
            return NULL;
        }
    }

    instance->log_file.use_count = 0;

    // Attempt to open the log file append
    instance->log_file.file = fopen(instance->log_filename, "a");
    if (instance->log_file.file == NULL) {
        char buffer[256];

        instance->last_failed_open_time = now;
        strerror_r(errno, buffer, sizeof(buffer));
        ERROR("rlm_govlogger: failed to open log file \"%s\": %s",
            instance->log_filename,
            buffer);
        return NULL;
    }

    DEBUG("rlm_govlogger: opened log file \"%s\" append",
                instance->log_filename);

    instance->last_failed_open_time = 0;
    instance->file_rotate_check_ctime = now + instance->rotate_wait_seconds;

    return instance->log_file.file;
}

/**
 *    Helper function to free a log file, decrementing the use count and closing it if it's the closed file and has no more users.
 */
static void free_logfile(rlm_govlogger_t *instance, FILE *file) {

    if (file == NULL) {
        return;
    }

    PTHREAD_MUTEX_LOCK(&(instance->file_mutex));

    if (instance->log_file.file == file) {
        instance->log_file.use_count--;
    }
    else {
        if (instance->closed_file.file == file) {
            // If it's the closed log file, and it's the last user we can close it
            instance->closed_file.use_count--;
            if (instance->closed_file.use_count <= 0) {
                fclose(instance->closed_file.file);
                DEBUG("rlm_govlogger: closed rotated log file as all threads finished");
                instance->closed_file.file = NULL;
                instance->closed_file.use_count = 0;
                send_file_to_post_processor(instance);
            }
        }
        else {
            ERROR("rlm_govlogger: Attempted to free a log file that is not the current or closed file");
        }
    }

    PTHREAD_MUTEX_UNLOCK(&(instance->file_mutex));
}


/**
 *    Helper function to get the log file, opening it if necessary, and checking for rotation if it has been a while since we last checked.
 *    Returns a pointer to the log file, or NULL if we don't have one.
 *    The returned file is thread safe to write to, but the caller should not attempt to close it.
 *    The file is opened append mode, so writes will be atomic and not interleaved with other processes writing to the same file.
 */
static FILE* get_logfile(rlm_govlogger_t *instance)
{
    time_t now = time(NULL);
    FILE *result;

    // We need to lock the mutex to ensure that only one thread is attempting to open or check the log file at a time, otherwise we could have multiple threads attempting to open the file at the same time, or one thread checking the file while another is in the middle of opening it, which could lead to multiple files being opened or
    PTHREAD_MUTEX_LOCK(&(instance->file_mutex));

    result = instance->log_file.file;

    // If we have no log file, attempt to open it
    if (result == NULL) {
        result = open_logfile(instance, now);
        if (result == NULL) {
            // Failed to open log file, return NULL
        	PTHREAD_MUTEX_UNLOCK(&(instance->file_mutex));
            return NULL;
        }
    }

    /* Do we need to check for log rotation?
     * We only check for rotation if it's been a while since we last checked,
     * to avoid the overhead of checking the file every time we get it. */
    time_t seconds_to_next_check = instance->file_rotate_check_ctime - now;
    if (seconds_to_next_check <= 0) {
        /*
         * Collect the exit status of the last log_prog_command run
         */
        if (instance->log_prog_pid != 0) {
            int wait_status;
            // Check if the log processing program is still running, if not we can clear the pid so it will be restarted on next check
            int wait_result = waitpid(instance->log_prog_pid, &wait_status, WNOHANG);
            if (wait_result == instance->log_prog_pid) {
                int exit_status = WEXITSTATUS(wait_status);
                if (exit_status) {
                    ERROR("rlm_govlogger: log processing program with pid %d has finished exit_status %d", instance->log_prog_pid, exit_status);
                }
                else {
                    DEBUG("rlm_govlogger: log processing program with pid %d has finished exit_status %d", instance->log_prog_pid, exit_status);
                }
                instance->log_prog_pid = 0;
            }
            else {
                if (wait_result == -1) {
                    char error_message[256];

                    strerror_r(errno, error_message, sizeof(error_message));
                    ERROR("rlm_govlogger: waitpid(%d) returned -1, only assume somebody else has collected exit status: %s", instance->log_prog_pid, error_message);
                    instance->log_prog_pid = 0;
                }
            }
        }

        /*
         * If there is not already a closed file, and the log_proceesor is NOT running we can rotate the file if needed, otherwise we need to wait until the next check to rotate the file as we don't want to have multiple rotated files building up if the log processor is slow to run or there is an issue with it.
         */
        if (instance->log_prog_pid != 0) {
           /* We update file_rotate_check_ctime to look again in couple of seconds to allow post processor to finish */
           DEBUG("rlm_govlogger: log processing program is still running with pid %d, skipping log rotation", instance->log_prog_pid);
           instance->file_rotate_check_ctime = now + 2;
        }
        else {
            if (instance->closed_file.file == NULL) {
                // Update the check time to check again in a while, but don't update the inode as we know it has been rotated
                instance->file_rotate_check_ctime = now + instance->rotate_wait_seconds;

                /* Move the current file to the closed file,
                 * so that any threads still using it can continue to use it,
                 * but we know it's been rotated and won't try to check it for rotation again until all users are gone. */
                DEBUG("rlm_govlogger: no threads using rotated file %s",
                        instance->rotated_log_filename);

                /*
                 * We rotate the file by renaming and closing. If other threads have a reference we wait until
                 * they have finished before we actually close the file, but we can rename it immediately so that
                 * any new threads will open a new file when they attempt to get the log file.
                 * This also means that if the log processing command is slow to run, we won't have multiple
                 * rotated files building up as we will not rotate again until the previous file has been processed and closed.
                 */
                rotate_logfile(instance);

                /*
                 * If somebody is already using this file we need to keep a copy until
                 * all users have free'd it.
                 */
                if (instance->log_file.use_count > 0) {
                    instance->closed_file = instance->log_file;
                    DEBUG("rlm_govlogger: log file \"%s\" was in use by %d threads(s) - keeping reference until finished",
                            instance->log_filename, instance->log_file.use_count);
                }
                else {
                    fclose(instance->log_file.file);
                    send_file_to_post_processor(instance);
                }

                result = open_logfile(instance, now);
            }
            else {
                /* We should try again quite soon to see if all threads stopped using rotated file */
                instance->file_rotate_check_ctime = now + 2;
                DEBUG("rlm_govlogger: Cannot reopen logfile \"%s\" as there are still %d threads(s) using the previous handle",
                    instance->log_filename, instance->closed_file.use_count);
            }
        }
    }
    else {
        // Worth a quick check time has not jumped....
        // if time changed the check may never happen
        if (seconds_to_next_check > instance->rotate_wait_seconds) {
            DEBUG("rlm_govlogger: system time appears to have jumped forward, resetting log rotation check time");
            instance->file_rotate_check_ctime = now + instance->rotate_wait_seconds;
        }
    }

    if (result != NULL) {
        // Bump up the use count for the file we are returning
        instance->log_file.use_count++;
    }

    PTHREAD_MUTEX_UNLOCK(&(instance->file_mutex));

    /* Check if we need to run the log process command */
    return result;
}

/**
 * Helper function to log a line for a given section, using the log_line or log_line_reference to determine what to log.
 */
static rlm_rcode_t CC_HINT(nonnull) log_generic(rlm_govlogger_t *instance, rlm_govlogger_section *section_config, REQUEST *request) {
    char const *log_line = section_config->log_line;
    char line[4096];

    line[0] = '\0';

    /* If we don't have a section config - skip */
    if (!section_config->name) return RLM_MODULE_NOOP;

    if (section_config->log_line_reference) {
        CONF_ITEM *ci;
        CONF_PAIR *cp;

        if (radius_xlat(line + 1, sizeof(line) - 1, request, section_config->log_line_reference, NULL, NULL) < 0) {
            return RLM_MODULE_FAIL;
        }

        line[0] = '.';    /* force to be in current section */

        /*
         *    Don't allow it to go back up
         */
        if (line[1] == '.') goto do_log;

        ci = cf_reference_item(NULL, section_config->cs, line);
        if (!ci) {
            RWDEBUG2("No such entry \"%s\"", line);
            return RLM_MODULE_NOOP;
        }

        if (!cf_item_is_pair(ci)) {
            RWDEBUG2("Entry \"%s\" is not a variable assignment ", line);
            goto do_log;
        }

        cp = cf_item_to_pair(ci);
        log_line = cf_pair_value(cp);
        if (!log_line) {
            RWDEBUG2("Entry \"%s\" has no value", line);
            return RLM_MODULE_OK;
        }

        /*
         *    Ensure we have a log line to log.  If the log_line is empty, we consider that to mean we should not log anything, and return OK.
         */
        if (!*log_line) return RLM_MODULE_OK;
    }

    do_log:
    /*
     *    FIXME: Check length.
     */
    DEBUG("%s: Expanding xlats in log line \"%s\"", section_config->log_prefix, log_line);
    if (radius_xlat(line, sizeof(line) - 1, request, log_line, NULL, NULL) < 0) {
        ERROR("%s: Failed to expand xlats in \"%s\"", section_config->log_prefix, log_line);
        return RLM_MODULE_FAIL;
    }

    DEBUG("Expanded log line length: %d", (int)strlen(line));
    FILE *logfile = get_logfile(instance);
    if (logfile == NULL) {
        return RLM_MODULE_NOOP;
    }

    fwrite(line, strlen(line), 1 , logfile);
    free_logfile(instance, logfile);
    return RLM_MODULE_NOOP;
}

static rlm_rcode_t CC_HINT(nonnull) mod_authorize(void *instance, REQUEST *request) {
    rlm_govlogger_t *inst = (rlm_govlogger_t *) instance;
    return log_generic(inst, &inst->authorize, request);
}

static rlm_rcode_t CC_HINT(nonnull) mod_authenticate(void *instance, REQUEST *request) {
    rlm_govlogger_t *inst = (rlm_govlogger_t *) instance;
    return log_generic(inst, &inst->authenticate, request);
}

static rlm_rcode_t CC_HINT(nonnull) mod_preact(void *instance, REQUEST *request) {
    rlm_govlogger_t *inst = (rlm_govlogger_t *) instance;
    return log_generic(inst, &inst->preacct, request);
}

static rlm_rcode_t CC_HINT(nonnull) mod_accounting(void *instance, REQUEST *request) {
    rlm_govlogger_t *inst = (rlm_govlogger_t *) instance;
    return log_generic(inst, &inst->accounting, request);
}

static rlm_rcode_t CC_HINT(nonnull) mod_pre_proxy(void *instance, REQUEST *request) {
    rlm_govlogger_t *inst = (rlm_govlogger_t *) instance;
    return log_generic(inst, &inst->pre_proxy, request);
}

static rlm_rcode_t CC_HINT(nonnull) mod_post_proxy(void *instance, REQUEST *request) {
    rlm_govlogger_t *inst = (rlm_govlogger_t *) instance;
    return log_generic(inst, &inst->post_proxy, request);
}

static rlm_rcode_t CC_HINT(nonnull) mod_post_auth(void *instance, REQUEST *request) {
    rlm_govlogger_t *inst = (rlm_govlogger_t *) instance;
    return log_generic(inst, &inst->post_auth, request);
}

static int parse_sub_section(CONF_SECTION *parent, rlm_govlogger_section *config, char const *name)
{
    CONF_SECTION *cs;

    cs = cf_section_sub_find(parent, name);
    if (!cs) {
        config->name = NULL;
        DEBUG("rlm_govlogger: No config section named %s, skipping", name);
        return 0;
    }

    if (cf_section_parse(cs, config, rlm_govlogger_section_config) < 0) {
        DEBUG("rlm_govlogger: Failed to parse config section named %s, skipping", name);
        config->name = NULL;
        return -1;
    }

    /*
     *  Add section name (Maybe add to headers later?).
     */
    config->name = name;

    /*
     *    Generate log prefix in the format "rlm_govlogger (SectionName)" or "rlm_govlogger (ParentSectionName.SectionName)"
     */
    snprintf(config->log_prefix, sizeof(config->log_prefix), "rlm_govlogger(%s):", name);


    /* Add link to parent config for reference in log_line_reference */
    config->parent = parent;
    config->cs = cs;

    DEBUG("%s Parsed config for section %s, log_line: %s", config->log_prefix, name, config->log_line);

    return 0;
}

/*
 * govlogger xlat function
 *
 * Currently only govlogger:max_sessions and govlogger:used_sessions are supported.
 */
static ssize_t govlogger_xlat(void *instance, UNUSED REQUEST *request, char const *query, char *out, size_t freespace)
{
	rlm_govlogger_t *inst = instance;

	if (strcmp(query, "max_sessions") == 0) {
		snprintf(out, freespace, "%d", inst->rlm_module_instance->max_sessions);
		return 0;
	}

	if (strcmp(query, "used_sessions") == 0) {
		/*
		 *      Playing with a data structure shared among threads
		 *      means that we need a lock, to avoid conflict.
		 */
		PTHREAD_MUTEX_LOCK(&(inst->rlm_module_instance->session_mutex));
		snprintf(out, freespace, "%d",
				rbtree_num_elements(inst->rlm_module_instance->session_tree));
		PTHREAD_MUTEX_UNLOCK(&(inst->rlm_module_instance->session_mutex));
		return 0;
	}
	return -1;
}

static inline int is_key_name_char(char c) {
    return isalpha((int)c) || c == '-';
}
/** Convert given attributes to a JSON document
 *
 * Usage is `%{json_encode:attr tmpl list}`
 *
 * @ingroup xlat_functions
 *
 * @param instance module instance
 * @param request the current request
 * @param fmt input to the xlat
 * @param out where to write the output
 * @param outlen space available for the output
 * @return length of output generated
 */
static ssize_t json_encode_xlat(void * instance, REQUEST *request, char const *fmt,
                      char *out, size_t outlen)
{
    rlm_govlogger_t const *inst = instance;
    ssize_t         slen;
    VALUE_PAIR      *json_vps = NULL, *vps;
    char const      *p = fmt;
    char            *json_str = NULL;

    /*
     * Iterate through the list of attribute templates in the xlat. For each
     * one we either add it to the list of attributes for the JSON document
     * or, if prefixed with '!', remove from the JSON list.
     */
    MYDEBUG("xlat_json:%s", fmt);

    // Strip leading whitespace, and check we have something to process
    while (isspace((uint8_t) *p)) p++;
    if (*p == '\0') return -1;

    while (*p) {

        if (*p == '!') {
            // Make end is initially at start of attribute - but skip ! char
            char const *end = ++p;


            /* if nothing after ! then its an error */
            if (*p == '\0') {
                /* May happen e.g. with '!' on its own at the end */
                REMARKER(fmt, (p - fmt), "Missing attribute name");
                fr_pair_list_free(&json_vps);
                return -1;
            }

            // if first char is not valid then its an error
            if (!is_key_name_char(*end)) {
                REMARKER(fmt, (p - fmt), "Invalid attribute name");
                fr_pair_list_free(&json_vps);
                return -1;
            }

            /* Scan to end of attribute - allowing a-z, A-Z, 0-9 and - */
            while (is_key_name_char(*end) || isdigit((int)*end)) end++;
            slen = end - p;

            /* If no vps we can just skip removal */
            if (json_vps) {
                /*
                 * Now loop through attributes finding match and remove from JSON list.
                 */
                MYDEBUG("Looking for attribute %.*s to remove from JSON list", (int)slen, p);
                for (VALUE_PAIR *vp = json_vps; vp; vp = vp->next) {

                    if ((ssize_t)strlen(vp->da->name) != slen)
                        continue;

                    if (strncmp(vp->da->name, p, slen) == 0) {
                        MYDEBUG("Removing attribute %s from JSON list", vp->da->name);
                        fr_pair_delete_by_da(&json_vps, vp->da);
                        break;
                    }
                }
            }
        }
        else {
            vp_tmpl_t *vpt = NULL;
            /* Decode next attr template */
            slen = tmpl_afrom_attr_substr(request, &vpt, p, REQUEST_CURRENT, PAIR_LIST_REQUEST, false, false);

            if (slen <= 0) {
                REMARKER(fmt, (p - fmt) -slen, fr_strerror());
                TALLOC_FREE(vpt);
                fr_pair_list_free(&json_vps);
                return -1;
            }

            /*
             * Get attributes from the template.
             * Missing attribute isn't an error (so -1, not 0).
             */
            if (tmpl_copy_vps(request, &vps, request, vpt) < -1) {
                REDEBUG("Error copying attributes");
                TALLOC_FREE(vpt);
                fr_pair_list_free(&json_vps);
                return -1;
            }

            /* Add template VPs to JSON list */
            fr_pair_add(&json_vps, vps);
            TALLOC_FREE(vpt);
        }

        /* Jump forward to next attr */
        p += slen;

        // Should have only whitespace or end of string
        if (*p != '\0' && !isspace((uint8_t)*p)) {
            REMARKER(fmt, (p - fmt), "Missing whitespace");
            fr_pair_list_free(&json_vps);
            return -1;
        }

        // Skip whitespace before next attribute template
        while (isspace((uint8_t) *p)) p++;
        MYDEBUG("Remaining to process: \"%s\"", p);
    }

    /*
     * Given the list of attributes we now have in json_vps,
     * convert them into a JSON document and append it to the
     * return cursor.
     */
    if (!json_vps) {
        DEBUG("No attributes to include in JSON, returning empty object");
        strncpy(out, "{}", outlen);
        return 2;
    }

    /* JSON string of a list of value pairs */
    json_str = fr_json_afrom_pair_list(request, json_vps, inst);
    if (!json_str) {
        REDEBUG("Failed to generate JSON string");
        fr_pair_list_free(&json_vps);
        return -1;
    }

    slen = snprintf(out, outlen, "%s", json_str);
    DEBUG("xlat_endoded_json: generated JSON string of length %d", (int)slen);

    TALLOC_FREE(json_str);
    fr_pair_list_free(&json_vps);

    return slen;
}

/*
 * Callback function type for when we find a memory section,
 * the callback will be called with the located memory address and a data pointer provided by the caller,
 * and should return 0 on success or non-zero on failure.
 * If 0 is returned searching continues, else find_memory_sections_by_name() wil return -1.
 */
typedef int (*memory_section_found_callback)(void *located_memory, void *data_pointer);

/* Given the name of the section (surrounded by spaces calls the callback with (void *address, callback_data_pointer)
   for each located section.
   Returns the number of located sections on success, but if the callback returns non zero for any section
   returns -1.
 */
static int find_memory_sections_by_name(FILE *fp, const char *section, memory_section_found_callback callback_function, void *callback_data_pointer) {
    char line[1024];
    int match_count = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *found = strstr(line, section);
        if (found == NULL)
            continue;

        found = strstr(found, " 0x");
        if (!found)
            continue;

        void *pointer;
        found += 3;
        int matched =  sscanf(found, "%p", &pointer);

        if (matched != 1)
            continue;

        DEBUG("rlm_govlogger: Found %s",line);
        match_count++;
        if (callback_function(pointer, callback_data_pointer)) {
            ERROR("rlm_govlogger: searching for section \"%s\" in talloc dump returned a callback function error",
                    section);
            return -1;
        }
    }
    return match_count;
}

/*
 * Callback function to store the found section address in a pointer provided via callback_data_pointer,
 * returns 0 on success, or non-zero on failure
 */
static int store_found_section(void *address, void* callback_data_pointer) {
    void **target = (void **)callback_data_pointer;
    *target = address;
    return 0;
}

static int mod_bootstrap(CONF_SECTION *conf, void *instance)
{
    rlm_govlogger_t *inst = talloc_get_type_abort(instance, rlm_govlogger_t);
    char                 *name;

    inst->name = cf_section_name2(conf);
    if (!inst->name) inst->name = cf_section_name1(conf);



    /*
     *      Register the govlogger xlat function
     */
    if (xlat_register(inst->name, govlogger_xlat, NULL, inst)) {
        cf_log_err_cs(conf, "Failed to register xlat function name %s", inst->name);
        return -1;
    }
    DEBUG("rlm_govlogger: registered xlat function %s", inst->name);
    /*
     * Register the govlogger_json xlat
     */

    name = talloc_asprintf(inst, "%s_json", inst->name);
    if (xlat_register(name, json_encode_xlat, NULL, inst)) {
        cf_log_err_cs(conf, "Failed to register json xlat function for %s", name);
        return -1;
    }
    DEBUG("rlm_govlogger: registered xlat function %s", name);
    talloc_free(name);

    /*
     *  Check the output format type and warn on unused
     *  format options
     */
    inst->output_mode = fr_str2int(fr_json_format_table, inst->output_mode_str, JSON_MODE_UNSET);
    if (inst->output_mode == JSON_MODE_UNSET) {
        cf_log_err_cs(conf, "output_mode value \"%s\" is invalid", inst->output_mode_str);
        return -1;
    }
    fr_json_format_verify(inst, true);

    return 0;
}

/*
 *    Do any per-module initialization that is separate to each
 *    configured instance of the module.  e.g. set up connections
 *    to external databases, read configuration files, set up
 *    dictionary entries, etc.
 */
static int mod_instantiate(CONF_SECTION *conf, void *instance) {
    rlm_govlogger_t *inst = instance;

    // Create a mutex to ensure the file update is thread safe
    // We will use a recursive mutex to count the number of users using the file descriptors
    {
        pthread_mutexattr_t Attr;

        pthread_mutexattr_init(&Attr);
        pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&inst->file_mutex, &Attr);
        pthread_mutexattr_destroy(&Attr);
    }

    inst->last_failed_open_time = 0;
    inst->file_rotate_check_ctime = 0;

    inst->log_file.file = NULL;
    inst->log_file.use_count = 0;

    inst->closed_file.file = NULL;
    inst->closed_file.use_count = 0;

    inst->log_prog_pid = 0;

    // If we have a log program we need the interval to be set
    if (inst->log_prog_command != NULL) {
       if (inst->rotated_log_filename == NULL) {
            cf_log_err_cs(conf, "rlm_govlogger: rotated_log_filename must be set if log_prog_command is set");
            return -1;
        }
        if (inst->rotate_wait_seconds < 1) {
            cf_log_err_cs(conf, "rlm_govlogger: rotate_wait_seconds must be at least 1 if log_prog_command is set");
            return -1;
        }
    }

    if (inst->log_prog_keep_stderr) {
        DEBUG("rlm_govlogger: log_prog_keep_stderr=true stderr from log_prog will go to stderr");
    }
    else {
        DEBUG("rlm_govlogger: stderr from log_prog will go to /dev/null");
    }
    if (inst->warn_if_rotated_file_present) {
        DEBUG("rlm_govlogger: warn_if_rotated_file_present=true warning will be produced if rotated logfile present when rotating next");
    }

    if (!inst->log_filename) {
        cf_log_err_cs(conf, "log_filename missing from configuration");
        return -1;
    } else {
        FILE *logfile;

        DEBUG("rlm_govlogger: log_filename: %s", inst->log_filename);
        DEBUG("rlm_govlogger: log file will be checked for rotation every %u seconds", inst->rotate_wait_seconds);
        logfile = get_logfile(inst);
        free_logfile(inst, logfile);
    }

    if (inst->rotate_wait_seconds < 1) {
        cf_log_err_cs(conf, "rlm_govlogger: rotate_wait_seconds must be at least 1");
        return -1;
    }

    /*
     *    Parse sub-section configs.
     */
    if (
        (parse_sub_section(conf, &inst->authorize, section_type_value[MOD_AUTHORIZE].section) < 0) ||
        (parse_sub_section(conf, &inst->authenticate, section_type_value[MOD_AUTHENTICATE].section) < 0) ||
        (parse_sub_section(conf, &inst->preacct, section_type_value[MOD_PREACCT].section) < 0) ||
        (parse_sub_section(conf, &inst->accounting, section_type_value[MOD_ACCOUNTING].section) < 0) ||
        (parse_sub_section(conf, &inst->pre_proxy, section_type_value[MOD_PRE_PROXY].section) < 0) ||
        (parse_sub_section(conf, &inst->post_proxy, section_type_value[MOD_POST_PROXY].section) < 0) ||
        (parse_sub_section(conf, &inst->post_auth, section_type_value[MOD_POST_AUTH].section) < 0)) {
        return -1;
    }

    module_instance_t *mod_inst = module_find(conf, "eap");
    if (mod_inst == NULL) {
        cf_log_err_cs(conf, "rlm_govlogger: Failed to find config section for eap - is the module enabled and configured?");
        return -1;
    }
    DEBUG("rlm_govlogger: Found eap module_instance_t %s", mod_inst->name);

    FILE *fp;
    char *memory_for_mapping = NULL;
    if (inst->talloc_debug_filename != NULL) {
        fp = fopen(inst->talloc_debug_filename, "w+");
        if (fp == NULL) {
            cf_log_err_cs(conf, "rlm_govlogger: Failed to open talloc debug file %s for writing", inst->talloc_debug_filename);
            return -1;
        }
    } else {
        // If we don't have a filename to dump the talloc structure to,
        // use a memory mapped file
        memory_for_mapping = talloc_array(NULL, char, inst->max_mmapped_mem_size);
        if (memory_for_mapping == NULL) {
            cf_log_err_cs(conf, "rlm_govlogger: Failed to allocate memory for talloc_report_full memory mapped file, size: %d bytes", inst->max_mmapped_mem_size);
            return -1;
        }

        fp = fmemopen(memory_for_mapping, inst->max_mmapped_mem_size, "w+");
        if (fp == NULL) {
            cf_log_err_cs(conf, "rlm_govlogger: Failed to open memory map file w+");
            return -1;
        }
    }

    // Save the talloc debug to file
    talloc_report_depth_file(mod_inst, 0, 10, fp);
    fflush(fp);

    // Seek to start of file
    fseek(fp, 0, SEEK_SET);

    // Search file for the location of the rlm_eap_t structure
    void *pointer = NULL;
    int match_count = find_memory_sections_by_name(fp, " rlm_eap_t ", store_found_section, &pointer);
    if (match_count == -1) {
		cf_log_err_cs(conf, "rlm_govlogger: Failed to find section \" rlm_eap_t \" in talloc_report_full");
		return -1;
	}
    if (match_count == 0) {
		cf_log_err_cs(conf, "rlm_govlogger: Failed to find section \" rlm_eap_t \" in talloc_report_full");
		return -1;
	}
    if (match_count > 1) {
    	cf_log_err_cs(conf, "rlm_govlogger: Found multiple sections named \" rlm_eap_t \" in talloc_report_full, expected only one");
    }

    // Now close the file and free the memory if we used a memory mapped file
    fclose(fp);
    if (memory_for_mapping != NULL) {
        talloc_free(memory_for_mapping);
    }

    inst->rlm_module_instance = (rlm_eap_t *)pointer;
    DEBUG("rlm_govlogger: eap max_sessions: %u\n",inst->rlm_module_instance->max_sessions);

    return 0;
}

/*
 *    Only free memory we allocated.  The strings allocated via
 *    cf_section_parse() do not need to be freed.
 */
static int mod_detach(void *instance) {
    rlm_govlogger_t *inst = instance;

    DEBUG("mod_detach()");

    /* free things here */
    FILE *log_file = inst->log_file.file;
    if (log_file != NULL) {
        DEBUG("Closing govlogger");
        fprintf(log_file, "GovLogger closing, use_count: %d\n",
                inst->log_file.use_count);
        fclose(log_file);
    }

    log_file = inst->closed_file.file;;
    if (log_file != NULL) {
        DEBUG("Closing govlogger rotated file");
        fprintf(log_file, "GovLogger closing rotated file, use_count: %d\n",
                inst->closed_file.use_count);
        fclose(log_file);
    }

    // Also we may have a post processing command running - we need to collect this
    if (inst->log_prog_pid != 0) {
        int wait_status;
        // Check if the log processing program is still running, if not we can clear the pid so it will be restarted on next check
        int wait_result = waitpid(inst->log_prog_pid, &wait_status, 0);
        DEBUG("Waiting for post processing command to complete");
        if (wait_result == inst->log_prog_pid) {
            DEBUG("rlm_govlogger: log processing program with pid %d has finished exit_status %d", inst->log_prog_pid, WEXITSTATUS(wait_status));
            inst->log_prog_pid = 0;
        }
        else if (wait_result == -1) {
            char error_message[256];

            strerror_r(errno, error_message, sizeof(error_message));
            DEBUG("rlm_govlogger: Failed to check status of log processing program with pid %d: %s", inst->log_prog_pid, error_message);
            inst->log_prog_pid = 0;
        }
    }

    pthread_mutex_destroy(&inst->file_mutex);
    return 0;
}

/*
 *    The module name should be the only globally exported symbol.
 *    That is, everything else should be 'static'.
 *
 *    If the module needs to temporarily modify it's instantiation
 *    data, the type should be changed to RLM_TYPE_THREAD_UNSAFE.
 *    The server will then take care of ensuring that the module
 *    is single-threaded.
 */
extern module_t rlm_govlogger;
module_t rlm_govlogger = {
    .magic         = RLM_MODULE_INIT,
    .name          = "govlogger",
    .type          = RLM_TYPE_THREAD_SAFE,
    .inst_size     = sizeof(rlm_govlogger_t),
    .config        = module_config,
    .bootstrap     = mod_bootstrap,
    .instantiate   = mod_instantiate,
    .detach        = mod_detach,
    .methods = {
    		[MOD_AUTHENTICATE] = mod_authenticate,
            [MOD_AUTHORIZE]    = mod_authorize,
            [MOD_PREACCT]      = mod_preact,
            [MOD_ACCOUNTING]   = mod_accounting,
			[MOD_POST_AUTH]    = mod_post_auth,
            [MOD_PRE_PROXY]    = mod_pre_proxy,
            [MOD_POST_PROXY]   = mod_post_proxy,
    },
};
