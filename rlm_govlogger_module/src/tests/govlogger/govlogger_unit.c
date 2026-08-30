/*
 * radattr.c	Map debugging tool.
 *
 * Version:	$Id: 24f1a291b1bc194a03acc1be7cd782786c6ef1e0 $
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Copyright 2015  Alan DeKok <aland@freeradius.org>
 */

RCSID("$Id: 24f1a291b1bc194a03acc1be7cd782786c6ef1e0 $")

#include <../../../modules/rlm_govlogger/rlm_govlogger.c>
#include <freeradius-devel/libradius.h>

#include <freeradius-devel/conf.h>
#include <freeradius-devel/modpriv.h>
#include <freeradius-devel/modcall.h>

#include <ctype.h>
#include <sys/stat.h>

#ifdef HAVE_GETOPT_H
#	include <getopt.h>
#endif

#include <assert.h>

#include <freeradius-devel/log.h>

#include <sys/wait.h>

#define UNUSED                  CC_HINT(unused)

/* Linker hacks */

char const *radlog_dir = NULL;
bool log_stripped_names;
char const *radacct_dir = NULL;
int error_count = 0;

FILE *outputfile;

#ifdef HAVE_PTHREAD_H
pid_t rad_fork(void) {
    return fork();
}

pid_t rad_waitpid(pid_t pid, int *status) {
    return waitpid(pid, status, 0);
}
#endif

RADCLIENT* client_find(UNUSED RADCLIENT_LIST const *clients, UNUSED fr_ipaddr_t const *ipaddr,
    UNUSED int proto) {
    return NULL;
}
RADCLIENT* client_find_old(UNUSED fr_ipaddr_t const *ipaddr) {
    return NULL;
}

RADCLIENT_LIST* client_list_parse_section(UNUSED CONF_SECTION *section, UNUSED bool tls_required) {
    return NULL;
}

int realms_init(UNUSED CONF_SECTION *config) {
    return 0;
}

void client_list_free(UNUSED RADCLIENT_LIST *clients) {
}

void realms_free(void) {
}

void listen_free(UNUSED rad_listen_t **head) {
}

/* Linker hacks */

#define ERROR_MESSAGE(format, ...)  do {                                                  \
        fprintf(outputfile, "ERROR: (%s() line %d) " format, __FUNCTION__ , __LINE__ ,  ## __VA_ARGS__ );\
        error_count++;                                                                    \
    } while(0)

#define GOOD_MESSAGE(format, ...)  do {                                                  \
        fprintf(outputfile, "SUCCESS: (%s() line %d) " format, __FUNCTION__ , __LINE__ ,  ## __VA_ARGS__ );\
    } while(0)

void init_govlogger(rlm_govlogger_t *inst);
void init_govlogger(rlm_govlogger_t *inst) {
    memset(inst, 0, sizeof(rlm_govlogger_t));

    // Create a mutex to ensure the file update is thread safe
    // We will use a recursive mutex to count the number of users using the file descriptors
    {
        pthread_mutexattr_t Attr;

        pthread_mutexattr_init(&Attr);
        pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&inst->file_mutex, &Attr);
        pthread_mutexattr_destroy(&Attr);
    }
}

