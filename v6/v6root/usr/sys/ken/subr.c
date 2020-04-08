#
/*
 */

#include "../param.h"
#include "../conf.h"
#include "../inode.h"
#include "../user.h"
#include "../buf.h"
#include "../systm.h"

/*
 * Bmap defines the structure of file system storage
 * by returning the physical block number on a device given the
 * inode and the logical block number in a file.
 * When convenient, it also leaves the physical
 * block number of the next block of the file in rablock
 * for use in read-ahead.
 */

/*
 * bmap()将逻辑块编号变换为物理块编号的函数
 * 参数: (1) ip, inode[]元素; (2) bn, 逻辑块编号
 */

bmap(ip, bn)
struct inode *ip;
int bn;
{
	register *bp, *bap, nb;
	int *nbp, d, i;

	d = ip->i_dev;
	if(bn & ~077777) { // 如果参数的逻辑块编号的值过大，则认为出错
		u.u_error = EFBIG;
		return(0);
	}

	if((ip->i_mode&ILARG) == 0) { // 如果没有设置inode[]元素的ILARG标志位，则按照直接参照的方式进行处理

		/*
		 * small file algorithm
		 */

		if((bn & ~7) != 0) { // 直接参照时，参数bn的值应该小于或者等于7（inode.i_addr[]的元素数）
		                     // 如果大于7，则切换至间接参照方式，这种情况只有在满足下述条件时候才会发生，即对文件的写入操作由writei()进行，且文件长度大于4KB
			/*
			 * convert small to large
			 */

			if ((bp = alloc(d)) == NULL) // 执行alloc()，从存储区域分配新的块，并取得相应的缓冲区
				return(NULL);
			bap = bp->b_addr; // 将inode.i_addr[]内的数据复制到刚取得的缓冲区，然后将inode.i_addr[]内的数据清0
			for(i=0; i<8; i++) {
				*bap++ = ip->i_addr[i];
				ip->i_addr[i] = 0;
			}
			ip->i_addr[0] = bp->b_blkno; // 将inode.i_addr[0]设置为刚取得的块的块编号
			bdwrite(bp); // 对块进行写入操作，inode.i_addr[]中原本容纳的数据被输出
			ip->i_mode =| ILARG; // 设置inode[]元素的ILARG标志位，然后跳转至large,进行间接参照处理
			goto large;
		}
		nb = ip->i_addr[bn]; // 一般直接参照处理，首先取得由参数指定的逻辑块编号指向的inode.i_addr[]的值
		if(nb == 0 && (bp = alloc(d)) != NULL) { // 由于文件长度变大，此处需要取得新的块，如果从inode.i_addr[]取得的值为0
			bdwrite(bp);                         // 且通过alloc()成功取得新的块的缓冲区，则将新取得的块的编号注册到inode.i_addr[],并设置inode[]元素的更新标志位
			nb = bp->b_blkno;
			ip->i_addr[bn] = nb;
			ip->i_flag =| IUPD;
		}
		rablock = 0; // 注册预读取块编号，如果逻辑块编号小于7,则注册与下一个逻辑块编号相对应的物理块编号，如果是从头开始按照顺序处理文件内容等情况，
		if (bn<7)    // 则很有可能会理解对下一个逻辑块进行处理，因此，此处将其注册为预处理块
			rablock = ip->i_addr[bn+1];
		return(nb); // 返回物理块编号
	}

	/*
	 * large file algorithm
	 */

    large: // 间接参照处理
	i = bn>>8; // 将逻辑块编号向右移动8 bit后的值赋予i，间接参照时候，inode.i_addr[]的每个元素对应256个块，向右移动8bit后的值(等于除以256的商)相当于inode.i_addr[]的数组下标
	if(bn & 0174000) // 如果逻辑块编号的值大于等于0174000( = 2048， 256*8) ，则采用双重间接参照，此时需要使用inode.i_addr[7]，因此将i的值设置为7
		i = 7;
	if((nb=ip->i_addr[i]) == 0) { // 如果inode.i_addr[i]的值为0，则设置inode[]元素的更新标志位，通过alloc()从存储区域取得的新的块（的缓冲区)
		ip->i_flag =| IUPD;       // 并将取得的块的块编号赋予inode.i_addr[i]，如果inode.i_addr[i]的值不为0，则通过bread()读取该块的内容
		if ((bp = alloc(d)) == NULL)
			return(NULL);
		ip->i_addr[i] = bp->b_blkno;
	} else
		bp = bread(d, nb);
	bap = bp->b_addr;

	/*
	 * "huge" fetch of double indirect block
	 */

	if(i == 7) { // 此处开始，为双重间接参照处理
		i = ((bn>>8) & 0377) - 7; // 计算第一级参照块中相应的块编号, 从向右移动8bit后的值中减去7， 表示从逻辑块编号中减去1792( 7*256)
                                  // 通过计算可以取得在双重间接参照(inode.i_addr[7])第一级参照块中的偏移量
		if((nb=bap[i]) == 0) {    // 如果第一级参照块中相应元素的块编号为0，通过alloc()从存储区域取得新的块（的缓冲区），并且将取得的块的编号分配给相应元素，然后执行bdwrite()，对块设备进行延迟写入
			if((nbp = alloc(d)) == NULL) {
				brelse(bp);
				return(NULL);
			}
			bap[i] = nbp->b_blkno;
			bdwrite(bp);
		} else { // 相应元素持有0以外的块编号时候，执行bread()读取该块的内容
			brelse(bp);
			nbp = bread(d, nb);
		}
		bp = nbp; // 用取得的块设备缓冲区更新变量bp和bap，对第二级参照块的遍历处理由此后一般的间接参照处理进行
		bap = bp->b_addr;
	}

	/*
	 * normal indirect fetch
	 */

	i = bn & 0377; // 此处开始为一般间接参照块的读取处理，i被设定为逻辑块编号的低比特位，相当于间接参照块中的偏移量
	if((nb=bap[i]) == 0 && (nbp = alloc(d)) != NULL) { // 由间接参照块中的偏移量取得块编号，如果为0，则尝试通过alloc()从存储区域取得新的块，将取得的块的块编号分配给偏移量
		nb = nbp->b_blkno;
		bap[i] = nb; // 然后执行bdwrite()对取得的块和间接参照块进行延迟写入，如果块编号不为0则释放间接参照块
		bdwrite(nbp);
		bdwrite(bp);
	} else
		brelse(bp);
	rablock = 0; // 将预读取块编号设定为与下一个逻辑块编号相对应的物理块编号
	if(i < 255)
		rablock = bap[i+1];
	return(nb); // 返回物理块编号
}

