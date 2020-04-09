#
/*
 */

#include "../param.h"
#include "../systm.h"
#include "../filsys.h"
#include "../conf.h"
#include "../buf.h"
#include "../inode.h"
#include "../user.h"

/*
 * 块设备中的inode和存储区域中的块，分别由超级块中的inode空闲队列(filsys.s_inode[])和存储空闲队列(filsys.s_free[])管理
 * filsys.s_inode[]和filsys.s_free[]的元素数为100，分别保存着未分配的inode编号和未分配的存储区域的块编号
 * filsys.s_ninode和filsys.s_nfree分别容纳inode空闲队列和存储空闲你队列中编号的数量
 * 当这两个变量的值变为0，需要从块设备取得未分配的inode编号和块编号补充空闲队列
 *
 */

/*
 * iinit is called once (from main)
 * very early in initialization.
 * It reads the root's super block
 * and initializes the current date
 * from the last modified date.
 *
 * panic: iinit -- cannot read the super
 * block. Usually because of an IO error.
 */

/*
 * iinit()读取根磁盘的超级块，并将其赋予mount[]的第一个元素
 * 在系统启动时候，被main()调用，并且仅调用一次
 *
 */

iinit()
{
	register *cp, *bp;

	(*bdevsw[rootdev.d_major].d_open)(rootdev, 1); // 打开根磁盘的处理，如果是RK磁盘，则不做任何处理
	bp = bread(rootdev, 1); // 读取超级块的内容
	cp = getblk(NODEV); // 取得NODEV块设备的缓冲区，将超级块的内容复制到此缓冲区，并释放用来读取超级块的缓冲区
	if(u.u_error)
		panic("iinit");
	bcopy(bp->b_addr, cp->b_addr, 256);
	brelse(bp);
	mount[0].m_bufp = cp; // 将根磁盘注册到mount[0]
	mount[0].m_dev = rootdev;
	cp = cp->b_addr; // 对超级块解锁，并清除只读标记
	cp->s_flock = 0;
	cp->s_ilock = 0;
	cp->s_ronly = 0;
	time[0] = cp->s_time[0]; // 将表示时间的time复制到超级块的filsys.s_time
	time[1] = cp->s_time[1];
}

/*
 * alloc will obtain the next available
 * free disk block from the free list of
 * the specified device.
 * The super block has up to 100 remembered
 * free blocks; the last of these is read to
 * obtain 100 more . . .
 *
 * no space on dev x/y -- when
 * the free list is exhausted.
 */

/*
 * alloc()是分配块设备存储区域中未使用的块的函数，
 * 首先取得filsys.s_nfree指向的filsys.s_free[]元素，然后使用该块编号取得块设备的缓冲区
 * 如果空闲队列为空，则从块设备存储区域取得未使用的块编号，将其补充至空闲队列，此处的补充处理与针对inode的补充处理有很大的不同
 * 块设备的每99个未使用的块编号用一个队列管理，队列头部的元素保持着队列中的元素数和下一个队列的块编号
 * 这个未使用的块编号的队列在执行 /etc/mkfs时候生成，通过复制该队列补充存储空闲队列
 * 参数: (1) dev，设备编号
 */

alloc(dev)
{
	int bno;
	register *bp, *ip, *fp;

	fp = getfs(dev); // 取得与参数指定的设备编号相对应的filsys结构体
	while(fp->s_flock) // 如果filsys结构体被加锁，则进入睡眠状态直至解锁
		sleep(&fp->s_flock, PINOD);
	do { // 进行循环直至取得合适的块编号，如果取得的块编号 未指向存储区域，badblock()将返回1
		if(fp->s_nfree <= 0) // 如果空闲队列为空，则跳转到nospace
			goto nospace;
		bno = fp->s_free[--fp->s_nfree];
		if(bno == 0) // 从空闲队列取得块编号，如果为0则跳转到nospace
			goto nospace;
	} while (badblock(fp, bno, dev));
	if(fp->s_nfree <= 0) { // 如果取得了合适的块编号，空闲队列变为空时候，对其进行补充处理
		fp->s_flock++; // 对filsys结构体加锁
		bp = bread(dev, bno); // 取得的块编号指向容纳着下一个未使用的块编号队列的块，执行bread()读取该块
		ip = bp->b_addr;
		fp->s_nfree = *ip++; // 在读取的块的头部容纳着该队列中的块编号的数量，将其复制到filsys.s_nfree
		bcopy(ip, fp->s_free, 100); // 执行bcopy()，将队列整体复制到filsys.s_free
		brelse(bp); // 释放缓冲区
		fp->s_flock = 0; // 将filsys结构体解锁
		wakeup(&fp->s_flock); // 唤醒正在等待解锁filsys结构体的进程
	}
	bp = getblk(dev, bno); // 利用取得的块编号，执行getblk()，取得对应的缓冲区
	clrbuf(bp); // 将取得的缓冲区清0
	fp->s_fmod = 1; // 设置filsys结构体的更新标志位
	return(bp);

nospace:
	fp->s_nfree = 0;
	prdev("no space", dev);
	u.u_error = ENOSPC;
	return(NULL);
}

