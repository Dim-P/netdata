/** @file parser.c
 *  @brief API to parse and search logs
 *
 *  @author Dimitris Pantazis
 */

//#if !defined(_XOPEN_SOURCE) && !defined(__DARWIN__) && !defined(__APPLE__)
#define _XOPEN_SOURCE 600 // required by strptime
//#endif

#include <stdio.h>
#include <string.h>
#include "helper.h"
#include <regex.h> 
#include "parser.h"
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

static regex_t vhost_regex, req_client_regex, cipher_suite_regex;
static int regexs_initialised = 0;

const char* const csv_auto_format_guess_matrix[] = {
    "$host:$server_port $remote_addr - - [$time_local] \"$request\" $status $body_bytes_sent $request_length $request_time $upstream_response_time", // csvVhostCustom4
    "$host:$server_port $remote_addr - - [$time_local] \"$request\" $status $body_bytes_sent - - $request_length $request_time",                     // csvVhostCustom3
    "$host:$server_port $remote_addr - - [$time_local] \"$request\" $status $body_bytes_sent $request_length $request_time $upstream_response_time", // csvVhostCustom2
    "$host:$server_port $remote_addr - - [$time_local] \"$request\" $status $body_bytes_sent $request_length $request_time",                         // csvVhostCustom1
    "$host:$server_port $remote_addr - - [$time_local] \"$request\" $status $body_bytes_sent",                                                       // csvVhostCommon
    "$remote_addr - - [$time_local] \"$request\" $status $body_bytes_sent - - $request_length $request_time $upstream_response_time",               // csvCustom4
    "$remote_addr - - [$time_local] \"$request\" $status $body_bytes_sent - - $request_length $request_time",                                       // csvCustom3
    "$remote_addr - - [$time_local] \"$request\" $status $body_bytes_sent $request_length $request_time $upstream_response_time",                   // csvCustom2
    "$remote_addr - - [$time_local] \"$request\" $status $body_bytes_sent $request_length $request_time",                                           // csvCustom1
    "$remote_addr - - [$time_local] \"$request\" $status $body_bytes_sent",                                                                         // csvCommon
    NULL}
;

typedef enum {
    STR2XX_SUCCESS = 0,
    STR2XX_OVERFLOW,
    STR2XX_UNDERFLOW,
    STR2XX_INCONVERTIBLE
} str2xx_errno;

/* Convert string s to int out.
 * https://stackoverflow.com/questions/7021725/how-to-convert-a-string-to-integer-in-c
 *
 * @param[out] out The converted int. Cannot be NULL.
 * @param[in] s Input string to be converted.
 *
 *     The format is the same as strtol,
 *     except that the following are inconvertible:
 *     - empty string
 *     - leading whitespace
 *     - any trailing characters that are not part of the number
 *     Cannot be NULL.
 *
 * @param[in] base Base to interpret string in. Same range as strtol (2 to 36).
 * @return Indicates if the operation succeeded, or why it failed.
 */
static inline str2xx_errno str2int(int *out, char *s, int base) {
    char *end;
    if (s[0] == '\0' || isspace(s[0])){
        #if ENABLE_PARSE_LOG_LINE_FPRINTS
        fprintf(stderr, "str2int error: STR2XX_INCONVERTIBLE 1\n");
        #endif
        return STR2XX_INCONVERTIBLE;
    }
    errno = 0;
    long l = strtol(s, &end, base);
    /* Both checks are needed because INT_MAX == LONG_MAX is possible. */
    if (l > INT_MAX || (errno == ERANGE && l == LONG_MAX)){
        #if ENABLE_PARSE_LOG_LINE_FPRINTS
        fprintf(stderr, "str2int error: STR2XX_OVERFLOW\n");
        #endif
        return STR2XX_OVERFLOW;
    }
    if (l < INT_MIN || (errno == ERANGE && l == LONG_MIN)){
        #if ENABLE_PARSE_LOG_LINE_FPRINTS
        fprintf(stderr, "str2int error: STR2XX_UNDERFLOW\n");
        #endif
        return STR2XX_UNDERFLOW;
    }
    if (*end != '\0'){
        #if ENABLE_PARSE_LOG_LINE_FPRINTS
        fprintf(stderr, "str2int error: STR2XX_INCONVERTIBLE 2\n");
        #endif
        return STR2XX_INCONVERTIBLE;
    }
    *out = l;
    return STR2XX_SUCCESS;
}

static inline str2xx_errno str2float(float *out, char *s) {
    char *end;
    if (s[0] == '\0' || isspace(s[0])){ 
        #if ENABLE_PARSE_LOG_LINE_FPRINTS
        fprintf(stderr, "str2float error: STR2XX_INCONVERTIBLE 1\n");
        #endif
        return STR2XX_INCONVERTIBLE;
    }
    errno = 0;
    float f = strtof(s, &end);
    /* Both checks are needed because INT_MAX == LONG_MAX is possible. */
    if (errno == ERANGE && f == HUGE_VALF){
        #if ENABLE_PARSE_LOG_LINE_FPRINTS
        fprintf(stderr, "str2float error: STR2XX_OVERFLOW\n");
        #endif
        return STR2XX_OVERFLOW;
    }
    if (errno == ERANGE && f == -HUGE_VALF){
        #if ENABLE_PARSE_LOG_LINE_FPRINTS
        fprintf(stderr, "str2float error: STR2XX_UNDERFLOW\n");
        #endif
        return STR2XX_UNDERFLOW;
    }
    if (*end != '\0'){
        #if ENABLE_PARSE_LOG_LINE_FPRINTS
        fprintf(stderr, "str2float error: STR2XX_INCONVERTIBLE 2\n");
        #endif
        return STR2XX_INCONVERTIBLE;
    }
    *out = f;
    return STR2XX_SUCCESS;
}

/* CSV Parse code adapted from: https://github.com/semitrivial/csv_parser */
void free_csv_line(char **parsed){
    char **ptr;
    for(ptr = parsed; *ptr; ptr++) freez(*ptr);
    freez(parsed);
}

static inline int count_fields(const char *line, const char delimiter){
    const char *ptr;
    int cnt, fQuote;

    for ( cnt = 1, fQuote = 0, ptr = line; *ptr; ptr++ ) {
        if ( fQuote ) {
            if ( *ptr == '\"' ) {
                if ( ptr[1] == '\"' ) {
                    ptr++;
                    continue;
                }
                fQuote = 0;
            }
            continue;
        }

        if(*ptr == '\"'){
        	fQuote = 1;
        	continue;
        }
        if(*ptr == delimiter){
        	cnt++;
        	continue;
        }
        //continue;

        /*switch( *ptr ) {
            case '\"':
                fQuote = 1;
                continue;
            case delimiter:
                cnt++;
                continue;
            default:
                continue;
        }*/
    }

    if ( fQuote ) {
        return -1;
    }

    return cnt;
}

/*
 *  Given a string containing no linebreaks, or containing line breaks
 *  which are escaped by "double quotes", extract a NULL-terminated
 *  array of strings, one for every cell in the row.
 */
