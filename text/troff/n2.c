/*
 * n2.c
 *
 * output, cleanup
 */

#include "tdef.h"
#include "fns.h"
#include "ext.h"
#include <setjmp.h>


extern	jmp_buf	sjbuf;
int	toolate;
int	error;

char	obuf[2*BUFSIZ];
char	*obufp = obuf;

int	xon	= 0;	/* records if in middle of \X */

int pchar(Tchar i)
{
	int j;
	static int hx = 0;	/* records if have seen HX */

	if (hx) {
		hx = 0;
		j = absmot(i);
		if (isnmot(i)) {
			if (j > dip->blss)
				dip->blss = j;
		} else {
			if (j > dip->alss)
				dip->alss = j;
			ralss = dip->alss;
		}
		return 0;
	}
	if (ismot(i)) {
		pchar1(i); 
		return 0;
	}
	switch (j = cbits(i)) {
	case 0:
	case IMP:
	case RIGHT:
	case LEFT:
		return 0;
	case HX:
		hx = 1;
		return 0;
	case XON:
		xon++;
		break;
	case XOFF:
		xon--;
		break;
	case PRESC:
		if (!xon && !tflg && dip == &d[0])
			j = eschar;	/* fall through */
	default:
		setcbits(i, trtab[j]);
	}
	pchar1(i);
	return 0;
}


void pchar1(Tchar i)
{
	int j;

	j = cbits(i);
	if (dip != &d[0]) {
		wbf(i);
		dip->op = offset;
		return;
	}
	if (!tflg && !print) {
		if (j == '\n')
			dip->alss = dip->blss = 0;
		return;
	}
	if (j == FILLER && !xon)
		return;
	if (tflg) {	/* transparent mode, undiverted */
		if (print)			/* assumes that it's ok to print */
			/* OUT "%c", j PUT;	/* i.e., is ascii */
			outascii(i);
		return;
	}
	if (TROFF && ascii)
		outascii(i);
	else
		ptout(i);
}


void outascii(Tchar i)	/* print i in best-guess ascii */
{
	char *p;
	int j = cbits(i);

/* is this ever called with NROFF set? probably doesn't work at all. */

	if (ismot(i))
		oput(' ');
	else if (j < ALPHABET && j >= ' ' || j == '\n' || j == '\t')
		oput(j);
	else if (j == DRAWFCN)
		oputs("\\D");
	else if (j == HYPHEN)
		oput('-');
	else if (j == MINUS)	/* special pleading for strange encodings */
		oputs("\\-");
	else if (j == PRESC)
		oputs("\\e");
	else if (j == FILLER)
		oputs("\\&");
	else if (j == UNPAD)
		oputs("\\ ");
	else if (j == OHC)	/* this will never occur;  stripped out earlier */
		oputs("\\%");
	else if (j == XON)
		oputs("\\X");
	else if (j == XOFF)
		oputs(" ");
	else if (j == LIG_FI)
		oputs("fi");
	else if (j == LIG_FL)
		oputs("fl");
	else if (j == LIG_FF)
		oputs("ff");
	else if (j == LIG_FFI)
		oputs("ffi");
	else if (j == LIG_FFL)
		oputs("ffl");
	else if (j == WORDSP) {		/* nothing at all */
		if (xon)		/* except in \X */
			oput(' ');

	} else if (j >= ALPHABET && j < nchnames + ALPHABET) {	/* \N DOESN'T WORK YET */
		p = chname(j);
		if (strlen(p) == 2)
			OUT "\\(%s", p PUT;
		else
			OUT "\\C'%s'", p PUT;
	} else {	/* WEIRD CHARACTER NAME */
		OUT " %d? ", j PUT;

	}
}

int flusho(void)
{
	if (NROFF && !toolate && t.twinit)
			fwrite(t.twinit, strlen(t.twinit), 1, ptid);

	if (obufp > obuf) {
		*obufp = 0;
		fputs(obuf, ptid);
		fflush(ptid);
		obufp = obuf;
	}
	toolate++;
	return 1;
}


void caseex(void)
{
	done(0);
}


void done(int x) 
{
	int i;

	error |= x;
	app = ds = lgf = 0;
	if (i = em) {
		donef = -1;
		eschar = '\\';
		em = 0;
		if (control(i, 0))
			longjmp(sjbuf, 1);
	}
	if (!nfo)
		done3(0);
	mflg = 0;
	dip = &d[0];
	if (woff)	/* BUG!!! This isn't set anywhere */
		wbf((Tchar)0);
	if (pendw)
		getword(1);
	pendnf = 0;
	if (donef == 1)
		done1(0);
	donef = 1;
	ip = 0;
	frame = stk;
	nxf = frame + 1;
	if (!ejf)
		tbreak();
	nflush++;
	eject((Stack *)0);
	longjmp(sjbuf, 1);
}


void done1(int x) 
{
	error |= x;
	if (numtabp[NL].val) {
		trap = 0;
		eject((Stack *)0);
		longjmp(sjbuf, 1);
	}
	if (!ascii)
		pttrailer();
	done2(0);
}


void done2(int x) 
{
	ptlead();
	if (TROFF && !ascii)
		ptstop();
	flusho();
	done3(x);
}

void done3(int x) 
{
	error |= x;
	flusho();
	if (NROFF)
		twdone();
	if (pipeflg)
		pclose(ptid);
	exit(error);
}


void edone(int x) 
{
	frame = stk;
	nxf = frame + 1;
	ip = 0;
	done(x);
}


void casepi(void)
{
	char buf[NTM];
	
	dwb_getline(buf, NTM);
	if (toolate || buf[0] == '\0' || (ptid = popen(buf, "w")) == NULL) {
		ERROR "pipe %s not created.", buf WARN;
		return;
	}
	toolate++;
	pipeflg++;
}