/*
 * place the specified disk block
 * back on the free list of the
 * specified device.
 */

/*
 * free() 用于释放存储区域的块，
 * 并将块编号追加到空闲队列，当空闲队列已满时候，将空闲队列的内容写入准备释放的块
 * 写入的内容为: 以当前的filsys.s_nfree为起始元素，其后跟随filsys.s_free[]元素，随后将filsys.s_nfree清0，并将filsys.s_free[0]设定为准备释放的块的块编号
 * 参数: (1) dev, 设备编号 (2) bno, 块编号
 */

free(dev, bno)
{
	register *fp, *bp, *ip;

	fp = getfs(dev); // 取得与参数指定的设备编号相对应的filsys结构体
	fp->s_fmod = 1; // 设置filsys结构体的更新标志位
	while(fp->s_flock) // 进入睡眠状态直至解锁filsys结构体
		sleep(&fp->s_flock, PINOD);
	if (badblock(fp, bno, dev)) // 如果参数的块设备的值有误，则返回
		return;
	if(fp->s_nfree <= 0) { // 如果filsys.s_nfree的值小于等于0，则将其设为1，并将filsys.s_free[0]设为0
		fp->s_nfree = 1;
		fp->s_free[0] = 0;
	}
	if(fp->s_nfree >= 100) { // 如果空闲队列已满，则将空闲队列写入块设备，并重置空闲队列
		fp->s_flock++; // 将filsys结构体加锁
		bp = getblk(dev, bno); // 取得与参数的块设备相对应的缓冲区
		ip = bp->b_addr;
		*ip++ = fp->s_nfree; // 将filsys.s_nfree保存至缓冲区的起始位置
		bcopy(fp->s_free, ip, 100); // 将空闲队列整体复制到缓冲区
		fp->s_nfree = 0; // 将filsys.s_nfree设定为0（将空闲队列清空）
		bwrite(bp); // 将保存空闲队列的缓冲区写入到块设备
		fp->s_flock = 0; // 将filsys结构体解锁
		wakeup(&fp->s_flock); // 唤醒正在等待解锁filsys结构体的进程
	}
	fp->s_free[fp->s_nfree++] = bno; // 向空闲队列追加块设备，如果之前为空闲队列已满情况，则空闲队列的头部将设定为下一个队列的块编号
	fp->s_fmod = 1; // 设置filsys结构体的更新标志位
}

/*
 * Check that a block number is in the
 * range between the I list and the size
 * of the device.
 * This is used mainly to check that a
 * garbage file system has not been mounted.
 *
 * bad block on dev x/y -- not in range
 */

/*
 * badblock()用于检查块编号是否合适，
 * 如果块编号指向块设备中的存储区域，则返回0，否则返回1
 * 参数: (1) afp, 超级块(filsys结构体) (2) abn, 块设备 (3) dev，设备编号
 */

badblock(afp, abn, dev)
{
	register struct filsys *fp;
	register char *bn;

	fp = afp;
	bn = abn;
	if (bn < fp->s_isize+2 || bn >= fp->s_fsize) {
		prdev("bad block", dev);
		return(1);
	}
	return(0);
}

/*
 * Allocate an unused I node
 * on the specified device.
 * Used with file creation.
 * The algorithm keeps up to
 * 100 spare I nodes in the
 * super block. When this runs out,
 * a linear search through the
 * I list is instituted to pick
 * up 100 more.
 */

