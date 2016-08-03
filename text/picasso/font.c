/*
 *
 * Typesetter font tables routines - for postprocessors.
 *
 */

/*
 * A slightly modified version of the dpost original -- DBK
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"			/* font table definitions */
#include "picasso.h"
#include "input.h"

TrFont	*dwb_mount[MAXFONTS+1];		/* mount table - pointers into fonts[] */
TrFont	fonts[MAXFONTS+2];		/* font data - guarantee one empty slot */

int	fcount;				/* entries in fonts[] */
int	mcount;				/* fonts currently in memory */
int	mlimit = MAXFONTS+1;		/* and the most we'll allow */

char	*chnames[MAXCH];		/* special character hash table */
int	nchnames;			/* number of entries in chnames[] */

extern int	devres;
extern int	unitwidth;
extern int	nfonts;
extern char	**lfonts;

static int getfont(char *, TrFont *);
static void freefonts(void);
static int findfont(char *);
static int mounted(int);
static int chindex(char *);
static int chadd(char *);
static int hash(char *, int);
static void release(void *);

/*****************************************************************************/

int
getdesc(char *path)

{

    char	buf[150];
    FILE	*fp;
    int		n;

/*
 *
 * Read a typesetter description file. Font and size lists are discarded. Only
 * used to get to the start of the next command.
 *
 */

    if ( (fp = fopen(path, "r")) == NULL )
	return(-1);

    while ( fscanf(fp, "%s", buf) != EOF ) {
	if ( strcmp(buf, "res") == 0 )
	    fscanf(fp, "%d", &devres);
	else if ( strcmp(buf, "unitwidth") == 0 )
	    fscanf(fp, "%d", &unitwidth);
	else if ( strcmp(buf, "sizes") == 0 )
	    while ( fscanf(fp, "%d", &n) != EOF && n != 0 ) ;
	else if ( strcmp(buf, "inmemory") == 0 )
	    fscanf(fp, "%d", &mlimit);
	else if ( strcmp(buf, "fonts") == 0 ) {
	    fscanf(fp, "%d", &nfonts);
	    if (nfonts > 0)
		lfonts = (char **) malloc(nfonts * sizeof (char *));
	    for ( n = 0; n < nfonts; n++ ) {
		fscanf(fp, "%s", buf);
		lfonts[n] = strsave(buf);
	    }
	} else if ( strcmp(buf, "charset") == 0 ) {
	    while ( fscanf(fp, "%s", buf) != EOF )
		chadd(buf);
	    break;
	}   /* End if */
	skipline(fp);
    }	/* End while */

    fclose(fp);
    return(1);

}   /* End of getdesc */

/*****************************************************************************/

static int
getfont(char *path, TrFont *fpos)

{

    FILE	*fin;
    Chwid	chtemp[MAXCH];
    static	Chwid chinit;
    int		i, nw = 0, n, wid, code;
    int		nospace = 1;
    char	buf[150], ch[20], s1[10], s2[10], s3[10], cmd[30];


/*
 *
 * Read a width table from path into *fpos. Skip unnamed characters, spacewidth,
 * ligatures, ascender/descender entries, and anything else not recognized. All
 * calls should come through mountfont().
 *
 */

    if ( fpos->state == INMEMORY )
	return(1);

    if ( (fin = fopen(path, "r")) == NULL )
	return(-1);

    if ( fpos->state == NEWFONT ) {
	if ( ++fcount > MAXFONTS+1 )
	    return(-1);
	fpos->path = strsave(path);
    }	/* End if */

    if ( ++mcount > mlimit && mcount > nfonts+1 )
	freefonts();

    for ( i = 0; i < 128-32; i++ )
	chtemp[i] = chinit;

    while ( fscanf(fin, "%s", cmd) != EOF ) {
	if ( strcmp(cmd, "name") == 0 ) {
	    release(fpos->name);
	    fscanf(fin, "%s", buf);
	    fpos->name = strsave(buf);
	} else if ( strcmp(cmd, "fontname") == 0 ) {
	    release(fpos->fontname);
	    fscanf(fin, "%s", buf);
	    fpos->fontname = strsave(buf);
	} else if ( strcmp(cmd, "special") == 0 )
	    fpos->specfont = 1;
	else if ( strcmp(cmd, "named") == 0 )	/* in prologue or somewhere else */
	    fpos->flags |= NAMED;
	else if (strcmp(cmd, "spacewidth") == 0) {
	    fscanf(fin, "%d", &wid);
	    chtemp[0].wid = wid;
	    chtemp[0].num = ' ';
	    nospace = 0;
	}
	else if ( strcmp(cmd, "charset") == 0 ) {
	    skipline(fin);
	    nw = 128-32;
	    while ( fgets(buf, sizeof(buf), fin) != NULL ) {
		sscanf(buf, "%s %s %s %s", ch, s1, s2, s3);
		if ( s1[0] != '"' ) {		/* not a synonym */
		    sscanf(s1, "%d", &wid);
		    if (s3[0] == '0')
			sscanf(s3, "%o", &code);
		    else sscanf(s3, "%d", &code);
		}   /* End if */
		if ( strlen(ch) == 1 ) {	/* it's ascii */
		    n = ch[0] - 32;		/* origin omits non-graphics */
		    chtemp[n].num = ch[0];
		    chtemp[n].wid = wid;
		    chtemp[n].code = code;
		} else if ( strcmp(ch, "---") != 0 ) {	/* ignore unnamed chars */
		    if ( (n = chindex(ch)) == -1 )	/* global? */
			n = chadd(ch);
		    chtemp[nw].num = n;
		    chtemp[nw].wid = wid;
		    chtemp[nw].code = code;
		    nw++;
		}   /* End else */
	    }	/* End while */
	    break;
	}   /* End else */
	skipline(fin);
    }	/* End while */

    fclose(fin);

    if (nospace) {
	chtemp[0].wid = 25;
	chtemp[0].num = ' ';
    }

    fpos->wp = (Chwid *)allocate(nw * sizeof(Chwid));
    for ( i = 0; i < nw; i++ )
	fpos->wp[i] = chtemp[i];

    fpos->nchars = nw;
    fpos->state = INMEMORY;

    return(1);

}   /* End of getfont */