void open_logfile_test(const char *log_filename);
void open_logfile_test(const char *log_filename) {
    rlm_govlogger_t config;
    time_t now = time(NULL);
    FILE *logfile;

    DEBUG("open_logfile_test()");

    init_govlogger(&config);
    config.log_filename = log_filename;

    logfile = open_logfile(&config, now);
    if (logfile == NULL) {
        ERROR_MESSAGE("failed to open test log file %s\n", log_filename);
    }
    else {
        GOOD_MESSAGE("successfully opened log file\n");
    }

    /* Has rotate log time been set?? */
    if (config.file_rotate_check_ctime != now + config.rotate_wait_seconds) {
        ERROR_MESSAGE("file_rotate_check_ctime not set correctly, expected %ld but got %ld\n",
            now + config.rotate_wait_seconds, config.file_rotate_check_ctime);
    }
    else {
        GOOD_MESSAGE("file_rotate_check_ctime set correctly to %ld\n",
            config.file_rotate_check_ctime);
    }

    /* Check the last failed open time is not set */
    if (config.last_failed_open_time != 0) {
        ERROR_MESSAGE("last_failed_open_time should be 0 but got %ld\n",
            config.last_failed_open_time);
    }
    else {
        GOOD_MESSAGE("last_failed_open_time correctly set to 0\n");
    }

    /* Check use count is 0 */
    if (config.log_file.use_count != 0) {
        ERROR_MESSAGE("log_file use_count should be 0 but got %d\n", config.log_file.use_count);
    }
    else {
        GOOD_MESSAGE("log_file use_count correctly set to 0\n");
    }

    fclose(logfile);
    unlink(log_filename);

    /* Now we set the last failed open time to now, and check that
     * we don't attempt to open the file again if we call open_logfile again before the retry time has passed */
    config.last_failed_open_time = now;

    logfile = open_logfile(&config, now);
    if (logfile != NULL) {
        ERROR_MESSAGE("should not have opened log file due to recent failed open, but it did\n");
    }
    else {
        GOOD_MESSAGE("correctly did not open log file due to recent failed open\n");
    }

    /* check that if we call open_logfile again after the retry time has passed, we do attempt to open the file again */
    now = time(NULL);
    config.last_failed_open_time = now + 2 - LOGFILE_OPEN_RETRY_SECONDS;
    logfile = open_logfile(&config, now);
    if (logfile != NULL) {
        ERROR_MESSAGE("should not have opened log file due to recent failed open, but it did\n");
    }
    else {
        GOOD_MESSAGE("correctly did not open log file due to recent failed open\n");
    }

    /* now after 3 seconds we should be able to open the file again */
    sleep(3);
    now = time(NULL);
    logfile = open_logfile(&config, now);
    if (logfile == NULL) {
        ERROR_MESSAGE("failed to open test log file after LOGFILE_OPEN_RETRY_SECONDS\n");
        return;
    }
    else {
        GOOD_MESSAGE("successfully opened log file after LOGFILE_OPEN_RETRY_SECONDS\n");
    }

    /* Has rotate log time been set?? */
    if (config.file_rotate_check_ctime != now + config.rotate_wait_seconds) {
        ERROR_MESSAGE("file_rotate_check_ctime not set correctly, expected %ld but got %ld\n",
            now + config.rotate_wait_seconds, config.file_rotate_check_ctime);
    }
    else {
        GOOD_MESSAGE("file_rotate_check_ctime set correctly to %ld\n",
            config.file_rotate_check_ctime);
    }

    /* Check the last failed open time is not set */
    if (config.last_failed_open_time != 0) {
        ERROR_MESSAGE("last_failed_open_time should be 0 but got %ld\n",
            config.last_failed_open_time);
    }
    else {
        GOOD_MESSAGE("last_failed_open_time correctly set to 0\n");
    }

    /* Check use count is 0 */
    if (config.log_file.use_count != 0) {
        ERROR_MESSAGE("log_file use_count should be 0 but got %d\n", config.log_file.use_count);
    }
    else {
        GOOD_MESSAGE("log_file use_count correctly set to 0\n");
    }

    rlm_govlogger.detach(&config);
    unlink(log_filename);
}

