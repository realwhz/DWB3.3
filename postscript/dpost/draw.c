/*
 *
 * Drawing routines used by dpost. Almost no real work is done here. Instead
 * the required calculations are done in special Postscript procedures that
 * include:
 *
 *
 *	Dl
 *
 *	  x1 y1 x y Dl -
 *
 *	    Starts a new path and then draws a line from the current point
 *	    (x, y) to (x1, y1).
 *
 *	De
 *
 *	  x y a b De -
 *
 *	    Starts a new path and then draws an ellipse that has its left side
 *	    at the current point (x, y) and horizontal and vertical axes lengths
 *	    given by a and b respectively.
 *
 *	Da
 *
 *	  x y dx1 dy1 dx2 dy2 Da -
 *
 *	    Starts a new segment and then draws a circular arc from the current
 *	    point (x, y) to (x + dx1 + dx2, y + dy1 + dy2). The center of the
 *	    circle is at (x + dx1, y + dy1). Arcs always go counter-clockwise
 *	    from the starting point to the end point.
 *
 *	DA
 *
 *	  x y dx1 dy1 dx2 dy2 DA -
 *
 *	    Draws a clockwise arc from (x, y) to (x + dx1 + dx2, y + dy1 + dy2)
 *	    with center at (x + dx1, y + dy1). Only needed when we're building
 *	    large paths that use arcs and want to control the current point. The
 *	    arguments passed to drawarc() will be whatever they would have been
 *	    for a counter-clockwise arc, so we need to map them into appropriate
 *	    arguments for PostScript's arcn operator. The mapping is,
 *
 *			x = hpos + dx1' + dx2'
 *			y = vpos + dy1' + dy2'
 *			dx1 = -dx2'
 *			dy1 = -dy2'
 *			dx2 = -dx1'
 *			dy2 = -dy1'
 *
 *	   where primed values represent the drawarc() arguments and (hpos, vpos)
 *	   is our current position.
 *
 *	Ds
 *
 *	  x0 y0 x1 y1 x2 y2 Ds -
 *
 *	    Starts a new segment and then draws a quadratic spline connecting
 *	    point ((x0 + x1)/2, (y0 + y1)/2) to ((x1 + x2)/2, (y1 + y2)/2).
 *	    The points used in Postscript's curveto procedure are given by,
 *
 *		x0' = (x0 + 5 * x1) / 6
 *		x1' = (x2 + 5 * x1) / 6
 *		x2' = (x1 + x2) / 2
 *
 *	    with similar equations for the y coordinates.
 *
 * By default all the PostScript drawing procedures begin with a newpath (just to
 * be safe) and end with a stroke, which essentially isolates the path elements
 * built by the drawing procedures. In order to accommodate big paths built from
 * smaller pieces each of the PostScript drawing procedures can forced to retain
 * the path that's being built. That's what happens in beginpath() when an "x X
 * BeginPath" command is read. beginpath() sets the PostScript variable inpath to
 * true, and that essentially eliminates the newpath/stroke pair that bracket the
 * individual pieces. In that case the path is terminated and drawn when dpost
 * reads an "x X DrawPath" command.
 *
 * Early versions of dpost included the PostScript drawing procedures as part of
 * the prologue, and as a result they were included with every job, even if they
 * were never used. This version has separated the drawing procedures from the
 * default prologue (they're now in *drawfile) and only includes them if they're
 * really needed, which is yet another convenient violation of page independence.
 * Routine getdraw() is responsible for adding *drawfile to the output file, and
 * if it can't read *drawfile it continues on as if nothing happened. That means
 * everything should still work if you append *drawfile to *prologue and then
 * delete *drawfile.
 *
 */

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "gen.h"			/* general purpose definitions */
#include "ext.h"			/* external variable definitions */
#include "motion.h"			/* positioning macros */
#include "draw.h"
#include "dpost.h"
#include "color.h"

static void arc_extreme(int, int, int, int);
static int quadrant(double, double);
static void parsebuf(char *);
static void getbaseline(void);