/*
 * ialloc()用来分配块设备中inode区域的未分配inode
 * 首先取得由filesys.s_ninode指向的filsys.s_inode[]元素，然后通过inode编号取得inode[]元素，同时递减filsys.s_ninode,
 * 将filsys.s_inode[]看做存放未使用inode编号的栈，将filsys.s_ninode看做栈指针，可能更容易理解
 * filsys.s_inode[]为空时候，从块设备的inode区域的头部开始按照顺序检查inode，将未使用的inode编号追加到filsys.s_inode[]
 * 由于当filsys.s_inode[]为空时候，才需要访问块设备，这与每当收到分配块设备inode的请求时候，则立即访问块设备的做法相比，在性能上更具优势
 * 参数: (1)dev, 设备编号
 */

ialloc(dev)
{
	register *fp, *bp, *ip;
	int i, j, k, ino;

	fp = getfs(dev);  // 取得与参数的设备编号相对应的filsys结构体（超级块）
	while(fp->s_ilock) // 进入睡眠状态直至解锁filsys结构体
		sleep(&fp->s_ilock, PINOD);
loop:
	if(fp->s_ninode > 0) { // 当inode空闲队列中还存在未分配的inode编号时候的处理
		ino = fp->s_inode[--fp->s_ninode]; // 取得位于inode空闲队列（栈）头部的inode编号
		ip = iget(dev, ino); // 利用所取得的inode编号调用iget()以取得inode[]元素
		if (ip==NULL)
			return(NULL);
		if(ip->i_mode == 0) { // 取得的inode[]元素的i_mode为0时候的处理(释放inode时相应的inode[]元素的i_mode被清0，如果i_mode不为0则说明该inode[]元素仍然处于使用中的状态，或者inode的释放处理未能正常执行）
			for(bp = &ip->i_mode; bp < &ip->i_addr[8];) // 将inode[]元素从imode至i_addr的部分清0
				*bp++ = 0;
			fp->s_fmod = 1; // 将filsys.s_fmod置1，以设置filsys结构体的更新标志位
			return(ip); // 返回inode[]元素
		}
		/*
		 * Inode was allocated after all.
		 * Look some more.
		 */
		iput(ip); // 如果inode.i_mode的值不为0，则释放取得的inode[]元素，然后返回loop处，并且再次尝试取得inode
		goto loop;
	}
	fp->s_ilock++; // 如果空闲队列中不存在尚未分配的inode编号，则取得filsys结构体的锁
	ino = 0;
	for(i=0; i<fp->s_isize; i++) { // 遍历块设备的inode的块
		bp = bread(dev, i+2);
		ip = bp->b_addr;
		for(j=0; j<256; j=+16) { // 遍历块中的所有inode
			ino++;
			if(ip[j] != 0) // 如果inode.i_mode不为0，则表示该inode处于使用中的状态,执行continue
				continue;
			for(k=0; k<NINODE; k++) // 如果inode[]中存在相应的元素则跳转到cont,似乎是在块设备和内存中都确认该inode未被分配，所以才将其视作未分配的inode
			if(dev==inode[k].i_dev && ino==inode[k].i_number)
				goto cont;
			fp->s_inode[fp->s_ninode++] = ino; // 将inode编号追加至空闲队列
			if(fp->s_ninode >= 100) // 如果空闲队列已满，则执行break退出队列
				break;
		cont:;
		}
		brelse(bp);
		if(fp->s_ninode >= 100) // 如果空闲队列已满，则执行break退出循环，终止补充空闲队列的处理
			break;
	}
	fp->s_ilock = 0; // 释放filsys结构体的锁
	wakeup(&fp->s_ilock); // 唤醒正在等待解锁filsys结构体的进程
	if (fp->s_ninode > 0) // 如果向空闲队列至少补充一个未分配的inode编号，则返回loop执行分配inode的处理
		goto loop;
	prdev("Out of inodes", dev);
	u.u_error = ENOSPC;
	return(NULL);
}

/*
 * Free the specified I node
 * on the specified device.
 * The algorithm stores up
 * to 100 I nodes in the super
 * block and throws away any more.
 */