static inline char **parse_csv( const char *line, const char delimiter, int num_fields) {
    char **buf, **bptr, *tmp, *tptr;
    const char *ptr;
    int fQuote, fEnd;

    if(num_fields < 0){
	    num_fields = count_fields(line, delimiter);

	    if ( num_fields == -1 ) {
	        return NULL;
	    }
	}

    buf = mallocz( sizeof(char*) * (num_fields+1) );

    if ( !buf ) {
        return NULL;
    }

    tmp = mallocz( strlen(line) + 1 );

    if ( !tmp ) {
        freez( buf );
        return NULL;
    }

    bptr = buf;

    for ( ptr = line, fQuote = 0, *tmp = '\0', tptr = tmp, fEnd = 0; ; ptr++ ) {
        if ( fQuote ) {
            if ( !*ptr ) {
                break;
            }

            if ( *ptr == '\"' ) {
                if ( ptr[1] == '\"' ) {
                    *tptr++ = '\"';
                    ptr++;
                    continue;
                }
                fQuote = 0;
            }
            else {
                *tptr++ = *ptr;
            }

            continue;
        }


        if(*ptr == '\"'){
        	fQuote = 1;
        	continue;
        }
        else if(*ptr == '\0'){
        	fEnd = 1;
        	*tptr = '\0';
        	*bptr = strdupz( tmp );

        	if ( !*bptr ) {
        		for ( bptr--; bptr >= buf; bptr-- ) {
        			freez( *bptr );
        		}
        		freez( buf );
        		freez( tmp );

        		return NULL;
        	}

        	bptr++;
        	tptr = tmp;
        	break;
        }
        else if(*ptr == delimiter){
        	*tptr = '\0';
        	*bptr = strdupz( tmp );

        	if ( !*bptr ) {
        		for ( bptr--; bptr >= buf; bptr-- ) {
        			freez( *bptr );
        		}
        		freez( buf );
        		freez( tmp );

        		return NULL;
        	}

        	bptr++;
        	tptr = tmp;

        	continue;
        }
        else{
        	*tptr++ = *ptr;
        	continue;
        }

        /*switch( *ptr ) {
            case '\"':
                fQuote = 1;
                continue;
            case '\0':
                fEnd = 1;
            case delimiter:
                *tptr = '\0';
                *bptr = strdupz( tmp );

                if ( !*bptr ) {
                    for ( bptr--; bptr >= buf; bptr-- ) {
                        freez( *bptr );
                    }
                    freez( buf );
                    freez( tmp );

                    return NULL;
                }

                bptr++;
                tptr = tmp;

                if ( fEnd ) {
                  break;
                } else {
                  continue;
                }

            default:
                *tptr++ = *ptr;
                continue;
        }*/

        if ( fEnd ) {
            break;
        }
    }

    *bptr = NULL;
    freez( tmp );
    return buf;
}

/**
 * @brief Search a buffer for a keyword
 * @details Search the source buffer for a keyword and copy matches to the destination buffer
 * @param src The source buffer to be searched
 * @param dest The destination buffer where the results will be written in
 * @param keyword The keyword to be searched for in the source buffer
 * @param ignore_case Case insensitive search if 1, it does not matter if keyword characters 
 * are upper or lower case. 
 * @todo Sanitise keyword (escape regex special characters)
 */
void search_keyword(char *src, char *dest, const char *keyword, const int ignore_case){
	char regexString[REGEX_SIZE];
	snprintf(regexString, REGEX_SIZE, ".*(%s).*", "error");

	regex_t regex_compiled;
	regmatch_t groupArray[1];
	int regex_flags = ignore_case ? REG_EXTENDED | REG_NEWLINE | REG_ICASE : REG_EXTENDED | REG_NEWLINE;

	if (regcomp(&regex_compiled, regexString, regex_flags)){
		fprintf_log(LOGS_MANAG_ERROR, stderr, "Could not compile regular expression.\n");
		fatal("Could not compile regular expression.\n");
	};

	size_t dest_off = 0;
	char *cursor = src;
	for (int m = 0; ; m++){
		if (regexec(&regex_compiled, cursor, 1, groupArray, 0)) break;  // No more matches

		unsigned int offset = 0;
		if (groupArray[0].rm_so == (size_t)-1) break;  // No more groups

		offset = groupArray[0].rm_eo;

		fprintf_log(LOGS_MANAG_DEBUG, stderr, "Match %u: [%2u-%2u]: %.*s\n",
			m, groupArray[0].rm_so, groupArray[0].rm_eo, groupArray[0].rm_eo - groupArray[0].rm_so,
			cursor + groupArray[0].rm_so);
		memcpy(&dest[dest_off], cursor + groupArray[0].rm_so, groupArray[0].rm_eo - groupArray[0].rm_so);
		dest_off += groupArray[0].rm_eo - groupArray[0].rm_so;
		dest[dest_off++] = '\n';
		cursor += offset;
	}

	regfree(&regex_compiled);

	fprintf_log(LOGS_MANAG_INFO, stderr, "Searching for keyword: %s \n=====***********=====\n%s\n=====***********=====\n", keyword, src);
	fprintf_log(LOGS_MANAG_INFO, stderr, "Results of search\n=====***********=====\n%s\n=====***********=====\n", dest);
}

/**
 * 
 * @brief Extract parser configuration from string
 * @param[in] log_format String that describes the log format
 * @param[in] delimiter Delimiter to be used when parsing a CSV log format
 * @return Struct that contains the extracted log format configuration
 */
Log_parser_config_t *read_parse_config(char *log_format, const char delimiter){
    int num_fields = count_fields(log_format, delimiter);
    if(num_fields <= 0) return NULL;

    /* If first execution of this function, initialise regexs */
    // TODO: Tests needed for following regexs.
    if(!regexs_initialised){
        assert(regcomp(&vhost_regex, "^[a-zA-Z0-9:.-]+$", REG_NOSUB | REG_EXTENDED) == 0);
        assert(regcomp(&req_client_regex, "^([0-9a-f:.]+|localhost)$", REG_NOSUB | REG_EXTENDED) == 0);
        assert(regcomp(&cipher_suite_regex, "^[A-Z0-9_-]+$", REG_NOSUB | REG_EXTENDED) == 0);
    }

    Log_parser_config_t *parser_config = callocz(1, sizeof(Log_parser_config_t));
    parser_config->num_fields = num_fields;
    parser_config->delimiter = delimiter;
    
    // fprintf(stderr, "ND-LGS: numFields: %d\n", num_fields);
    char **parsed_format = parse_csv(log_format, delimiter, num_fields); // parsed_format is NULL-terminated
    parser_config->fields = callocz(num_fields, sizeof(log_line_field_t));
    unsigned int fields_off = 0;

    for(int i = 0; i < num_fields; i++ ){
		fprintf(stderr, "Field %d (%s) is ", i, parsed_format[i]);

        if(strcmp(parsed_format[i], "$host:$server_port") == 0 || 
           strcmp(parsed_format[i], "%v:%p") == 0) {
            fprintf(stderr, "VHOST_WITH_PORT\n");
            parser_config->fields[fields_off++] = VHOST_WITH_PORT;
            continue;
        }

		if(strcmp(parsed_format[i], "$host") == 0 || 
		   strcmp(parsed_format[i], "$http_host") == 0 ||
		   strcmp(parsed_format[i], "%v") == 0) {
			fprintf(stderr, "VHOST\n");
			parser_config->fields[fields_off++] = VHOST;
			continue;
		}

		if(strcmp(parsed_format[i], "$server_port") == 0 || 
		   strcmp(parsed_format[i], "%p") == 0) {
			fprintf(stderr, "PORT\n");
			parser_config->fields[fields_off++] = PORT;
			continue;
		}

		if(strcmp(parsed_format[i], "$scheme") == 0) {
			fprintf(stderr, "REQ_SCHEME\n");
			parser_config->fields[fields_off++] = REQ_SCHEME;
			continue;
		}

		if(strcmp(parsed_format[i], "$remote_addr") == 0 || 
		   strcmp(parsed_format[i], "%a") == 0 ||
		   strcmp(parsed_format[i], "%h") == 0) {
			fprintf(stderr, "REQ_CLIENT\n");
			parser_config->fields[fields_off++] = REQ_CLIENT;
			continue;
		}

		if(strcmp(parsed_format[i], "$request") == 0 || 
		   strcmp(parsed_format[i], "%r") == 0) {
			fprintf(stderr, "REQ\n");
			parser_config->fields[fields_off++] = REQ;
			continue;
		}

		if(strcmp(parsed_format[i], "$request_method") == 0 || 
		   strcmp(parsed_format[i], "%m") == 0) {
			fprintf(stderr, "REQ_METHOD\n");
			parser_config->fields[fields_off++] = REQ_METHOD;
			continue;
		}

		if(strcmp(parsed_format[i], "$request_uri") == 0 || 
		   strcmp(parsed_format[i], "%U") == 0) {
			fprintf(stderr, "REQ_URL\n");
			parser_config->fields[fields_off++] = REQ_URL;
			continue;
		}

		if(strcmp(parsed_format[i], "$server_protocol") == 0 || 
		   strcmp(parsed_format[i], "%H") == 0) {
			fprintf(stderr, "REQ_PROTO\n");
			parser_config->fields[fields_off++] = REQ_PROTO;
			continue;
		}

		if(strcmp(parsed_format[i], "$request_length") == 0 || 
		   strcmp(parsed_format[i], "%I") == 0) {
			fprintf(stderr, "REQ_SIZE\n");
			parser_config->fields[fields_off++] = REQ_SIZE;
			continue;
		}

		if(strcmp(parsed_format[i], "$request_time") == 0 || 
		   strcmp(parsed_format[i], "%D") == 0) {
			fprintf(stderr, "REQ_PROC_TIME\n");
			parser_config->fields[fields_off++] = REQ_PROC_TIME;
			continue;
		}

		if(strcmp(parsed_format[i], "$status") == 0 || 
		   strcmp(parsed_format[i], "%>s") == 0 ||
		   strcmp(parsed_format[i], "%s") == 0) {
			fprintf(stderr, "RESP_CODE\n");
			parser_config->fields[fields_off++] = RESP_CODE;
			continue;
		}

		if(strcmp(parsed_format[i], "$bytes_sent") == 0 || 
		   strcmp(parsed_format[i], "$body_bytes_sent") == 0 ||
		   strcmp(parsed_format[i], "%b") == 0 ||
		   strcmp(parsed_format[i], "%O") == 0 ||
		   strcmp(parsed_format[i], "%B") == 0) {
			fprintf(stderr, "RESP_SIZE\n");
			parser_config->fields[fields_off++] = RESP_SIZE;
			continue;
		}

		if(strcmp(parsed_format[i], "$upstream_response_time") == 0) {
			fprintf(stderr, "UPS_RESP_TIME\n");
			parser_config->fields[fields_off++] = UPS_RESP_TIME;
			continue;
		}

		if(strcmp(parsed_format[i], "$ssl_protocol") == 0) {
			fprintf(stderr, "SSL_PROTO\n");
			parser_config->fields[fields_off++] = SSL_PROTO;
			continue;
		}

		if(strcmp(parsed_format[i], "$ssl_cipher") == 0) {
			fprintf(stderr, "SSL_CIPHER_SUITE\n");
			parser_config->fields[fields_off++] = SSL_CIPHER_SUITE;
			continue;
		}

		if(strcmp(parsed_format[i], "$time_local") == 0 || strcmp(parsed_format[i], "[$time_local]") == 0 ||
		   strcmp(parsed_format[i], "%t") == 0 || strcmp(parsed_format[i], "[%t]") == 0) {
			fprintf(stderr, "TIME\n");
            parser_config->fields = reallocz(parser_config->fields, (num_fields + 1) * sizeof(log_line_field_t));
			parser_config->fields[fields_off++] = TIME;
            parser_config->fields[fields_off++] = TIME; // TIME takes 2 fields
            parser_config->num_fields++;                // TIME takes 2 fields
			continue;
		}

		fprintf(stderr, "UNKNOWN OR CUSTOM\n");
		parser_config->fields[fields_off++] = CUSTOM;
		continue;

	}

    for(int i = 0; parsed_format[i] != NULL; i++) freez(parsed_format[i]);
    return parser_config;
}