int	gotdraw = FALSE;		/* TRUE when *drawfile has been added */
int	gotbaseline = FALSE;		/* TRUE after *baselinefile is added */
int	inpath = FALSE;			/* TRUE if we're putting pieces together */

/*
 *
 * All these should be defined in file dpost.c.
 *
 */

extern int		hpos;
extern int		vpos;
extern int		encoding;
extern int		maxencoding;
extern int		realencoding;

extern char		*drawfile;
extern char		*baselinefile;
extern FILE		*tf;

/*****************************************************************************/

void
getdraw(void)

{

/*
 *
 * Responsible for making sure the PostScript drawing procedures are downloaded
 * from *drawfile. Stuff is done at most once per job, and only if the job needs
 * them. For now I've decided not to quit if we can't read the drawing file. That
 * pretty much assumes an old version of prologue is being used that includes all
 * the drawing procedures.
 *
 */

    if ( gotdraw == FALSE )
	exportfile(drawfile);

    if ( tf == stdout )
	gotdraw = TRUE;

}   /* End of getdraw */

/*****************************************************************************/

void
drawline(int dx, int dy)

    /* int		dx, dy;			/ * endpoint is (hpos+dx, vpos+dy) */

{

/*
 *
 * Draws a line from (hpos, vpos) to (hpos+dx, vpos+dy), and leaves the current
 * position at the endpoint.
 *
 */

    if ( dx == 0 && dy == 0 )
	drawcirc(1);
    else fprintf(tf, "%d %d %d %d Dl\n", hpos + dx, vpos + dy, hpos, vpos);

    if ( dobbox == TRUE ) {
	cover((double)hpos, (double)-vpos);
	cover((double)(hpos + dx), (double)-(vpos + dy));
    }	/* End if */

    hgoto(hpos+dx);			/* where troff expects to be */
    vgoto(vpos+dy);

    resetpos();				/* not sure where the printer is */

}   /* End of drawline */

/*****************************************************************************/

void
drawcirc(int d)

    /* int		d;			/ * diameter of the circle */

{

/*
 *
 * Draws a circle of diameter d with the left 'side' of the circle at the
 * current point. After we're finished drawing we move the current position
 * to the right side.
 *
 */

    drawellip(d, d);

}   /* End of drawcirc */

/*****************************************************************************/

void
drawellip(int a, int b)

/*    int		a, b;			axes lengths for the ellipse */

{

/*
 *
 * Draws an ellipse having axes lengths horizontally and vertically of a and
 * b. The left side of the ellipse is at the current point. After we're done
 * drawing the path we move the current position to the right side.
 *
 */

    if ( a == 0 && b == 0 )
	return;

    fprintf(tf, "%d %d %d %d De\n", hpos, vpos, a, b);

    if ( dobbox == TRUE ) {
	cover((double)hpos, (double)-(vpos + b/2));
	cover((double)(hpos+a), (double)-(vpos - b/2));
    }	/* End if */

    hgoto(hpos + a);			/* where troff expects to be */
    vgoto(vpos);

    resetpos();				/* not sure where the printer is */

}   /* End of drawellip */

/*****************************************************************************/

void
drawarc(int dx1, int dy1, int dx2, int dy2, int c)

    /* int		dx1, dy1;		/ * vector from current pos to center */
    /* int		dx2, dy2;		/ * from center to end of the arc */
    /* int		c;			/ * clockwise if c is A */

