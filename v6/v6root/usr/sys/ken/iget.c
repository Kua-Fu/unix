#
#include "../param.h"
#include "../systm.h"
#include "../user.h"
#include "../inode.h"
#include "../filsys.h"
#include "../conf.h"
#include "../buf.h"

/*
 * 内核使用户可以通过文件、目录等易于理解和管理的概念访问块设备上的数据
 * 如何使用块设备上的区域？内核如何管理块设备上的空闲区域？文件的实体是什么？如何命名并管理文件？用户程序如何操作文件和目录？
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

/*
 * Look up an inode by device,inumber.
 * If it is in core (in the inode structure),
 * honor the locking protocol.
 * If it is not in core, read it in from the
 * specified device.
 * If the inode is mounted on, perform
 * the indicated indirection.
 * In all cases, a pointer to a locked
 * inode structure is returned.
 *
 * printf warning: no inodes -- if the inode
 *	structure is full
 * panic: no imt -- if the mounted file
 *	system is not in the mount table.
 *	"cannot happen"
 */

/*
 * iget() 用于取得inode[]元素
 * 如果inode[]中不存在所需要的元素，则获取新的元素并命名，然后将从块设备读取的inode的数据复制到上述新元素，并返回该元素
 * 如果inode[]中存在所需要的元素，递增参照计数器并返回该元素，由于无需读取块设备处理速度也相应提高，但是如果该元素处于被加锁状态，则进入睡眠状态直至元素被解锁
 * 如果设置了inode[]元素的IMOUNT标志位，则取得mount[]元素，并返回与对应设备的根目录相对应的inode[]元素
 * 通过该处理，使得利用同一个命名空间管理多个挂载设备成为可能（即使得在同一个目录树下同时载入多个挂载设备的根目录成为可能）
 *
 */

iget(dev, ino) // 参数: (1) dev，设备编号；(2) ino, inode编号
{
	register struct inode *p;
	register *ip2;
	int *ip1;
	register struct mount *ip;

loop:
	ip = NULL;
	for(p = &inode[0]; p < &inode[NINODE]; p++) { // 从起始位置遍历inode[]， 寻找未使用的元素，同时确认对象元素是否在inode[]中已经存在
		if(dev==p->i_dev && ino==p->i_number) { // 找到与参数dev、ino相对应的元素时候的处理
			if((p->i_flag&ILOCK) != 0) { // 如果对象元素被加锁，则设置IWANT标志位（表示存在等待该inode[]元素的进程）并进入睡眠状态，唤醒后返回loop再次尝试
				p->i_flag =| IWANT;
				sleep(p, PINOD);
				goto loop;
			}
			if((p->i_flag&IMOUNT) != 0) { // 如果设置了IMOUNT标志位，则从mount[]找到对应的元素，将dev设定为被挂载设备的设备编号
				for(ip = &mount[0]; ip < &mount[NMOUNT]; ip++)
				if(ip->m_inodp == p) {
					dev = ip->m_dev; // 将dev设定为被挂载设备的设备编号
					ino = ROOTINO;   // 将ino设定为ROOTINO (1)
					goto loop;
				}
				panic("no imt");
			}
			p->i_count++; // 如果对象元素既未被加锁，也没有设置IMOUNT标志位的话，递增该元素的参照计数器并加锁，然后返回该元素
			p->i_flag =| ILOCK;
			return(p);
		}
		if(ip==NULL && p->i_count==0) // 将inode[]中距起始位置最近的未使用元素赋予ip
			ip = p;
	}
	if((p=ip) == NULL) { // 如果在inode[]中未找到与参数dev、ino相对应的元素，且inode[]中不存在未使用元素时候，按出错处理
		printf("Inode table overflow\n");
		u.u_error = ENFILE;
		return(NULL);
	}
	p->i_dev = dev; // 为inode[]中未使用的元素命名，递增参照计数器并且对该元素加锁（同时清除其他标志位），然后将预读取逻辑块编号设置为-1（无效）
	p->i_number = ino;
	p->i_flag = ILOCK;
	p->i_count++;
	p->i_lastr = -1;
	ip = bread(dev, ldiv(ino+31,16)); // 读取块设备中该inode所在的块
	/*
	 * Check I/O errors
	 */
	if (ip->b_flags&B_ERROR) { // 在读取块设备发生错误时候进行错误处理
		brelse(ip);
		iput(p);
		return(NULL);
	}
	ip1 = ip->b_addr + 32*lrem(ino+31, 16); // 将块设备中inode的i_mode至i_addr数据复制到inode[]元素
	ip2 = &p->i_mode;
	while(ip2 < &p->i_addr[8])
		*ip2++ = *ip1++;
	brelse(ip); // 释放缓冲区，返回inode[]元素
	return(p);
}

/*
 * Decrement reference count of
 * an inode structure.
 * On the last reference,
 * write the inode out and if necessary,
 * truncate and deallocate the file.
 */

/*
 * input() 用来递减inode[]元素的参照计数器的值，当参照计数器的值为0时，将inode[]元素的内容写回块设备
 * 此外，当文件不再被任何目录参照时候进行删除文件的处理
 * 文件删除处理包含下面步骤：
 * （1）执行itrunc()， 释放使用中的存储区域，将文件长度和inode.i_addr[]清0
 * （2）将inode.i_mode清0
 * （3）执行ifree(), 将inode编号返还至空闲队列
 */

iput(p)
struct inode *p; // 参数： p, inode[]的元素
{
	register *rp;

