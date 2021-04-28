/** @file parser.c
 *  @brief API to parse and search logs
 *
 *  @author Dimitris Pantazis
 */

#include <stdio.h>
#include <string.h>
#include "helper.h"
#include <regex.h> 
#include "parser.h"
#define __USE_XOPEN // required by strptime
#include <time.h>
#include <sys/time.h>

static regex_t vhost_regex, req_client_regex, cipher_suite_regex;
static int regexs_initialised = 0;

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
    if (s[0] == '\0' || isspace(s[0]))
        return STR2XX_INCONVERTIBLE;
    errno = 0;
    long l = strtol(s, &end, base);
    /* Both checks are needed because INT_MAX == LONG_MAX is possible. */
    if (l > INT_MAX || (errno == ERANGE && l == LONG_MAX))
        return STR2XX_OVERFLOW;
    if (l < INT_MIN || (errno == ERANGE && l == LONG_MIN))
        return STR2XX_UNDERFLOW;
    if (*end != '\0')
        return STR2XX_INCONVERTIBLE;
    *out = l;
    return STR2XX_SUCCESS;
}

static inline str2xx_errno str2float(float *out, char *s) {
    char *end;
    if (s[0] == '\0' || isspace(s[0]))
        return STR2XX_INCONVERTIBLE;
    errno = 0;
    float f = strtof(s, &end);
    /* Both checks are needed because INT_MAX == LONG_MAX is possible. */
    if (errno == ERANGE && f == HUGE_VALF)
        return STR2XX_OVERFLOW;
    if (errno == ERANGE && f == -HUGE_VALF)
        return STR2XX_UNDERFLOW;
    if (*end != '\0')
        return STR2XX_INCONVERTIBLE;
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
        free( buf );
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
        	*bptr = strdup( tmp );

        	if ( !*bptr ) {
        		for ( bptr--; bptr >= buf; bptr-- ) {
        			free( *bptr );
        		}
        		free( buf );
        		free( tmp );

        		return NULL;
        	}

        	bptr++;
        	tptr = tmp;
        	break;
        }
        else if(*ptr == delimiter){
        	*tptr = '\0';
        	*bptr = strdup( tmp );

        	if ( !*bptr ) {
        		for ( bptr--; bptr >= buf; bptr-- ) {
        			free( *bptr );
        		}
        		free( buf );
        		free( tmp );

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
                *bptr = strdup( tmp );

                if ( !*bptr ) {
                    for ( bptr--; bptr >= buf; bptr-- ) {
                        free( *bptr );
                    }
                    free( buf );
                    free( tmp );

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
    free( tmp );
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
    
    char **parsed_format = parse_csv(log_format, delimiter, num_fields); // parsed_format is NULL-terminated
    parser_config->fields = callocz(num_fields, sizeof(log_line_field_t));
    unsigned int fields_off = 0;

    for(int i = 0; i < num_fields; i++ ){
		fprintf(stderr, "Field %d (%s) is ", i, parsed_format[i]);

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

		if(strcmp(parsed_format[i], "$host:$server_port") == 0 || 
		   strcmp(parsed_format[i], "%v:%p") == 0) {
			fprintf(stderr, "VHOST_WITH_PORT\n");
			parser_config->fields[fields_off++] = VHOST_WITH_PORT;
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
		   strcmp(parsed_format[i], "%i") == 0) {
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

		if(strcmp(parsed_format[i], "$time_local") == 0 ||
		   strcmp(parsed_format[i], "%t") == 0) {
			fprintf(stderr, "TIME\n");
			parser_config->fields[fields_off++] = TIME;
            parser_config->fields[fields_off++] = TIME; // TIME takes 2 fields
			continue;
		}

		fprintf(stderr, "UNKNOWN OR CUSTOM\n");
		parser_config->fields[fields_off++] = CUSTOM;
		continue;

	}

    freez(log_format);
    for(int i = 0; parsed_format[i] != NULL; i++) freez(parsed_format[i]);
    return parser_config;
}

#define DISABLE_PARSE_LOG_LINE_FPRINTS 0
static Log_line_parsed_t *parse_log_line(log_line_field_t *fields_format, const int num_fields_format, const char *line, const char delimiter, const int verify){
    Log_line_parsed_t *log_line_parsed = callocz(1, sizeof(Log_line_parsed_t));
#if DISABLE_PARSE_LOG_LINE_FPRINTS
    fprintf(stderr, "Original line: %s\n", line);
#endif
    int num_fields = count_fields(line, delimiter);
    char **parsed = parse_csv(line, delimiter, num_fields);
#if DISABLE_PARSE_LOG_LINE_FPRINTS
    fprintf(stderr, "Number of items: %d\n", num_fields);
#endif
    // int parsed_offset = 0;
    for(int i = 0; i < num_fields_format; i++ ){
        #if DISABLE_PARSE_LOG_LINE_FPRINTS
        fprintf(stderr, "===\nField %d:%s\n", i, parsed[i]);
        #endif

        if(fields_format[i] == VHOST_WITH_PORT && strcmp(parsed[i], "-")){
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: VHOST_WITH_PORT):%s\n", i, parsed[i]);
            #endif
            char *sep = strrchr(parsed[i], ':');
            if(sep) *sep = '\0';
            else parsed[i][0] = '\0';
        }

        if((fields_format[i] == VHOST_WITH_PORT || fields_format[i] == VHOST) && strcmp(parsed[i], "-")){
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: VHOST):%s\n", i, parsed[i]);
            #endif
            // nginx $host and $http_host return ipv6 in [], apache doesn't
            // TODO: TEST! This case hasn't been tested!
            char *pch = strchr(parsed[i], ']');
            if(pch){
                //parsed[i][strcspn(parsed[i], "]")] = 0;
                *pch = '\0';
                memmove(parsed[i], parsed[i]+1, strlen(parsed[i]));
            }
            if(verify){
                int rc = regexec(&vhost_regex, parsed[i], 0, NULL, 0);
                if(!rc) log_line_parsed->vhost = strdup(parsed[i]);
                else if (rc == REG_NOMATCH) fprintf(stderr, "VHOST is invalid\n");
                else assert(0); 
            }
            else log_line_parsed->vhost = strdup(parsed[i]);
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted VHOST:%s\n", log_line_parsed->vhost);
            #endif
        }

        if((fields_format[i] == VHOST_WITH_PORT || fields_format[i] == PORT) && strcmp(parsed[i], "-")){
            char *port;
            if(fields_format[i] == VHOST_WITH_PORT) port = &parsed[i][strlen(parsed[i]) + 1];
            else port = parsed[i];
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: PORT):%s\n", i, port);
            #endif
            if(str2int(&log_line_parsed->port, port, 10) == STR2XX_SUCCESS){
                if(verify){
                    if(log_line_parsed->port < 80 || log_line_parsed->port > 49151){
                        log_line_parsed->port = 0;
                        fprintf(stderr, "PORT is invalid (<80 or >49151)\n");
                    }
                }
            }
            else fprintf(stderr, "Error while extracting PORT from string\n");
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted PORT:%d\n", log_line_parsed->port);
            #endif
        }

        if(fields_format[i] == REQ_SCHEME && strcmp(parsed[i], "-")){
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_SCHEME):%s\n", i, parsed[i]);
            #endif
            snprintf(log_line_parsed->req_scheme, REQ_SCHEME_MAX_LEN, "%s", parsed[i]); 
            if(verify){
                if(strlen(parsed[i]) >= REQ_SCHEME_MAX_LEN || 
                    (strcmp(log_line_parsed->req_scheme, "http") && 
                     strcmp(log_line_parsed->req_scheme, "https"))){
                    fprintf(stderr, "REQ_SCHEME is invalid (must be either 'http' or 'https')\n");
                    log_line_parsed->req_scheme[0] = '\0';
                }
            }
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted REQ_SCHEME:%s\n", log_line_parsed->req_scheme);
            #endif
        }

        if(fields_format[i] == REQ_CLIENT && strcmp(parsed[i], "-")){
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_CLIENT):%s\n", i, parsed[i]);
            #endif
            if(verify){
                int rc = regexec(&req_client_regex, parsed[i], 0, NULL, 0);
                if(!rc) log_line_parsed->req_client = strdup(parsed[i]);
                else if (rc == REG_NOMATCH) fprintf(stderr, "REQ_CLIENT is invalid\n");
                else assert(0); // Can also use: regerror(rc, &req_client_regex, msgbuf, sizeof(msgbuf));
            }
            else log_line_parsed->req_client = strdup(parsed[i]);
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted REQ_CLIENT:%s\n", log_line_parsed->req_client);
            #endif
        }

        char *req_first_sep, *req_last_sep = NULL;
        if(fields_format[i] == REQ && strcmp(parsed[i], "-")){
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ):%s\n", i, parsed[i]);
            #endif
            req_first_sep = strchr(parsed[i], ' ');
            req_last_sep = strrchr(parsed[i], ' ');
            if(!req_first_sep || req_first_sep == req_last_sep) parsed[i][0] = '\0';
            else *req_first_sep = *req_last_sep = '\0';
        }

        if((fields_format[i] == REQ || fields_format[i] == REQ_METHOD) && strcmp(parsed[i], "-")){
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_METHOD):%s\n", i, parsed[i]);
            #endif
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
                    fprintf(stderr, "REQ_METHOD is invalid\n");
                    log_line_parsed->req_method[0] = '\0';
                }
            }
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted REQ_METHOD:%s\n", log_line_parsed->req_method);
            #endif
        }

        if((fields_format[i] == REQ || fields_format[i] == REQ_URL) && strcmp(parsed[i], "-")){
            if(fields_format[i] == REQ){ 
                log_line_parsed->req_URL = req_first_sep ? strdup(req_first_sep + 1) : NULL;
            }   
            else log_line_parsed->req_URL = strdup(parsed[i]);
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_URL):%s\n", i, log_line_parsed->req_URL);   
            //if(verify){} ??
            fprintf(stderr, "Extracted REQ_URL:%s\n", log_line_parsed->req_URL);
            #endif
        }

        if((fields_format[i] == REQ || fields_format[i] == REQ_PROTO) && strcmp(parsed[i], "-")){
            char *req_proto = NULL;
            if(fields_format[i] == REQ){ 
                req_proto = req_last_sep ? req_last_sep + 1 : NULL;
            }
            else req_proto = parsed[i];
            if(verify){
                if(!req_proto || !*req_proto || strlen(req_proto) < 6 || strncmp(req_proto, "HTTP/", 5) || 
                    (strcmp(&req_proto[5], "1") && 
                     strcmp(&req_proto[5], "1.0") && 
                     strcmp(&req_proto[5], "1.1") && 
                     strcmp(&req_proto[5], "2") && 
                     strcmp(&req_proto[5], "2.0"))){
                    fprintf(stderr, "REQ_PROTO is invalid\n");
                    log_line_parsed->req_proto[0] = '\0';
                }
                else snprintf(log_line_parsed->req_proto, REQ_PROTO_MAX_LEN, "%s", &req_proto[5]); 
            }
            else snprintf(log_line_parsed->req_proto, REQ_PROTO_MAX_LEN, "%s", &req_proto[5]); 
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_PROTO):%s\n", i, req_proto);
            fprintf(stderr, "Extracted REQ_PROTO:%s\n", log_line_parsed->req_proto);
            #endif
        }

        if(fields_format[i] == REQ_SIZE){
            /* TODO: Differentiate between '-' or 0 and an invalid request size. 
             * right now, all these will set req_size == 0 */
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_SIZE):%s\n", i, parsed[i]);
            #endif
            if(!strcmp(parsed[i], "-")) log_line_parsed->req_size = 0; // Request size can be '-' 
            else if(str2int(&log_line_parsed->req_size, parsed[i], 10) == STR2XX_SUCCESS){
                if(verify){
                    if(log_line_parsed->req_size < 0){
                        log_line_parsed->req_size = 0;
                        fprintf(stderr, "REQ_SIZE is invalid (<0)\n");
                    }
                }
            }
            else fprintf(stderr, "Error while extracting REQ_SIZE from string\n");
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted REQ_SIZE:%d\n", log_line_parsed->req_size);
            #endif
        }

        if(fields_format[i] == REQ_PROC_TIME && strcmp(parsed[i], "-")){
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: REQ_PROC_TIME):%s\n", i, parsed[i]);
            #endif
            if(strchr(parsed[i], '.')){ // nginx time is in seconds with a milliseconds resolution.
                float f = 0;
                if(str2float(&f, parsed[i]) == STR2XX_SUCCESS) log_line_parsed->req_proc_time = (int) (f * 1.0E6);
                else fprintf(stderr, "Error while extracting REQ_PROC_TIME from string\n");
            }
            else{ // apache time is in microseconds
                if(str2int(&log_line_parsed->req_proc_time, parsed[i], 10) != STR2XX_SUCCESS)
                    fprintf(stderr, "Error while extracting REQ_PROC_TIME from string\n");
            }
            if(verify){
                if(log_line_parsed->req_proc_time < 0){
                    log_line_parsed->req_proc_time = 0;
                    fprintf(stderr, "REQ_PROC_TIME is invalid (<0)\n");
                }
            }
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted REQ_PROC_TIME:%d\n", log_line_parsed->req_proc_time);
            #endif
        }

        if(fields_format[i] == RESP_CODE && strcmp(parsed[i], "-")){
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: RESP_CODE):%s\n", i, parsed[i]);
            #endif
            if(str2int(&log_line_parsed->resp_code, parsed[i], 10) == STR2XX_SUCCESS){  
                if(verify){
                    // rfc7231
                    // Informational responses (100–199),
                    // Successful responses (200–299),
                    // Redirects (300–399),
                    // Client errors (400–499),
                    // Server errors (500–599).
                    if(log_line_parsed->resp_code < 100 || log_line_parsed->resp_code > 599){
                        log_line_parsed->resp_code = 0;
                        fprintf(stderr, "RESP_CODE is invalid (<100 or >600)\n");
                    }
                }
            }
            else fprintf(stderr, "Error while extracting RESP_CODE from string\n");
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted RESP_CODE:%d\n", log_line_parsed->resp_code);
            #endif
        }

        if(fields_format[i] == RESP_SIZE){
            /* TODO: Differentiate between '-' or 0 and an invalid request size. 
             * right now, all these will set resp_size == 0 */
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: RESP_SIZE):%s\n", i, parsed[i]);
            #endif
            if(!strcmp(parsed[i], "-")) log_line_parsed->resp_size = 0; // Request size can be '-' 
            else if(str2int(&log_line_parsed->resp_size, parsed[i], 10) == STR2XX_SUCCESS){
                if(verify){
                    if(log_line_parsed->resp_size < 0){
                        log_line_parsed->resp_size = 0;
                        fprintf(stderr, "RESP_SIZE is invalid (<0)\n");
                    }
                }
            }
            else fprintf(stderr, "Error while extracting RESP_SIZE from string\n");
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted RESP_SIZE:%d\n", log_line_parsed->resp_size);
            #endif
        }

        if(fields_format[i] == UPS_RESP_TIME && strcmp(parsed[i], "-")){
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: UPS_RESP_TIME):%s\n", i, parsed[i]);
            #endif
            /* Times of several responses are separated by commas and colons. Following the 
             * Go parser implementation, where only the first one is kept, the others are 
             * discarded. Also, there must be no space in between them. Needs testing... */
            char *pch = strchr(parsed[i], ',');
            if(pch) *pch = '\0';

            if(strchr(parsed[i], '.')){ // nginx time is in seconds with a milliseconds resolution.
                float f = 0;
                if(str2float(&f, parsed[i]) == STR2XX_SUCCESS) log_line_parsed->ups_resp_time = (int) (f * 1.0E6);
                else fprintf(stderr, "Error while extracting UPS_RESP_TIME from string\n");
            }
            else{ // unlike in the REQ_PROC_TIME case, apache doesn't have an equivalent here
                fprintf(stderr, "Error while extracting UPS_RESP_TIME from string\n");
            }
            if(verify){
                if(log_line_parsed->ups_resp_time < 0){
                    log_line_parsed->ups_resp_time = 0;
                    fprintf(stderr, "UPS_RESP_TIME is invalid (<0)\n");
                }
            }
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted UPS_RESP_TIME:%d\n", log_line_parsed->ups_resp_time);
            #endif
        }

        if(fields_format[i] == SSL_PROTO && strcmp(parsed[i], "-")){
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: SSL_PROTO):%s\n", i, parsed[i]);
            #endif
            if(verify){
                if(strcmp(parsed[i], "TLSv1") && 
                     strcmp(parsed[i], "TLSv1.1") &&
                     strcmp(parsed[i], "TLSv1.2") &&
                     strcmp(parsed[i], "TLSv1.3") &&
                     strcmp(parsed[i], "SSLv2") &&
                     strcmp(parsed[i], "SSLv3")){
                    fprintf(stderr, "SSL_PROTO is invalid\n");
                    log_line_parsed->ssl_proto[0] = '\0';
                }
                else snprintf(log_line_parsed->ssl_proto, REQ_PROTO_MAX_LEN, "%s", parsed[i]); 
            }
            else snprintf(log_line_parsed->ssl_proto, REQ_PROTO_MAX_LEN, "%s", parsed[i]); 
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted SSL_PROTO:%s\n", log_line_parsed->ssl_proto);
            #endif
        }

        if(fields_format[i] == SSL_CIPHER_SUITE && strcmp(parsed[i], "-")){
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Item %d (type: SSL_CIPHER_SUITE):%s\n", i, parsed[i]);
            #endif
            if(verify){
                if(!strchr(parsed[i], '-') && !strchr(parsed[i], '_')){
                    fprintf(stderr, "SSL_CIPHER_SUITE is invalid\n");
                    log_line_parsed->ssl_cipher_suite[0] = '\0';
                } 
                else snprintf(log_line_parsed->ssl_cipher_suite, SSL_CIPHER_SUITE_MAX_LEN, "%s", parsed[i]); 
            }
            else snprintf(log_line_parsed->ssl_cipher_suite, SSL_CIPHER_SUITE_MAX_LEN, "%s", parsed[i]); 
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted SSL_CIPHER_SUITE:%s\n", log_line_parsed->ssl_cipher_suite);
            #endif
        }

        if(fields_format[i] == TIME && strcmp(parsed[i], "-")){
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Items %d + %d (type: TIME - 2 fields):%s %s\n", i, i+1, parsed[i], parsed[i+1]);
            #endif
            char *pch = strchr(parsed[i], '[');
            if(pch) memmove(parsed[i], parsed[i]+1, strlen(parsed[i])); //%d/%b/%Y:%H:%M:%S %z

            struct tm ltm = {0};
            strptime(parsed[i], "%d/%b/%Y:%H:%M:%S", &ltm);
            log_line_parsed->timestamp = mktime(&ltm);


            // Deal with 2nd part of datetime i.e. timezone
            //TODO: Error handling in case of parsed[++i] is not timezone??
            pch = strchr(parsed[++i], ']');
            if(pch) *pch = '\0';
            int timezone = strtol(parsed[i], NULL, 10);
            int timezone_h = timezone / 100;
            int timezone_m = timezone % 100;
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Timezone: int:%d, hrs:%d, mins:%d\n", timezone, timezone_h, timezone_m);
            #endif

            log_line_parsed->timestamp = mktime(&ltm) + timezone_h * 3600 + timezone_m * 60;
            #if DISABLE_PARSE_LOG_LINE_FPRINTS
            fprintf(stderr, "Extracted TIME:%lu\n", log_line_parsed->timestamp);
            #endif

            //if(verify){} ??
        }



        //parsed_offset++;
    }

    free_csv_line(parsed);
    return log_line_parsed;
}

Log_parser_metrics_t parse_text_buf(char *text, size_t text_size, log_line_field_t *fields, int num_fields, const char delimiter, const int verify){
    Log_parser_metrics_t metrics = {0};
    if(!text_size || !text || !*text) return metrics;

    char *line_start = text, *line_end = text;
    while(line_end = strchr(line_start, '\n')){

        char *line = strndup(line_start, (size_t) (line_end - line_start));
        if(!line) fatal("Fatal when extracting line from text buffer in parse_text_buf()");
        Log_line_parsed_t *line_parsed = parse_log_line(fields, num_fields, line, delimiter, verify);
        // TODO: Refactor the following, can be done inside parse_log_line() function to save a strcmp() call.
        if(!strcmp(line_parsed->req_method, "GET")) metrics.req_method.get++;
        if(!strcmp(line_parsed->req_method, "POST")) metrics.req_method.post++;
        freez(line_parsed->vhost);
        freez(line_parsed->req_client);
        freez(line_parsed->req_URL);
        freez(line_parsed);
        free(line); // WARNING! use free() not freez() here!

        line_start = line_end + 1;
        metrics.num_lines++;
        
    }

    fprintf(stderr, "NDLGS Total numLines: %lld\n", metrics.num_lines);
    return metrics;
}