{

/*
 *
 * If c isn't set to 'A' a counter-clockwise arc is drawn from the current point
 * (hpos, vpos) to (hpos+dx1+dx2, vpos+dy1+dy2). The center of the circle is the
 * point (hpos+dx1, vpos+dy1). If c is 'A' the arc goes clockwise from the point
 * (hpos+dx1+dx2, vpos+dy1+dy2) to (hpos, vpos). Clockwise arcs are only needed
 * if we're building a larger path out of pieces that include arcs, and want to
 * have PostScript manage the path for us. Arguments (for a clockwise arc) are
 * what would have been supplied if the arc was drawn in a counter-clockwise
 * direction, and are converted to values suitable for use with PostScript's arcn
 * operator.
 *
 */

    if ( (dx1 != 0 || dy1 != 0) && (dx2 != 0 || dy2 != 0) ) {
	if ( c != 'A' )
	    fprintf(tf, "%d %d %d %d %d %d Da\n", hpos, vpos, dx1, dy1, dx2, dy2);
	else fprintf(tf, "%d %d %d %d %d %d DA\n", hpos+dx1+dx2, vpos+dy1+dy2,
						-dx2, -dy2, -dx1, -dy1);

	if ( dobbox == TRUE )
	    arc_extreme(dx1, dy1, dx2, dy2);
    }	/* End if */

    hgoto(hpos + dx1 + dx2);		/* where troff expects to be */
    vgoto(vpos + dy1 + dy2);

    resetpos();				/* not sure where the printer is */

}   /* End of drawarc */

/*****************************************************************************/

void
drawspline(FILE *fp, int flag)

    /* FILE	*fp;			/ * input for point list */
    /* int		flag;			/ * flag!=1 connect end points */

{

    int		x[100], y[100];
    int		i, N;

/*
 *
 * Spline drawing routine for Postscript printers. The complicated stuff is
 * handled by procedure Ds, which should be defined in the library file. I've
 * seen wrong implementations of troff's spline drawing, so fo the record I'll
 * write down the parametric equations and the necessary conversions to Bezier
 * cubic splines (as used in Postscript).
 *
 *
 * Parametric equation (x coordinate only):
 *
 *
 *	    (x2 - 2 * x1 + x0)    2                    (x0 + x1)
 *	x = ------------------ * t   + (x1 - x0) * t + ---------
 *		    2					   2
 *
 *
 * The coefficients in the Bezier cubic are,
 *
 *
 *	A = 0
 *	B = (x2 - 2 * x1 + x0) / 2
 *	C = x1 - x0
 *
 *
 * while the current point is,
 *
 *	current-point = (x0 + x1) / 2
 *
 * Using the relationships given in the Postscript manual (page 121) it's easy to
 * see that the control points are given by,
 *
 *
 *	x0' = (x0 + 5 * x1) / 6
 *	x1' = (x2 + 5 * x1) / 6
 *	x2' = (x1 + x2) / 2
 *
 *
 * where the primed variables are the ones used by curveto. The calculations
 * shown above are done in procedure Ds using the coordinates set up in both
 * the x[] and y[] arrays.
 *
 * A simple test of whether your spline drawing is correct would be to use cip
 * to draw a spline and some tangent lines at appropriate points and then print
 * the file.
 *
 */

    for ( N = 2; N < sizeof(x)/sizeof(x[0]); N++ )
	if (fscanf(fp, "%d %d", &x[N], &y[N]) != 2)
		break;

    x[0] = x[1] = hpos;
    y[0] = y[1] = vpos;

    for (i = 1; i < N; i++) {
	x[i+1] += x[i];
	y[i+1] += y[i];
    }	/* End for */

    x[N] = x[N-1];
    y[N] = y[N-1];

    for (i = ((flag!=1)?0:1); i < ((flag!=1)?N-1:N-2); i++) {
	fprintf(tf, "%d %d %d %d %d %d Ds\n", x[i], y[i], x[i+1], y[i+1], x[i+2], y[i+2]);
	if ( dobbox == TRUE ) {		/* could be better */
	    cover((double)(x[i] + x[i+1])/2,(double)-(y[i] + y[i+1])/2);
	    cover((double)x[i+1], (double)-y[i+1]);
	    cover((double)(x[i+1] + x[i+2])/2, (double)-(y[i+1] + y[i+2])/2);
	}   /* End if */
    }	/* End for */

    hgoto(x[N]);			/* where troff expects to be */
    vgoto(y[N]);

    resetpos();				/* not sure where the printer is */

}   /* End of drawspline */

/*****************************************************************************/

static void
arc_extreme(int dx1, int dy1, int dx2, int dy2)