void get_free_logfile_test(const char *log_filename);
void get_free_logfile_test(const char *log_filename) {
    rlm_govlogger_t instance;
    char rotated_filename[256];
    char log_prog_command[560];
    char buffer[512];

    DEBUG("get_free_logfile_test()");

    init_govlogger(&instance);

    snprintf(rotated_filename, sizeof(rotated_filename), "%s.rotated", log_filename);
    snprintf(log_prog_command, sizeof(log_prog_command), "cat %s >> %s.total && rm %s", rotated_filename, log_filename, rotated_filename);

    instance.rotate_wait_seconds = 100;
    instance.log_filename = log_filename;
    instance.rotated_log_filename = rotated_filename;

    instance.log_prog_command = log_prog_command;
    instance.log_prog_nice = 10;

    /* Quick tidy to ensure the test logfile is not present */
    unlink(instance.log_filename);
    unlink(instance.rotated_log_filename);

    /* Initial open should return a filehandle */
    FILE *logfile = get_logfile(&instance);
    if (logfile == NULL) {
        ERROR_MESSAGE("failed to get log file on initial open\n");
    }
    else {
        GOOD_MESSAGE("successfully got log file on initial call\n");
    }

    if (instance.file_rotate_check_ctime == 0) {
        ERROR_MESSAGE(
            "file_rotate_check_ctime should be set on initial get_logfile call, but was not\n");
    }
    else {
        GOOD_MESSAGE("file_rotate_check_ctime correctly set on initial get_logfile call\n");
    }

    if (instance.log_file.use_count != 1) {
        ERROR_MESSAGE("use count should be 1 after initial open, but got %d\n",
            instance.log_file.use_count);
    }
    else {
        GOOD_MESSAGE("use count correctly set to 1 after initial open\n");
    }

    /* Second call should return same logfile and increment use count */
    FILE *logfile2 = get_logfile(&instance);
    if (logfile2 == NULL) {
        ERROR_MESSAGE("failed to get log file on second call\n");
    }
    else {
        GOOD_MESSAGE("successfully got log file on second call\n");
    }

    if (instance.log_file.use_count != 2) {
        ERROR_MESSAGE("use count should be 2 after seconds call, but got %d\n",
            instance.log_file.use_count);
    }
    else {
        GOOD_MESSAGE("use count correctly set to 2 after second call\n");
    }

    if (logfile != logfile2) {
        ERROR_MESSAGE(
            "should have returned the same file handle on second call, but got different handles\n");
    }
    else {
        GOOD_MESSAGE("successfully returned the same file handle on second call\n");
    }

    /* Now free_logfile should decrement the use count, but not close the file as it's still in use */
    free_logfile(&instance, logfile2);
    if (instance.log_file.use_count != 1) {
        ERROR_MESSAGE("use count should be 1 after freeing one handle, but was %d\n",
            instance.log_file.use_count);
    }
    else {
        GOOD_MESSAGE("correctly decremented use count to 1 after freeing first handle\n");
    }

    /* Check we can write to the first handle (i.e. it hasnt been closed */
    if (fprintf(logfile, "Test log entry\n") < 0) {
        ERROR_MESSAGE(
            "logfile should still be open and writable after freeing second handle, but fprintf failed\n");
    }
    else {
        GOOD_MESSAGE(
            "logfile is still open and writable after freeing second handle as expected\n");
    }

    /* Now free_logfile should decrement the use count to 0 */
    free_logfile(&instance, logfile);
    if (instance.log_file.use_count != 0) {
        ERROR_MESSAGE("use count should be 0 after freeing second handle, but was %d\n",
            instance.log_file.use_count);
    }
    else {
        GOOD_MESSAGE("correctly decremented use count to 0 after freeing second handle\n");
    }

    /* Now lets force the file to rotate - so we get a different file handle */
    DEBUG("Test Forcing rotation");
    instance.file_rotate_check_ctime = 0;
    logfile2 = get_logfile(&instance);

    /* The handle should be different, but the target file should be the same */
    if (logfile2 == NULL) {
        ERROR_MESSAGE("failed to get log file after rotation time passed\n");
    }
    else {
        GOOD_MESSAGE("successfully got log file after rotation time passed\n");
    }

    /* There was no need to keep old handle open as we had no users */
    if (instance.closed_file.file != NULL) {
        ERROR_MESSAGE(
            "closed_file should be NULL as there were no users of file when last instance freed");
    }
    else {
        GOOD_MESSAGE("did not set closed_file when last instance freed before rotation\n");
    }

    /* Use count should be 1 */
    if (instance.log_file.use_count != 1) {
        ERROR_MESSAGE("Use count following rotation should be 1\n");
    }
    else {
        GOOD_MESSAGE("Use count was 1 as expected\n");
    }

    /* Check the file was rotated, and has the log line in it */
    FILE *check = fopen(instance.rotated_log_filename, "r");
    if (check == NULL) {
        ERROR_MESSAGE("No rotated file %s\n", instance.rotated_log_filename);
    }
    else {
        GOOD_MESSAGE("File was rotated as expected\n");
    }

    /* Is the first line TEst log entry */
    if (fgets(buffer, sizeof(buffer), check) == NULL) {
        ERROR_MESSAGE("Failed to read line from rotated logfile\n");
    }
    else {
        GOOD_MESSAGE("Read line from rotated log file\n");
    }
    if (strcmp(buffer, "Test log entry\n") != 0) {
        ERROR_MESSAGE("First line of rotated log file was not \"Test log entry\"\n");
    }
    else {
        GOOD_MESSAGE("First line of rotated log file matched expected\n");
    }
    if (fgets(buffer, sizeof(buffer), check) != NULL) {
        ERROR_MESSAGE("Rotated log file had more than one line\n");
    }
    else {
        GOOD_MESSAGE("Rotated log file contained only single line\n");
    }
    fclose(check);

    /* Check we can write to the new handle */
    if (fprintf(logfile2, "Test log entry after rotation\n") < 1) {
        ERROR_MESSAGE("logfile2 should be writable after rotation, but fprintf failed\n");
    }
    else {
        GOOD_MESSAGE("logfile2 is open and writable after rotation as expected\n");
    }

    /* try to open it again */
    DEBUG("Getting again should not rotate");
    logfile = get_logfile(&instance);
    if (logfile == NULL) {
        ERROR_MESSAGE("failed to get second log file after rotation\n");
    }
    else {
        GOOD_MESSAGE("successfully got second log file after rotation\n");
    }
    DEBUG("... and it didnt");

    /* Check use count correctly incremented */
    if (instance.log_file.use_count != 2) {
        ERROR_MESSAGE(
            "use count should be 2 after getting second handle post rotation, but got %d\n",
            instance.log_file.use_count);
    }
    else {
        GOOD_MESSAGE("correctly set use count to 2 after getting second handle post rotation\n");
    }

    /*
     * Wait for child to exit
     */
    sleep(2);

    DEBUG("Forcing rotation once again, but with user of file...");
    /* Now we try the rotation logic again, but this time we have a user of the file,
     * so it should move the old file to closed_file, and keep it open until the user has finished with it */
    instance.file_rotate_check_ctime -= instance.rotate_wait_seconds + 1;
    free_logfile(&instance, logfile);
    logfile = get_logfile(&instance);
    DEBUG("...done");

    if (logfile == NULL) {
        ERROR_MESSAGE("get_free_logfile_test() failed on rotation with file in use by another thread\n");
    }

    DEBUG("Should have rotated but without close and process");

    /* Now the handles should differ as logfile2 is now rotated file */
    if (logfile == logfile2) {
        ERROR_MESSAGE("should have returned a different file handle after rotation with file\n");
    }
    else {
        GOOD_MESSAGE("correctly returned a different file handle after rotation with file\n");
    }

    /* We should have a rotated instance */
    if (instance.closed_file.file == NULL) {
        ERROR_MESSAGE(
            "closed_file should not be NULL as we have reopened with old file still in use\n");
    }
    else {
        GOOD_MESSAGE("correctly set closed_file when rotating with file still in use\n");
    }

    /* Check use counts are both 1 as we have two files in use once */
    if (instance.log_file.use_count != 1) {
        ERROR_MESSAGE(
            "current log_file use count should be 1 after rotation with file still in use, but got %d\n",
            instance.log_file.use_count);
    }
    else {
        GOOD_MESSAGE(
            "correctly set current log_file use count to 1 after rotation with file still in use\n");
    }
    if (instance.closed_file.use_count != 1) {
        ERROR_MESSAGE(
            "closed log_file use count should be 1 after rotation with file still in use, but got %d\n",
            instance.closed_file.use_count);
    }
    else {
        GOOD_MESSAGE(
            "correctly set closed_file log_file use count to 1 after rotation with file still in use\n");
    }

    /* Now check freeing handles correctly closes rotated file */
    DEBUG("Now freeing logfiles, first current, second rotated");
    free_logfile(&instance, logfile);
    free_logfile(&instance, logfile2);

    logfile = get_logfile(&instance);
    free_logfile(&instance, logfile);
    DEBUG("Should have forced post processing");

    if (instance.closed_file.file != NULL) {
        ERROR_MESSAGE("closed_file should be NULL after freeing all handles, but was not\n");
    }
    else {
        GOOD_MESSAGE("correctly set closed_file to NULL after freeing all handles\n");
    }

    /* And the main file use count should be 0 */
    if (instance.log_file.use_count != 0) {
        ERROR_MESSAGE("use count not 0 after freeing all handles, but got %d\n",
            instance.log_file.use_count);
    }
    else {
        GOOD_MESSAGE("correctly set use count to 0 after freeing all handles\n");
    }

    mod_detach(&instance);
}

