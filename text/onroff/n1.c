/*
 * n1.c
 *
 *	consume options, initialization, main loop,
 *	input routines, escape function calling
 */

#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <setjmp.h>
/* define VAR to get routines that use varargs; undef it to get original troff code */
#define VAR
#ifdef VAR
#include <varargs.h>
#endif

#include "tdef.h"
#include "ext.h"

#ifdef NROFF
#include "tw.h"
#endif

jmp_buf sjbuf;
extern	char	*sprintf();
static	char *sprintn();
static	printn();
filep	ipl[NSO];
long	offl[NSO];
long	ioff;
char	cfname[NSO+1][NS] = {  "stdin" };	/*file name stack*/
int	cfline[NSO];		/*input line count stack*/
char	*progname;	/* program name (troff) */

extern	void catch(), kcatch();

main(argc, argv)
int	argc;
char	**argv;
{
	register char	*p;
	register j;
	register tchar i;
	int eileenct;		/*count to test for "Eileen's loop"*/
	char	**oargv, *getenv();

	progname = argv[0];
	signal(SIGHUP, catch);
	if (signal(SIGINT, catch) == SIG_IGN) {
		signal(SIGHUP, SIG_IGN);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
	}
	signal(SIGPIPE, catch);
	signal(SIGTERM, kcatch);
	oargv = argv;
	mrehash();
	nrehash();
	init0();

#ifdef NROFF
	if ((p = getenv("NROFFTERM")) != 0)
		strcpy(devname, p);
#else
	if ((p = getenv("TYPESETTER")) != 0)
		strcpy(devname, p);
#endif

	while (--argc > 0 && (++argv)[0][0] == '-')
		switch (argv[0][1]) {

		case 'F':	/* switch font tables from default */
			if (argv[0][2] != '\0') {
				strcpy(termtab, &argv[0][2]);
				strcpy(fontdir, &argv[0][2]);
			} else {
				argv++; argc--;
				strcpy(termtab, argv[0]);
				strcpy(fontdir, argv[0]);
			}
			continue;
		case 0:
			goto start;
		case 'i':
			stdi++;
			continue;
		case 'q':
#ifdef	NROFF
			quiet++;
			save_tty();
#else
			errprint("-q option ignored in troff");
#endif	NROFF
			continue;
		case 'n':
			npn = ctoi(&argv[0][2]);
			continue;
		case 'u':	/* set emboldening amount */
			bdtab[3] = ctoi(&argv[0][2]);
			if (bdtab[3] < 0 || bdtab[3] > 50)
				bdtab[3] = 0;
			continue;
		case 's':
			if (!(stop = ctoi(&argv[0][2])))
				stop++;
			continue;
		case 'r':
			eibuf = sprintf(ibuf+strlen(ibuf), ".nr %c %s\n",
				argv[0][2], &argv[0][3]);
			continue;
		case 'c':
		case 'm':
			if (mflg++ >= NMF) {
				errprint("Too many macro packages: %s", argv[0]);
				continue;
			}
			strcpy (mfiles[nmfi], nextf);
			strcat (mfiles[nmfi++], &argv[0][2]);
			continue;
		case 'o':
			getpn(&argv[0][2]);
			continue;
		case 'T':
			strcpy(devname, &argv[0][2]);
			dotT++;
			continue;
#ifdef NROFF
		case 'h':
			hflg++;
			continue;
		case 'z':
			no_out++;
			continue;
		case 'e':
			eqflg++;
			continue;
#endif
#ifndef NROFF
		case 'z':
			no_out++;
		case 'a':
			ascii = 1;
			nofeed++;
			continue;
		case 'f':
			nofeed++;
			continue;
#endif
		default:
			errprint("unknown option %s", argv[0]);
			done(02);
		}

start:
	init1(oargv[0][0]);
	argp = argv;
	rargc = argc;
	nmfi = 0;
	init2();
	setjmp(sjbuf);
	eileenct = 0;		/*reset count for "Eileen's loop"*/
loop:
	copyf = lgf = nb = nflush = nlflg = 0;
	if (ip && rbf0(ip) == 0 && ejf && frame->pframe <= ejl) {
		nflush++;
		trap = 0;
		eject((struct s *)0);
		if (eileenct > 20) {
			errprint("job looping; check abuse of macros");
			ejf = 0;	/*try to break Eileen's loop*/
			eileenct = 0;
		} else
			eileenct++;
		goto loop;
	}
	eileenct = 0;		/*reset count for "Eileen's loop"*/
	i = getch();
	if (pendt)
		goto Lt;
	if ((j = cbits(i)) == XPAR) {
		copyf++;
		tflg++;
		while (cbits(i) != '\n')
			pchar(i = getch());
		tflg = 0;
		copyf--;
		goto loop;
	}
	if (j == cc || j == c2) {
		if (j == c2)
			nb++;
		copyf++;
		while ((j = cbits(i = getch())) == ' ' || j == '\t')
			;
		ch = i;
		copyf--;
		control(getrq(), 1);
		flushi();
		goto loop;
	}
Lt:
	ch = i;
	text();
	if (nlflg)
		numtab[HP].val = 0;
	goto loop;
}


