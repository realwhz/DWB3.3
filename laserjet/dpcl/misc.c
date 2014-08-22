/*
 *
 * A few general purpose routines.
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>

#include "gen.h"			/* a few general purpose definitions */
#include "ext.h"			/* external variable declarations */

int	nolist = 0;			/* number of specified ranges */
int	olist[50];			/* processing range pairs */

/*****************************************************************************/

out_list(str)

    char	*str;

{

    int		start, stop;

/*
 *
 * Called to get the processing ranges that were specified by the -o option.
 * The range syntax should be identical to the one used in nroff and troff.
 *
 */

    while ( *str && nolist < sizeof(olist) - 2 ) {
	start = stop = str_convert(&str, 0);

	if ( *str == '-' && *str++ )
	    stop = str_convert(&str, 9999);

	if ( start > stop )
	    error(FATAL, "illegal range %d-%d", start, stop);

	olist[nolist++] = start;
	olist[nolist++] = stop;

	if ( *str != '\0' ) str++;
    }	/* End while */

    olist[nolist] = 0;

}   /* End of out_list */

/*****************************************************************************/

in_olist(num)

    int		num;

{

    int		i;

/*
 *
 * Returns ON if num represents a page that we're supposed to print. If no
 * ranges were selected nolist will be 0 and we print everything.
 *
 */

    if ( nolist == 0 )			/* everything's included */
	return(ON);

    for ( i = 0; i < nolist; i += 2 )
	if ( num >= olist[i] && num <= olist[i+1] )
	    return(ON);

    return(OFF);

}   /* End of in_olist */

/*****************************************************************************/

cat(file)

    char	*file;

{

    int		fd_in;
    int		fd_out;
    char	buf[512];
    int		count;

/*
 *
 * Copy file to stdout. Return TRUE if it worked.
 *
 */

    fflush(stdout);

    if ( (fd_in = open(file, O_RDONLY)) == -1 )
	return(FALSE);

    fd_out = fileno(stdout);
    while ( (count = read(fd_in, buf, sizeof(buf))) > 0 )
	write(fd_out, buf, count);

    close(fd_in);
    return(TRUE);

}   /* End of cat */

/*****************************************************************************/

putint(n, fp)

    int		n;
    FILE	*fp;

{

/*
 *
 * Writes the value stored in the lower 16 bits of n as the next two bytes in
 * file *fp.
 *
 */

    putc(n >> BYTE, fp);
    putc(n & BMASK, fp);

}   /* End of putint */

/*****************************************************************************/

getint(fp)

    FILE	*fp;

{

    int		c1;
    int		c2;
    int		val;

/*
 *
 * Reads the next two bytes from *fp, make an integer, and return it.
 *
 */

    c1 = getc(fp);
    c2 = getc(fp);
    val = (c1 & 0200) ? ~0 : 0;
    val = (val << BYTE) | (c1 & BMASK);
    val = (val << BYTE) | (c2 & BMASK);
    return(val);

}   /* End of getint */

/*****************************************************************************/

str_convert(str, err)

    char	**str;
    int		err;

{

    int		i;

/*
 *
 * Grab an integer from *str and returns it to the caller. Return err if *str
 * isn't an integer. *str is updated after each digit is processed.
 *
 */


    if ( ! isdigit(**str) )		/* something's wrong */
	return(err);

    for ( i = 0; isdigit(**str); *str += 1 )
	i = 10 * i + **str - '0';

    return(i);

}   /* End of str_convert */

/*****************************************************************************/

error(kind, mesg, a1, a2, a3)

    int		kind;
    char	*mesg;
    unsigned	a1, a2, a3;

{

/*
 *
 * Called when we've run into some kind of program error. *mesg is printed
 * using the control string arguments a?. Quit if we're not ignoring errors
 * and kind is FATAL.
 *
 */

    if ( mesg != NULL && *mesg != '\0' ) {
	fprintf(stderr, "%s: ", prog_name);
	fprintf(stderr, mesg, a1, a2, a3);
	if ( lineno > 0 )
	    fprintf(stderr, " (line %d)", lineno);
	if ( position > 0 )
	    fprintf(stderr, " (near byte %d)", position);
	putc('\n', stderr);
    }	/* End if */

    if ( kind == FATAL && ignore == OFF ) {
	if ( temp_file != NULL )
	    unlink(temp_file);
	exit(x_stat | 01);
    }	/* End if */

}   /* End of error */

/*****************************************************************************/

void interrupt(sig)

    int		sig;

{

/*
 *
 * Called when we get a signal that we're supposed to catch.
 *
 */

    if ( temp_file != NULL )
	unlink(temp_file);

    exit(1);

}   /* End of interrupt */

/*****************************************************************************/

