# rlm_govlogger
## Metadata
<dl>
  <dt>category</dt><dd>Logging Framework</dd>
</dl>

## Summary

Module to log to a file optimised for performance, providing access to hidden EAP module session usage, an improved JSON serialisation and ability to run an
external log processing script.

This module is compatible with FreeRadius versions 3.2.10.

# Configuration

## Global settings

The parameters below have the default in brackets, if the default is M the parameter is mandatory.

- **encode** Configuration section describing the govlogger_json xlat configuration.
- **max_mmapped_mem_size** (104857600) If **talloc_debug_filename** has **not** been specified this is the memory temporarly allocated for the talloc debug dump file. The default value should be sufficient.
- **log_file** (M) This is the filename govlooger writes formatted log data to. If the file fails to open no logging is performed and file open errors are reported every 30 seconds to STDOUT.
- **log_prog_command** (undefined) An optional command (run inside /bin/bash) which can be run periodically to process the rotated logfile. The program is executed once a rotated log file has no threads writing and is closed. The command is run using /bin/sh so shell redirections and pipes can be used where approriate. As log events may span several lines the post processing script should maintain state.
- **log_prog_keep_stderr** (no) If true (yes) stderr output from the **log_prog_command** returns to the current stderr stream, else it directed to /dev/null.
- **log_prog_nice** The nice level to run the **log_prog_coomand**, by increasing the nice level it lowers the impact on the freeradius server when the **log_prog_command** is executed. Recommended value 10.
- **rotated_log_file** (M) The name of the rotated log file.
- **rotate_interval** (20) The time in seconds between successive log rotations. This time may be longer if the system is busy.
- **talloc_debug_filename** The module dumps the talloc debug information to a memory mapped file then parses the file to find the location of the eap module configuration to access the session count information. If you wish to see the talloc memory data you can specify a file to use.
- **warn_if_rotated_file_present** (no) If true (yes) when rotating the current log file if the rotated log file exists a radius error is produced. If true it is important the log processing command deletes the rotated log file on success.

### The **encode** json xlat configuration

The **govlogger_json** xlat has been optimised for speed, and is based on the rlm_json module with a few improvements. The encode options are the same:

- **output_mode** (object) set the format of JSON documents, examples of each format are given below. This may be one of:
    - **object**:
		```
		{
		    "User-Name": {
		        "type": "string",
		        "value": "bob"
		    },
		    "Filter-Id": {
		        "type": "string",
		        "value": ["ab","cd"]
		    }
		} 
		```

    - **object_simple**:
        ```
        {
            "User-Name": "bob",
            "Filter-Id": ["ab","cd"]
        }
        ```
    - **array**:
        ```
        [
            {
                "name": "User-Name",
                "type": "string",
                "value": "bob"
            },
            {
                "name": "Filter-Id",
                "type": "string",
                "value": "ab"
            },
            {
                "name": "Filter-Id",
                "type": "string",
                "value": "cd"
            }
        ]
        ```
        or when "single_value_as_array" option is set
        ```
        [
            {
                "name": "User-Name",
                "type": "string",
                "value": ["bob"]
            },
            {
                "name": "Filter-Id",
                "type": "string",
                "value": ["ab","cd"]
            }
       ]
       ```
    - **array_of_names**
		```
		[
		  "User-Name",
		  "Filter-Id",
		  "Filter-Id"
		]
		```
	- **array_of_values**
		```
		[
		  "bob",
		  "ab",
		  "cd"
		]
		```
- **prefix** Add a colon-delimited prefix to all attribute names in the output document. For example, with a prefix of "foo", User-Name will be output as foo:User-Name.
- **single_value_as_array** always put values in an array. Output formats will by default put single values as a JSON object (string, integer, etc). More than one value will, depending on the output format, be added as an array. When this option is enabled, values will always be added as an array.
- **enum_as_integer** output the integer value of enumerated attributes. Where an attribute has enum values, the textual representation of the value will normally be output. Enable this option to force the numeric value instead.
- **always_string** force all values to be strings. Integer values are normally written to the JSON document as numbers (i.e. without quotes). Enable this option to force all values to be as quoted strings.
- **binary_format** how to encode octets (binary) values. Controls the representation of binary data in JSON output. - "raw" (default): write bytes directly as a JSON string - "base16": base16(hex)-encode the binary data - "base64": base64-encode the binary data



## Per section settings

These parameters can be specified in each of the freeradius stages configurations authenticate, authorize, preact, accounting, post-auth, pre-proxy and post-proxy.

The config items are:

