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
	REQ_SIZE,         // nginx: $request_length         apache: %i
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
		unsigned long timestamp;
} Log_line_parsed_t;

typedef struct log_parser_config{
    int num_fields;             /**< Number of fields in the parsed log format. */
    log_line_field_t *fields;   /**< Array of parsed log format of num_fields length. */
    const char delimiter;       /**< Delimiter that separates the fields in the log format. */
} Log_parser_config_t;

void search_keyword(char *src, char *dest, const char *keyword, const int ignore_case);

#endif  // PARSER_H_
