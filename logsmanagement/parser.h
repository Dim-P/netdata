/** @file parser.h
 *  @brief Header of parser.c 
 *
 *  @author Dimitris Pantazis
 */

#ifndef PARSER_H_
#define PARSER_H_

/* Following max lengths include NUL terminating char */
#define REQ_SCHEME_MAX_LEN 6
#define REQ_METHOD_MAX_LEN 18
#define REQ_PROTO_MAX_LEN 4 
#define SSL_PROTO_MAX_LEN 8 
#define SSL_CIPHER_SUITE_MAX_LEN 256 // TODO: Check max len for ssl cipher suite string is indeed 256
#define REGEX_SIZE 100U /**< Max size of regular expression (used in keyword search) in bytes **/
#define LOG_PARSER_BUFFS_LINE_REALLOC_SCALE_FACTOR 1.5

typedef enum{
	VHOST, 		      // nginx: $host ($http_host)      apache: %v
	PORT,             // nginx: $server_port            apache: %p
	VHOST_WITH_PORT,  // nginx: $host:$server_port      apache: %v:%p
	REQ_SCHEME,       // nginx: $scheme                 apache: -
	REQ_CLIENT,       // nginx: $remote_addr            apache: %a (%h)
	REQ,			  // nginx: $request                apache: %r
	REQ_METHOD,       // nginx: $request_method         apache: %m
	REQ_URL,          // nginx: $request_uri            apache: %U
	REQ_PROTO,        // nginx: $server_protocol        apache: %H
	REQ_SIZE,         // nginx: $request_length         apache: %I
	REQ_PROC_TIME,    // nginx: $request_time           apache: %D  
	RESP_CODE,        // nginx: $status                 apache: %s, %>s
	RESP_SIZE,        // nginx: $bytes_sent, $body_bytes_sent apache: %b, %O, %B // Should separate %b from %O ?
	UPS_RESP_TIME,    // nginx: $upstream_response_time apache: -
	SSL_PROTO,        // nginx: $ssl_protocol           apache: -
	SSL_CIPHER_SUITE, // nginx: $ssl_cipher             apache: -
	TIME,             // nginx: $time_local             apache: %t
	CUSTOM
} log_line_field_t;

typedef struct log_line_parsed{
		//char vhost[255];
		//char port[6];
		//char req_scheme[6];
		//char req_client[46]; // https://superuser.com/questions/381022/how-many-characters-can-an-ip-address-be#comment2219013_381029
		//char req_method[20];
		char *vhost;
		int  port;
		char req_scheme[REQ_SCHEME_MAX_LEN];
		char *req_client;
		char req_method[REQ_METHOD_MAX_LEN];
		char *req_URL;
		char req_proto[REQ_PROTO_MAX_LEN];
		int req_size;
		int req_proc_time;
		int resp_code;
		int resp_size;
		int ups_resp_time;
		char ssl_proto[SSL_PROTO_MAX_LEN];
		char ssl_cipher_suite[SSL_CIPHER_SUITE_MAX_LEN];
		uint64_t timestamp;
} Log_line_parsed_t;

typedef struct log_parser_buffs{
	Log_line_parsed_t log_line_parsed;
	char *line;
	size_t line_len_max;
}Log_parser_buffs_t;

typedef struct log_parser_config{
    log_line_field_t *fields;   
    int num_fields;             /**< Number of strings in the fields array. */
    char delimiter;       /**< Delimiter that separates the fields in the log format. */
} Log_parser_config_t;

// IMPORTANT! Ensure no pointer are contained inside log_parser_metrics as there are shallow copy operations used on this struct.
typedef struct log_parser_metrics{
    unsigned long long num_lines_total; /**< Number of total lines parsed in log source file. */
    unsigned long long num_lines_rate;  /**< Number of new lines parsed. */
    struct log_parser_metrics_req_method{
		int acl, baseline_control, bind, checkin, checkout, connect, copy, delet, get,
		head, label, link, lock, merge, mkactivity, mkcalendar, mkcol, mkredirectref,
		mkworkspace, move, options, orderpatch, patch, post, pri, propfind, proppatch,
		put, rebind, report, search, trace, unbind, uncheckout, unlink, unlock, update,
		updateredirectref;
	} req_method;  
	struct log_parser_metrics_req_proto{
		int http_1, http_1_1, http_2, other;
	} req_proto;
	struct log_parser_metrics_bandwidth{
		long long int req_size, resp_size;
	} bandwidth;
	struct log_parser_metrics_resp_code_family{
		int resp_1xx, resp_2xx, resp_3xx, resp_4xx, resp_5xx, other; // TODO: Can there be "other"?
	} resp_code_family; 
	unsigned int resp_code[501]; /**< Array counting occurences of response codes. Each item represents the respective response code by adding 100 to its index, e.g. resp_code[102] counts how many 202 codes were detected. 501st item represents "other" */  
	struct log_parser_metrics_resp_code_type{ /* Note: 304 and 401 should be treated as resp_success */
		int resp_success, resp_redirect, resp_bad, resp_error, other; // TODO: Can there be "other"?
	} resp_code_type;
	struct log_parser_metrics_ssl_proto{
		int tlsv1, tlsv1_1, tlsv1_2, tlsv1_3, sslv2, sslv3, other;
	} ssl_proto;
} Log_parser_metrics_t;

void search_keyword(char *src, char *dest, const char *keyword, const int ignore_case);
Log_parser_config_t *read_parse_config(char *log_format, const char delimiter);
Log_parser_metrics_t parse_text_buf(Log_parser_buffs_t *parser_buffs, char *text, size_t text_size, log_line_field_t *fields, int num_fields, const char delimiter, const int verify);

#endif  // PARSER_H_