* **govlogger_line** The format of the logged line. An example is:
    ```
    post-auth {
        govlogger_line = "%c.%M post-auth: { \"State\": \"%{State}\", \"Request\": %{govlogger_json:request:[*] !EAP-Message !Message-Authenticator !Service-Type !MS-CHAP-Challenge !MS-CHAP2-Response !TLS-Cert-Valid-Since !TLS-Cert-Common-Name !TLS-Cert-Issuer !TLS-Cert-Subject !TLS-Client-Cert-Common-Name !TLS-Client-Cert-Issuer !TLS-Client-Cert-Subject !TLS-Client-Cert-Valid-Since !TLS-Client-Cert-X509v3-Authority-Key-Identifier !TLS-Client-Cert-X509v3-Basic-Constraints !TLS-Client-Cert-X509v3-Subject-Key-Identifier }, \"Session\": %{govlogger_json:session-state:[*] !TLS-Session-Information !TLS-Session-Version !Framed-MTU},\"eap_sessions\":%{govlogger:used_sessions}}\n"
    }
    ```
    In the above "the govlogger_json" xlat is used to produce json, the "!key" are entries that are removed from the JSON map. The "eap_sessions" is added to allow monitoring of the eap session usage.
      
* **govlogger_line_reference** can be used to chose a suitable log format from a map, the reference is first expanded to find the log format before the xlats are expanded: 

	```
	accounting {
	    govlogger_line = "..."
	    govlogger_line_reference = "Accounting-Request.%{&Acct-Status-Type || 'unknown'}"
	    Accounting-Request {
	        Start = "Connect: [%{User-Name}] (did %{Called-Station-Id} cli %{Calling-Station-Id} port %{NAS-Port} ip %{Framed-IP-Address})"
	        Stop = "Disconnect: [%{User-Name}] (did %{Called-Station-Id} cli %{Calling-Station-Id} port %{NAS-Port} ip %{Framed-IP-Address}) %{Acct-Session-Time} seconds"
	        Interim-Update = ""
	        Accounting-On = "NAS %{Net.Src.IP} (%{&NAS-IP-Address || &NAS-IPv6-Address}) just came online"
	        Accounting-Off = "NAS %{Net.Src.IP} (%{&NAS-IP-Address || &NAS-IPv6-Address}) just went offline"
	        unknown = "NAS %{Net.Src.IP} (%{&NAS-IP-Address || &NAS-IPv6-Address}) sent unknown Acct-Status-Type %{Acct-Status-Type}"
	    }
	}
	```

Here the locally declared Accounting-Request is used to select an alternate **log_line**. If expansion of **govlogger_line_reference** does not yield anything logging falls back to the default **govlogger_line**.

# xlats

The following xlats are added:

## **govlogger:used_sessions**

Expands to the number of EAP sessions currently in use.


## **govlogger:max_sessions**

Expands to the maximum sessions configured in the EAP module.

## **govlogger_json** 

The json serialiser, with improvements from the rlm_json module.

# SUGGESTED CONFIGURATION

The following configuration is suggested in the GovWiFi FreeRadius frontend:

