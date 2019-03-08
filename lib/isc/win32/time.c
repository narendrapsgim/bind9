/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * See the COPYRIGHT file distributed with this work for additional
 * information regarding copyright ownership.
 */

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <windows.h>

#include <isc/assertions.h>
#include <isc/string.h>
#include <isc/time.h>
#include <isc/tm.h>
#include <isc/util.h>

/*
 * struct FILETIME uses "100-nanoseconds intervals".
 * NS / S = 1000000000 (10^9).
 * While it is reasonably obvious that this makes the needed
 * conversion factor 10^7, it is coded this way for additional clarity.
 */
#define NS_PER_S 	1000000000
#define NS_INTERVAL	100
#define INTERVALS_PER_S (NS_PER_S / NS_INTERVAL)

/***
 *** Absolute Times
 ***/

static const isc_time_t epoch = { { 0, 0 } };
LIBISC_EXTERNAL_DATA const isc_time_t * const isc_time_epoch = &epoch;

/***
 *** Intervals
 ***/

static const isc_interval_t zero_interval = { 0 };
LIBISC_EXTERNAL_DATA const isc_interval_t * const isc_interval_zero = &zero_interval;

void
isc_interval_set(isc_interval_t *i, unsigned int seconds,
		 unsigned int nanoseconds)
{
	REQUIRE(i != NULL);
	REQUIRE(nanoseconds < NS_PER_S);

	/*
	 * This rounds nanoseconds up not down.
	 */
	i->interval = (LONGLONG)seconds * INTERVALS_PER_S
		+ (nanoseconds + NS_INTERVAL - 1) / NS_INTERVAL;
}

bool
isc_interval_iszero(const isc_interval_t *i) {
	REQUIRE(i != NULL);
	if (i->interval == 0)
		return (true);

	return (false);
}

void
isc_time_set(isc_time_t *t, unsigned int seconds, unsigned int nanoseconds) {
	SYSTEMTIME epoch1970 = { 1970, 1, 4, 1, 0, 0, 0, 0 };
	FILETIME temp;
	ULARGE_INTEGER i1;

	REQUIRE(t != NULL);
	REQUIRE(nanoseconds < NS_PER_S);

	SystemTimeToFileTime(&epoch1970, &temp);

	i1.LowPart = temp.dwLowDateTime;
	i1.HighPart = temp.dwHighDateTime;

	i1.QuadPart += (unsigned __int64)nanoseconds/100;
	i1.QuadPart += (unsigned __int64)seconds*10000000;

	t->absolute.dwLowDateTime = i1.LowPart;
	t->absolute.dwHighDateTime = i1.HighPart;
}

void
isc_time_settoepoch(isc_time_t *t) {
	REQUIRE(t != NULL);

	t->absolute.dwLowDateTime = 0;
	t->absolute.dwHighDateTime = 0;
}

bool
isc_time_isepoch(const isc_time_t *t) {
	REQUIRE(t != NULL);

	if (t->absolute.dwLowDateTime == 0 &&
	    t->absolute.dwHighDateTime == 0)
		return (true);

	return (false);
}