void
catch()
{
	done3(01);
}


void
kcatch()
{
	signal(SIGTERM, SIG_IGN);
	done3(01);
}


init0()
{
	eibuf = ibufp = ibuf;
	ibuf[0] = 0;
	numtab[NL].val = -1;
}


init1(a)
char	a;
{
	register char	*p;
	char	*mktemp();
	register i;

	p = mktemp("/usr/tmp/trtmpXXXXX");
	if (a == 'a')
		p = &p[9];
	if ((close(creat(p, 0600))) < 0) {
		errprint("cannot create temp file.");
		exit(-1);
	}
	ibf = open(p, 2);
	unlkp = p;
	for (i = NTRTAB; --i; )
		trtab[i] = i;
	trtab[UNPAD] = ' ';
}


init2()
{
	register i;
	extern char	*setbrk();

	ttyod = 2;
	iflg = 0;
	obufp = obuf;
	ptinit();
	mchbits();
	cvtime();
	numtab[PID].val = getpid();
	olinep = oline;
	ioff = 0;
	numtab[HP].val = init = 0;
	numtab[NL].val = -1;
	nfo = 0;
	ifile = 0;
	copyf = raw = 0;
	eibuf = sprintf(ibuf+strlen(ibuf), ".ds .T %s\n", devname);
	numtab[CD].val = -1;	/* compensation */
	cpushback(ibuf);
	ibufp = ibuf;
	nx = mflg;
	frame = stk = (struct s *)setbrk(STACKSIZE);
	dip = &d[0];
	nxf = frame + 1;
#ifdef INCORE
	for (i = 0; i < NEV; i++) {
		extern tchar corebuf[];
		envcopy((struct env *) &corebuf[i * sizeof(env)/sizeof(tchar)], &env);
	}
#else
	for (i = NEV; i--; )
		write(ibf, (char *) & env, sizeof(env));
#endif
}

#include <time.h>

cvtime()
{
	long	tt;
	struct tm *ltime, *localtime();

	time(&tt);
	ltime = localtime(&tt);
	numtab[YR].val = ltime->tm_year;
	numtab[MO].val = ltime->tm_mon + 1;	/* troff uses 1..12 */
	numtab[DY].val = ltime->tm_mday;
	numtab[DW].val = ltime->tm_wday + 1;	/* troff uses 1..7 */

/*	register i;
/*	static int ms[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
/* 	tt -= 3600 * ZONE;	/* 5hrs for EST */
/* 	numtab[DY].val = (tt / 86400L) + 1;
/* 	numtab[DW].val = (numtab[DY].val + 3) % 7 + 1;
/* 	for (numtab[YR].val = 70; ; numtab[YR].val++) {
/* 		if ((numtab[YR].val) % 4)
/* 			ms[1] = 28;
/* 		else 
/* 			ms[1] = 29;
/* 		for (i = 0; i < 12; ) {
/* 			if (numtab[DY].val <= ms[i]) {
/* 				numtab[MO].val = i + 1;
/* 				return;
/* 			}
/* 			numtab[DY].val -= ms[i++];
/* 		}
/* 	}
*/
}


ctoi(s)
	register char *s;
{
	register n;

	while (*s == ' ')
		s++;
	n = 0;
	while (isdigit(*s))
		n = 10 * n + *s++ - '0';
	return n;
}


