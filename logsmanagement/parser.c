/** @file parser.c
 *  @brief API to parse and search logs
 *
 *  @author Dimitris Pantazis
 */

#include <stdio.h>
#include <string.h>
#include "helper.h"
#include <regex.h> 

#define REGEX_SIZE 100U /**< Max size of regular expression in bytes **/

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