/*
 * ifree()用于释放inode, 将被释放的inode的inode编号追加到空闲队列
 * 如果空闲队列已满，则丢弃inode编号，丢弃的inode编号在通过ialloc()补充空闲队列时候被回收
 * 参数: (1) dev, 设备编号 (2) ino, inode编号
 */

ifree(dev, ino)
{
	register *fp;

	fp = getfs(dev); // 取得与参数指定的设备编号相对应的filsys结构体
	if(fp->s_ilock) // 如果filsys结构体被加锁，则不做任何处理立即返回，尽管此时未能将当前的inode编号回收到空闲队列，但是在通过ialloc()补充空闲队列时候，一定会进行回收
		return;
	if(fp->s_ninode >= 100) // 如果空闲队列已满时候，不做任何处理立即返回，同上，在ialloc()时候，会进行回收
		return;
	fp->s_inode[fp->s_ninode++] = ino; // 将inode编号返回给空闲队列
	fp->s_fmod = 1; // 设置filsys结构体的更新标志位
}

/*
 * getfs maps a device number into
 * a pointer to the incore super
 * block.
 * The algorithm is a linear
 * search through the mount table.
 * A consistency check of the
 * in core free-block and i-node
 * counts.
 *
 * bad count on dev x/y -- the count
 *	check failed. At this point, all
 *	the counts are zeroed which will
 *	almost certainly lead to "no space"
 *	diagnostic
 * panic: no fs -- the device is not mounted.
 *	this "cannot happen"
 */

/*
 * getfs()用来取得与设备编号相对应的filsys结构体（超级块）
 * 该函数在mount[]中寻找与参数指定的设备编号相对应的元素
 * 参数: (1) dev, 设备编号
 */

getfs(dev)
{
	register struct mount *p;
	register char *n1, *n2;

	for(p = &mount[0]; p < &mount[NMOUNT]; p++)
	if(p->m_bufp != NULL && p->m_dev == dev) {
		p = p->m_bufp->b_addr;
		n1 = p->s_nfree;
		n2 = p->s_ninode;
		if(n1 > 100 || n2 > 100) {
			prdev("bad count", dev);
			p->s_nfree = 0;
			p->s_ninode = 0;
		}
		return(p);
	}
	panic("no fs");
}

/*
 * update is the internal name of
 * 'sync'. It goes through the disk
 * queues to initiate sandbagged IO;
 * goes through the I nodes to write
 * modified nodes; and it goes through
 * the mount table to initiate modified
 * super blocks.
 */

/*
 * update()用来同步内存中的数据和块设备中的数据
 * 将尚未写入块设备的mount[], inode[]和buf[]的内容写入到块设备
 *
 */

update()
{
	register struct inode *ip;
	register struct mount *mp;
	register *bp;

	if(updlock) // 取得updlock的锁，如果已经加锁，则不做任何处理立即返回，updlock是用于排他处理的变量
		return;
	updlock++;
	for(mp = &mount[0]; mp < &mount[NMOUNT]; mp++) // 遍历mount[]
		if(mp->m_bufp != NULL) { // 如果未设置元素的更新标志位，或者已经被加锁，或者处于只读状态，则对该元素不做任何处理
			ip = mp->m_bufp->b_addr;
			if(ip->s_fmod==0 || ip->s_ilock!=0 ||
			   ip->s_flock!=0 || ip->s_ronly!=0)
				continue;
			bp = getblk(mp->m_dev, 1); // 读取超级块的内容
			ip->s_fmod = 0; // 清除更新标志位
			ip->s_time[0] = time[0]; // 更新超级块的filsys.s_time
			ip->s_time[1] = time[1];
			bcopy(ip, bp->b_addr, 256); // 将超级块的内容复制到缓冲区，并将缓冲区的内容写入到块设备
			bwrite(bp);
		}
	for(ip = &inode[0]; ip < &inode[NINODE]; ip++) // 遍历inode[], 如果元素未被加锁，则加锁后，调用iupdat()， 将inode[]元素的内容写入块设备
		if((ip->i_flag&ILOCK) == 0) {
			ip->i_flag =| ILOCK;
			iupdat(ip, time);
			prele(ip);
		}
	updlock = 0; // 释放updlock上的锁
	bflush(NODEV); // 调用bflush()刷新块设备的缓冲区，因为参数未NODEV, 所以会刷新所有设备的块设备缓冲区
}
