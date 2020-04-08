#
/*
 */

#include "../param.h"
#include "../systm.h"
#include "../reg.h"
#include "../buf.h"
#include "../filsys.h"
#include "../user.h"
#include "../inode.h"
#include "../file.h"
#include "../conf.h"

/*
 * the fstat system call.
 */
fstat()
{
	register *fp;

	fp = getf(u.u_ar0[R0]);
	if(fp == NULL)
		return;
	stat1(fp->f_inode, u.u_arg[0]);
}

/*
 * the stat system call.
 */
stat()
{
	register ip;
	extern uchar;

	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	stat1(ip, u.u_arg[1]);
	iput(ip);
}

/*
 * The basic routine for fstat and stat:
 * get the inode and pass appropriate parts back.
 */
stat1(ip, ub)
int *ip;
{
	register i, *bp, *cp;

	iupdat(ip, time);
	bp = bread(ip->i_dev, ldiv(ip->i_number+31, 16));
	cp = bp->b_addr + 32*lrem(ip->i_number+31, 16) + 24;
	ip = &(ip->i_dev);
	for(i=0; i<14; i++) {
		suword(ub, *ip++);
		ub =+ 2;
	}
	for(i=0; i<4; i++) {
		suword(ub, *cp++);
		ub =+ 2;
	}
	brelse(bp);
}

/*
 * the dup system call.
 */
dup()
{
	register i, *fp;

	fp = getf(u.u_ar0[R0]);
	if(fp == NULL)
		return;
	if ((i = ufalloc()) < 0)
		return;
	u.u_ofile[i] = fp;
	fp->f_count++;
}

/*
 * the mount system call.
 */
smount() // 系统调用mount的处理函数，参数列表: (1) u.u_arg[0], 与挂载设备相对应的特殊文件的路径；(2) u.u_arg[1]，挂载点的路径; (3) u.u_arg[2] 读写标志，如果为1，表示以只读方式挂载
{
	int d;
	register *ip;
	register struct mount *mp, *smp;
	extern uchar;

	d = getmdev(); // 执行getmdev()函数取得准备挂载的块设备的大编号
	if(u.u_error)
		return;
	u.u_dirp = u.u_arg[1]; // 取得代表挂载点的inode[]元素
	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	if(ip->i_count!=1 || (ip->i_mode&(IFBLK&IFCHR))!=0) // 确认无人访问代表挂载点的inode[]元素，同时确认该元素不为字符设备或者块设备的特殊文件
		goto out;
	smp = NULL; // 寻找mount[]中未使用的元素，（1）如果不存在未使用元素，（2）或者准备挂载的设备已经被挂载，则认为出错
	for(mp = &mount[0]; mp < &mount[NMOUNT]; mp++) {
		if(mp->m_bufp != NULL) {
			if(d == mp->m_dev) // （2）
				goto out;
		} else
		if(smp == NULL) // （1）
			smp = mp;
	}
	if(smp == NULL)
		goto out;
	(*bdevsw[d.d_major].d_open)(d, !u.u_arg[2]); // 进行打开挂载设备的处理
	if(u.u_error)
		goto out;
	mp = bread(d, 1); // 将挂载设备的超级块读入到缓冲区
	if(u.u_error) {
		brelse(mp);
		goto out1;
	}
	smp->m_inodp = ip; // 对所取得的mount[]元素进行初始化处理，通过getblk()取得尚未分配给任何设备的块设备缓冲区，并注册到mount[]元素
	smp->m_dev = d;
	smp->m_bufp = getblk(NODEV);
	bcopy(mp->b_addr, smp->m_bufp->b_addr, 256);
	smp = smp->m_bufp->b_addr;
	smp->s_ilock = 0;
	smp->s_flock = 0;
	smp->s_ronly = u.u_arg[2] & 1;
	brelse(mp); // 释放读取超级块的缓冲区
	ip->i_flag =| IMOUNT; // 设置代表挂载点的inode[]元素的IMOUNT标志位
	prele(ip); // 解除代表挂载点的inode[]元素的锁
	return;

out:
	u.u_error = EBUSY;
out1:
	iput(ip);
}

/*
 * the umount system call.
 */
sumount() // 系统调用umount对应的处理函数，用于卸载指定块设备的文件系统，从mount[]中释放相应的元素，并清除代表挂载点的inode[]元素的IMOUNT标志位，
{         // 如果准备卸载的设备仍处于使用中状态，则中断卸载处理， 参数为: (1) u.u_arg[0]，挂载点的路径
	int d;
	register struct inode *ip;
	register struct mount *mp;

	update(); // 卸载前，先将内存中数据保存到块设备
	d = getmdev(); // 取得准备卸载的设备的设备编号
	if(u.u_error)
		return;
	for(mp = &mount[0]; mp < &mount[NMOUNT]; mp++) // 在mount[]中寻找与卸载设备相对应的元素
		if(mp->m_bufp!=NULL && d==mp->m_dev)
			goto found;
	u.u_error = EINVAL;
	return;

found:
	for(ip = &inode[0]; ip < &inode[NINODE]; ip++) // 在inode[]中寻找属于卸载设备的元素，如果存在则说明该设备仍处于使用中的状态，此时将终止卸载处理
		if(ip->i_number!=0 && d==ip->i_dev) {
			u.u_error = EBUSY;
			return;
		}
	(*bdevsw[d.d_major].d_close)(d, 0); // 进行关闭卸载设备的处理
	ip = mp->m_inodp; // 清除与挂载点相对的inode[]元素的IMOUNT标志位，并释放该元素
	ip->i_flag =& ~IMOUNT;
	iput(ip);
	ip = mp->m_bufp; // 释放mount[]元素
	mp->m_bufp = NULL;
	brelse(ip); // 释放块设备缓冲区，该缓冲区容纳着被卸载设备的超级块的数据
}

/*
 * Common code for mount and umount.
 * Check that the user's argument is a reasonable
 * thing on which to mount, and return the device number if so.
 */
getmdev() // 执行smount()和 sumount()共用处理的函数，首先检查作为系统调用的第一个参数的特殊文件的路径是否何时，如果合适，则返回该设备的编号
{
	register d, *ip;
	extern uchar;

	ip = namei(&uchar, 0); // 取得与用户输入的第一个参数的路径相对应的inode[]元素
	if(ip == NULL)
		return;
	if((ip->i_mode&IFMT) != IFBLK) // 如果不为块设备的特殊文件则出错
		u.u_error = ENOTBLK;
	d = ip->i_addr[0]; // 特殊文件的inode.i_addr[0]中保存着设备编号
	if(ip->i_addr[0].d_major >= nblkdev) // 如果设备的大编号的值过大则出错
		u.u_error = ENXIO;
	iput(ip); // 释放inode[]元素，返回设备编号
	return(d);
}