isc_result_t
isc_time_now(isc_time_t *t) {
	REQUIRE(t != NULL);

	GetSystemTimeAsFileTime(&t->absolute);

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_time_nowplusinterval(isc_time_t *t, const isc_interval_t *i) {
	ULARGE_INTEGER i1;

	REQUIRE(t != NULL);
	REQUIRE(i != NULL);

	GetSystemTimeAsFileTime(&t->absolute);

	i1.LowPart = t->absolute.dwLowDateTime;
	i1.HighPart = t->absolute.dwHighDateTime;

	if (UINT64_MAX - i1.QuadPart < (unsigned __int64)i->interval)
		return (ISC_R_RANGE);

	i1.QuadPart += i->interval;

	t->absolute.dwLowDateTime  = i1.LowPart;
	t->absolute.dwHighDateTime = i1.HighPart;

	return (ISC_R_SUCCESS);
}

int
isc_time_compare(const isc_time_t *t1, const isc_time_t *t2) {
	REQUIRE(t1 != NULL && t2 != NULL);

	return ((int)CompareFileTime(&t1->absolute, &t2->absolute));
}

isc_result_t
isc_time_add(const isc_time_t *t, const isc_interval_t *i, isc_time_t *result)
{
	ULARGE_INTEGER i1;

	REQUIRE(t != NULL && i != NULL && result != NULL);

	i1.LowPart = t->absolute.dwLowDateTime;
	i1.HighPart = t->absolute.dwHighDateTime;

	if (UINT64_MAX - i1.QuadPart < (unsigned __int64)i->interval)
		return (ISC_R_RANGE);

	i1.QuadPart += i->interval;

	result->absolute.dwLowDateTime = i1.LowPart;
	result->absolute.dwHighDateTime = i1.HighPart;

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_time_subtract(const isc_time_t *t, const isc_interval_t *i,
		  isc_time_t *result) {
	ULARGE_INTEGER i1;

	REQUIRE(t != NULL && i != NULL && result != NULL);

	i1.LowPart = t->absolute.dwLowDateTime;
	i1.HighPart = t->absolute.dwHighDateTime;

	if (i1.QuadPart < (unsigned __int64) i->interval)
		return (ISC_R_RANGE);

	i1.QuadPart -= i->interval;

	result->absolute.dwLowDateTime = i1.LowPart;
	result->absolute.dwHighDateTime = i1.HighPart;

	return (ISC_R_SUCCESS);
}

uint64_t
isc_time_microdiff(const isc_time_t *t1, const isc_time_t *t2) {
	ULARGE_INTEGER i1, i2;
	LONGLONG i3;

	REQUIRE(t1 != NULL && t2 != NULL);

	i1.LowPart  = t1->absolute.dwLowDateTime;
	i1.HighPart = t1->absolute.dwHighDateTime;
	i2.LowPart  = t2->absolute.dwLowDateTime;
	i2.HighPart = t2->absolute.dwHighDateTime;

	if (i1.QuadPart <= i2.QuadPart)
		return (0);

	/*
	 * Convert to microseconds.
	 */
	i3 = (i1.QuadPart - i2.QuadPart) / 10;

	return (i3);
}

uint32_t
isc_time_seconds(const isc_time_t *t) {
	SYSTEMTIME epoch1970 = { 1970, 1, 4, 1, 0, 0, 0, 0 };
	FILETIME temp;
	ULARGE_INTEGER i1, i2;
	LONGLONG i3;

	SystemTimeToFileTime(&epoch1970, &temp);

	i1.LowPart  = t->absolute.dwLowDateTime;
	i1.HighPart = t->absolute.dwHighDateTime;
	i2.LowPart  = temp.dwLowDateTime;
	i2.HighPart = temp.dwHighDateTime;

	i3 = (i1.QuadPart - i2.QuadPart) / 10000000;

	return ((uint32_t)i3);
}

isc_result_t
isc_time_secondsastimet(const isc_time_t *t, time_t *secondsp) {
	time_t seconds;

	REQUIRE(t != NULL);

	seconds = (time_t)isc_time_seconds(t);

	INSIST(sizeof(unsigned int) == sizeof(uint32_t));
	INSIST(sizeof(time_t) >= sizeof(uint32_t));

	if (isc_time_seconds(t) > (~0U>>1) && seconds <= (time_t)(~0U>>1))
		return (ISC_R_RANGE);

	*secondsp = seconds;

	return (ISC_R_SUCCESS);
}


uint32_t
isc_time_nanoseconds(const isc_time_t *t) {
	ULARGE_INTEGER i;

	i.LowPart  = t->absolute.dwLowDateTime;
	i.HighPart = t->absolute.dwHighDateTime;
	return ((uint32_t)(i.QuadPart % 10000000) * 100);
}

void
isc_time_formattimestamp(const isc_time_t *t, char *buf, unsigned int len) {
	FILETIME localft;
	SYSTEMTIME st;
	char DateBuf[50];
	char TimeBuf[50];

	REQUIRE(t != NULL);
	REQUIRE(buf != NULL);
	REQUIRE(len > 0);

	if (FileTimeToLocalFileTime(&t->absolute, &localft) &&
	    FileTimeToSystemTime(&localft, &st)) {
		GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, "dd-MMM-yyyy",
			      DateBuf, 50);
		GetTimeFormat(LOCALE_USER_DEFAULT, TIME_NOTIMEMARKER|
			      TIME_FORCE24HOURFORMAT, &st, NULL, TimeBuf, 50);

		snprintf(buf, len, "%s %s.%03u", DateBuf, TimeBuf,
			 st.wMilliseconds);

	} else {
		strlcpy(buf, "99-Bad-9999 99:99:99.999", len);
	}
}

void
isc_time_formathttptimestamp(const isc_time_t *t, char *buf, unsigned int len) {
	SYSTEMTIME st;
	char DateBuf[50];
	char TimeBuf[50];

/* strftime() format: "%a, %d %b %Y %H:%M:%S GMT" */

	REQUIRE(t != NULL);
	REQUIRE(buf != NULL);
	REQUIRE(len > 0);

	if (FileTimeToSystemTime(&t->absolute, &st)) {
		GetDateFormat(LOCALE_USER_DEFAULT, 0, &st,
			      "ddd',' dd MMM yyyy", DateBuf, 50);
		GetTimeFormat(LOCALE_USER_DEFAULT,
			      TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT,
			      &st, "hh':'mm':'ss", TimeBuf, 50);

		snprintf(buf, len, "%s %s GMT", DateBuf, TimeBuf);
	} else {
		buf[0] = 0;
	}
}

isc_result_t
isc_time_parsehttptimestamp(char *buf, isc_time_t *t) {
	struct tm t_tm;
	time_t when;
	char *p;

	REQUIRE(buf != NULL);
	REQUIRE(t != NULL);

	p = isc_tm_strptime(buf, "%a, %d %b %Y %H:%M:%S", &t_tm);
	if (p == NULL)
		return (ISC_R_UNEXPECTED);
	when = isc_tm_timegm(&t_tm);
	if (when == -1)
		return (ISC_R_UNEXPECTED);
	isc_time_set(t, (unsigned int)when, 0);
	return (ISC_R_SUCCESS);
}

void
isc_time_formatISO8601L(const isc_time_t *t, char *buf, unsigned int len) {
	SYSTEMTIME st;
	char DateBuf[50];
	char TimeBuf[50];

	/* strtime() format: "%Y-%m-%dT%H:%M:%S" */

	REQUIRE(t != NULL);
	REQUIRE(buf != NULL);
	REQUIRE(len > 0);

	if (FileTimeToSystemTime(&t->absolute, &st)) {
		GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, "yyyy-MM-dd",
			      DateBuf, 50);
		GetTimeFormat(LOCALE_USER_DEFAULT,
			      TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT,
			      &st, "hh':'mm':'ss", TimeBuf, 50);
		snprintf(buf, len, "%sT%s", DateBuf, TimeBuf);
	} else {
		buf[0] = 0;
	}
}

