
#ifdef GOV_LOGGER_CONF_PARSE
#else
#define GOV_LOGGER_CONF_PARSE

#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/modules.h>
#include <freeradius-devel/rad_assert.h>
#include <freeradius-devel/modpriv.h>
#include <freeradius-devel/modcall.h>
#include <freeradius-devel/parser.h>
#include <rlm_eap.h>
#include "../rlm_json/config.h"

/*
 *    This is the per stage section config, log_line is the line to log, log_line_reference can be used
 *    to reference a structure (see rlm_linelog)
 */
typedef struct {
    CONF_SECTION *cs;
    CONF_SECTION *parent;            //!< Pointer to parent section for reference in log_line_reference
    char const *name;                //!< Section name. i.e. Authenticate
    char const *log_line;            // The line to log - which can have XLAT expantions
    char const *log_line_reference;  // A log line which can contain references to other sections, which will be resolved before logging. This allows you to have a common log line in one section, and then reference it from multiple sections without having to duplicate the log line.
    char log_prefix[128];            // A prefix to add to log messages from this section, this is automatically generated from the section name and parent section name (if it exists) in the format "rlm_govlogger (SectionName)" or "rlm_govlogger (ParentSectionName.SectionName)"
} rlm_govlogger_section;

/**
 * We want to keep track of the log file and how many users are currently using it,
 * so that when we rotate the log file we can keep the old one open until all users have finished with it,
 * and then close it.
 */
typedef struct {
    FILE *file;
    unsigned int use_count;
} file_with_use_count;

/** List of possible JSON format output modes.
 *
 */
typedef enum {
	JSON_MODE_UNSET = 0,
	JSON_MODE_OBJECT,
	JSON_MODE_OBJECT_SIMPLE,
	JSON_MODE_ARRAY,
	JSON_MODE_ARRAY_OF_VALUES,
	JSON_MODE_ARRAY_OF_NAMES
} json_mode_type_t;

/*
 *    This is the govlogger configuration.
 *    It holds the logfile, rotated logfile and a mutex to manage it's access.
 *    The only configurable items are:
 *      log_filename - The filename to use for logging
 *      rotate_wait_seconds - How long to wait after rotating the logfile to ensure nothing is writing to it
 */
typedef struct {
    CONF_SECTION *cs;
    char const *name;
    char const *log_filename;         // Name of the logfile
    char const *rotated_log_filename; // Name to rotate the log file to
    unsigned int rotate_wait_seconds; // How often we will check for file rotation
    char const *log_prog_command;     // The command to run to process the log file
    pid_t log_prog_pid;               // The pid of the currently running log processing program, if any
    unsigned int log_prog_nice;       // The nice level to run the log processing program at, default 0 (same as parent)
    bool warn_if_rotated_file_present;// If on file rotation the rotated file is present produce warning
    bool log_prog_keep_stderr;        // Keep stderr stream open for log rotate program

    file_with_use_count log_file;     // Current log file
    file_with_use_count closed_file;  // Previous log file, which may still have users if the
                                      // file has been rotated but we haven't reopened it yet

    time_t file_rotate_check_ctime;   // We will not check for file rotation on each log event, but every rotate_wait_seconds,
                                      // this is the time we need to recheck
    time_t last_failed_open_time;     // If we fail to open the file we don't want to continuously report the error - this allows us to retry later
    pthread_mutex_t file_mutex;       // A thread mutex to protect shared data related to the log file

    char const *talloc_debug_filename;// A filename to dump the talloc structure to for debugging purposes,
                                      // this is used to find the location of the rlm_eap_t structure for the eap module
                                      // a memory mapped file is used if NOT specified

    unsigned int max_mmapped_mem_size;// The maximum size of the memory map file used to find the rlm_eap_t
                                      // structure for the eap module, in bytes, default 100MB
                                      // Note: this is NOT used if talloc_debug_filename is specified

    /*
     * This is data for the EAP module
     */
    rlm_eap_t *rlm_module_instance;

    rlm_govlogger_section    authorize;    //!< Configuration specific to authorisation.
    rlm_govlogger_section    authenticate; //!< Configuration specific to authentication.
    rlm_govlogger_section    preacct;      //!< Configuration specific to preacct.
    rlm_govlogger_section    accounting;   //!< Configuration specific to accounting.
    rlm_govlogger_section    post_auth;    //!< Configuration specific to Post-auth
    rlm_govlogger_section    pre_proxy;    //!< Configuration specific to pre_proxy
    rlm_govlogger_section    post_proxy;   //!< Configuration specific to post_proxy

    /*
     * This is for the gov_json xlat
     */
    char const       *attr_prefix;    //!< Prefix to add to all attribute names
    bool             value_as_array;  //!< Use JSON array for multiple attribute values.
    bool             enum_as_int;	  //!< Output enums as value, not their string representation.
    bool             dates_as_int;	  //!< Output dates as epoch seconds, not their string representation.
    bool             always_string;	  //!< Output all data types as strings.

    char const       *output_mode_str;//!< For CONF_PARSER only.
    json_mode_type_t output_mode;	  //!< Determine the format of JSON document to generate.

    bool             include_type;	  //!< Include attribute type where possible.
} rlm_govlogger_t;


#endif