{

    double	x0, y0, x1, y1, xc, yc;  /* start, end, center */
    double	r, xmin, ymin, xmax, ymax;
    int		j, k;

/*
 *
 * bounding box of a circular arc             Eric Grosse  24 May 84
 *
 * Conceptually, this routine generates a list consisting of the start,
 * end, and whichever north, east, south, and west points lie on the arc.
 * The bounding box is then the range of this list.
 *     list = {start,end}
 *     j = quadrant(start)
 *     k = quadrant(end)
 *     if( j==k && long way 'round )  append north,west,south,east
 *     else
 *       while( j != k )
 *          append center+radius*[j-th of north,west,south,east unit vectors]
 *          j += 1  (mod 4)
 *     return( bounding box of list )
 * The following code implements this, with simple optimizations.
 *
 */

    x0 = hpos;
    y0 = -vpos;
    x1 = hpos + dx1 + dx2;
    y1 = -(vpos + dy1 + dy2);
    xc = hpos + dx1;
    yc = -(vpos + dy1);

    x0 -= xc; y0 -= yc;			/* move to center */
    x1 -= xc; y1 -= yc;
    xmin = (x0<x1)?x0:x1; ymin = (y0<y1)?y0:y1;
    xmax = (x0>x1)?x0:x1; ymax = (y0>y1)?y0:y1;
    r = sqrt(x0*x0 + y0*y0);
    if (r > 0.0) {
	j = quadrant(x0,y0);
	k = quadrant(x1,y1);
	if (j == k && y1*x0 < x1*y0) {
	    /* viewed as complex numbers, if Im(z1/z0)<0, arc is big */
	    if( xmin > -r) xmin = -r; if( ymin > -r) ymin = -r;
	    if( xmax <  r) xmax =  r; if( ymax <  r) ymax =  r;
	} else {
	    while (j != k) {
		switch (j) {
		    case 1: if( ymax <  r) ymax =  r; break; /* north */
		    case 2: if( xmin > -r) xmin = -r; break; /* west */
		    case 3: if( ymin > -r) ymin = -r; break; /* south */
		    case 4: if( xmax <  r) xmax =  r; break; /* east */
		}   /* End switch */
		j = j%4 + 1;
	    }	/* End while */
	}   /* End else */
    }	/* End if */

    xmin += xc; ymin += yc;
    xmax += xc; ymax += yc;
    cover(xmin, ymin);
    cover(xmax, ymax);

}   /* End of arc_extreme */

/*****************************************************************************/

static int
quadrant(double x, double y)

{

    if (     x>=0.0 && y> 0.0) return(1);
    else if( x< 0.0 && y>=0.0) return(2);
    else if( x<=0.0 && y< 0.0) return(3);
    else if( x> 0.0 && y<=0.0) return(4);
    else		       return 0;	/* shut up lint */

}   /* End of quadrant */

/*****************************************************************************/

void
beginpath(char *buf, int copy)

    /* char	*buf;			/ * whatever followed "x X BeginPath" */
    /* int		copy;			/ * ignore *buf if FALSE */

{

/*
 *
 * Called from devcntrl() whenever an "x X BeginPath" command is read. It's used
 * to mark the start of a sequence of drawing commands that should be grouped
 * together and treated as a single path. By default the drawing procedures in
 * *drawfile treat each drawing command as a separate object, and usually start
 * with a newpath (just as a precaution) and end with a stroke. The newpath and
 * stroke isolate individual drawing commands and make it impossible to deal with
 * composite objects. "x X BeginPath" can be used to mark the start of drawing
 * commands that should be grouped together and treated as a single object, and
 * part of what's done here ensures that the PostScript drawing commands defined
 * in *drawfile skip the newpath and stroke, until after the next "x X DrawPath"
 * command. At that point the path that's been built up can be manipulated in
 * various ways (eg. filled and/or stroked with a different line width).
 *
 * String *buf is unnecessary and is only included for compatibility with an early
 * verion of that's still in use. In that version "x X BeginObject" marked the
 * start of a graphical object, and whatever followed it was passed along in *buf
 * and copied to the output file. Color selection is one of the options that's
 * available in parsebuf(), so if we get here we add *colorfile to the output
 * file before doing anything important.
 *
 */

    if ( inpath == FALSE ) {
	flushtext();
	getdraw();
	getcolor();
	fprintf(tf, "gsave\n");
	fprintf(tf, "newpath\n");
	fprintf(tf, "%d %d m\n", hpos, vpos);
	fprintf(tf, "/inpath true def\n");
	if ( copy == TRUE )
	    fprintf(tf, "%s", buf);
	inpath = TRUE;
    }	/* End if */

}   /* End of beginpath */