#ifndef VAR
errprint(s, s1, s2, s3, s4, s5)	/* error message printer */
	char *s, *s1, *s2, *s3, *s4, *s5;
{
	fdprintf(stderr, "%s: ", progname);
	fdprintf(stderr, s, s1, s2, s3, s4, s5);
	if (numtab[CD].val > 0)
		fdprintf(stderr, "; line %d, file %s", numtab[CD].val, cfname[ifi]);
	fdprintf(stderr, "\n");
	stackdump();
}
#else /* VAR */
/* VARARGS */
errprint(va_alist)	/* error message printer */
va_dcl
{
	va_list ap;

	fdprintf(stderr, "%s: ", progname);
	va_start(ap);
	fdprintf1(stderr, &ap);
	va_end(ap);
	if (numtab[CD].val > 0)
		fdprintf(stderr, "; line %d, file %s", numtab[CD].val, cfname[ifi]);
	fdprintf(stderr, "\n");
	stackdump();
}
#endif /* VAR */

/*
 * Scaled down version of C Library printf.
 * Only %s %u %d (==%u) %o %c %x %D are recognized.
 */
#define	putchar(n)	(*pfbp++ = (n))	/* NO CHECKING! */

static char	pfbuf[NTM];
static char	*pfbp = pfbuf;
int	stderr	 = 2;	/* NOT stdio value */

#ifndef VAR
/* VARARGS2 */
fdprintf(fd, fmt, x1)
int	fd;
register char	*fmt;
{
	register c;
	register char *adx;
	char	*s;
	register i;

	pfbp = pfbuf;
	adx = (char*)&x1;
loop:
	while ((c = *fmt++) != '%') {
		if (c == '\0') {
			if (fd == stderr)
				write(stderr, pfbuf, (int)(pfbp - pfbuf));
			else {
				*pfbp = 0;
				pfbp = pfbuf;
				while (*pfbp) {
					*obufp++ = *pfbp++;
					if (obufp >= &obuf[OBUFSZ])
						flusho();
				}
			}
			return;
		}
		putchar(c);
	}
	c = *fmt++;
	if (c == 'd') {
		i = *(int*)adx;
		if (i < 0) {
			putchar('-');
			i = -i;
		}
		printn((long)i, 10);
		adx += sizeof(int);
	} else if (c == 'u' || c == 'o' || c == 'x') {
		i = *(int*)adx;
		printn((long)i, c == 'o' ? 8 : (c == 'x' ? 16 : 10));
		adx += sizeof(int);
	}
	else if (c == 'c') {
		c = *(int*)adx & 0377;
		if (c > 0177 || (c < 040 && c != '\n' && c != '\t'))
			putchar('\\');
		putchar(c & 0177);
		adx += sizeof(int);
	} else if (c == 's') {
		s = *(char**)adx;
		while (c = *s++)
			putchar(c);
		adx += sizeof(char**);
	} else if (c == 'D') {
		printn(*(long*)adx, 10);
		adx += sizeof(long);
	} else if (c == 'O') {
		printn(*(long*)adx, 8);
		adx += sizeof(long);
	}
	goto loop;
}
#else /* VAR */
/* VARARGS */
fdprintf(va_alist)
va_dcl
{
	va_list ap;
	int fd;

	va_start(ap);
	fd = va_arg(ap, int);
	fdprintf1(fd, &ap);
	va_end(ap);
}

