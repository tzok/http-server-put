#include "headers.h"

/*!
 * Create a timestamp according to RFC 822
 * @param buffer Pointer to memory location where output will be saved
 */
void makeTimestamp(char *buffer)
{
	time_t reference;
	time(&reference);
	struct tm *curTime = gmtime(&reference);

	/* week starts with Sunday */
	char *wkday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	char *month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
			"Sep", "Oct", "Nov", "Dec" };
	sprintf(buffer, "Date: %s, %02d %s %04d %02d:%02d:%02d GMT\n",
			wkday[curTime->tm_wday], curTime->tm_mday, month[curTime->tm_mon],
			1900 + curTime->tm_year, curTime->tm_hour, curTime->tm_min,
			curTime->tm_sec);
}