/*****************************************************************************/

void
drawpath(char *buf, int copy)

{

/*
 *
 * Called from devcntrl() whenever an "x X DrawPath" command is read. It marks the
 * end of the path started by the last "x X BeginPath" command and uses whatever
 * has been passed along in *buf to manipulate the path (eg. fill and/or stroke
 * the path). Once that's been done the drawing procedures are restored to their
 * default behavior in which each drawing command is treated as an isolated path.
 * The new version (called after "x X DrawPath") has copy set to FALSE, and calls
 * parsebuf() to figure out what goes in the output file. It's a feeble attempt
 * to free users and preprocessors (like pic) from having to know PostScript. The
 * comments in parsebuf() describe what's handled.
 *
 * In the early version a path was started with "x X BeginObject" and ended with
 * "x X EndObject". In both cases *buf was just copied to the output file, and
 * was expected to be legitimate PostScript that manipulated the current path.
 * The old escape sequence will be supported for a while (for Ravi), and always
 * call this routine with copy set to TRUE.
 * 
 *
 */

    if ( inpath == TRUE ) {
	if ( copy == TRUE )
	    fprintf(tf, "%s", buf);
	else parsebuf(buf);
	fprintf(tf, "grestore\n");
	fprintf(tf, "/inpath false def\n");
	reset();
	inpath = FALSE;
    }	/* End if */

}   /* End of drawpath */

/*****************************************************************************/

static void
parsebuf(char *buf)

    /* char	*buf;			/ * whatever followed "x X DrawPath" */