fdprintf1(fd, app)
int	fd;
va_list *app;
{
	register c;
	char	*s;
	register i;
	register char	*fmt;

	pfbp = pfbuf;
	fmt = va_arg(*app, char *);
loop:
	while ((c = *fmt++) != '%') {
		if (c == '\0') {
			if (fd == stderr)
				write(stderr, pfbuf, (int)(pfbp - pfbuf));
			else {
				*pfbp = 0;
				pfbp = pfbuf;
				while (*pfbp) {
					*obufp++ = *pfbp++;
					if (obufp >= &obuf[OBUFSZ])
						flusho();
				}
			}
			return;
		}
		putchar(c);
	}
	c = *fmt++;
	if (c == 'd') {
		i = va_arg(*app, int);
		if (i < 0) {
			putchar('-');
			i = -i;
		}
		printn((long)i, 10);
	} else if (c == 'u' || c == 'o' || c == 'x') {
		i = va_arg(*app, int);
		printn((long)i, c == 'o' ? 8 : (c == 'x' ? 16 : 10));
	}
	else if (c == 'c') {
		c = va_arg(*app, int) & 0377;
		if (c > 0177 || (c < 040 && c != '\n' && c != '\t'))
			putchar('\\');
		putchar(c & 0177);
	} else if (c == 's') {
		s = va_arg(*app, char *);
		while (c = *s++)
			putchar(c);
	} else if (c == 'D') {
		printn(va_arg(*app, long), 10);
	} else if (c == 'O') {
		printn(va_arg(*app, long), 8);
	}
	goto loop;
}
#endif /* VAR */


/*
 * Print an unsigned integer in base b.
 */
static printn(n, b)
register long	n;
register b;
{
	char s[20];
	register char *p;
	register c;

	if (n < b) {
		if(n >= 0) {
			putchar("0123456789ABCDEF"[n]);
			return;
		}
		putchar('-');
		n = -n;
	}
	p = s+20;
	*--p = 0;
	for(;;) {
		*--p = "0123456789ABCDEF"[n % b];
		if(!(n /= b))
			break;
	}
	while(c = *p++)
		putchar(c);
}

/* scaled down version of library sprintf */
/* same limits as fdprintf */
/* returns pointer to \0 that ends the string */

#ifndef VAR
/* VARARGS2 */
char *sprintf(str, fmt, x1)
char	*str;
char	*fmt;
{
	register c;
	register char *adx;
	char	*s;
	register i;

	adx = (char*)&x1;
loop:
	while ((c = *fmt++) != '%') {
		if (c == '\0') {
			*str = 0;
			return str;
		}
		*str++ = c;
	}
	c = *fmt++;
	if (c == 'd') {
		i = *(int*)adx;
		if (i < 0) {
			*str++ = '-';
			i = -i;
		}
		str = sprintn(str, (long)i, 10);
		adx += sizeof(int);
	} else if (c == 'u' || c == 'o' || c == 'x') {
		i = *(int*)adx;
		str = sprintn(str, (long)i, c == 'o' ? 8 : (c == 'x' ? 16 : 10));
		adx += sizeof(int);
	}
	else if (c == 'c') {
		c = *(int*)adx & 0377;
		if (c > 0177 || c < 040)
			*str++ = '\\';
		*str++ = c & 0177;
		adx += sizeof(int);
	} else if (c == 's') {
		s = *(char**)adx;
		while (c = *s++)
			*str++ = c;
		adx += sizeof(char**);
	} else if (c == 'D') {
		str = sprintn(str, *(long*)adx, 10);
		adx += sizeof(long);
	} else if (c == 'O') {
		str = sprintn(str, *(long*)adx, 8);
		adx += sizeof(long);
	}
	goto loop;
}
#else /* VAR */
/* VARARGS */
char *sprintf(va_alist)
va_dcl
{
	char *str;
	char *fmt;
	register c;
	char	*s;
	register i;
	va_list ap;


	va_start(ap);
	str = va_arg(ap, char *);
	fmt = va_arg(ap, char *);
loop:
	while ((c = *fmt++) != '%') {
		if (c == '\0') {
			va_end(ap);
			*str = 0;
			return str;
		}
		*str++ = c;
	}
	c = *fmt++;
	if (c == 'd') {
		i = va_arg(ap, int);
		if (i < 0) {
			*str++ = '-';
			i = -i;
		}
		str = sprintn(str, (long)i, 10);
	} else if (c == 'u' || c == 'o' || c == 'x') {
		i = va_arg(ap, int);
		str = sprintn(str, (long)i, c == 'o' ? 8 : (c == 'x' ? 16 : 10));
	}
	else if (c == 'c') {
		c = va_arg(ap, int) & 0377;
		if (c > 0177 || c < 040)
			*str++ = '\\';
		*str++ = c & 0177;
	} else if (c == 's') {
		s = va_arg(ap, char *);
		while (c = *s++)
			*str++ = c;
	} else if (c == 'D') {
		str = sprintn(str, va_arg(ap, long), 10);
	} else if (c == 'O') {
		str = sprintn(str, va_arg(ap, long), 8);
	}
	goto loop;
}
#endif /* VAR */

