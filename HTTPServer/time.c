#include "headers.h"

/*!
 * parses a date encoded in 'buffer' to a standard ANSI C date/time representation
 * accepted standars: RFC-822, RFC-850, ANSI C asctime()
 * @param buffer Pointer to memory containing a date in one of the forms mentioned above
 * @param date Pointer to memory where the standarized date/time structure will be saved
 */
void parseDate(const char *buffer, struct tm *date) {

	int len;

	char firstElement[20];
	sscanf(buffer, "%s", firstElement);
	len = strlen(firstElement);

	switch (len) {

	/* RFC 822, updated by RFC 1123; firstElement "[wkday]," */
	case 4:
		strptime(buffer, "%a, %d %b %Y %T GMT", date);
		break;

		/*  ANSI C's asctime() format; firstElement "[wkday]" */
	case 3:
		strptime(buffer, "%a %b %d %T %Y", date);
		break;

		/* RFC 850, obsoleted by RFC 1036; firstElement "[weekdey],
		 * " */
	default:
		strptime(buffer, "%A, %d-%b-%y %T GMT", date);
	}

}

/*!
 * Get files modification date
 * @param path Absolute or relative file path
 * @param date Pointer to memory where date/time structure will be saved
 * @return 1 if the file exists, 0 otherwise
 */
int fileModDate(const char *path, struct tm *date) {

	struct stat attrib;
	struct tm *tempDate;
	FILE * file = fopen(path, "r");

	/* if file exists */
	if (file) {
		fclose(file);

		/* get file attributes */
		stat(path, &attrib);
		tempDate = gmtime(&(attrib.st_mtime));

		*date = *tempDate;
		return 1;

	}
	return 0;
}

/*!
 * Get current date/time
 * @param date Pointer to memory where the current date is going to be stored
 */
void now(struct tm *date) {
	time_t rawtime;
	struct tm * tempDate;

	time(&rawtime);
	tempDate = gmtime(&rawtime);
	*date = *tempDate;
}

/*!
 * Converts a date to an RFC-1123 date
 * @param buffer Pointer to memory where RFC-1123 date will be saved
 * @param date Pointer to the date being converted
 */
int dateToStr(char *buffer, const struct tm *date) {
	return strftime(buffer, 40, "Date: %a, %d %b %Y %T GMT\n", date);
}

/*!
 * Compares two dates
 * @param date1 Pointer to the first date
 * @param date2 Pointer to the second date
 * @return A value > 0 when date1 represents a date before date2, 0 when they are equal, < 0 when date2 < date1
 */
int compareDates(const struct tm *date1, const struct tm *date2) {
	time_t sec1;
	time_t sec2;

	sec1 = mktime((struct tm*) date1);
	sec2 = mktime((struct tm*) date2);

	return (sec2 - sec1);
}

