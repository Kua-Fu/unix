#
/*
 */

/*
 * RK disk driver
 */

#include "../param.h"
#include "../buf.h"
#include "../conf.h"
#include "../user.h"

#define	RKADDR	0177400
#define	NRK	4
#define	NRKBLK	4872

#define	RESET	0
#define	GO	01
#define	DRESET	014
#define	IENABLE	0100
#define	DRY	0200
#define	ARDY	0100
#define	WLO	020000
#define	CTLRDY	0200

struct {
	int rkds; // drive status寄存器，表示磁盘状态，只有在表示磁盘驱动中发生错误时候才使用
	int rker; // error寄存器，表示错误状态，只有在表示磁盘驱动中发生错误时候才使用
	int rkcs; // control status寄存器，用于控制磁盘
	int rkwc; // word count寄存器，表示待传送数据的长度
	int rkba; // current bus address寄存器，表示内存中待传送数据的地址
	int rkda; // disk address寄存器，表示磁盘中待传送数据的地址
};

struct	devtab	rktab;
struct	buf	rrkbuf;

rkstrategy(abp)
struct buf *abp;
{
	register struct buf *bp;
	register *qc, *ql;
	int d;

	bp = abp;
	if(bp->b_flags&B_PHYS)
		mapalloc(bp);
	d = bp->b_dev.d_minor-7;
	if(d <= 0)
		d = 1;
	if (bp->b_blkno >= NRKBLK*d) {
		bp->b_flags =| B_ERROR;
		iodone(bp);
		return;
	}
	bp->av_forw = 0;
	spl5();
	if (rktab.d_actf==0)
		rktab.d_actf = bp;
	else
		rktab.d_actl->av_forw = bp;
	rktab.d_actl = bp;
	if (rktab.d_active==0)
		rkstart();
	spl0();
}

rkaddr(bp)
struct buf *bp;
{
	register struct buf *p;
	register int b;
	int d, m;

	p = bp;
	b = p->b_blkno;
	m = p->b_dev.d_minor - 7;
	if(m <= 0)
		d = p->b_dev.d_minor;
	else {
		d = lrem(b, m);
		b = ldiv(b, m);
	}
	return(d<<13 | (b/12)<<4 | b%12);
}

rkstart()
{
	register struct buf *bp;

	if ((bp = rktab.d_actf) == 0)
		return;
	rktab.d_active++;
	devstart(bp, &RKADDR->rkda, rkaddr(bp), 0);
}

rkintr()
{
	register struct buf *bp;

	if (rktab.d_active == 0)
		return;
	bp = rktab.d_actf;
	rktab.d_active = 0;
	if (RKADDR->rkcs < 0) {		/* error bit */
		deverror(bp, RKADDR->rker, RKADDR->rkds);
		RKADDR->rkcs = RESET|GO;
		while((RKADDR->rkcs&CTLRDY) == 0) ;
		if (++rktab.d_errcnt <= 10) {
			rkstart();
			return;
		}
		bp->b_flags =| B_ERROR;
	}
	rktab.d_errcnt = 0;
	rktab.d_actf = bp->av_forw;
	iodone(bp);
	rkstart();
}

rkread(dev)
{

	physio(rkstrategy, &rrkbuf, dev, B_READ);
}

rkwrite(dev)
{

	physio(rkstrategy, &rrkbuf, dev, B_WRITE);
}