{

    char	*p;			/* usually the next token */
    char	*p1;			/* for grabbing arguments */
    char	*pend;			/* end of the original string (ie. *buf) */
    int		gsavelevel = 0;		/* non-zero if we've done a gsave */

/*
 *
 * Simple minded attempt at parsing the string that followed an "x X DrawPath"
 * command. Everything not recognized here is simply ignored - there's absolutely
 * no error checking and what was originally in buf is clobbered by strtok().
 * A typical *buf might look like,
 *
 *	gray .9 fill stroke
 *
 * to fill the current path with a gray level of .9 and follow that by stroking the
 * outline of the path. Since unrecognized tokens are ignored the last example
 * could also be written as,
 *
 *	with gray .9 fill then stroke
 *
 * The "with" and "then" strings aren't recognized tokens and are simply discarded.
 * The "stroke", "fill", and "wfill" force out appropriate PostScript code and are
 * followed by a grestore. In otherwords changes to the grahics state (eg. a gray
 * level or color) are reset to default values immediately after the stroke, fill,
 * or wfill tokens. For now "fill" gets invokes PostScript's eofill operator and
 * "wfill" calls fill (ie. the operator that uses the non-zero winding rule).
 *
 * The tokens that cause temporary changes to the graphics state are "gray" (for
 * setting the gray level), "color" (for selecting a known color from the colordict
 * dictionary defined in *colorfile), and "line" (for setting the line width). All
 * three tokens can be extended since strncmp() makes the comparison. For example
 * the strings "line" and "linewidth" accomplish the same thing. Colors are named
 * (eg. "red"), but must be appropriately defined in *colorfile. For now all three
 * tokens must be followed immediately by their single argument. The gray level
 * (ie. the argument that follows "gray") should be a number between 0 and 1, with
 * 0 for black and 1 for white.
 *
 * To pass straight PostScript through enclose the appropriate commands in double
 * quotes. Straight PostScript is only bracketed by the outermost gsave/grestore
 * pair (ie. the one from the initial "x X BeginPath") although that's probably
 * a mistake. Suspect I may have to change the double quote delimiters.
 *
 */

    pend = buf + strlen(buf);
    p = strtok(buf, " \n");

    while ( p != NULL ) {
	if ( gsavelevel == 0 ) {
	    fprintf(tf, "gsave\n");
	    gsavelevel++;
	}   /* End if */
	if ( strcmp(p, "stroke") == 0 ) {
	    fprintf(tf, "closepath stroke\ngrestore\n");
	    gsavelevel--;
	} else if ( strcmp(p, "openstroke") == 0 ) {
	    fprintf(tf, "stroke\ngrestore\n");
	    gsavelevel--;
	} else if ( strcmp(p, "fill") == 0 ) {
	    fprintf(tf, "eofill\ngrestore\n");
	    gsavelevel--;
	} else if ( strcmp(p, "wfill") == 0 ) {
	    fprintf(tf, "fill\ngrestore\n");
	    gsavelevel--;
	} else if ( strcmp(p, "sfill") == 0 ) {
	    fprintf(tf, "eofill\ngrestore\ngsave\nstroke\ngrestore\n");
	    gsavelevel--;
	} else if ( strncmp(p, "gray", strlen("gray")) == 0 ) {
	    p1 = strtok(NULL, " \n");
	    fprintf(tf, "%s setgray\n", p1);
	} else if ( strncmp(p, "color", strlen("color")) == 0 ) {
	    p1 = strtok(NULL, " \n");
	    fprintf(tf, "/%s setcolor\n", p1);
	} else if ( strncmp(p, "line", strlen("line")) == 0 ) {
	    p1 = strtok(NULL, " \n");
	    fprintf(tf, "%s resolution mul 2 div setlinewidth\n", p1);
	} else if ( strncmp(p, "reverse", strlen("reverse")) == 0 )
	    fprintf(tf, "reversepath\n");
	else if ( *p == '"' ) {
	    for ( ; gsavelevel > 0; gsavelevel-- )
		fprintf(tf, "grestore\n");
	    if ( (p1 = p + strlen(p)) < pend )
		*p1 = ' ';
	    p = strtok(p, "\"\n");
	    fprintf(tf, "%s\n", p);
	}   /* End else */
	p = strtok(NULL, " \n");
    }	/* End while */

    for ( ; gsavelevel > 0; gsavelevel-- )
	fprintf(tf, "grestore\n");

}   /* End of parsebuf */

/*****************************************************************************/

static void
getbaseline(void)

{

/*
 *
 * Responsible for making sure the PostScript procedures needed for printing text
 * along an arbitrary baseline are downloaded from *baselinefile. Done at most
 * once per job, and only if the the stuff is really used.
 *
 */

    if ( gotbaseline == FALSE )
	exportfile(baselinefile);

    if ( tf == stdout )
	gotbaseline = TRUE;

}   /* End of getbaseline */

/*****************************************************************************/

void
newbaseline(char *buf)

    /* char	*buf;			/ * whatever followed "x X NewBaseline" */