void post_process_command_test(const char *log_filename);
void post_process_command_test(const char *log_filename) {
    rlm_govlogger_t instance;
    char rotated_filename[256];
    char log_prog_command[270];

    DEBUG("post_process_command_test()");

    init_govlogger(&instance);

    // Create the rotated filename and log_prog_command
    snprintf(rotated_filename, sizeof(rotated_filename), "%s.rotated", log_filename);
    snprintf(log_prog_command, sizeof(log_prog_command), "sleep 5 && rm %s", rotated_filename);

    instance.last_failed_open_time = 0;
    instance.file_rotate_check_ctime = 0;

    instance.log_file.file = NULL;
    instance.log_file.use_count = 0;

    instance.closed_file.file = NULL;
    instance.closed_file.use_count = 0;

    instance.rotate_wait_seconds = 2;

    instance.log_filename = log_filename;
    instance.rotated_log_filename = rotated_filename;
    instance.log_prog_command = log_prog_command;

    /* Quick tidy to ensure the test logfile is not present */
    unlink(instance.log_filename);
    unlink("banana.rotated");

    /* Initial open should return a filehandle */
    FILE *logfile = get_logfile(&instance);
    if (logfile == NULL) {
        ERROR_MESSAGE("failed to get log file on initial open\n");
    }

    FILE *logfile2 = get_logfile(&instance);
    if (logfile2 == NULL) {
        ERROR_MESSAGE("failed to get log file on second open\n");
    }

    if (logfile != logfile2) {
        ERROR_MESSAGE(
            "should have returned the same file handle on initial call, but got different handles\n");
    }
    else {
        GOOD_MESSAGE("successfully returned the same file handle on initial calls\n");
    }

    if (instance.log_file.use_count != 2) {
        ERROR_MESSAGE("use count should be 2 after twp opens but got %d\n",
            instance.log_file.use_count);
    }
    else {
        GOOD_MESSAGE("use count correctly set to 2 after initial opens\n");
    }

    mod_detach(&instance);
}