	rp = p;
	if(rp->i_count == 1) { // 递减inode[]元素参照计数器的值，使其变为0时候的处理
		rp->i_flag =| ILOCK; // 将inode[]元素加锁
		if(rp->i_nlink <= 0) { // 当文件不再被任何目录参照时候进行删除文件的处理
			itrunc(rp);
			rp->i_mode = 0;
			ifree(rp->i_dev, rp->i_number);
		}
		iupdat(rp, time); // 将inode[]元素的数据写回块设备
		prele(rp); // 将inode[]元素解锁
		rp->i_flag = 0; // 将inode[]元素的i_flag和i_number清0
		rp->i_number = 0;
	}
	rp->i_count--; // 递减参照计数器的值
	prele(rp); // 将inode[]元素解锁，这是针对在iput()之外加锁的处理
}

/*
 * Check accessed and update flags on
 * an inode structure.
 * If either is on, update the inode
 * with the corresponding dates
 * set to the argument tm.
 */

/*
 * iupdat()将inode[]元素的内容写入块设备，写入处理只在处置了inode[]元素的更新标志位（IUPD）或者参照标志位(IACC) 时才会进行
 */

iupdat(p, tm) // 参数: (1) p, inode[]元素; (2) tm, 当前时间
int *p;
int *tm;
{
	register *ip1, *ip2, *rp;
	int *bp, i;

	rp = p;
	if((rp->i_flag&(IUPD|IACC)) != 0) { // 更新标志位或者设置了参照标志位时候的处理
		if(getfs(rp->i_dev)->s_ronly) // 执行getfs()读取超级块，如果为只读则直接返回
			return;
		i = rp->i_number+31; // 从块设备中读取inode所在的块
		bp = bread(rp->i_dev, ldiv(i,16));
		ip1 = bp->b_addr + 32*lrem(i, 16); // 更新位于块设备中的inode, 更新的范围为i_mode至i_addr
		ip2 = &rp->i_mode;
		while(ip2 < &rp->i_addr[8])
			*ip1++ = *ip2++;
		if(rp->i_flag&IACC) { // 如果设置了参照标志位，则更新inode的参照时间
			*ip1++ = time[0];
			*ip1++ = time[1];
		} else
			ip1 =+ 2;
		if(rp->i_flag&IUPD) { // 如果设置了更新标志位，则更新inode的更新时间
			*ip1++ = *tm++;
			*ip1++ = *tm;
		}
		bwrite(bp); // 执行bwrite()，将包括已经更新的inode在内的块写入块设备
	}
}

/*
 * Free all the disk blocks associated
 * with the specified inode structure.
 * The blocks of the file are removed
 * in reverse order. This FILO
 * algorithm will tend to maintain
 * a contiguous free list much longer
 * than FIFO.
 */

/*
 * itrun()将参数inode[]元素使用的存储区域的块编号返还给空闲列表，然后将文件长度和inode.i_addr[]全部清0
 * 间接参照时候，将相应的间接块也一并返还到空闲队列，文件数据本身仍保持在块设备的存储空间中，直到该区域被别的数据覆盖
 * 参数: (1) ip, inode[]元素
 */

itrunc(ip)
int *ip;
{
	register *rp, *bp, *cp;
	int *dp, *ep;

	rp = ip;
	if((rp->i_mode&(IFCHR&IFBLK)) != 0) // 如果是特殊文件，则不做任何处理立即返回
		return;
	for(ip = &rp->i_addr[7]; ip >= &rp->i_addr[0]; ip--) // 遍历inode.i_addr[]
	if(*ip) {
		if((rp->i_mode&ILARG) != 0) { // 如果设置了ILARG标志位，则遍历间接参照块
			bp = bread(rp->i_dev, *ip); // 读取与inode.i_addr[]元素指向的块编号相对应的块
			for(cp = bp->b_addr+512; cp >= bp->b_addr; cp--) // 参照块容纳着快编号的队列，从后向前遍历队列，通过执行free()，每次都返还一个块编号给空闲队列
			if(*cp) {
				if(ip == &rp->i_addr[7]) { // 因为inode.i_addr[7]供双重间接参照使用，首先读取各块编号所指向的块，再通过free()将其中保存的块编号队列返还给空闲队列
					dp = bread(rp->i_dev, *cp);
					for(ep = dp->b_addr+512; ep >= dp->b_addr; ep--)
					if(*ep)
						free(rp->i_dev, *ep);
					brelse(dp);
				}
				free(rp->i_dev, *cp);
			}
			brelse(bp);
		}
		free(rp->i_dev, *ip); // 执行free(),将inode.i_addr[]元素指向的块编号返还给空闲队列，并将inode.i_addr[]元素的值设置为0
		*ip = 0;
	}
	rp->i_mode =& ~ILARG; // 重置inode[]元素的ILARG标志位，将文件长度设置为0，并设置更新标志位
	rp->i_size0 = 0;
	rp->i_size1 = 0;
	rp->i_flag =| IUPD;
}

/*
 * Make a new file.
 */
maknode(mode)
{
	register *ip;

	ip = ialloc(u.u_pdir->i_dev);
	if (ip==NULL)
		return(NULL);
	ip->i_flag =| IACC|IUPD;
	ip->i_mode = mode|IALLOC;
	ip->i_nlink = 1;
	ip->i_uid = u.u_uid;
	ip->i_gid = u.u_gid;
	wdir(ip);
	return(ip);
}

/*
 * Write a directory entry with
 * parameters left as side effects
 * to a call to namei.
 */
wdir(ip)
int *ip;
{
	register char *cp1, *cp2;

	u.u_dent.u_ino = ip->i_number;
	cp1 = &u.u_dent.u_name[0];
	for(cp2 = &u.u_dbuf[0]; cp2 < &u.u_dbuf[DIRSIZ];)
		*cp1++ = *cp2++;
	u.u_count = DIRSIZ+2;
	u.u_segflg = 1;
	u.u_base = &u.u_dent;
	writei(u.u_pdir);
	iput(u.u_pdir);
}
