/** @file parser.c
 *  @brief API to parse and search logs
 *
 *  @author Dimitris Pantazis
 */

#include <stdio.h>
#include <string.h>
#include "helper.h"
#include <regex.h> 

#define MAX_GROUPS 1U // Change to 2 to get subexpression position
#define REGEX_SIZE 100U /**< Max size of regular expression in bytes **/

/**
 * @brief Search a buffer for a keyword
 * @details Search the source buffer for a keyword and copy matches to the destination buffer
 * @param src The source buffer to be searched
 * @param dest The destination buffer where the results will be written in
 * @param keyword The keyword to be searched for in the source buffer
 * @param ignore_case Case insensitive search if 1, it does not matter if keyword characters 
 * are upper or lower case. 
 */
void search_keyword(char *src, char *dest, const char *keyword, const int ignore_case){
	char regexString[REGEX_SIZE];
	snprintf(regexString, REGEX_SIZE, ".*(%s).*", "error");

	regex_t regex_compiled;
	regmatch_t groupArray[MAX_GROUPS];
	int regex_flags = ignore_case ? REG_EXTENDED | REG_NEWLINE | REG_ICASE : REG_EXTENDED | REG_NEWLINE;

	if (regcomp(&regex_compiled, regexString, regex_flags)){
		fprintf_log(LOGS_MANAG_ERROR, stderr, "Could not compile regular expression.\n");
		fatal("Could not compile regular expression.\n");
	};

	size_t dest_off = 0;
	char *cursor = src;
	for (int m = 0; ; m++){
		if (regexec(&regex_compiled, cursor, MAX_GROUPS, groupArray, 0)) break;  // No more matches

		unsigned int g = 0;
		unsigned int offset = 0;
		for (g = 0; g < MAX_GROUPS; g++){
			if (groupArray[g].rm_so == (size_t)-1) break;  // No more groups

			if (g == 0) offset = groupArray[g].rm_eo;

			char cursorCopy[strlen(cursor) + 1];
			strcpy(cursorCopy, cursor);
			cursorCopy[groupArray[g].rm_eo] = 0;
			fprintf_log(LOGS_MANAG_INFO, stderr, "Match %u, Group %u: [%2u-%2u]: %s\n",
				m, g, groupArray[g].rm_so, groupArray[g].rm_eo,
				cursorCopy + groupArray[g].rm_so);
			strcpy(&dest[dest_off], cursorCopy + groupArray[g].rm_so);
	        dest_off += groupArray[g].rm_eo - groupArray[g].rm_so;
	        dest[dest_off++] = '\n';
		}
		cursor += offset;
	}

	regfree(&regex_compiled);

	fprintf_log(LOGS_MANAG_INFO, stderr, "Searching for keyword: %s \n=====***********=====\n%s\n=====***********=====\n", keyword, src);
	fprintf_log(LOGS_MANAG_INFO, stderr, "Results of search\n=====***********=====\n%s\n=====***********=====\n", dest);
}