/*
 * Pass back  c  to the user at his location u_base;
 * update u_base, u_count, and u_offset.  Return -1
 * on the last character of the user's read.
 * u_base is in the user address space unless u_segflg is set.
 */
passc(c)
char c;
{

	if(u.u_segflg)
		*u.u_base = c; else
		if(subyte(u.u_base, c) < 0) {
			u.u_error = EFAULT;
			return(-1);
		}
	u.u_count--;
	if(++u.u_offset[1] == 0)
		u.u_offset[0]++;
	u.u_base++;
	return(u.u_count == 0? -1: 0);
}

/*
 * Pick up and return the next character from the user's
 * write call at location u_base;
 * update u_base, u_count, and u_offset.  Return -1
 * when u_count is exhausted.  u_base is in the user's
 * address space unless u_segflg is set.
 */
cpass()
{
	register c;

	if(u.u_count == 0)
		return(-1);
	if(u.u_segflg)
		c = *u.u_base; else
		if((c=fubyte(u.u_base)) < 0) {
			u.u_error = EFAULT;
			return(-1);
		}
	u.u_count--;
	if(++u.u_offset[1] == 0)
		u.u_offset[0]++;
	u.u_base++;
	return(c&0377);
}

/*
 * Routine which sets a user error; placed in
 * illegal entries in the bdevsw and cdevsw tables.
 */
nodev()
{

	u.u_error = ENODEV;
}

/*
 * Null routine; placed in insignificant entries
 * in the bdevsw and cdevsw tables.
 */
nulldev()
{
}

/*
 * copy count words from from to to.
 */
bcopy(from, to, count)
int *from, *to;
{
	register *a, *b, c;

	a = from;
	b = to;
	c = count;
	do
		*b++ = *a++;
	while(--c);
}