#ifdef DETATCH_CHILD_PROCESS
void test_shell_execute(void);
void test_shell_execute() {
    // Remove any spurious test output file from previous test runs
    unlink("/tmp/govlogger_test_shell_execute.txt");

    if (!run_child_command("echo test >> /tmp/govlogger_test_shell_execute.txt",0)) {
        ERROR_MESSAGE("run_child_command() failed to run first shell command\n");
        return;
    }

    if (!run_child_command("sleep 1 && echo finished >> /tmp/govlogger_test_shell_execute.txt",10)) {
        ERROR_MESSAGE("run_child_command() failed to run second shell command\n");
        return;
    }

    sleep(2);

    FILE *file = fopen("/tmp/govlogger_test_shell_execute.txt", "r");
    if (file == NULL) {
        ERROR_MESSAGE("Failed to open output file from shell command\n");
        return;
    }

    fprintf(outputfile, "Successfully opened output file from shell command\n");

    char *buffer = NULL;
    size_t read_len;
    if (getline(&buffer, &read_len, file) < 1) {
        ERROR_MESSAGE("Failed to read output from shell command\n");
        fclose(file);
        return;
    }

    fprintf(outputfile, "Read output from shell command: %s", buffer);

    if (strcmp(buffer, "test\n") != 0) {
        ERROR_MESSAGE("Unexpected output from shell command: %s\n", buffer);
        fclose(file);
        return;
    }

    free(buffer);
    buffer = NULL;

    if (getline(&buffer,  &read_len, file) < 1) {
        ERROR_MESSAGE("Failed to read output from shell command\n");
        fclose(file);
        return;
    }

    fprintf(outputfile, "Read output from shell command: %s", buffer);

    if (strcmp(buffer, "finished\n") != 0) {
        ERROR_MESSAGE("Unexpected output from shell command: %s\n", buffer);
        fclose(file);
        return;
    }

    free(buffer);
    buffer = NULL;

    // Now next read should return -1
    if (getline(&buffer,  &read_len, file) != -1) {
        ERROR_MESSAGE("Expected end of file from shell command output, but got: %s\n", buffer);
        fclose(file);
        return;
    }
    buffer = NULL;

    fprintf(outputfile, "Successfully reached end of file from shell command output\n");
    unlink("/tmp/govlogger_test_shell_execute.txt");

    // Final check is to ensure we have NO children
    pid_t pid = getpid();

    char line[256];
    snprintf(line, sizeof(line), "/proc/%d/task/%d/children", pid, pid);

    FILE *children = fopen(line,"r");
    if (!children) {
        ERROR_MESSAGE("Failed to open %s to check for child processes\n", line);
        return;
    }

    if (getline(&buffer,  &read_len, file) != -1) {
        ERROR_MESSAGE("Did not expect any child processes, but got: %s\n", buffer);
        fclose(children);
        return;
    }

    fclose(children);
    fprintf(outputfile, "Successfully verified no child processes remain after running shell commands\n");

    mod_detatch(instance);
}
#endif