static Log_line_parsed_t *parse_log_line(Log_parser_config_t *parser_config, Log_parser_buffs_t *parser_buffs, const char *line, const int verify){
    log_line_field_t *fields_format = parser_config->fields;
    const int num_fields_config = parser_config->num_fields;
    const char delimiter = parser_config->delimiter;

    parser_buffs->log_line_parsed = (Log_line_parsed_t) {};
    Log_line_parsed_t *log_line_parsed = &parser_buffs->log_line_parsed;
#if ENABLE_PARSE_LOG_LINE_FPRINTS
    fprintf(stderr, "Original line:%s\n", line);
#endif
    int num_fields_line = count_fields(line, delimiter);
    char **parsed = parse_csv(line, delimiter, num_fields_line);
#if ENABLE_PARSE_LOG_LINE_FPRINTS
    fprintf(stderr, "Number of items in line: %d and expected from config: %d\n", num_fields_line, num_fields_config);
#endif
    // assert(num_fields_config == num_fields_line); // TODO: REMOVE FROM PRODUCTION - Handle error instead?
    if(num_fields_config != num_fields_line){ 
        free_csv_line(parsed);
        return NULL;
    }
    for(int i = 0; i < num_fields_config; i++ ){
        #if ENABLE_PARSE_LOG_LINE_FPRINTS
        fprintf(stderr, "===\nField %d:%s\n", i, parsed[i]);
        #endif

        if(fields_format[i] == CUSTOM){
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: CUSTOM or UNKNOWN):%s\n", i, parsed[i]);
            #endif
            continue;
        }

        if(fields_format[i] == VHOST_WITH_PORT){
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: VHOST_WITH_PORT):%s\n", i, parsed[i]);
            #endif

            if(unlikely(!strcmp(parsed[i], "-"))){
                log_line_parsed->vhost[0] = '\0';
                log_line_parsed->port = INVALID_PORT;
                log_line_parsed->parsing_errors++;
                continue;
            }

            char *sep = strrchr(parsed[i], ':');
            if(likely(sep)) *sep = '\0';
            else { 
                // parsed[i][0] = '\0';
                log_line_parsed->vhost[0] = '\0';
                log_line_parsed->port = INVALID_PORT;
                log_line_parsed->parsing_errors++;
                continue;
            }
        }

        if(fields_format[i] == VHOST_WITH_PORT || fields_format[i] == VHOST){
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: VHOST):%s\n", i, parsed[i]);
            #endif

            if(unlikely(!strcmp(parsed[i], "-"))){
                log_line_parsed->vhost[0] = '\0';
                log_line_parsed->parsing_errors++;
                continue;
            }

            // nginx $host and $http_host return ipv6 in [], apache doesn't
            // TODO: TEST! This case hasn't been tested!
            char *pch = strchr(parsed[i], ']');
            if(pch){
                *pch = '\0';
                memmove(parsed[i], parsed[i]+1, strlen(parsed[i]));
            }
            if(verify){
                int rc = regexec(&vhost_regex, parsed[i], 0, NULL, 0);
                if(likely(!rc)) snprintf(log_line_parsed->vhost, VHOST_MAX_LEN, "%s", parsed[i]);
                else if (rc == REG_NOMATCH) {
                    #if ENABLE_PARSE_LOG_LINE_FPRINTS
                    fprintf(stderr, "VHOST is invalid\n");
                    #endif
                    log_line_parsed->vhost[0] = '\0';
                    log_line_parsed->parsing_errors++;
                }
                else assert(0); 
            }
            else snprintf(log_line_parsed->vhost, VHOST_MAX_LEN, "%s", parsed[i]);
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted VHOST:%s\n", log_line_parsed->vhost);
            #endif

            if(fields_format[i] == VHOST) continue;
        }

        if(fields_format[i] == VHOST_WITH_PORT || fields_format[i] == PORT){
            char *port;
            if(fields_format[i] == VHOST_WITH_PORT) port = &parsed[i][strlen(parsed[i]) + 1];
            else port = parsed[i];
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: PORT):%s\n", i, port);
            #endif

            if(unlikely(!strcmp(parsed[i], "-"))){
                log_line_parsed->port = INVALID_PORT;
                log_line_parsed->parsing_errors++;
                continue;
            }

            if(likely(str2int(&log_line_parsed->port, port, 10) == STR2XX_SUCCESS)){
                if(verify){
                    if(unlikely(log_line_parsed->port < 80 || log_line_parsed->port > 49151)){
                        #if ENABLE_PARSE_LOG_LINE_FPRINTS
                        fprintf(stderr, "PORT is invalid (<80 or >49151)\n");
                        #endif
                        log_line_parsed->port = INVALID_PORT;
                        log_line_parsed->parsing_errors++;
                    }
                }
            }
            else{
                #if ENABLE_PARSE_LOG_LINE_FPRINTS
                fprintf(stderr, "Error while extracting PORT from string\n");
                #endif
                log_line_parsed->port = INVALID_PORT;
                log_line_parsed->parsing_errors++;
            }
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted PORT:%d\n", log_line_parsed->port);
            #endif

            continue;
        }

        if(fields_format[i] == REQ_SCHEME){
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_SCHEME):%s\n", i, parsed[i]);
            #endif

            if(unlikely(!strcmp(parsed[i], "-"))){
                log_line_parsed->req_scheme[0] = '\0';
                log_line_parsed->parsing_errors++;
                continue;
            }

            snprintf(log_line_parsed->req_scheme, REQ_SCHEME_MAX_LEN, "%s", parsed[i]); 
            if(verify){
                if(strlen(parsed[i]) >= REQ_SCHEME_MAX_LEN || 
                    (strcmp(log_line_parsed->req_scheme, "http") && 
                     strcmp(log_line_parsed->req_scheme, "https"))){
                    #if ENABLE_PARSE_LOG_LINE_FPRINTS
                    fprintf(stderr, "REQ_SCHEME is invalid (must be either 'http' or 'https')\n");
                    #endif
                    log_line_parsed->req_scheme[0] = '\0';
                    log_line_parsed->parsing_errors++;
                }
            }
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted REQ_SCHEME:%s\n", log_line_parsed->req_scheme);
            #endif
            continue;
        }

        if(fields_format[i] == REQ_CLIENT){
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_CLIENT):%s\n", i, parsed[i]);
            #endif

            if(unlikely(!strcmp(parsed[i], "-"))){
                log_line_parsed->req_client[0] = '\0';
                log_line_parsed->parsing_errors++;
                continue;
            }

            if(verify){
                int rc = regexec(&req_client_regex, parsed[i], 0, NULL, 0);
                if(likely(!rc)) snprintf(log_line_parsed->req_client, REQ_CLIENT_MAX_LEN, "%s", parsed[i]);
                else if (rc == REG_NOMATCH) {
                    #if ENABLE_PARSE_LOG_LINE_FPRINTS
                    fprintf(stderr, "REQ_CLIENT is invalid\n");
                    #endif
                    snprintf(log_line_parsed->req_client, REQ_CLIENT_MAX_LEN, "%s", INVALID_CLIENT_IP_STR);
                    log_line_parsed->parsing_errors++;
                }
                else assert(0); // Can also use: regerror(rc, &req_client_regex, msgbuf, sizeof(msgbuf));
            }
            else snprintf(log_line_parsed->req_client, REQ_CLIENT_MAX_LEN, "%s", parsed[i]);
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted REQ_CLIENT:%s\n", log_line_parsed->req_client);
            #endif

            continue;
        }

        char *req_first_sep, *req_last_sep = NULL;
        if(fields_format[i] == REQ && strcmp(parsed[i], "-")){
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ):%s\n", i, parsed[i]);
            #endif
            req_first_sep = strchr(parsed[i], ' ');
            req_last_sep = strrchr(parsed[i], ' ');
            if(!req_first_sep || req_first_sep == req_last_sep) {
                // parsed[i][0] = '\0';
                log_line_parsed->req_method[0] = '\0';
                log_line_parsed->req_URL = NULL;
                log_line_parsed->req_proto[0] = '\0';
                log_line_parsed->parsing_errors++;
                continue;
            }
            else *req_first_sep = *req_last_sep = '\0';
        }

        if(fields_format[i] == REQ || fields_format[i] == REQ_METHOD){
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_METHOD):%s\n", i, parsed[i]);
            #endif

            if(unlikely(!strcmp(parsed[i], "-"))){
                log_line_parsed->req_method[0] = '\0';
                log_line_parsed->parsing_errors++;
            }
            else{
                snprintf(log_line_parsed->req_method, REQ_METHOD_MAX_LEN, "%s", parsed[i]); 
                if(verify){
                    if(strlen(parsed[i]) >= REQ_METHOD_MAX_LEN || 
                        (strcmp(log_line_parsed->req_method, "ACL") && 
                         strcmp(log_line_parsed->req_method, "BASELINE-CONTROL") && 
                         strcmp(log_line_parsed->req_method, "BIND") && 
                         strcmp(log_line_parsed->req_method, "CHECKIN") && 
                         strcmp(log_line_parsed->req_method, "CHECKOUT") && 
                         strcmp(log_line_parsed->req_method, "CONNECT") && 
                         strcmp(log_line_parsed->req_method, "COPY") && 
                         strcmp(log_line_parsed->req_method, "DELETE") && 
                         strcmp(log_line_parsed->req_method, "GET") && 
                         strcmp(log_line_parsed->req_method, "HEAD") && 
                         strcmp(log_line_parsed->req_method, "LABEL") && 
                         strcmp(log_line_parsed->req_method, "LINK") && 
                         strcmp(log_line_parsed->req_method, "LOCK") && 
                         strcmp(log_line_parsed->req_method, "MERGE") && 
                         strcmp(log_line_parsed->req_method, "MKACTIVITY") && 
                         strcmp(log_line_parsed->req_method, "MKCALENDAR") && 
                         strcmp(log_line_parsed->req_method, "MKCOL") && 
                         strcmp(log_line_parsed->req_method, "MKREDIRECTREF") && 
                         strcmp(log_line_parsed->req_method, "MKWORKSPACE") && 
                         strcmp(log_line_parsed->req_method, "MOVE") && 
                         strcmp(log_line_parsed->req_method, "OPTIONS") && 
                         strcmp(log_line_parsed->req_method, "ORDERPATCH") && 
                         strcmp(log_line_parsed->req_method, "PATCH") && 
                         strcmp(log_line_parsed->req_method, "POST") && 
                         strcmp(log_line_parsed->req_method, "PRI") && 
                         strcmp(log_line_parsed->req_method, "PROPFIND") && 
                         strcmp(log_line_parsed->req_method, "PROPPATCH") && 
                         strcmp(log_line_parsed->req_method, "PUT") && 
                         strcmp(log_line_parsed->req_method, "REBIND") && 
                         strcmp(log_line_parsed->req_method, "REPORT") && 
                         strcmp(log_line_parsed->req_method, "SEARCH") && 
                         strcmp(log_line_parsed->req_method, "TRACE") && 
                         strcmp(log_line_parsed->req_method, "UNBIND") && 
                         strcmp(log_line_parsed->req_method, "UNCHECKOUT") && 
                         strcmp(log_line_parsed->req_method, "UNLINK") && 
                         strcmp(log_line_parsed->req_method, "UNLOCK") && 
                         strcmp(log_line_parsed->req_method, "UPDATE") && 
                         strcmp(log_line_parsed->req_method, "UPDATEREDIRECTREF"))){
                        #if ENABLE_PARSE_LOG_LINE_FPRINTS
                        fprintf(stderr, "REQ_METHOD is invalid\n");
                        #endif
                        log_line_parsed->req_method[0] = '\0';
                        log_line_parsed->parsing_errors++;
                    }
                }
                #if ENABLE_PARSE_LOG_LINE_FPRINTS
                fprintf(stderr, "Extracted REQ_METHOD:%s\n", log_line_parsed->req_method);
                #endif
            }
            
            if(fields_format[i] == REQ_METHOD) continue;
        }

        if(fields_format[i] == REQ || fields_format[i] == REQ_URL){
            if(fields_format[i] == REQ) log_line_parsed->req_URL = req_first_sep ? strdupz(req_first_sep + 1) : NULL;
            else log_line_parsed->req_URL = strdupz(parsed[i]);
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_URL):%s\n", i, log_line_parsed->req_URL ? log_line_parsed->req_URL : "NULL!");   
            //if(verify){} ??
            if(unlikely(!strcmp(parsed[i], "-"))){
                log_line_parsed->req_URL = NULL;
                log_line_parsed->parsing_errors++;
            }
            fprintf(stderr, "Extracted REQ_URL:%s\n", log_line_parsed->req_URL ? log_line_parsed->req_URL : "NULL!");
            #endif

            if(fields_format[i] == REQ_URL) continue;
        }

        if(fields_format[i] == REQ || fields_format[i] == REQ_PROTO){
            char *req_proto = NULL;
            if(fields_format[i] == REQ) req_proto = req_last_sep ? req_last_sep + 1 : NULL;
            else req_proto = parsed[i];

            if(unlikely(!strcmp(parsed[i], "-"))){
                log_line_parsed->req_proto[0] = '\0';
                log_line_parsed->parsing_errors++;
            }

            if(verify){
                if(!req_proto || !*req_proto || strlen(req_proto) < 6 || strncmp(req_proto, "HTTP/", 5) || 
                    (strcmp(&req_proto[5], "1") && 
                     strcmp(&req_proto[5], "1.0") && 
                     strcmp(&req_proto[5], "1.1") && 
                     strcmp(&req_proto[5], "2") && 
                     strcmp(&req_proto[5], "2.0"))){
                    #if ENABLE_PARSE_LOG_LINE_FPRINTS
                    fprintf(stderr, "REQ_PROTO is invalid\n");
                    #endif
                    log_line_parsed->req_proto[0] = '\0';
                    log_line_parsed->parsing_errors++;
                }
                else snprintf(log_line_parsed->req_proto, REQ_PROTO_MAX_LEN, "%s", &req_proto[5]); 
            }
            else snprintf(log_line_parsed->req_proto, REQ_PROTO_MAX_LEN, "%s", &req_proto[5]); 
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_PROTO):%s\n", i, req_proto);
            fprintf(stderr, "Extracted REQ_PROTO:%s\n", log_line_parsed->req_proto);
            #endif

            if(fields_format[i] == REQ_PROTO) continue;
        }

        if(fields_format[i] == REQ_SIZE){
            /* TODO: Differentiate between '-' or 0 and an invalid request size. 
             * right now, all these will set req_size == 0 */
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_SIZE):%s\n", i, parsed[i]);
            #endif
            if(!strcmp(parsed[i], "-")) log_line_parsed->req_size = 0; // Request size can be '-' 
            else if(str2int(&log_line_parsed->req_size, parsed[i], 10) == STR2XX_SUCCESS){
                if(verify){
                    if(log_line_parsed->req_size < 0){
                        #if ENABLE_PARSE_LOG_LINE_FPRINTS
                        fprintf(stderr, "REQ_SIZE is invalid (<0)\n");
                        #endif
                        log_line_parsed->req_size = 0;
                        log_line_parsed->parsing_errors++;
                    }
                }
            }
            else{
                fprintf(stderr, "Error while extracting REQ_SIZE from string\n");
                log_line_parsed->parsing_errors++;
            }
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted REQ_SIZE:%d\n", log_line_parsed->req_size);
            #endif

            continue;
        }

        if(fields_format[i] == REQ_PROC_TIME){
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_PROC_TIME):%s\n", i, parsed[i]);
            #endif

            if(unlikely(!strcmp(parsed[i], "-"))){
                log_line_parsed->req_proc_time = 0;
                log_line_parsed->parsing_errors++;
            }

            if(strchr(parsed[i], '.')){ // nginx time is in seconds with a milliseconds resolution.
                float f = 0;
                if(str2float(&f, parsed[i]) == STR2XX_SUCCESS) log_line_parsed->req_proc_time = (int) (f * 1.0E6);
                else{ 
                    fprintf(stderr, "Error while extracting REQ_PROC_TIME from string\n");
                    log_line_parsed->req_proc_time = 0;
                    log_line_parsed->parsing_errors++;
                }
            }
            else{ // apache time is in microseconds
                if(str2int(&log_line_parsed->req_proc_time, parsed[i], 10) != STR2XX_SUCCESS){
                    #if ENABLE_PARSE_LOG_LINE_FPRINTS
                    fprintf(stderr, "Error while extracting REQ_PROC_TIME from string\n");
                    #endif
                    log_line_parsed->req_proc_time = 0;
                    log_line_parsed->parsing_errors++;
                }
            }
            if(verify){
                if(log_line_parsed->req_proc_time < 0){
                    #if ENABLE_PARSE_LOG_LINE_FPRINTS
                    fprintf(stderr, "REQ_PROC_TIME is invalid (<0)\n");
                    #endif
                    log_line_parsed->req_proc_time = 0;
                    log_line_parsed->parsing_errors++;
                }
            }
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted REQ_PROC_TIME:%d\n", log_line_parsed->req_proc_time);
            #endif

            continue;
        }

        if(fields_format[i] == RESP_CODE){
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: RESP_CODE):%s\n", i, parsed[i]);
            #endif

            if(unlikely(!strcmp(parsed[i], "-"))){
                log_line_parsed->resp_code = 0;
                log_line_parsed->parsing_errors++;
            }

            if(str2int(&log_line_parsed->resp_code, parsed[i], 10) == STR2XX_SUCCESS){  
                if(verify){
                    // rfc7231
                    // Informational responses (100–199),
                    // Successful responses (200–299),
                    // Redirects (300–399),
                    // Client errors (400–499),
                    // Server errors (500–599).
                    if(log_line_parsed->resp_code < 100 || log_line_parsed->resp_code > 599){
                        #if ENABLE_PARSE_LOG_LINE_FPRINTS
                        fprintf(stderr, "RESP_CODE is invalid (<100 or >600)\n");
                        #endif
                        log_line_parsed->resp_code = 0;
                        log_line_parsed->parsing_errors++;
                    }
                }
            }
            else{ 
                #if ENABLE_PARSE_LOG_LINE_FPRINTS
                fprintf(stderr, "Error while extracting RESP_CODE from string\n");
                #endif
                log_line_parsed->parsing_errors++;
            }
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted RESP_CODE:%d\n", log_line_parsed->resp_code);
            #endif

            continue;
        }

        if(fields_format[i] == RESP_SIZE){
            /* TODO: Differentiate between '-' or 0 and an invalid request size. 
             * right now, all these will set resp_size == 0 */
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: RESP_SIZE):%s\n", i, parsed[i]);
            #endif
            if(!strcmp(parsed[i], "-")) log_line_parsed->resp_size = 0; // Request size can be '-' 
            else if(str2int(&log_line_parsed->resp_size, parsed[i], 10) == STR2XX_SUCCESS){
                if(verify){
                    if(log_line_parsed->resp_size < 0){
                        #if ENABLE_PARSE_LOG_LINE_FPRINTS
                        fprintf(stderr, "RESP_SIZE is invalid (<0)\n");
                        #endif
                        log_line_parsed->resp_size = 0;
                        log_line_parsed->parsing_errors++;
                    }
                }
            }
            else {
                #if ENABLE_PARSE_LOG_LINE_FPRINTS
                fprintf(stderr, "Error while extracting RESP_SIZE from string\n");
                #endif
                log_line_parsed->parsing_errors++;
            }
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted RESP_SIZE:%d\n", log_line_parsed->resp_size);
            #endif

            continue;
        }

        if(fields_format[i] == UPS_RESP_TIME && strcmp(parsed[i], "-")){
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: UPS_RESP_TIME):%s\n", i, parsed[i]);
            #endif

            if(unlikely(!strcmp(parsed[i], "-"))){
                log_line_parsed->ups_resp_time = 0;
                log_line_parsed->parsing_errors++;
            }

            /* Times of several responses are separated by commas and colons. Following the 
             * Go parser implementation, where only the first one is kept, the others are 
             * discarded. Also, there must be no space in between them. Needs testing... */
            char *pch = strchr(parsed[i], ',');
            if(pch) *pch = '\0';

            if(strchr(parsed[i], '.')){ // nginx time is in seconds with a milliseconds resolution.
                float f = 0;
                if(str2float(&f, parsed[i]) == STR2XX_SUCCESS) log_line_parsed->ups_resp_time = (int) (f * 1.0E6);
                else { 
                    #if ENABLE_PARSE_LOG_LINE_FPRINTS
                    fprintf(stderr, "Error while extracting UPS_RESP_TIME from string\n");
                    #endif
                    log_line_parsed->ups_resp_time = 0;
                    log_line_parsed->parsing_errors++;
                }
            }
            else{ // unlike in the REQ_PROC_TIME case, apache doesn't have an equivalent here
                #if ENABLE_PARSE_LOG_LINE_FPRINTS
                fprintf(stderr, "Error while extracting UPS_RESP_TIME from string\n");
                #endif
                log_line_parsed->parsing_errors++;
            }
            if(verify){
                if(log_line_parsed->ups_resp_time < 0){
                    #if ENABLE_PARSE_LOG_LINE_FPRINTS
                    fprintf(stderr, "UPS_RESP_TIME is invalid (<0)\n");
                    #endif
                    log_line_parsed->ups_resp_time = 0;
                    log_line_parsed->parsing_errors++;
                }
            }
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted UPS_RESP_TIME:%d\n", log_line_parsed->ups_resp_time);
            #endif

            continue;
        }

        if(fields_format[i] == SSL_PROTO){
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: SSL_PROTO):%s\n", i, parsed[i]);
            #endif

            if(unlikely(!strcmp(parsed[i], "-"))){
                log_line_parsed->ssl_proto[0] = '\0';
                log_line_parsed->parsing_errors++;
            }

            if(verify){
                if(strcmp(parsed[i], "TLSv1") && 
                     strcmp(parsed[i], "TLSv1.1") &&
                     strcmp(parsed[i], "TLSv1.2") &&
                     strcmp(parsed[i], "TLSv1.3") &&
                     strcmp(parsed[i], "SSLv2") &&
                     strcmp(parsed[i], "SSLv3")){
                    #if ENABLE_PARSE_LOG_LINE_FPRINTS
                    fprintf(stderr, "SSL_PROTO is invalid\n");
                    #endif
                    log_line_parsed->ssl_proto[0] = '\0';
                    log_line_parsed->parsing_errors++;
                }
                else snprintf(log_line_parsed->ssl_proto, SSL_PROTO_MAX_LEN, "%s", parsed[i]); 
            }
            else snprintf(log_line_parsed->ssl_proto, SSL_PROTO_MAX_LEN, "%s", parsed[i]); 
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted SSL_PROTO:%s\n", log_line_parsed->ssl_proto);
            #endif

            continue;
        }

        if(fields_format[i] == SSL_CIPHER_SUITE){
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: SSL_CIPHER_SUITE):%s\n", i, parsed[i]);
            #endif

            if(unlikely(!strcmp(parsed[i], "-"))){
                log_line_parsed->ssl_cipher[0] = '\0';
                log_line_parsed->parsing_errors++;
            }

            if(verify){
                int rc = regexec(&cipher_suite_regex, parsed[i], 0, NULL, 0);
                if(likely(!rc)) snprintf(log_line_parsed->ssl_cipher, SSL_CIPHER_SUITE_MAX_LEN, "%s", parsed[i]); 
                else if (rc == REG_NOMATCH) {
                    #if ENABLE_PARSE_LOG_LINE_FPRINTS
                    fprintf(stderr, "SSL_CIPHER_SUITE is invalid\n");
                    #endif
                    log_line_parsed->ssl_cipher[0] = '\0';
                    log_line_parsed->parsing_errors++;
                }
                else assert(0); // Can also use: regerror(rc, &cipher_suite_regex, msgbuf, sizeof(msgbuf));
            }
            else snprintf(log_line_parsed->ssl_cipher, SSL_CIPHER_SUITE_MAX_LEN, "%s", parsed[i]); 
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted SSL_CIPHER_SUITE:%s\n", log_line_parsed->ssl_cipher);
            #endif

            continue;
        }

        if(fields_format[i] == TIME){
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Items %d + %d (type: TIME - 2 fields):%s %s\n", i, i+1, parsed[i], parsed[i+1]);
            #endif

            if(!strcmp(parsed[i], "-")){
                log_line_parsed->timestamp = 0;
                log_line_parsed->parsing_errors++;
                ++i;
                continue;
            }

            char *pch = strchr(parsed[i], '[');
            if(pch) memmove(parsed[i], parsed[i]+1, strlen(parsed[i])); //%d/%b/%Y:%H:%M:%S %z
            struct tm ltm = {0};
            if(strptime(parsed[i], "%d/%b/%Y:%H:%M:%S", &ltm) == NULL){
                #if ENABLE_PARSE_LOG_LINE_FPRINTS
                fprintf(stderr, "TIME field parsing failed\n");
                #endif
                log_line_parsed->timestamp = 0;
                log_line_parsed->parsing_errors++;
                ++i;
                continue;
            }

            // char month[20];
            // sscanf(parsed[i], "%d/%[^/]/%d:%d:%d:%d", &ltm.tm_mday, month, &ltm.tm_year, &ltm.tm_hour, &ltm.tm_min, &ltm.tm_sec);            
            // ltm.tm_mon = 4;
            //fprintf(stderr, "day:%d month:%s year:%d sec:%d min:%d, hour:%d\n", ltm.tm_mday, month, ltm.tm_year, ltm.tm_sec, ltm.tm_min, ltm.tm_hour);
            //log_line_parsed->timestamp = (long int) mktime(&ltm);


            // Deal with 2nd part of datetime i.e. timezone
            //TODO: Error handling in case of parsed[++i] is not timezone??
            pch = strchr(parsed[++i], ']');
            if(pch) *pch = '\0';
            long int timezone = strtol(parsed[i], NULL, 10);
            long int timezone_h = timezone / 100;
            long int timezone_m = timezone % 100;
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Timezone: int:%ld, hrs:%ld, mins:%ld\n", timezone, timezone_h, timezone_m);
            #endif

            log_line_parsed->timestamp = (int64_t) mktime(&ltm) + (int64_t) timezone_h * 3600 + (int64_t) timezone_m * 60;
            #if ENABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted TIME:%lu\n", log_line_parsed->timestamp);
            #endif

            //if(verify){} ??
        }


    }

    free_csv_line(parsed);
    return log_line_parsed;
}