{

    char	*p;			/* for eliminating white space etc. */

/*
 *
 * Called from devcntrl() whenever an "x X NewBaseline" command is recognized. We
 * assume whatever is in *buf is a set of parametric equations that describe the
 * new baseline. Equations for x(t), y(t), dx/dt, and dy/dt must be written in
 * PostScript, bracketed by { and } characters, and supplied in exactly that order.
 * In particular the equation for x must come first in *buf and it ends up as the
 * last one on the stack, while the equation for dy/dt comes last (in *buf) and
 * ends up on the top of the PostScript stack. For example if *buf is given by,
 *
 *	{} {180 mul 3.1416 div cos} {pop 1} {180 mul 3.1416 div sin neg}
 *
 * text will be printed along the curve y = cos(x).
 *
 * Angles given in radians must be converted to degrees for the PostScript trig
 * functions, and things are scaled so that 1 unit maps into 1 inch. In the last
 * example the cosine curve that describes the baseline has an amplitude of 1 inch.
 * As another example of this rather confusing syntax if *buf is,
 *
 *	{} {} {pop 1} {pop 1}
 *
 * the baseline will be the 45 degree line y = x.
 *
 * When any of the four functions is used they're called with a single number on
 * the stack that's equal to the current value of the parameter t. The coordinate
 * system axes run parallel to the PostScript coordinate system that's currently
 * being used.
 *
 */

    for ( p = buf; *p; p++ )		/* eliminate trailing '\n' */
	if ( *p == '\n' ) {
	    *p = '\0';
	    break;
	}   /* End if */

    for ( p = buf; *p && (*p == ' ' || *p == ':'); p++ ) ;

    if ( *p != '\0' ) {			/* something's there */
	flushtext();
	getbaseline();
	fprintf(tf, "mark resolution %s newbaseline\n", p);
	reset();
    }	/* End if */

}   /* End of newbaseline */

/*****************************************************************************/

void
drawtext(char *buf)

    /* char	*buf;			/ * whatever followed "x X DrawText */

{

    char	*p;			/* for eliminating white space etc. */

/*
 *
 * Called from devcntrl() whenever an "x X DrawText command is recognized. *buf
 * should contain three arguments in the following order. First comes the text we
 * want to print along the current baseline. Right now the string should be given
 * as a PostScript string using characters '(' and ')' as the delimiters. Next in
 * *buf comes a justification mode that can be the words left, right, or center.
 * Last comes a number that represents the starting value of the parameter t that's
 * given as the argument to the parametric equations that describe the current
 * baseline. For example if *buf is given by,
 *
 *	(hello world) left .5
 *
 * hello world will be printed along the path described by the current baseline
 * and left justified at whatever (x(.5), y(.5)) happens to be. Usually will be
 * preceeded by an "x X NewBaseline" call that defines the current baseline. The
 * origin of the coordinate system used by the parametric equations will be the
 * current point.
 *
 */

    for ( p = buf; *p; p++ )		/* eliminate trailing '\n' */
	if ( *p == '\n' ) {
	    *p = '\0';
	    break;
	}   /* End if */

    for ( p = buf; *p && (*p == ' ' || *p == ':'); p++ ) ;

    if ( *p != '\0' ) {			/* something's there */
	flushtext();
	getbaseline();
	xymove(hpos, vpos);
	fprintf(tf, "mark %s drawfunnytext\n", p);
	resetpos();
    }	/* End if */

}   /* End of drawtext */

/*****************************************************************************/

void
settext(char *buf)

{

    char	*p;

/*
 *
 * Does whatever is needed to ensure any text that follows will be set along the
 * curve described by the PostScript procedures listed in *buf. If *buf doesn't
 * contain anything useful (eg. just a newline) things are restored to whatever
 * they originally were. Doesn't work well if we try to start in the middle of a
 * line of text.
 *
 * The parametric equations needed are,
 *
 *	x = f(t)
 *	y = g(t)
 *	dx/dt = f'(t)
 *	dy/dt = g'(t)
 *
 * and must be given as proper PostScript procedures. The equation for x must come
 * first (ie. it ends up on the bottom of the stack) and the equation for dy/dt
 * must be given last (ie. it ends up on top of the stack). For example if *buf
 * is given by,
 *
 *	{} {180 mul 3.1416 div cos} {pop 1} {180 mul 3.1416 div sin neg}
 *
 * text will be set along the curve y=cos(x).
 *
 */

    flushtext();
    getbaseline();

    for ( p = buf; *p && *p == ' '; p++ ) ;

    if ( *p && *p != '\n' ) {
	encoding = maxencoding + 2;
	fprintf(tf, "mark resolution %s newbaseline\n", buf);
    } else encoding = realencoding;

    fprintf(tf, "%d setdecoding\n", encoding);
    resetpos();

}   /* End of settext */

/*****************************************************************************/

