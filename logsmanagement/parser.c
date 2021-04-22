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

int count_fields(const char *line, const char delimiter){
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
char **parse_csv( const char *line, const char delimiter, int num_fields) {
    char **buf, **bptr, *tmp, *tptr;
    const char *ptr;
    int fQuote, fEnd;

    if(num_fields < 0){
	    num_fields = count_fields(line, delimiter);

	    if ( num_fields == -1 ) {
	        return NULL;
	    }
	}

    buf = malloc( sizeof(char*) * (num_fields+1) );

    if ( !buf ) {
        return NULL;
    }

    tmp = malloc( strlen(line) + 1 );

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

void parse_config_str(char *log_format, const char delimiter, struct File_info *p_file_info){
	int num_fields = count_fields(log_format, delimiter);
    char **parsed_format = parse_csv(log_format, delimiter, num_fields);
    if(num_fields > 0){
        p_file_info->parser_config = mallocz(Log_parser_config_t);
        p_file_info->parse_config->num_fields = num_fields;
    }
    
    log_line_field_t *fields = calloc(num_fields, sizeof(log_line_field_t));
    unsigned int fields_off = 0;

    freez(log_format);
    // TODO: Free each element of array **parsed_format
}