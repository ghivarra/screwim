/*
 * Copuright (c) 2016 JoungKyun.Kim <http://oops.org>. All rights reserved.
 *
 * This file is part of mod_screwim
 *
 * This program is forked from PHP screw who made by Kunimasa Noda in
 * PM9.com, Inc. and, his information is follows:
 *   http://www.pm9.com
 *   kuni@pm9.com
 *   https://sourceforge.net/projects/php-screw/
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

typedef unsigned long  ULong; /* 32 bits or more */

#include "php_screwim.h"
#include "my_screw.h"


#ifdef HAVE_GETOPT_LONG
static struct option long_options[] = { // {{{
	/* Options without arguments: */
	{ "decode", no_argument,       NULL, 'd' },
	{ "help",   no_argument,       NULL, 'h' },
	{ "view",   no_argument,       NULL, 'v' },

	/* Options accepting an argument: */
	{ "hlen",   required_argument, NULL, 'H' },
	{ "key",    required_argument, NULL, 'k' },
	{ 0, 0, 0, 0 }
}; // }}}
#endif

void usage (void) {
	fprintf (
		stderr,
		"%s : encode or decode php file\n"
		"Usage: %s [OPTION] PHP_FILE\n"
		"   -d,     --decode   decrypt encrypted php script\n"
		"   -h,     --help     this help messages\n"
		"   -H VAL, --hlen=VAL length of magic key(SCREWIM_LEN or PM9SCREW_LEN).\n"
	    "                      use -d mode\n"
		"   -k VAL, --key=VAL  key bytes. use with -d mode\n"
		"   -v,     --view     print head length and key byte of this file\n",
		PACKAGE_STRING, PACKAGE_NAME
	);
	exit (1);
}

unsigned short revert_endian (size_t x) {
	int a, b;

	a = ((x & 0xff00) >> 8) & 0x000000ff;
	b = (x & 0x00ff) << 8;
	//printf ("\n#### %x - %x - %x\n", a, b, a ^ b);

	return a ^ b;
}

short * generate_key (char * p, int no) {
	int     len = strlen (p);
	int     i, j = 0;
	char    buf[5] = { 0, };
	short * r;
	char  * endptr;

	r = (short *) malloc (sizeof (short) * no);

	for ( i=0; i<len; i+=4 ) {
		int n = strlen (p + i);
		memset (buf, '0', 4);

		if ( n > 4 )
			n = 4;
		else if ( n < 2 )
			break;
		else if ( n == 3 )
			n--;

		strncpy (buf, p + i, n);

		r[j] = (short) revert_endian (strtoul (buf, &endptr, 16));
		//printf ("%s %d %d\n", buf, n, r[j]);
		j++;
	}

	return r;
}

int main (int argc, char ** argv) {
	FILE   * fp;
	struct   stat stat_buf;
	char   * datap, * newdatap;
	ULong    datalen, newdatalen;
	int      cryptkey_len = sizeof (screwim_mycryptkey) / sizeof (short);
	char     newfilename[256];
	int      i;

	// for getopt
	int      opt;
	int      mode = 0; // not 0, decode mode
	int      hlen = 0; // given magic key length
	char   * key = NULL;

#ifdef HAVE_GETOPT_LONG
	while ( (opt = getopt_long (argc, (char *const *)argv, "dhH:k:v", long_options, (int *) 0)) != EOF )
#else
	while ( (opt = getopt (argc, (char *const *)argv, "dhH:k:v")) != EOF )
#endif
	{
		switch (opt) {
			case 'd' :
				mode++;
				break;
			case 'H' :
				hlen = atoi (optarg);
				break;
			case 'k' :
				key = optarg;
				break;
			case 'v' :
				printf ("HEADER LENGTH : %d\n", SCREWIM_LEN);
				printf ("KEY BYTE      : ");

				for ( i=0; i<cryptkey_len; i++ ) {
					printf ("%04x", revert_endian (screwim_mycryptkey[i]));
				}
				printf ("\n");

				return 0;
			case 'h' :
			default :
				usage ();
		}
	}

	if ( (argc - optind) != 1 )
		usage ();

	if ( ! mode && (key != NULL || hlen != 0) ) {
		fprintf (stderr, "Error: -H and -k options must be used with -d option\n\n");
		usage ();
	}

	fp = fopen (argv[optind], "r");
	if ( fp == NULL ) {
		fprintf (stderr, "File not found(%s)\n", argv[optind]);
		return 1;
	}

	fstat (fileno (fp), &stat_buf);
	datalen = stat_buf.st_size;
	datap = (char *) malloc (datalen + 1);
	memset (datap, 0, datalen + 1);
	if ( mode )
		fread (datap, datalen, 1, fp);
	else
		fread (datap, datalen, 1, fp);
	fclose (fp);

	sprintf (newfilename, "%s.%s", argv[optind], mode ? "discrew" : "screw");

	if ( mode ) {
		char  * ndatap;
		int     mlen = (hlen != 0) ? hlen : SCREWIM_LEN;
		short * user_key = NULL, * ckey = NULL;
		int     clen;
		// if give -H option, Given the -H option, it means that you do not
		// know what a MAGIC KEY is.
		if ( hlen != 0 ) {
			if ( memcmp (datap, SCREWIM, SCREWIM_LEN) != 0 ) {
				fprintf (stderr, "Not Crypted file(%s)\n", argv[optind]);
				return 0;
			}
		}

		datalen -= mlen;
		ndatap = malloc (datalen + 1);
		memset (ndatap, 0, datalen + 1);
		memcpy (ndatap, datap + mlen, datalen);

		if ( key != NULL ) {
			clen = strlen (key) / 4;

			if ( (strlen (key) % 4) != 0 )
				clen++;

			user_key = generate_key (key, clen);
			ckey = user_key;
		} else {
			ckey = screwim_mycryptkey;
			clen = cryptkey_len;
		}

		for( i=0; i<datalen; i++ ) {
			//ndatap[i] = (char) screwim_mycryptkey[(datalen - i) % clen] ^ (~(ndatap[i]));
			//printf ("#### -> %d\n", ckey[i%4]);
			ndatap[i] = (char) ckey[(datalen - i) % clen] ^ (~(ndatap[i]));
		}
		
		if ( user_key != NULL )
			free (user_key);

		newdatap = zdecode (ndatap, datalen, &newdatalen);
		free (ndatap);
	} else {
		if ( memcmp (datap, SCREWIM, SCREWIM_LEN) == 0 ) {
			fprintf (stderr, "Already Crypted(%s)\n", argv[optind]);
			return 0;
		}

		newdatap = zencode (datap, datalen, &newdatalen);

		for( i=0; i<newdatalen; i++ ) {
			newdatap[i] = (char) screwim_mycryptkey[(newdatalen - i) % cryptkey_len] ^ (~(newdatap[i]));
		}
	}

	fp = fopen (newfilename, "wb");
	if ( fp == NULL ) {
		fprintf (stderr, "Can not create %scrypt file(%s)\n", argv[optind], mode ? "de" : "");
		return 1;
	}
	if ( !mode )
		fwrite (SCREWIM, SCREWIM_LEN, 1, fp);
	fwrite (newdatap, newdatalen, 1, fp);
	fclose (fp);
	fprintf (stderr, "Success %s(%s)\n", mode ? "Decrypting" : "Crypting", newfilename);
	free (newdatap);
	free (datap);

	return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