static inline void extract_metrics(Log_parser_config_t *parser_config, Log_line_parsed_t *line_parsed, Log_parser_metrics_t *metrics){

    /* Extract number of parsed lines */
    metrics->num_lines_total++;
    metrics->num_lines_rate++;

    /* Extract vhost */
    // TODO: Reduce number of reallocs
    if((parser_config->chart_config & CHART_VHOST) && line_parsed->vhost && *line_parsed->vhost){
        int i;
        for(i = 0; i < metrics->vhost_arr.size; i++){
            if(!strcmp(metrics->vhost_arr.vhosts[i].name, line_parsed->vhost)){
                metrics->vhost_arr.vhosts[i].count++;
                break;
            }
        }
        if(metrics->vhost_arr.size == i){ // Vhost not found in array - need to append
            metrics->vhost_arr.size++;
            metrics->vhost_arr.vhosts = reallocz(metrics->vhost_arr.vhosts, metrics->vhost_arr.size * sizeof(struct log_parser_metrics_vhost));
            snprintf(metrics->vhost_arr.vhosts[metrics->vhost_arr.size - 1].name, VHOST_MAX_LEN, "%s", line_parsed->vhost);
            metrics->vhost_arr.vhosts[metrics->vhost_arr.size - 1].count = 1;
        }
    }

    /* Extract port */
    // TODO: Reduce number of reallocs
    if((parser_config->chart_config & CHART_PORT) && line_parsed->port){
        int i;
        for(i = 0; i < metrics->port_arr.size; i++){
            if(metrics->port_arr.ports[i].port == line_parsed->port){
                metrics->port_arr.ports[i].count++;
                break;
            }
        }
        if(metrics->port_arr.size == i){ // Port not found in array - need to append
            metrics->port_arr.size++;
            metrics->port_arr.ports = reallocz(metrics->port_arr.ports, metrics->port_arr.size * sizeof(struct log_parser_metrics_port));
            metrics->port_arr.ports[metrics->port_arr.size - 1].port = line_parsed->port;
            metrics->port_arr.ports[metrics->port_arr.size - 1].count = 1;
        } 
    }

    /* Extract client metrics */
    if((parser_config->chart_config & (CHART_IP_VERSION | CHART_REQ_CLIENT_CURRENT | CHART_REQ_CLIENT_ALL_TIME)) && line_parsed->req_client && *line_parsed->req_client){
        if(!strcmp(line_parsed->req_client, INVALID_CLIENT_IP_STR)){
            if(parser_config->chart_config & CHART_IP_VERSION) metrics->ip_ver.invalid++;
        }
        else if(strchr(line_parsed->req_client, ':')){
            /* IPv6 version */
            if(parser_config->chart_config & CHART_IP_VERSION) metrics->ip_ver.v6++;

            /* Unique Client IPv6 Address */
            if(parser_config->chart_config & (CHART_REQ_CLIENT_CURRENT | CHART_REQ_CLIENT_ALL_TIME)){
                int i;
                for(i = 0; i < metrics->req_clients_current_arr.ipv6_size; i++){
                    if(!strcmp(metrics->req_clients_current_arr.ipv6_req_clients[i], line_parsed->req_client)) break;
                }
                if(metrics->req_clients_current_arr.ipv6_size == i){ // Req client not found in array - need to append
                    metrics->req_clients_current_arr.ipv6_size++;
                    metrics->req_clients_current_arr.ipv6_req_clients = reallocz(metrics->req_clients_current_arr.ipv6_req_clients, 
                        metrics->req_clients_current_arr.ipv6_size * sizeof(*metrics->req_clients_current_arr.ipv6_req_clients));
                    snprintf(metrics->req_clients_current_arr.ipv6_req_clients[metrics->req_clients_current_arr.ipv6_size - 1], 
                        REQ_CLIENT_MAX_LEN, "%s", line_parsed->req_client);
                }
            }
        }
        else{
            /* IPv4 version */
            if(parser_config->chart_config & CHART_IP_VERSION) metrics->ip_ver.v4++;

            /* Unique Client IPv4 Address */
            if(parser_config->chart_config & (CHART_REQ_CLIENT_CURRENT | CHART_REQ_CLIENT_ALL_TIME)){
                int i;
                for(i = 0; i < metrics->req_clients_current_arr.ipv4_size; i++){
                    if(!strcmp(metrics->req_clients_current_arr.ipv4_req_clients[i], line_parsed->req_client)) break;
                }
                if(metrics->req_clients_current_arr.ipv4_size == i){ // Req client not found in array - need to append
                    metrics->req_clients_current_arr.ipv4_size++;
                    metrics->req_clients_current_arr.ipv4_req_clients = reallocz(metrics->req_clients_current_arr.ipv4_req_clients, 
                        metrics->req_clients_current_arr.ipv4_size * sizeof(*metrics->req_clients_current_arr.ipv4_req_clients));
                    snprintf(metrics->req_clients_current_arr.ipv4_req_clients[metrics->req_clients_current_arr.ipv4_size - 1], 
                        REQ_CLIENT_MAX_LEN, "%s", line_parsed->req_client);
                }
            }
        }
    }

    /* Extract request method */
    if(parser_config->chart_config & CHART_REQ_METHODS){
        if(!strcmp(line_parsed->req_method, "ACL")) metrics->req_method.acl++;
        else if(!strcmp(line_parsed->req_method, "BASELINE-CONTROL")) metrics->req_method.baseline_control++;
        else if(!strcmp(line_parsed->req_method, "BIND")) metrics->req_method.bind++;
        else if(!strcmp(line_parsed->req_method, "CHECKIN")) metrics->req_method.checkin++;
        else if(!strcmp(line_parsed->req_method, "CHECKOUT")) metrics->req_method.checkout++;
        else if(!strcmp(line_parsed->req_method, "CONNECT")) metrics->req_method.connect++;
        else if(!strcmp(line_parsed->req_method, "COPY")) metrics->req_method.copy++;
        else if(!strcmp(line_parsed->req_method, "DELETE")) metrics->req_method.delet++;
        else if(!strcmp(line_parsed->req_method, "GET")) metrics->req_method.get++;
        else if(!strcmp(line_parsed->req_method, "HEAD")) metrics->req_method.head++;
        else if(!strcmp(line_parsed->req_method, "LABEL")) metrics->req_method.label++;
        else if(!strcmp(line_parsed->req_method, "LINK")) metrics->req_method.link++;
        else if(!strcmp(line_parsed->req_method, "LOCK")) metrics->req_method.lock++;
        else if(!strcmp(line_parsed->req_method, "MERGE")) metrics->req_method.merge++;
        else if(!strcmp(line_parsed->req_method, "MKACTIVITY")) metrics->req_method.mkactivity++;
        else if(!strcmp(line_parsed->req_method, "MKCALENDAR")) metrics->req_method.mkcalendar++;
        else if(!strcmp(line_parsed->req_method, "MKCOL")) metrics->req_method.mkcol++;
        else if(!strcmp(line_parsed->req_method, "MKREDIRECTREF")) metrics->req_method.mkredirectref++;
        else if(!strcmp(line_parsed->req_method, "MKWORKSPACE")) metrics->req_method.mkworkspace++;
        else if(!strcmp(line_parsed->req_method, "MOVE")) metrics->req_method.move++;
        else if(!strcmp(line_parsed->req_method, "OPTIONS")) metrics->req_method.options++;
        else if(!strcmp(line_parsed->req_method, "ORDERPATCH")) metrics->req_method.orderpatch++;
        else if(!strcmp(line_parsed->req_method, "PATCH")) metrics->req_method.patch++;
        else if(!strcmp(line_parsed->req_method, "POST")) metrics->req_method.post++;
        else if(!strcmp(line_parsed->req_method, "PRI")) metrics->req_method.pri++;
        else if(!strcmp(line_parsed->req_method, "PROPFIND")) metrics->req_method.propfind++;
        else if(!strcmp(line_parsed->req_method, "PROPPATCH")) metrics->req_method.proppatch++;
        else if(!strcmp(line_parsed->req_method, "PUT")) metrics->req_method.put++;
        else if(!strcmp(line_parsed->req_method, "REBIND")) metrics->req_method.rebind++;
        else if(!strcmp(line_parsed->req_method, "REPORT")) metrics->req_method.report++;
        else if(!strcmp(line_parsed->req_method, "SEARCH")) metrics->req_method.search++;
        else if(!strcmp(line_parsed->req_method, "TRACE")) metrics->req_method.trace++;
        else if(!strcmp(line_parsed->req_method, "UNBIND")) metrics->req_method.unbind++;
        else if(!strcmp(line_parsed->req_method, "UNCHECKOUT")) metrics->req_method.uncheckout++;
        else if(!strcmp(line_parsed->req_method, "UNLINK")) metrics->req_method.unlink++;
        else if(!strcmp(line_parsed->req_method, "UNLOCK")) metrics->req_method.unlock++;
        else if(!strcmp(line_parsed->req_method, "UPDATE")) metrics->req_method.update++;
        else if(!strcmp(line_parsed->req_method, "UPDATEREDIRECTREF")) metrics->req_method.updateredirectref++;
    }

    /* Extract request protocol */
    if(parser_config->chart_config & CHART_REQ_PROTO){
        if(!strcmp(line_parsed->req_proto, "1") || !strcmp(line_parsed->req_proto, "1.0")) metrics->req_proto.http_1++;
        else if(!strcmp(line_parsed->req_proto, "1.1")) metrics->req_proto.http_1_1++;
        else if(!strcmp(line_parsed->req_proto, "2") || !strcmp(line_parsed->req_proto, "2.0")) metrics->req_proto.http_2++;
        else metrics->req_proto.other++;
    }

    /* Extract bytes received and sent */
    if(parser_config->chart_config & CHART_BANDWIDTH){
        metrics->bandwidth.req_size += line_parsed->req_size;
        metrics->bandwidth.resp_size += line_parsed->resp_size;
    }

    /* Extract request processing time */
    if((parser_config->chart_config & CHART_REQ_PROC_TIME) && line_parsed->req_proc_time){
        if(line_parsed->req_proc_time < metrics->req_proc_time.min || metrics->req_proc_time.min == 0) metrics->req_proc_time.min = line_parsed->req_proc_time;
        if(line_parsed->req_proc_time > metrics->req_proc_time.max || metrics->req_proc_time.max == 0) metrics->req_proc_time.max = line_parsed->req_proc_time;
        metrics->req_proc_time.sum += line_parsed->req_proc_time;
        metrics->req_proc_time.count++;
    }

    /* Extract response code family, response code & response code type */
    if(parser_config->chart_config & (CHART_RESP_CODE_FAMILY | CHART_RESP_CODE | CHART_RESP_CODE_TYPE)){
        switch(line_parsed->resp_code / 100){
            /* Note: 304 and 401 should be treated as resp_success */
            case 1:
                metrics->resp_code_family.resp_1xx++;
                metrics->resp_code[line_parsed->resp_code - 100]++;
                metrics->resp_code_type.resp_success++;
                break;
            case 2:
                metrics->resp_code_family.resp_2xx++;
                metrics->resp_code[line_parsed->resp_code - 100]++;
                metrics->resp_code_type.resp_success++;
                break;
            case 3:
                metrics->resp_code_family.resp_3xx++;
                metrics->resp_code[line_parsed->resp_code - 100]++;
                if(line_parsed->resp_code == 304) metrics->resp_code_type.resp_success++;
                else metrics->resp_code_type.resp_redirect++;
                break;
            case 4:
                metrics->resp_code_family.resp_4xx++;
                metrics->resp_code[line_parsed->resp_code - 100]++;
                if(line_parsed->resp_code == 401) metrics->resp_code_type.resp_success++;
                else metrics->resp_code_type.resp_bad++;
                break;
            case 5:
                metrics->resp_code_family.resp_5xx++;
                metrics->resp_code[line_parsed->resp_code - 100]++;
                metrics->resp_code_type.resp_error++;
                break;
            default:
                metrics->resp_code_family.other++;
                metrics->resp_code[500]++;
                metrics->resp_code_type.other++;
                break;
        }
    }

    /* Extract SSL protocol */
    if(parser_config->chart_config & CHART_SSL_PROTO){
        if(!strcmp(line_parsed->ssl_proto, "TLSv1")) metrics->ssl_proto.tlsv1++;
        else if(!strcmp(line_parsed->ssl_proto, "TLSv1.1")) metrics->ssl_proto.tlsv1_1++;
        else if(!strcmp(line_parsed->ssl_proto, "TLSv1.2")) metrics->ssl_proto.tlsv1_2++;
        else if(!strcmp(line_parsed->ssl_proto, "TLSv1.3")) metrics->ssl_proto.tlsv1_3++;
        else if(!strcmp(line_parsed->ssl_proto, "SSLv2")) metrics->ssl_proto.sslv2++;
        else if(!strcmp(line_parsed->ssl_proto, "SSLv3")) metrics->ssl_proto.sslv3++;
        else metrics->ssl_proto.other++;
    }

    /* Extract SSL cipher suite */
    // TODO: Reduce number of reallocs
    if((parser_config->chart_config & CHART_SSL_CIPHER) && line_parsed->ssl_cipher && *line_parsed->ssl_cipher){
        int i;
        for(i = 0; i < metrics->ssl_cipher_arr.size; i++){
            if(!strcmp(metrics->ssl_cipher_arr.ssl_ciphers[i].string, line_parsed->ssl_cipher)){
                metrics->ssl_cipher_arr.ssl_ciphers[i].count++;
                break;
            }
        }
        if(metrics->ssl_cipher_arr.size == i){ // SSL cipher suite not found in array - need to append
            metrics->ssl_cipher_arr.size++;
            metrics->ssl_cipher_arr.ssl_ciphers = reallocz(metrics->ssl_cipher_arr.ssl_ciphers, metrics->ssl_cipher_arr.size * sizeof(struct log_parser_metrics_ssl_cipher));
            snprintf(metrics->ssl_cipher_arr.ssl_ciphers[metrics->ssl_cipher_arr.size - 1].string, SSL_CIPHER_SUITE_MAX_LEN, "%s", line_parsed->ssl_cipher);
            metrics->ssl_cipher_arr.ssl_ciphers[metrics->ssl_cipher_arr.size - 1].count = 1;
        }
    }
}

