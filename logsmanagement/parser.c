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
    Log_parser_config_t *parser_config = mallocz(sizeof(Log_parser_config_t));
    parser_config->num_fields = num_fields;
    parser_config->delimiter = delimiter;
    
    char **parsed_format = parse_csv(log_format, delimiter, num_fields); // parsed_format is NULL-terminated
    parser_config->fields = calloc(num_fields, sizeof(log_line_field_t));
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
			continue;
		}

		fprintf(stderr, "UNKNOWN OR CUSTOM\n");
		parser_config->fields[fields_off++] = CUSTOM;
		continue;

	}

    freez(log_format);
    for(int i = 0; parsed_format[i] != NULL; i++) freez(parsed_format[i]);
}