/*
 * Print an unsigned integer in base b.
 */
static char *sprintn(s, n, b)
	register char *s;
	register long n;
{
	register long	a;

	if (n < 0) {	/* shouldn't happen */
		*s++ = '-';
		n = -n;
	}
	if (a = n / b)
		s = sprintn(s, a, b);
	*s++ = "0123456789ABCDEF"[(int)(n%b)];
	return s;
}


control(a, b)
register int	a, b;
{
	register int	j;

	if (a == 0 || (j = findmn(a)) == -1)
		return(0);
	if (contab[j].f == 0) {
		nxf->nargs = 0;
		if (b)
			collect();
		flushi();
		return pushi(contab[j].mx, a);
	} else if (b)
		return((*contab[j].f)(0));
	else
		return(0);
}


getrq()
{
	register i, j;

	if (((i = getach()) == 0) || ((j = getach()) == 0))
		goto rtn;
	i = PAIR(i, j);
rtn:
	return(i);
}

/*
 * table encodes some special characters, to speed up tests
 * in getchar, viz FLSS, RPT, f, \b, \n, fc, tabch, ldrch
 */

char
gchtab[NCHARS] = {
	000,004,000,000,010,000,000,000, /* fc, ldr */
	001,002,001,000,001,000,000,000, /* \b, tab, nl, RPT */
	000,000,000,000,000,000,000,000,
	000,001,000,000,000,000,000,000, /* FLSS */
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,001,000, /* f */
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
};