Log_parser_metrics_t parse_text_buf(Log_parser_buffs_t *parser_buffs, char *text, size_t text_size, Log_parser_config_t *parser_config, const int verify){
    Log_parser_metrics_t metrics = {0};
    metrics.vhost_arr.vhosts = NULL;
    if(!text_size || !text || !*text) return metrics;

    char *line_start = text, *line_end = text;
    while(line_end = strchr(line_start, '\n')){

        #if MEASURE_PARSE_TEXT_TIME
        struct timespec begin, end;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &begin);
        struct rusage begin_rusage, end_rusage;
        if(getrusage(1, &begin_rusage) !=0) exit(-1);
        #endif

        size_t line_len = (size_t) (line_end - line_start);
        
        if(!parser_buffs->line || (line_len + 1) > parser_buffs->line_len_max){
            parser_buffs->line_len_max = (line_len + 1) * LOG_PARSER_BUFFS_LINE_REALLOC_SCALE_FACTOR;
            parser_buffs->line = reallocz(parser_buffs->line, parser_buffs->line_len_max);
        }
        if(!parser_buffs->line) fatal("Fatal when extracting line from text buffer in parse_text_buf()");

        memcpy(parser_buffs->line, line_start, line_len);
        parser_buffs->line[line_len] = '\0';
        // fprintf(stderr, "line:%s\n", parser_buffs->line);
        
        Log_line_parsed_t *line_parsed = parse_log_line(parser_config, parser_buffs, parser_buffs->line, verify);
        // TODO: Error handling in case line_parsed == NULL !!
        
        // TODO: Refactor the following, can be done inside parse_log_line() function to save a strcmp() call.
        extract_metrics(parser_config, line_parsed, &metrics);
        
        freez(line_parsed->req_URL);

        line_start = line_end + 1;
        
        #if MEASURE_PARSE_TEXT_TIME
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
        if(getrusage(1, &end_rusage) != 0) exit(-1);
        fprintf (stderr, "R:%.9lfs T:%.9lfs\n", (
            end_rusage.ru_stime.tv_usec 
            + end_rusage.ru_utime.tv_usec 
            - begin_rusage.ru_stime.tv_usec 
            - begin_rusage.ru_utime.tv_usec) / 1000000.0 
        + (double) (end_rusage.ru_stime.tv_sec 
            + end_rusage.ru_utime.tv_sec  
            - begin_rusage.ru_stime.tv_sec 
            - begin_rusage.ru_utime.tv_sec), 
        (end.tv_nsec - begin.tv_nsec) / 1000000000.0 + (end.tv_sec - begin.tv_sec));
        #endif
        
    }

    //fprintf(stderr, "NDLGS Total numLines: %lld\n", metrics.num_lines);
    return metrics;
}

Log_parser_config_t *auto_detect_parse_config(Log_parser_buffs_t *parser_buffs, const char delimiter){
    for(int i = 0; csv_auto_format_guess_matrix[i] != NULL; i++){
        fprintf(stderr, "Auto detection iteration: %d\n", i);
        Log_parser_config_t *parser_config = read_parse_config(csv_auto_format_guess_matrix[i], delimiter);
        Log_line_parsed_t *line_parsed = parse_log_line(parser_config, parser_buffs, parser_buffs->line, 1);
        if(line_parsed){
            freez(line_parsed->req_URL);
            fprintf(stderr, "Auto-detection errors: %d iter:%d\n", line_parsed->parsing_errors, i);
            if(line_parsed->parsing_errors == 0){
                fprintf(stderr, "Auto detected log format (iter:%d):%s\n", i, csv_auto_format_guess_matrix[i]);
                return parser_config;
            }
        }
        freez(parser_config->fields);
        freez(parser_config);
    }
    return NULL;
}