```
govlogger {
	#
	#  The only options for the JSON module are to control the output
	#  format of the `json_encode` xlat.
	#
	encode {
		#
		#  output_mode:: set the format of JSON documenta
		#  that should be created. This may be one of:
		#
		#  - object
		#  - object_simple
		#  - array
		#  - array_of_names
		#  - array_of_values
		#
		#  Examples of each format are given below.
		#
		output_mode = object_simple

		#
		#  ### Formatting options for attribute names
		#
		attribute {
			#
			#  prefix:: Add a colon-delimited prefix to all
			#  attribute names in the output document. For example,
			#  with a prefix of "foo", `User-Name` will be output as
			#  `foo:User-Name`.
			#
#			prefix =
		}

		#
		#  ### Formatting options for attribute values
		#
		value {

			#
			#  single_value_as_array:: always put values in an array
			#
			#  Output formats will by default put single values as a
			#  JSON object (string, integer, etc). More than one
			#  value will, depending on the output format, be added
			#  as an array.
			#
			#  When this option is enabled, values will always be
			#  added as an array.
			#
#			single_value_as_array = no

			#
			#  enum_as_integer:: output the integer value of
			#  enumerated attributes
			#
			#  Where an attribute has enum values, the textual
			#  representation of the value will normally be output.
			#  Enable this option to force the numeric value
			#  instead.
			#
#			enum_as_integer = no

			#
			#  dates_as_integer:: output dates as seconds since the
			#  epoch
			#
			#  Where an attribute is a date, the textual
			#  representation of the date will normally be output.
			#  Enable this option to force the date to be rendered
			#  as seconds since the epoch instead.
			#
#			dates_as_integer = no

			#
			#  always_string:: force all values to be strings
			#
			#  Integer values are normally written to the JSON
			#  document as numbers (i.e. without quotes). Enable
			#  this option to force all values to be as quoted
			#  strings.
			#
#			always_string = no
		}

	}

    log_file = "govlogs.log"
    rotated_log_file = "govlogs.rotated"

    # How often we run the log process script
    log_prog_interval_seconds = 10

    # The script for handling the logs
    # The --pretty --canonical are in here to make it look nice but it increases data size - leave out for production
    log_prog_command = "/usr/bin/process_gov_logs --pretty --canonical --state /healthcheck/statefile --reduce_duplicates --reduce_freeradius_proxied --reduce_last_date_only --reduce_drop_eap_peap --delete --wait 6 --logfile ${rotated_log_file} >> /healthcheck/reduced_logs.out"

    # By lowering the nice level we do not affect freeradius operation
    log_prog_nice = 10

    # Warn if the file still exists on next rotate (can be caused by crash in above script)
    warn_if_rotated_file_present = yes

    # Send stderr to output - then appears in docker log
    log_prog_keep_stderr = yes

    post-auth {
        govlogger_line = "%c.%M post-auth: {\"State\":\"%{State}\",\"Request\":%{govlogger_json:request:[*] !EAP-Message !Message-Authenticator !Service-Type !MS-CHAP-Challenge !MS-CHAP2-Response !TLS-Cert-Valid-Since !TLS-Cert-Common-Name !TLS-Cert-Issuer !TLS-Cert-Subject !TLS-Client-Cert-Common-Name !TLS-Client-Cert-Issuer !TLS-Client-Cert-Subject !TLS-Client-Cert-Valid-Since !TLS-Client-Cert-X509v3-Authority-Key-Identifier !TLS-Client-Cert-X509v3-Basic-Constraints !TLS-Client-Cert-X509v3-Subject-Key-Identifier},\"Session\":%{govlogger_json:session-state:[*] !TLS-Session-Information !TLS-Session-Version !Framed-MTU},\"eap_sessions\":%{govlogger:used_sessions}}\n"
    }

    authorize {
        govlogger_line = "%c.%M authorize: {\"State\":\"%{State}\",\"Request\":%{govlogger_json:request:[*] !EAP-Message !Message-Authenticator !Service-Type !MS-CHAP-Challenge !MS-CHAP2-Response !TLS-Cert-Valid-Since !TLS-Cert-Common-Name !TLS-Cert-Issuer !TLS-Cert-Subject !TLS-Client-Cert-Common-Name !TLS-Client-Cert-Issuer !TLS-Client-Cert-Subject !TLS-Client-Cert-Valid-Since !TLS-Client-Cert-X509v3-Authority-Key-Identifier !TLS-Client-Cert-X509v3-Basic-Constraints !TLS-Client-Cert-X509v3-Subject-Key-Identifier},\"Session\":%{govlogger_json:session-state:[*] !TLS-Session-Information !TLS-Session-Version !Framed-MTU},\"eap_sessions\":%{govlogger:used_sessions}}\n"
    }

    authenticate {
        govlogger_line = "%c.%M authenticate: {\"State\":\"%{State}\",\"Request\": %{govlogger_json:request:[*] !EAP-Message !Message-Authenticator !Service-Type !MS-CHAP-Challenge !MS-CHAP2-Response !TLS-Cert-Valid-Since !TLS-Cert-Common-Name !TLS-Cert-Issuer !TLS-Cert-Subject !TLS-Client-Cert-Common-Name !TLS-Client-Cert-Issuer !TLS-Client-Cert-Subject !TLS-Client-Cert-Valid-Since !TLS-Client-Cert-X509v3-Authority-Key-Identifier !TLS-Client-Cert-X509v3-Basic-Constraints !TLS-Client-Cert-X509v3-Subject-Key-Identifier},\"Session\":%{govlogger_json:session-state:[*] !TLS-Session-Information !TLS-Session-Version !Framed-MTU},\"eap_sessions\":%{govlogger:used_sessions}}\n"
    }
}
```

# Building
Due the the rather nasty use of boilermake, and outdated configure.ac files in FreeRadius 3.2.x some changes are needed to the `src/tests/all.mk` file to ensure the unit tests for the logger are run.

To build the module, download the source for FreeRadius 3.2.x and then unpack this code over the top.

To ensure the module is built it should be appended to the list of stable modules in src/modules/stable (see below)

```
git clone -b v3.2.x https://github.com/FreeRADIUS/freeradius-server.git
git clone https://github.com/GovWifi/govwifi-radius-custom-module.git
cd freeradius-server
(cd ../govwifi-radius-custom-module && tar cf - src) | tar xvf -
sed -E -i 's/^(SUBMAKEFILES .*)/\1 govlogger\/all.mk/' src/tests/all.mk
echo rlm_govlogger >> src/modules/stable
./configure CPPFLAGS=-DX509_V_FLAG_PARTIAL_CHAIN=1 --sysconfdir=/etc --without-ruby
make
make test
make tests.govlogger
```