/*****************************************************************************/

int
mountfont(char *path, int m)

{

    TrFont	*fpos;

/*
 *
 * Mount font table from file path at position m. Mounted fonts are guaranteed
 * to be in memory.
 *
 */

    if ( m < 0 || m > MAXFONTS )
	return(-1);

    if ( dwb_mount[m] != NULL ) {
	if (dwb_mount[m]->path != NULL && strcmp(path, dwb_mount[m]->path) == 0 ) {
	    if ( dwb_mount[m]->state == INMEMORY )
		return(1);
	} else {
	    dwb_mount[m]->mounted--;
	    dwb_mount[m] = NULL;
	}   /* End else */
    }	/* End if */

    dwb_mount[m] = fpos = &fonts[findfont(path)];
    dwb_mount[m]->mounted++;
    return(getfont(path, fpos));

}   /* End of mountfont */

/*****************************************************************************/

static void
freefonts(void)

{

    int		n;

/*
 *
 * Release memory used by all unmounted fonts - except for the path string.
 *
 */

    for ( n = 0; n < MAXFONTS+2; n++ )
	if ( fonts[n].state == INMEMORY && fonts[n].mounted == 0 ) {
	    release(fonts[n].wp);
	    fonts[n].wp = NULL;
	    fonts[n].state = RELEASED;
	    mcount--;
	}   /* End if */

}   /* End of freefonts */

/*****************************************************************************/

static int
findfont(char *path)

{

    int n;

/*
 *
 * Look for path in the fonts table. Returns the index where it was found or can
 * be inserted (if not found).
 *
 */

    for ( n = hash(path, MAXFONTS+2); fonts[n].state != NEWFONT; n = (n+1) % (MAXFONTS+2) )
	if ( strcmp(path, fonts[n].path) == 0 )
	    break;
    return(n);

}   /* End of findfont */

/*****************************************************************************/

static int
mounted(int m)

{

/*
 *
 * Return 1 if a font is mounted at position m.
 *
 */

    return((m >= 0 && m <= MAXFONTS && dwb_mount[m] != NULL) ? 1 : 0);

}   /* End of mounted */

/*****************************************************************************/

int
onfont(int c, int m)

{

    register TrFont	*fp;
    register Chwid	*cp, *ep;

/*
 *
 * Returns the position of character c in the font mounted at m, or -1 if the
 * character is not found.
 *
 */

    if ( mounted(m) ) {
	fp = dwb_mount[m];
	if ( c < 128 ) {
	    if ( fp->wp[c-32].num == c )	/* ascii at front */
		return c - 32;
#if 0
	    else return -1;
#endif
	}   /* End if */

	cp = &fp->wp[128-32];
	ep = &fp->wp[fp->nchars];
	for ( ; cp < ep; cp++ )			/* search others */
#if 0
    	    if ( cp->num == c )
#else
    	    if ( cp->code == c )
#endif
		return cp - &fp->wp[0];
    }	/* End if */

    return -1;

}   /* End of onfont */

/*****************************************************************************/

int
chwidth(int n, int m)

{

/*
 *
 * Return width of the character at position n in the font mounted at m. Skip
 * bounds checks - assume it's already been done.
 *
 */

    return(dwb_mount[m]->wp[n].wid);

}   /* End of chwidth */

/*****************************************************************************/

#if 0
static int
chcode(int n, int m)

{

/*
 *
 * Return typesetter code for the character at position n in the font mounted
 * at m. Skip bounds checks - assume it's already been done. 
 *
 */

    return(dwb_mount[m]->wp[n].code);

}   /* End of chcode */
#endif