tchar
getch()
{
	register int	k;
	register tchar i, j;
	tchar setht(), setslant();

g0:
	if(ch) {
		i = ch;
		if (cbits(i) == '\n')
			nlflg++;
		ch = 0;
		return(i);
	}

	if (nlflg)
		return('\n');
	i = getch0();
	if (ismot(i))
		return(i);
	k = cbits(i);
	if (k != ESC) {
		if (k >= sizeof(gchtab)/sizeof(gchtab[0]) || gchtab[k] == 0)
			return(i);
		if (k == '\n') {
			if (cbits(i) == '\n') {
				nlflg++;
				if (ip == 0)
					numtab[CD].val++; /* line number */
			}
			return(k);
		}
		if (k == FLSS) {
			copyf++; 
			raw++;
			i = getch0();
			if (!fi)
				flss = i;
			copyf--; 
			raw--;
			goto g0;
		}
		if (k == RPT) {
			setrpt();
			goto g0;
		}
		if (!copyf) {
			if (k == 'f' && lg && !lgf) {
				i = getlg(i);
				return(i);
			}
			if (k == fc || k == tabch || k == ldrch) {
				if ((i = setfield(k)) == 0)
					goto g0; 
				else 
					return(i);
			}
			if (k == '\b') {
				i = makem(-width(' ' | chbits));
				return(i);
			}
		}
		return(i);
	}
	k = cbits(j = getch0());
	if (ismot(j))
		return(j);
	switch (k) {

	case '\n':	/* concealed newline */
		goto g0;
	case 'n':	/* number register */
		setn();
		goto g0;
	case '*':	/* string indicator */
		setstr();
		goto g0;
	case '$':	/* argument indicator */
		seta();
		goto g0;
	case '{':	/* LEFT */
		i = LEFT;
		goto gx;
	case '}':	/* RIGHT */
		i = RIGHT;
		goto gx;
	case '"':	/* comment */
		while (cbits(i = getch0()) != '\n')
			;
		nlflg++;
		if (ip == 0)
			numtab[CD].val++;
		return(i);
	case ESC:	/* double backslash */
		i = eschar;
		goto gx;
	case 'e':	/* printable version of current eschar */
		i = PRESC;
		goto gx;
	case ' ':	/* unpaddable space */
		i = UNPAD;
		goto gx;
	case '\'':	/* \(aa */
		i = ACUTE;
		goto gx;
	case '`':	/* \(ga */
		i = GRAVE;
		goto gx;
	case '_':	/* \(ul */
		i = UNDERLINE;
		goto gx;
	case '-':	/* current font minus */
		i = MINUS;
		goto gx;
	case '&':	/* filler */
		i = FILLER;
		goto gx;
	case 'c':	/* to be continued */
		i = CONT;
		goto gx;
	case '!':	/* transparent indicator */
		i = XPAR;
		goto gx;
	case 't':	/* tab */
		i = '\t';
		return(i);
	case 'a':	/* leader (SOH) */
/* old:		*pbp++ = LEADER; goto g0; */
		i = LEADER;
		return i;
	case '%':	/* ohc */
		i = OHC;
		return(i);
	case 'g':	/* return format of a number register */
		setaf();
		goto g0;
	case '.':	/* . */
		i = '.';
gx:
		setsfbits(i, sfbits(j));
		return(i);
	}
	if (copyf) {
		*pbp++ = j;
		return(eschar);
	}
	switch (k) {

	case 'p':	/* spread */
		spread++;
		goto g0;
	case '(':	/* special char name \(xx */
	case 'C':	/* 		\C'...' */
		if ((i = setch(k)) == 0)
			goto g0;
		return(i);
	case 'N':	/* absolute character number */
		if ((i = setabs()) == 0)
			goto g0;
		return i;
	case 's':	/* size indicator */
		setps();
		goto g0;
	case 'H':	/* character height */
		return(setht());
	case 'S':	/* slant */
		return(setslant());
	case 'f':	/* font indicator */
		setfont(0);
		goto g0;
	case 'w':	/* width function */
		setwd();
		goto g0;
	case 'v':	/* vert mot */
		if (i = vmot())
			return(i);
		goto g0;
	case 'h': 	/* horiz mot */
		if (i = hmot())
			return(i);
		goto g0;
	case 'z':	/* zero with char */
		return(setz());
	case 'l':	/* hor line */
		setline();
		goto g0;
	case 'L':	/* vert line */
		setvline();
		goto g0;
	case 'D':	/* drawing function */
		setdraw();
		goto g0;
	case 'X':	/* \X'...' for copy through */
		setxon();
		goto g0;
	case 'b':	/* bracket */
		setbra();
		goto g0;
	case 'o':	/* overstrike */
		setov();
		goto g0;
	case 'k':	/* mark hor place */
		if ((k = findr(getsn())) != -1) {
			numtab[k].val = numtab[HP].val;
		}
		goto g0;
	case '0':	/* number space */
		return(makem(width('0' | chbits)));
#ifdef NROFF
	case '|':
	case '^':
		goto g0;
#else
	case '|':	/* narrow space */
		return(makem((int)(EM)/6));
	case '^':	/* half narrow space */
		return(makem((int)(EM)/12));
#endif
	case 'x':	/* extra line space */
		if (i = xlss())
			return(i);
		goto g0;
	case 'u':	/* half em up */
	case 'r':	/* full em up */
	case 'd':	/* half em down */
		return(sethl(k));
	default:
		return(j);
	}
	/* NOTREACHED */
}

setxon()	/* \X'...' for copy through */
{
	tchar xbuf[NC];
	register tchar *i;
	tchar c;
	int delim, k;

	if (ismot(c = getch()))
		return;
	delim = cbits(c);
	i = xbuf;
	*i++ = XON | chbits;
	while ((k = cbits(c = getch())) != delim && k != '\n' && i < xbuf+NC-1) {
		if (k == ' ')
			setcbits(c, WORDSP);
		*i++ = c | ZBIT;
	}
	*i++ = XOFF | chbits;
	*i = 0;
	pushback(xbuf);
}


char	ifilt[32] = {
	0, 001, 002, 003, 0, 005, 006, 007, 010, 011, 012};