void
isc_time_formatISO8601Lms(const isc_time_t *t, char *buf, unsigned int len) {
	SYSTEMTIME st;
	char DateBuf[50];
	char TimeBuf[50];

	/* strtime() format: "%Y-%m-%dT%H:%M:%S.SSS" */

	REQUIRE(t != NULL);
	REQUIRE(buf != NULL);
	REQUIRE(len > 0);

	if (FileTimeToSystemTime(&t->absolute, &st)) {
		GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, "yyyy-MM-dd",
			      DateBuf, 50);
		GetTimeFormat(LOCALE_USER_DEFAULT,
			      TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT,
			      &st, "hh':'mm':'ss", TimeBuf, 50);
		snprintf(buf, len, "%sT%s.%03u", DateBuf, TimeBuf,
			 st.wMilliseconds);
	} else {
		buf[0] = 0;
	}
}

void
isc_time_formatISO8601(const isc_time_t *t, char *buf, unsigned int len) {
	SYSTEMTIME st;
	char DateBuf[50];
	char TimeBuf[50];

	/* strtime() format: "%Y-%m-%dT%H:%M:%SZ" */

	REQUIRE(t != NULL);
	REQUIRE(buf != NULL);
	REQUIRE(len > 0);

	if (FileTimeToSystemTime(&t->absolute, &st)) {
		GetDateFormat(LOCALE_NEUTRAL, 0, &st, "yyyy-MM-dd",
			      DateBuf, 50);
		GetTimeFormat(LOCALE_NEUTRAL,
			      TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT,
			      &st, "hh':'mm':'ss", TimeBuf, 50);
		snprintf(buf, len, "%sT%sZ", DateBuf, TimeBuf);
	} else {
		buf[0] = 0;
	}
}

void
isc_time_formatISO8601ms(const isc_time_t *t, char *buf, unsigned int len) {
	SYSTEMTIME st;
	char DateBuf[50];
	char TimeBuf[50];

	/* strtime() format: "%Y-%m-%dT%H:%M:%S.SSSZ" */

	REQUIRE(t != NULL);
	REQUIRE(buf != NULL);
	REQUIRE(len > 0);

	if (FileTimeToSystemTime(&t->absolute, &st)) {
		GetDateFormat(LOCALE_NEUTRAL, 0, &st, "yyyy-MM-dd",
			      DateBuf, 50);
		GetTimeFormat(LOCALE_NEUTRAL,
			      TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT,
			      &st, "hh':'mm':'ss", TimeBuf, 50);
		snprintf(buf, len, "%sT%s.%03uZ", DateBuf, TimeBuf,
			 st.wMilliseconds);
	} else {
		buf[0] = 0;
	}
}

void
isc_time_formatshorttimestamp(const isc_time_t *t, char *buf, unsigned int len)
{
	SYSTEMTIME st;
	char DateBuf[50];
	char TimeBuf[50];

	/* strtime() format: "%Y%m%d%H%M%SSSS" */

	REQUIRE(t != NULL);
	REQUIRE(buf != NULL);
	REQUIRE(len > 0);

	if (FileTimeToSystemTime(&t->absolute, &st)) {
		GetDateFormat(LOCALE_NEUTRAL, 0, &st, "yyyyMMdd",
			      DateBuf, 50);
		GetTimeFormat(LOCALE_NEUTRAL,
			      TIME_NOTIMEMARKER | TIME_FORCE24HOURFORMAT,
			      &st, "hhmmss", TimeBuf, 50);
		snprintf(buf, len, "%s%s%03u", DateBuf, TimeBuf,
			 st.wMilliseconds);
	} else {
		buf[0] = 0;
	}
}