/*****************************************************************************/

static int
chindex(char *s)

{

    int i;

/*
 *
 * Look for s in global character name table. Hash table is guaranteed to have
 * at least one empty slot (by chadd()) so the loop terminate.
 *
 */

    for ( i = hash(s, MAXCH); chnames[i] != NULL; i = (i+1) % MAXCH )
	if ( strcmp(s, chnames[i]) == 0 )
	    return(i+128);
    return(-1);

}   /* End of chindex */

/*****************************************************************************/

static int
chadd(char *s)

{

    int i;

/*
 *
 * Add s to the global character name table. Leave one empty slot so loops
 * terminate.
 *
 */

    if ( nchnames >= MAXCH - 1 )
	yyerror("out of table space adding character %s", s);

    for ( i = hash(s, MAXCH); chnames[i] != NULL; i = (i+1) % MAXCH ) ;

    nchnames++;
    chnames[i] = strsave(s);

    return(i+128);

}   /* End of chadd */

/*****************************************************************************/

char *chname(n)

    int		n;

{

/*
 *
 * Returns string for the character with index n.
 *
 */

    return(chnames[n-128]);

}   /* End of chname */

/*****************************************************************************/

static int
hash(char *s, int l)

{

    int i;

/*
 *
 * Return the hash index for string s in a table of length l. Probably should
 * make i unsigned and mod once at the end.
 *
 */

    for ( i = 0; *s != '\0'; s++ )
	i = (i * 10 + *s) % l;
    return(i);

}   /* End of hash */

/*****************************************************************************/

char *strsave(s)

    char	*s;

{

    char	*ptr = NULL;

/*
 *
 * Make a permanent copy of string s.
 *
 */

    if ( s != NULL ) {
	ptr = (char *)allocate(strlen(s)+1);
	strcpy(ptr, s);
    }	/* End if */
    return(ptr);

}   /* End of strsave */

/*****************************************************************************/

char *allocate(count)

    int		count;

{

    char	*ptr;

/*
 *
 * Allocates count bytes. Free unmounted fonts if the first attempt fails. To
 * be absolutely correct all memory allocation should be done by this routine.
 *
 */

    if ( (ptr = (char *)malloc(count)) == NULL ) {
	freefonts();
	if ( (ptr = (char *)malloc(count)) == NULL )
	    yyerror("out of space in allocate (font.c)");
    }	/* End if */
    return(ptr);

}   /* End of allocate */

/*****************************************************************************/

static void
release(void *ptr)

{

/*
 *
 * Free memory provided ptr isn't NULL.
 *
 */

    if ( ptr != NULL )
	free(ptr);

}   /* End of release */

/*****************************************************************************/

#if 0
static void
dumpmount(int m)

{

/*
 *
 * Dumps the font mounted at position n.
 *
 */

    if ( dwb_mount[m] != NULL )
	dumpfont((dwb_mount[m] - &fonts[0]));
    else fprintf(stderr, "no font mounted at %d\n", m);

}   /* End of dumpmount */
#endif

/*****************************************************************************/

#if 0
static void
dumpfont(int n)

{

    int		i;
    TrFont	*fpos;
    char	*str;

/*
 *
 * Dump of everything known about the font saved at fonts[n].
 *
 */

    fpos = &fonts[n];

    if ( fpos->state ) {
	fprintf(stderr, "path %s\n", fpos->path);
	fprintf(stderr, "state %d\n", fpos->state);
	fprintf(stderr, "flags %d\n", fpos->flags);
	fprintf(stderr, "mounted %d\n", fpos->mounted);
	fprintf(stderr, "nchars %d\n", fpos->nchars);
	fprintf(stderr, "special %d\n", fpos->specfont);
	fprintf(stderr, "name %s\n", fpos->name);
	fprintf(stderr, "fontname %s\n", fpos->fontname);
	if ( fpos->state == INMEMORY ) {
	    fprintf(stderr, "charset\n");
	    for ( i = 0; i < fpos->nchars; i++ ) {
		if ( fpos->wp[i].num > 0 ) {
		    if ( fpos->wp[i].num < 128 )
			fprintf(stderr, "%c\t%d\t%d\n", fpos->wp[i].num,
				fpos->wp[i].wid, fpos->wp[i].code);
		    else {
			str = chname(fpos->wp[i].num);
			if ( *str == '#' && isdigit(*(str+1)) && isdigit(*(str+2)) )
			    str = "---";
			fprintf(stderr, "%s\t%d\t%d\n", str, fpos->wp[i].wid,
				fpos->wp[i].code);
		    }	/* End else */
		}   /* End if */
	    }	/* End for */
	} else fprintf(stderr, "charset: not in memory\n");
    } else fprintf(stderr, "empty font: %d\n", n);

    putc('\n', stderr);

}   /* End of dumpfont */
#endif

/*****************************************************************************/