tchar
getch0()
{
	register int	j;
	register tchar i;

again:
	if (pbp > lastpbp)
		i = *--pbp;
	else if (ip) {
#ifdef INCORE
		extern tchar corebuf[];
		i = corebuf[ip];
		if (i == 0)
			i = rbf();
		else {
			if ((++ip & (BLK - 1)) == 0) {
				--ip;
				(void)rbf();
			}
		}
#else
		i = rbf();
#endif
	} else {
		if (donef || ndone)
			done(0);
		if (nx || ibufp >= eibuf) {
			if (nfo==0) {
g0:
				if (nextfile()) {
					if (ip)
						goto again;
					if (ibufp < eibuf)
						goto g2;
				}
			}
			nx = 0;
			if ((j = read(ifile, ibuf, IBUFSZ)) <= 0)
				goto g0;
			ibufp = ibuf;
			eibuf = ibuf + j;
			if (ip)
				goto again;
		}
g2:
		i = *ibufp++ & 0177;
		ioff++;
		if (i >= 040 && i < 0177)
			goto g4;
		if (i != 0177) 
			i = ifilt[i];
	}
	if (cbits(i) == IMP && !raw)
		goto again;
	if ((i == 0 || i == 0177) && !init && !raw) {
		goto again;
	}
g4:
	if (ismot(i))
		return i;
	if (copyf == 0 && sfbits(i) == 0)
		i |= chbits;
	if (cbits(i) == eschar && !raw)
		setcbits(i, ESC);
	return(i);
}

pushback(b)
register tchar *b;
{
	register tchar *ob = b;

	while (*b++)
		;
	b--;
	while (b > ob && pbp < &pbbuf[NC-3])
		*pbp++ = *--b;
	if (pbp >= &pbbuf[NC-3]) {
		errprint("pushback overflow");
		done(2);
	}
}

cpushback(b)
register char *b;
{
	register char *ob = b;

	while (*b++)
		;
	b--;
	while (b > ob && pbp < &pbbuf[NC-3])
		*pbp++ = *--b;
	if (pbp >= &pbbuf[NC-3]) {
		errprint("cpushback overflow");
		done(2);
	}
}

nextfile()
{
	register char	*p;

n0:
	if (ifile)
		close(ifile);
	if (nx  ||  nmfi < mflg) {
		p = mfiles[nmfi++];
		if (*p != 0)
			goto n1;
	}
	if (ifi > 0) {
		if (popf())
			goto n0; /* popf error */
		return(1); /* popf ok */
	}
	if (rargc-- <= 0) {
		if ((nfo -= mflg) && !stdi)
			done(0);
		nfo++;
		numtab[CD].val = ifile = stdi = mflg = 0;
		strcpy(cfname[ifi], "stdin");
		ioff = 0;
		return(0);
	}
	p = (argp++)[0];
n1:
	numtab[CD].val = 0;
	if (p[0] == '-' && p[1] == 0) {
		ifile = 0;
		strcpy(cfname[ifi], "stdin");
	} else if ((ifile = open(p, 0)) < 0) {
		errprint("cannot open file %s", p);
		nfo -= mflg;
		done(02);
	} else
		strcpy(cfname[ifi],p);
	nfo++;
	ioff = 0;
	return(0);
}


popf()
{
	register i;
	register char	*p, *q;

	ioff = offl[--ifi];
	numtab[CD].val = cfline[ifi];		/*restore line counter*/
	ip = ipl[ifi];
	if ((ifile = ifl[ifi]) == 0) {
		p = xbuf;
		q = ibuf;
		ibufp = xbufp;
		eibuf = xeibuf;
		while (q < eibuf)
			*q++ = *p++;
		return(0);
	}
	if (lseek(ifile, (long)(ioff & ~(IBUFSZ-1)), 0) == (long) -1
	   || (i = read(ifile, ibuf, IBUFSZ)) < 0)
		return(1);
	eibuf = ibuf + i;
	ibufp = ibuf;
/*
	if (ttyname(ifile) == 0)
*/
		if ((ibufp = ibuf + (int)(ioff & (IBUFSZ - 1))) > eibuf)
			return(1);
	return(0);
}


flushi()
{
	if (nflush)
		return;
	ch = 0;
	copyf++;
	while (!nlflg) {
		if (donef && (frame == stk))
			break;
		getch();
	}
	copyf--;
}


getach()
{
	register tchar i;
	register j;

	lgf++;
	j = cbits(i = getch());
	/* this test ought to be more general and more careful */
	if (ismot(i) || j == ' ' || j == '\n' || j == RIGHT || j & 0200) {
		ch = i;
		j = 0;
	}
	lgf--;
	return(j & 0177);
}


