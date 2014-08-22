#include "e.h"
#include <ctype.h>

#ifndef UNANSI
void setsize(char *p)	/* set size as found in p */
#else /* UNANSI */
void setsize(p)	/* set size as found in p */
	char *p;
#endif /* UNANSI */
{
	nszstack++;
	szstack[nszstack] = 0;		/* assume relative */
	if (*p == '+') {
		ps += atoi(p+1);
		if (szstack[nszstack-1] != 0)	/* propagate absolute size */
			szstack[nszstack] = ps;
	} else if (*p == '-') {
		ps -= atoi(p+1);
		if (szstack[nszstack-1] != 0)
			szstack[nszstack] = ps;
	} else if (isdigit(*p)) {
		if (szstack[nszstack-1] == 0)
			printf(".nr %d \\n(.s\n", 99-nszstack);
		else
			printf(".nr %d %d\n", 99-nszstack, ps);
		szstack[nszstack] = ps = atoi(p);
	} else {
		ERROR "illegal size %s ignored", p WARNING;
	}
	dprintf(".\tsetsize %s; ps = %d\n", p, ps);
}

int size(int p1, int p2)
{
		/* old size in p1, new in ps */
	dwb_yyval = p2;
	dprintf(".\tS%d <- \\s%d %d \\s%d; b=%g, h=%g\n", 
		dwb_yyval, ps, p2, p1, ebase[dwb_yyval], eht[dwb_yyval]);
	if (szstack[nszstack] != 0) {
		printf(".ds %d %s\\*(%d\\s\\n(%d\n", dwb_yyval, ABSPS(ps), p2, 99-nszstack);
	} else
		printf(".ds %d %s\\*(%d%s\n", dwb_yyval, DPS(p1,ps), p2, DPS(ps,p1));
	nszstack--;
	ps = p1;
	return dwb_yyval;
}

int globsize(void)
{
	char temp[20];

	getstr(temp, sizeof(temp));
	if (temp[0] == '+') {
		gsize += atoi(temp+1);
		if (szstack[0] != 0)
			szstack[0] = gsize;
	} else if (temp[0] == '-') {
		gsize -= atoi(temp+1);
		if (szstack[0] != 0)
			szstack[0] = gsize;
	} else  if (isdigit(temp[0])) {
		gsize = atoi(temp);
		szstack[0] = gsize;
		printf(".nr 99 \\n(.s\n");
	} else {
		ERROR "illegal gsize %s ignored", temp WARNING;
	}
	dwb_yyval = eqnreg = 0;
	ps = gsize;
	if (gsize < 12 && !dps_set)		/* sub and sup size change */
		deltaps = gsize / 3;
	else if (gsize < 20)
		deltaps = gsize / 4;
	else
		deltaps = gsize / 5;
	return dwb_yyval;
}