int main(int argc, char *argv[]) {
    char *logfilename = NULL;
    char *test_output_file = NULL;

    outputfile = stdout;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-')
            break;

        switch (argv[i][1]) {
        case 't':
            if (i + 1 >= argc) {
                ERROR_MESSAGE("Missing argument for -t\n");
                exit(1);
            }
            test_output_file = argv[i + 1];
            outputfile = fopen(test_output_file, "w");
            if (!outputfile) {
                ERROR_MESSAGE("Failed to open test output file %s\n", test_output_file);
                exit(1);
            }
            i++;
            break;

        case 'l':
            if (i + 1 >= argc) {
                ERROR_MESSAGE("Missing argument for -l\n");
                exit(1);
            }
            logfilename = argv[i + 1];
            i++;
            break;

        case 'h':
            fprintf(outputfile, "Usage: %s [-l logfilename] [-t test_outfile]\n", argv[0]);
            exit(0);

        default:
            ERROR_MESSAGE("Unknown argument: %s\n", argv[i]);
            exit(1);
        }
    }

    rad_debug_lvl = 3;
    if (logfilename == NULL) {
        ERROR_MESSAGE("Log filename must be provided with -l\n");
        exit(1);
    }

    open_logfile_test(logfilename);
    get_free_logfile_test(logfilename);
#ifdef DETATCH_CHILD_PROCESS
    test_shell_execute();
#endif
    post_process_command_test(logfilename);

    if (error_count > 0) {
        ERROR("Tests completed with %d error(s)\n", error_count);
        exit(1);
    }

    fprintf(outputfile, "All tests completed successfully\n");
    exit(0);
}