casenx()
{
	lgf++;
	skip();
	getname();
	nx++;
	if (nmfi > 0)
		nmfi--;
	strcpy(mfiles[nmfi], nextf);
	nextfile();
	nlflg++;
	ip = 0;
	pendt = 0;
	frame = stk;
	nxf = frame + 1;
}


getname()
{
	register int	j, k;
	tchar i;

	lgf++;
	for (k = 0; k < (NS - 1); k++) {
		if (((j = cbits(i = getch())) <= ' ') || (j > 0176))
			break;
		nextf[k] = j;
	}
	nextf[k] = 0;
	ch = i;
	lgf--;
	return(nextf[0]);
}


caseso()
{
	register i;
	register char	*p, *q;

	lgf++;
	nextf[0] = 0;
	if (skip() || !getname() || ((i = open(nextf, 0)) < 0) || (ifi >= NSO)) {
		errprint("can't open file %s", nextf);
		done(02);
	}
	strcpy(cfname[ifi+1], nextf);
	cfline[ifi] = numtab[CD].val;		/*hold line counter*/
	numtab[CD].val = 0;
	flushi();
	ifl[ifi] = ifile;
	ifile = i;
	offl[ifi] = ioff;
	ioff = 0;
	ipl[ifi] = ip;
	ip = 0;
	nx++;
	nflush++;
	if (!ifl[ifi++]) {
		p = ibuf;
		q = xbuf;
		xbufp = ibufp;
		xeibuf = eibuf;
		while (p < eibuf)
			*q++ = *p++;
	}
}

caself()	/* set line number and file */
{
	int n;

	if (skip())
		return;
	n = atoi();
	cfline[ifi] = numtab[CD].val = n - 2;
	if (skip())
		return;
	if (getname())
		strcpy(cfname[ifi], nextf);
}


casecf()
{	/* copy file without change */
#ifndef NROFF
	int	fd, n;
	char	buf[512];
	extern int hpos, esc, po;

	lgf++;
	nextf[0] = 0;
	if (skip() || !getname() || (fd = open(nextf, 0)) < 0) {
		errprint("can't open file %s", nextf);
		done(02);
	}
	lgf--;
	/* make it into a clean state, be sure that everything is out */
	tbreak();
	hpos = po;
	esc = 0;
	ptesc();	/* to left margin */
	esc = un;
	ptesc();
	ptlead();
	ptps();
	ptfont();
	flusho();
	while ((n = read(fd, buf, sizeof buf)) > 0)
		write(ptid, buf, n);
	close(fd);
	ptps();
	ptfont();
#endif
}


casesy()	/* call system */
{
	char	sybuf[NTM];
	int	i;

	lgf++;
	copyf++;
	skip();
	for (i = 0; i < NTM - 2; i++)
		if ((sybuf[i] = getch()) == '\n' || sybuf[i] == RIGHT)
			break;
	sybuf[i] = 0;
	system(sybuf);
	copyf--;
	lgf--;
}


getpn(a)
	register char *a;
{
	register int n, neg;

	if (*a == 0)
		return;
	neg = 0;
	for ( ; *a; a++)
		switch (*a) {
		case '+':
		case ',':
			continue;
		case '-':
			neg = 1;
			continue;
		default:
			n = 0;
			if (isdigit(*a)) {
				do
					n = 10 * n + *a++ - '0';
				while (isdigit(*a));
				a--;
			} else
				n = 9999;
			*pnp++ = neg ? -n : n;
			neg = 0;
			if (pnp >= &pnlist[NPN-2]) {
				errprint("too many page numbers");
				done3(-3);
			}
		}
	if (neg)
		*pnp++ = -9999;
	*pnp = -32767;
	print = 0;
	pnp = pnlist;
	if (*pnp != -32767)
		chkpn();
}


setrpt()
{
	tchar i, j;

	copyf++;
	raw++;
	i = getch0();
	copyf--;
	raw--;
	if (i < 0 || cbits(j = getch0()) == RPT)
		return;
	i &= BYTEMASK;
	while (i>0 && pbp < &pbbuf[NC-3]) {
		i--;
		*pbp++ = j;
	}
}
