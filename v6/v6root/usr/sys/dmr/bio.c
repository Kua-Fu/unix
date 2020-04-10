#
/*
 * 块设备指的是磁盘等可以处理大量数据的设备，包括程序在内的数据被保存在块设备中，并于执行时候读取到内存中
 * 内核如何提高对块设备的访问性能？ 块设备驱动如何操作块设备？
 * 块设备: （1）处理单位, 块(512字节), （2）地址，从起始块开始依次分配0,1,2等地址 （3）访问方式，可以根据需要访问任意块（随机访问）
 *（4）处理速度，一般而言，高速 （5）特征，适用于传输大量数据，内核通过使用缓冲区buffer可以实现缓存cache, 异步处理，延迟写入等功能，文件系统构筑在块设备之上， （6）例如：磁盘、磁带
 * 字符设备：（1）处理单位，文字（1字节）（2）地址，不分配地址，数据用过后即丢弃 （3） 访问方式：只能从队列的起始位置开始访问数据（顺序访问）
 *（4）处理速度，一般而言，低速 （5） 特征，适用于传输少量数据，（6）例如：行打印机、控制终端
 *
 * 设备驱动：是操作设备的程序，对1个设备或者相同种类的设备，通常存在与其对应的1个设备驱动，设备驱动由设备驱动表管理
 * 需要操作某个设备时候，设备驱动表中相应的驱动将被调用，管理块设备驱动的设备驱动表为 bdevsw[], block device switch, 管理字符设备驱动的设备驱动表为cdevsw[], character device switch
 *
 * 设备通过类别class 和长度为16bit的设备编号管理， 类别用来区分设备和字符设备，设备编号的高位8bit为大编号 major， 低8位bit为小编号 minor, 大编号表示设备种类，小编号分配给各个设备
 * 类别决定使用的设备驱动表，大编号决定设备驱动表使用的设备驱动，小编号的用途依设备而异
 *
 * 特殊文件，为了使用某个设备，必须生成对应的特殊文件，系统管理者通过执行用户程序 /etc/mknod生成特殊文件，特殊文件包括设备类别和设备编号等内容
 * 特殊文件在/dev下生成，文件名为代表该设备的字符串和小编号的组合，例如: /dev/rk0
 * /etc/mknod执行成功后，特殊文件被添加到指定目录，将自动生成对应的inode, 在该inode内会设置表示设备类别的标志变量，同时会设置inode.i_addr[0]为设备编号
 * 如果对特殊文件执行open, read , write, close系统调用，则在系统调用的处理函数内部将调用与大编号对应的设备驱动，来操作设备
 * 从用户角度，操作一般文件与操作设备具有相同的处理界面，例如：将用户程序的输出对象由文件变为打印机
 * fp = open('/home/test.log')   ---> fp = open('/dev/xxx')
 *
 * 访问块设备的处理由块设备子系统统一进行，块设备子系统由若干函数构成，此外，对块设备进行数据读写时候，使用的缓冲区也由块设备子系统进行管理
 *
 * 块设备子系统通过缓冲区与块设备进行数据交换，缓冲区由设备编号和块编号命名，对某个块进行处理时候，首先检查是否存在所需的缓冲区，
 * 如果已经存在则使用该缓冲区，如果尚未存在，则从未分配的缓冲区中获取新的缓冲区，对其命名后使用
 * 使用缓冲区的目的主要有两个：
 * （1）为了在多个进程同时访问同一个块时候保持数据的一致性， 通过使用缓冲区和排他处理可以实现这一个要求
 * （2）将需要经常进行读写操作的块的copy（缓存）保存在内存中以改善性能
 * 缓存能够减少对块设备的访问次数，由于块设备的访问速度与CPU的执行速度相比非常缓慢，因此减少访问次数将使得系统性能得到相应提升
 * 此外，还采取了通过将需要访问的数据预先读取到缓冲区，或者是在写满缓冲区后再向块设备写入数据等方法，提供系统的性能
 * 缓冲区由buf结构体的数组buf[]管理，通过将b_dev设定为设备编号，b_blkno设定为块编号为缓冲区命名
 *
 * buf结构体用于保存缓冲区的状态等管理数据，块设备中数据的copy保存在buffers[]的数组元素中，buf.b_addr则指向改元素，
 * buf.b_addr和buffers[]之间的关联关系通过binit()设定
 * buf[]通过b-list和av-list的双重环形队列管理
 * b-list用来管理已经分配给各块设备的缓冲区，通过buf.b_forw, 和 buf.b_back构成环形队列，位于各b-list头部的元素为devtab结构体
 * 由块设备驱动表dbevsw[]的d_tab指定，devtab结构体也用于设备处理队列
 * 但是，对于尚未分配给设备(NODEV)的缓冲区而言，位于b-list头部的元素为bfreelist
 *
 * av-list用来管理处于非应用状态的缓冲区(B_BUSY以外), 通过buf.av_forw和buf.av_back构成环形队列
 * 由av-list管理的缓冲区将被重新分配（给其他设备), 与尚未分配给设备的b-list相同，位于av-list头部的元素为bfreelist
 * 无论是b-list还是av-list，当队列为空时候，队列头部的元素指向自身
 *
 * RAW输入输出：向块设备传送数据时候可以不通过缓冲区，并且不受块长度(512字节)的限制，这称为RAW（无处理）输入输出，用于对块设备的数据进行整体备份等处理
 * 一般情况下，输入输出功能需要通过缓冲区与虚拟地址空间交换数据，但是RAW输入输出可以直接传送数据，无需通过缓冲区
 * 为了使用RAW输入输出功能，系统管理者需要生成专用的特殊文件，首先生成供字符设备使用的特殊文件，然后将其注册到字符设备驱动表
 *
 * 小结:
 * 设备分为块设备和字符设备两种
 * 设备驱动有设备驱动表进行管理
 * 设备由类别和设备编号进行管理
 * 为了使用设备，必须提前生成特殊文件
 * 对块设备的处理集中在块设备子系统中
 * 通过块设备缓冲区访问块设备，缓冲区保证了数据的一致性，同时也起到了缓存的作用
 * 读取分为同步读取和异步读取，预读取功能采用了异步读取
 * 写入分为同步写入和异步写入，此外还有延迟写入
 * 通过RAW输入输出传输数据时候，无需缓冲区，也不受块长度的限制
 *
 */

#include "../param.h"
#include "../user.h"
#include "../buf.h"
#include "../conf.h"
#include "../systm.h"
#include "../proc.h"
#include "../seg.h"

/*
 * This is the set of buffers proper, whose heads
 * were declared in buf.h.  There can exist buffer
 * headers not pointing here that are used purely
 * as arguments to the I/O routines to describe
 * I/O to be done-- e.g. swbuf, just below, for
 * swapping.
 */
char	buffers[NBUF][514];
struct	buf	swbuf;

/*
 * Declarations of the tables for the magtape devices;
 * see bdwrite.
 */
int	tmtab;
int	httab;

/*
 * The following several routines allocate and free
 * buffers with various side effects.  In general the
 * arguments to an allocate routine are a device and
 * a block number, and the value is a pointer to
 * to the buffer header; the buffer is marked "busy"
 * so that no on else can touch it.  If the block was
 * already in core, no I/O need be done; if it is
 * already busy, the process waits until it becomes free.
 * The following routines allocate a buffer:
 *	getblk
 *	bread
 *	breada
 * Eventually the buffer must be released, possibly with the
 * side effect of writing it out, by using one of
 *	bwrite
 *	bdwrite
 *	bawrite
 *	brelse
 */

/*
 * Read in (if necessary) the block and return a buffer pointer.
 */

/*
 * 读取分为同步读取和异步读取两种类型
 * 同步读取时候，进程首先使用getblk()取得缓冲区，然后对块设备提出读取请求，并执行iowait()进入睡眠状态，当块设备的处理完成后，由中断处理函数执行的iodone()唤醒入睡的进程，随后，该进程执行brelse()， 释放获取的缓冲区
 * 异步读取的时候，进程继续原有处理，无需等待块设备的读取处理结束，缓冲区由中断处理函数执行的iodone()自动释放，当已经设置了缓冲区的B_ASYNC的标志位时候，表示当前的读取位异步读取
 * 异步读取采用预读取的方式，该方式指按照顺序读取属于某个文件的块时候，预先将下一个块的数据读取到缓冲区
 *
 * bread()是进行同步读取的函数，它通过检查由getblk()获取的缓冲区的B_DONE标志位来判断缓冲区内的数据是否是最新，如果为最新则不会访问设备，否则会对设备进行访问
 * 参数: (1) dev, 设备编号 (2) blkno, 块编号
 */

bread(dev, blkno)
{
	register struct buf *rbp;

	rbp = getblk(dev, blkno);
	if (rbp->b_flags&B_DONE)
		return(rbp);
	rbp->b_flags =| B_READ; // 设置B_READ标志位，将读取长度设定为-256(表示512字节），然后执行设定于bdevsw[]中的设备访问函数
	rbp->b_wcount = -256;
	(*bdevsw[dev.d_major].d_strategy)(rbp);
	iowait(rbp); // 执行iowait()， 进入等待状态直到设备的读取处理结束
	return(rbp); // 返回该缓冲区，该缓冲区用来容纳从设备中读取的数据
}

/*
 * Read in the block, like bread, but also start I/O on the
 * read-ahead block (which is not allocated to the caller)
 */

/*
 * breada() 是从块设备中读取数据，同时也负责预读取的函数，预读取是异步进行的，当设备的读取处理结束时候，由块设备驱动执行的iodone()释放缓冲区
 * 参数: (1) adev, 设备编号 (2) blkno, 块编号 (3) rablkno，预读取块编号
 *
 */

breada(adev, blkno, rablkno)
{
	register struct buf *rbp, *rabp;
	register int dev;

	dev = adev;
	rbp = 0;
	if (!incore(dev, blkno)) { // 执行incore()， 检查准备读取的设备的块的缓冲区是否存在
		rbp = getblk(dev, blkno); // 如果缓冲区不存在，则执行getblk()获取缓冲区，如果尚未设置获取的缓冲区的B_DONE标志位，则启动从设备读取数据的处理
		if ((rbp->b_flags&B_DONE) == 0) {
			rbp->b_flags =| B_READ;
			rbp->b_wcount = -256;
			(*bdevsw[adev.d_major].d_strategy)(rbp);
		}
	}
	if (rablkno && !incore(dev, rablkno)) { // 准备进行预处理，且缓冲区不存在的时候的处理
		rabp = getblk(dev, rablkno); // 执行getblk()获取缓冲区，如果已经设置了获取的缓冲区的B_DONE标志位，则表示已有数据被读取，因此执行brelse()释放缓冲区
		if (rabp->b_flags & B_DONE)
			brelse(rabp);
		else { // 如果未设置B_DONE标志位，则开始从设备读取数的处理，注意，此处并没有等待读取处理执行结束
			rabp->b_flags =| B_READ|B_ASYNC;
			rabp->b_wcount = -256;
			(*bdevsw[adev.d_major].d_strategy)(rabp);
		}
	}
	if (rbp==0) // 如果准备读取（非预读取）的块的缓冲区已经存在，则执行bread()读取数据，并返回该缓冲区
		return(bread(dev, blkno));
	iowait(rbp); // 执行iowait()等待设备的同步读取处理结束
	return(rbp); // 返回同步读取的缓冲区
}

/*
 * Write the buffer, waiting for completion.
 * Then release the buffer.
 */

/*
 * 写入可以分为同步写入，异步写入，延迟写入 3中类型，同步写入和异步写入的区别与读取处理的情况相同，前者等待设备处理结束，后者并不等待
 * 延迟写入收，首先执行getblk()取得缓冲区，随后将准备写入设备的数据写入到该缓冲区，但是此时并不对设备提出写入请求，而是在设置缓冲区的B_ASYNC和B_DELWRI标志位后，执行brelse()释放缓冲区
 * 经过一段时间后，当再次执行getblk() (出于与上述延迟写入不同的原因），并试图再次分配已经设置了B_DELWRI标志位的缓冲区时候，将以异步执行的方式对设备提出写入数据的请求
 * 此外，当执行刷新缓冲区的函数bflush()时候，也会使用已设置B_DELWRI标志位的缓冲区，对设备进行写入处理
 *
 * bwrite()将缓冲区内的数据写入设备，如果未设置B_ASYNC标志位则进行同步写入，否则进行异步写入
 * 参数: (1) bp, 缓冲区
 */

bwrite(bp)
struct buf *bp;
{
	register struct buf *rbp;
	register flag;

	rbp = bp;
	flag = rbp->b_flags;
	rbp->b_flags =& ~(B_READ | B_DONE | B_ERROR | B_DELWRI); // 清除下面的标志位, B_READ, B_DONE, B_ERROR, B_DELWRI
	rbp->b_wcount = -256;
	(*bdevsw[rbp->b_dev.d_major].d_strategy)(rbp); // 执行在bdevsw[]中注册的设备访问函数
	if ((flag&B_ASYNC) == 0) { // 如果未设置B_ASYNC标志位，则等待设备处理结束后，调用brelse()释放缓冲区
		iowait(rbp);
		brelse(rbp);
	} else if ((flag&B_DELWRI)==0) // 如果已经设置了B_ASYNC标志位，则不等待设备处理结束，此外，如果未设置B_DELWRI标志位，则调用geterror()对错误进行检查
		geterror(rbp);
}

/*
 * Release the buffer, marking it so that if it is grabbed
 * for another purpose it will be written out before being
 * given up (e.g. when writing a partial block where it is
 * assumed that another write for the same block will soon follow).
 * This can't be done for magtape, since writes must be done
 * in the same order as requested.
 */

/*
 * bdwrite()进行延迟写入，该函数首先设置表示延迟写入的B_DELWRI标志位，然后释放缓冲区
 * 参数:(1) bp, 缓冲区
 *
 */

bdwrite(bp)
struct buf *bp;
{
	register struct buf *rbp;
	register struct devtab *dp;

	rbp = bp;
	dp = bdevsw[rbp->b_dev.d_major].d_tab;
	if (dp == &tmtab || dp == &httab) // 如果写入的对象为磁带设备，则调用bawrite()并立即进行写入处理
		bawrite(rbp);
	else {
		rbp->b_flags =| B_DELWRI | B_DONE;
		brelse(rbp);
	}
}

/*
 * Release the buffer, start I/O on it, but don't wait for completion.
 */

/*
 * bawrite()是进行异步写入的函数，该函数首先设置B_ASYNC标志位，然后调用bwrite()
 *  参数: (1) bp, 缓冲区
 *
 */

bawrite(bp)
struct buf *bp;
{
	register struct buf *rbp;

	rbp = bp;
	rbp->b_flags =| B_ASYNC;
	bwrite(rbp);
}

/*
 * release the buffer, with no I/O implied.
 */

/*
 * brelse()用来释放缓冲区（清除B_BUSY标志位), 并将被释放的缓冲区追加到av-list，已经分配给b-list的缓冲区不会从b-list中删除
 * 参数: (1) bp, 缓冲区
 *
 */

brelse(bp)
struct buf *bp;
{
	register struct buf *rbp, **backp;
	register int sps;

	rbp = bp;
	if (rbp->b_flags&B_WANTED) // 唤醒正在等待当前缓冲区的进程
		wakeup(rbp);
	if (bfreelist.b_flags&B_WANTED) { // 唤醒正在等待缓冲区要补充到av-list的进程，清除bfreelist的B_WANTED标志位
		bfreelist.b_flags =& ~B_WANTED;
		wakeup(&bfreelist);
	}
	if (rbp->b_flags&B_ERROR) // 如果由于设备操作错误导致设置了缓冲区的错误标志，则将小编号设置为-1， 缓冲区的设备编号发生了变化，如果不通过av-list重新分配，此缓冲区将无法再次取得，
		rbp->b_dev.d_minor = -1;  /* no assoc. on error */ // 因为其中容纳的数据有可能是错误的，需要防止错误数据被使用
	backp = &bfreelist.av_back;
	sps = PS->integ; // 保存 PSW, 将处理器优先级提升到6防止发生中断
	spl6();
	rbp->b_flags =& ~(B_WANTED|B_BUSY|B_ASYNC); // 清除B_WANTED, B_BUSY, 和B_ASYNC标志位
	(*backp)->av_forw = rbp; // 将缓冲区返回至av-list的末尾
	rbp->av_back = *backp;
	*backp = rbp;
	rbp->av_forw = &bfreelist;
	PS->integ = sps; // 将处理器优先级返回原值
}

/*
 * See if the block is associated with some buffer
 * (mainly to avoid getting hung up on a wait in breada)
 */

/*
 * incore()检查分配给某个设备的某个块的缓冲区是否存在
 * 如果存在，则返回该缓冲区，如果不存在则返回0
 * incore()遍历该设备的b-list， 对buf结构体的b_blkno和b_dev分别进行检查
 * 参数: (1) dev, 设备编号 (2) blkno, 块编号
 */

incore(adev, blkno)
{
	register int dev;
	register struct buf *bp;
	register struct devtab *dp;

	dev = adev;
	dp = bdevsw[adev.d_major].d_tab;
	for (bp=dp->b_forw; bp != dp; bp = bp->b_forw)
		if (bp->b_blkno==blkno && bp->b_dev==dev)
			return(bp);
	return(0);
}

/*
 * Assign a buffer for the given block.  If the appropriate
 * block is already associated, return it; otherwise search
 * for the oldest non-busy buffer and reassign it.
 * When a 512-byte area is wanted for some random reason
 * (e.g. during exec, for the user arglist) getblk can be called
 * with device NODEV to avoid unwanted associativity.
 */

/*
 * getblk()是取得根据设备编号与块编号命名的缓冲区的函数
 * 按顺序遍历相应设备的b-list, 寻找是否存在所需的缓冲区，找到后将其从av-list中删除（设置标志位为B_BUSY), 并返回该缓冲区
 * 如果已经设置了该缓冲区的标志位B_BUSY, 则设置标志位为B_WANTED并进入睡眠状态，当正在使用该缓冲区的其他进程将其释放后，进程将被唤醒
 * 如果b-list中不存在所需的缓冲区，则取得位于av-list头部的缓冲区（设置其标志位为B_BUSY), 对其重新命名，并追加到b-list的头部，然后返回该缓冲区，设置标志位B_BUSY意在表示该缓冲区正处于使用中的状态
 * 在尝试从av-list取得缓冲区时候，如果该缓冲区的类型为B_DELWRI(延迟写入), 则需要对设备进行异步读写
 * 参数: (1) dev, 设备编号 (2) blkno, 块编号
 */

getblk(dev, blkno)
{
	register struct buf *bp;
	register struct devtab *dp;
	extern lbolt;

	if(dev.d_major >= nblkdev) // 如果大编号的值过大，则调用panic()
		panic("blkdev");

    loop:
	if (dev < 0) // NODEV(-1)时候的处理，将dp设置为NODEV的b-list的起始元素
		dp = &bfreelist;
	else { // 非NODEV时候的处理
		dp = bdevsw[dev.d_major].d_tab; // 从bdevsw[]中取得相应设备的devtab结构体(b-list的起始元素）
		if(dp == NULL)
			panic("devtab");
		for (bp=dp->b_forw; bp != dp; bp = bp->b_forw) { // 遍历b-list，检查是否存在所需的缓冲区
			if (bp->b_blkno!=blkno || bp->b_dev!=dev)
				continue;
			spl6(); // 如果成功找到，则将处理器优先级提高到6，防止发生中断，由于块设备处理结束时候，引发的中断处理等会操作缓冲区，因此抑制中断可以避免在操作缓冲区时候发生冲突
			if (bp->b_flags&B_BUSY) { // 如果此缓冲区正在使用，则设置B_WANTED标志位并进入睡眠状态
				bp->b_flags =| B_WANTED;
				sleep(bp, PRIBIO);
				spl0();
				goto loop;
			}
			spl0(); // 将处理器优先级重置为0， 设置处于使用中的标志位，然后将缓冲区从av-list中删除，返回该缓冲区
			notavail(bp);
			return(bp);
		}
	}
	spl6(); // 如果在b-list中没有找到所需的缓冲区，或是希望取得NODEV的缓冲区时候，从av-list中取得缓冲区，并将处理器优先级提升为6，防止发生中断
	if (bfreelist.av_forw == &bfreelist) { // av-list为空时的处理，设置av-list的起始元素bfreelist的B_WANTED标志位并进入睡眠状态
		bfreelist.b_flags =| B_WANTED;
		sleep(&bfreelist, PRIBIO);
		spl0();
		goto loop;
	}
	spl0(); // 将处理器的优先级重置为0
	notavail(bp = bfreelist.av_forw); // 取得av-list的起始元素的缓冲区，将其从av-list中删除，并且对其设置B_BUSY标志位
	if (bp->b_flags & B_DELWRI) { // 如果取得的缓冲区设置了B_BDELWRI(延迟写入)标志位，则立刻对设备进行异步写入，且无需等待设备的处理结束
		bp->b_flags =| B_ASYNC;
		bwrite(bp);
		goto loop;
	}
	bp->b_flags = B_BUSY | B_RELOC; // B_RELOC标志位在UNIX V6中未被使用，此时只是设置了B_BUSY（和B_RELOC)标志位
	bp->b_back->b_forw = bp->b_forw; // 将缓冲区从当前分配的b-list中删除
	bp->b_forw->b_back = bp->b_back;
	bp->b_forw = dp->b_forw; // 将缓冲区追加到新的b-list的起始位置
	bp->b_back = dp;
	dp->b_forw->b_back = bp;
	dp->b_forw = bp;
	bp->b_dev = dev; // 为缓冲区命名
	bp->b_blkno = blkno;
	return(bp); // 返回缓冲区
}

/*
 * Wait for I/O completion on the buffer; return errors
 * to the user.
 */

/*
 * iowait()是等待设备处理结束的函数，使得进程进入睡眠状态直至设置缓冲区的B_DONE标志位
 * 参数: (1) bp, 缓冲区
 *
 */

iowait(bp)
struct buf *bp;
{
	register struct buf *rbp;

	rbp = bp;
	spl6();
	while ((rbp->b_flags&B_DONE)==0)
		sleep(rbp, PRIBIO);
	spl0();
	geterror(rbp);
}

/*
 * Unlink a buffer from the available list and mark it busy.
 * (internal interface)
 */

/*
 * notavail()是将缓冲区从av-list中删除，同时设置B_BUSY（使用中)标志位的函数
 * 参数: (1) bp，缓冲区
 *
 */

notavail(bp)
struct buf *bp;
{
	register struct buf *rbp;
	register int sps;

	rbp = bp;
	sps = PS->integ;
	spl6(); // 将优先级提升到6，防止发生中断，由于块设备引发的中断处理等会操作缓冲区，因此抑制中断可以避免在操作缓冲区时候发生冲突
	rbp->av_back->av_forw = rbp->av_forw; // 从av-list中删除对象缓冲区
	rbp->av_forw->av_back = rbp->av_back; // 设置B_BUSY标志位，表明此缓冲区正在使用
	rbp->b_flags =| B_BUSY;
	PS->integ = sps; // 将处理器优先级恢复原值
}

/*
 * Mark I/O complete on a buffer, release it if I/O is asynchronous,
 * and wake up anyone waiting for it.
 */

/*
 * iodone()是在块设备的处理结束后被调用的函数，用来设置缓冲区的B_DONE标志位
 * 它在块设备处理结束时候执行的中断处理函数中被调用
 * 同步读写时候，唤醒正在等待缓冲区的进程，异步读写时候，执行brelse()释放该缓冲区
 * 参数: (1) bp, 缓冲区
 */

iodone(bp)
struct buf *bp;
{
	register struct buf *rbp;

	rbp = bp;
	if(rbp->b_flags&B_MAP) // 在PDP-11/40的环境下不做任何处理
		mapfree(rbp);
	rbp->b_flags =| B_DONE;
	if (rbp->b_flags&B_ASYNC)
		brelse(rbp);
	else {
		rbp->b_flags =& ~B_WANTED;
		wakeup(rbp);
	}
}

/*
 * Zero the core associated with a buffer.
 */

/*
 * clrbuf()将缓冲区的数据清0
 * 参数: (1) bp, 缓冲区
 */
clrbuf(bp)
int *bp;
{
	register *p;
	register c;

	p = bp->b_addr;
	c = 256;
	do
		*p++ = 0;
	while (--c);
}

/*
 * Initialize the buffer I/O system by freeing
 * all buffers and setting all device buffer lists to empty.
 */

/*
 * binit()是对缓冲区进行初始化的函数，在系统启动时候由main()调用，且仅调用一次
 * binit()对buf[]和buffers[]进行关联，将所有的缓冲区追加到av-list和处于NODEV状态的b-list
 * 此外，统计位于dbevsw[]中的设备驱动的数量，将其设置到nblkdev中
 */

binit()
{
	register struct buf *bp;
	register struct devtab *dp;
	register int i;
	struct bdevsw *bdp;

	bfreelist.b_forw = bfreelist.b_back =
	    bfreelist.av_forw = bfreelist.av_back = &bfreelist;
	for (i=0; i<NBUF; i++) {
		bp = &buf[i];
		bp->b_dev = -1; // 设备编号-1表示该缓冲区处于NODEV状态
		bp->b_addr = buffers[i];
		bp->b_back = &bfreelist;
		bp->b_forw = bfreelist.b_forw;
		bfreelist.b_forw->b_back = bp;
		bfreelist.b_forw = bp;
		bp->b_flags = B_BUSY;
		brelse(bp);
	}
	i = 0; // 对赋予bdevsw[]的devtab.d_tab进行初始化处理，将位于各设备b-list头部的devtab结构体的成员变量b_forw, b_back指向自身
	for (bdp = bdevsw; bdp->d_open; bdp++) {
		dp = bdp->d_tab;
		if(dp) {
			dp->b_forw = dp;
			dp->b_back = dp;
		}
		i++;
	}
	nblkdev = i; // 将nblkdev设置为块设备（驱动）的数量
}

/*
 * Device start routine for disks
 * and other devices that have the register
 * layout of the older DEC controllers (RF, RK, RP, TM)
 */
#define	IENABLE	0100
#define	WCOM	02
#define	RCOM	04
#define	GO	01
devstart(bp, devloc, devblk, hbcom)
struct buf *bp;
int *devloc;
{
	register int *dp;
	register struct buf *rbp;
	register int com;

	dp = devloc;
	rbp = bp;
	*dp = devblk;			/* block address */
	*--dp = rbp->b_addr;		/* buffer address */
	*--dp = rbp->b_wcount;		/* word count */
	com = (hbcom<<8) | IENABLE | GO |
		((rbp->b_xmem & 03) << 4);
	if (rbp->b_flags&B_READ)	/* command + x-mem */
		com =| RCOM;
	else
		com =| WCOM;
	*--dp = com;
}

/*
 * startup routine for RH controllers.
 */
#define	RHWCOM	060
#define	RHRCOM	070

rhstart(bp, devloc, devblk, abae)
struct buf *bp;
int *devloc, *abae;
{
	register int *dp;
	register struct buf *rbp;
	register int com;

	dp = devloc;
	rbp = bp;
	if(cputype == 70)
		*abae = rbp->b_xmem;
	*dp = devblk;			/* block address */
	*--dp = rbp->b_addr;		/* buffer address */
	*--dp = rbp->b_wcount;		/* word count */
	com = IENABLE | GO |
		((rbp->b_xmem & 03) << 8);
	if (rbp->b_flags&B_READ)	/* command + x-mem */
		com =| RHRCOM; else
		com =| RHWCOM;
	*--dp = com;
}

/*
 * 11/70 routine to allocate the
 * UNIBUS map and initialize for
 * a unibus device.
 * The code here and in
 * rhstart assumes that an rh on an 11/70
 * is an rh70 and contains 22 bit addressing.
 */
int	maplock;
mapalloc(abp)
struct buf *abp;
{
	register i, a;
	register struct buf *bp;

	if(cputype != 70)
		return;
	spl6();
	while(maplock&B_BUSY) {
		maplock =| B_WANTED;
		sleep(&maplock, PSWP);
	}
	maplock =| B_BUSY;
	spl0();
	bp = abp;
	bp->b_flags =| B_MAP;
	a = bp->b_xmem;
	for(i=16; i<32; i=+2)
		UBMAP->r[i+1] = a;
	for(a++; i<48; i=+2)
		UBMAP->r[i+1] = a;
	bp->b_xmem = 1;
}

mapfree(bp)
struct buf *bp;
{

	bp->b_flags =& ~B_MAP;
	if(maplock&B_WANTED)
		wakeup(&maplock);
	maplock = 0;
}

/*
 * swap I/O
 */

/*
 * swap()是进行交换处理的函数，该函数使用buf结构体类型的变量swbuf，进行RAW输入输出实现交换处理
 * 因为swbuf只有一个，无法同时交换两个以上的进程，所以需要进行排他处理
 * swap()为swbuf设定是同的参数，调用交换磁盘的设备驱动读写数据，因为参数的长度以64字节为单位，所以需要将地址变为字节单位，再将长度（字长）变为2的补数的形式
 * 这些值最后都会赋予交换磁盘的寄存器
 * 交换处理因为使用RAW输入输出，所以每次的传送量都不受块长度的限制，但是，并不具备缓存的功能
 * 参数: (1) blkno, 交换磁盘中的块编号 (2) coreaddr, 交换对象的物理内存地址，64字节为单位 (3) count, 交换对象的长度，64字节为单位 (4) rdflg, 0:换出， 1:换入
 *
 * 交换磁盘的设备编号通过swapdev指定， 系统管理者可以根据实际环境修改这个值，但是需要对内核进行重新构筑，下面代码中大编号和小编号都为0
 *
 */

swap(blkno, coreaddr, count, rdflg)
{
	register int *fp;

	fp = &swbuf.b_flags;
	spl6();
	while (*fp&B_BUSY) { // 如果有其他进程使用swbuf, 则设置swbuf.b_flags的B_WANTED标志位，然后进入睡眠状态
		*fp =| B_WANTED;
		sleep(fp, PSWP);
	}
	*fp = B_BUSY | B_PHYS | rdflg; // 如果可以使用swbuf, 则设置swbuf.b_flags的B_BUSY标志位、B_PHYS标志位（RAW输入输出）和rdflg标志位（读取或者写入）
	swbuf.b_dev = swapdev; // 设置swbuf的参数启动交换磁盘的输入输出，将通过参数设定的以64字节为单位的地址和长度分别转换为字节单位和2的补数的字单位
	swbuf.b_wcount = - (count<<5);	/* 32 w/block */
	swbuf.b_blkno = blkno;
	swbuf.b_addr = coreaddr<<6;	/* 64 b/block */
	swbuf.b_xmem = (coreaddr>>10) & 077;
	(*bdevsw[swapdev>>8].d_strategy)(&swbuf); // 调用交换磁盘的设备驱动
	spl6();
	while((*fp&B_DONE)==0) // 进入睡眠状态等待交换磁盘的处理结束
		sleep(fp, PSWP);
	if (*fp&B_WANTED) // 交换磁盘处理结束后，唤醒正在等待swbuf的进程
		wakeup(fp);
	spl0();
	*fp =& ~(B_BUSY|B_WANTED); // 清除swbuf的B_BUSY标志位和B_WANTED标志位
	return(*fp&B_ERROR); // 返回交换处理成功与否的标志，访问当对设备失败时候，设备驱动将设定swbuf.B_ERROR标志位
}

/*
 * make sure all write-behind blocks
 * on dev (or NODEV for all)
 * are flushed out.
 * (from umount and update)
 */

/*
 * bflush()将延迟写入缓冲区内的数据一次性写入设备
 * bflush()被update()调用，而update()被panic()， sync(), sumount()调用
 * 在UNIX V6的环境中，守护进程/etc/update每隔30秒运行一次sync指令
 * 参数: (1) dev, 处理对象的设备编号，值为-1, (NODEV)时候刷新所有设备的延迟写入缓冲区
 *
 */

bflush(dev)
{
	register struct buf *bp;

loop:
	spl6();
	for (bp = bfreelist.av_forw; bp != &bfreelist; bp = bp->av_forw) {
		if (bp->b_flags&B_DELWRI && (dev == NODEV||dev==bp->b_dev)) {
			bp->b_flags =| B_ASYNC;
			notavail(bp);
			bwrite(bp);
			goto loop;
		}
	}
	spl0();
}

/*
 * Raw I/O. The arguments are
 *	The strategy routine for the device
 *	A buffer, which will always be a special buffer
 *	  header owned exclusively by the device for this purpose
 *	The device number
 *	Read/write flag
 * Essentially all the work is computing physical addresses and
 * validating them.
 */

/*
 * physio()是进行RAW输入输出的函数，传送数据的地址、长度以及在块设备中的偏移量，都通过user结构体指定
 * physio()由注册于字符设备驱动表中用于RAW输入输出的设备驱动调用，所使用的缓冲区不是buf[], 而是供RAW输入输出专用的缓冲区
 * 处理的流程是: 读入用户APR的值，根据虚拟地址直接计算处物理地址，然后向缓冲区传递参数，再执行访问块设备的函数
 * 传递给physio()的user结构体成员变量：
 * (1) u.u_base, 传送数据的虚拟地址（字节为单位） (2) u.u_offset, 块设备中的偏移量（单位为字节） (3) u.u_count, 传送数据长度（字节为单位）
 * 参数：(1) start, 指向块设备访问函数的指针 (2) abp, 缓冲区， 使用的是RAW输入输出专用的缓冲区 (3) dev, 设备编号 (4) rw, 指定是读取还是写入
 *
 */

physio(strat, abp, dev, rw)
struct buf *abp;
int (*strat)();
{
	register struct buf *bp;
	register char *base;
	register int nb;
	int ts;

	bp = abp;
	base = u.u_base;
	/*
	 * Check odd base, odd count, and address wraparound
	 */
	if (base&01 || u.u_count&01 || base>=base+u.u_count) // 检查作为参数传递进来的user结构体的成员变量
		goto bad;  // (1) 传送的虚拟地址为偶数， (2) 传送的数据长度为偶数 (3) 虚拟地址和数据长度之和是否溢出 (4) 是否试图传送位于代码段领域的数据 （5）传送数据是否位于数据领域(下限）和 栈领域（上限）之间
	ts = (u.u_tsize+127) & ~0177; // ts的值为代码段的长度（以64字节为单位，并以128*64字节为单位向上取整）
	if (u.u_sep)
		ts = 0;
	nb = (base>>6) & 01777; // nb的值为传送数据的虚拟地址（以64字节为单位）
	/*
	 * Check overlap with text. (ts and nb now
	 * in 64-byte clicks)
	 */
	if (nb < ts)
		goto bad;
	/*
	 * Check that transfer is either entirely in the
	 * data or in the stack: that is, either
	 * the end is in the data or the start is in the stack
	 * (remember wraparound was already checked).
	 */
	if ((((base+u.u_count)>>6)&01777) >= ts+u.u_dsize
	    && nb < 1024-u.u_ssize)
		goto bad;
	spl6();
	while (bp->b_flags&B_BUSY) { // 如果供RAW输入输出使用的缓冲区正在使用，则设置B_WANTED标志位，然后进入睡眠状态直到缓冲区被释放
		bp->b_flags =| B_WANTED;
		sleep(bp, PRIBIO);
	}
	bp->b_flags = B_BUSY | B_PHYS | rw; // 将作为参数的user结构体传递给缓冲区，根据用户PAR的值从虚拟地址计算得到物理地址，将buf.b_xmem设定为物理内存第16位之后的部分
	bp->b_dev = dev;  // 从u.u_offset计算出块编号，同时设定执行进程的SLOCK标志位，防止进程被换出至交换空间
	/*
	 * Compute physical address by simulating
	 * the segmentation hardware.
	 */
	bp->b_addr = base&077;
	base = (u.u_sep? UDSA: UISA)->r[nb>>7] + (nb&0177);
	bp->b_addr =+ base<<6;
	bp->b_xmem = (base>>10) & 077;
	bp->b_blkno = lshift(u.u_offset, -9);
	bp->b_wcount = -((u.u_count>>1) & 077777);
	bp->b_error = 0;
	u.u_procp->p_flag =| SLOCK;
	(*strat)(bp); // 执行设备驱动的访问函数
	spl6();
	while ((bp->b_flags&B_DONE) == 0) // 进入睡眠状态，等待块设备处理结束
		sleep(bp, PRIBIO);
	u.u_procp->p_flag =& ~SLOCK; // 清除SLOCK标志位
	if (bp->b_flags&B_WANTED) // 如果有进程在等待供RAW输入输出使用的缓冲区，则将其唤醒
		wakeup(bp);
	spl0();
	bp->b_flags =& ~(B_BUSY|B_WANTED);
	u.u_count = (-bp->b_resid)<<1; // buf.b_resid中保存有因出错而没能传送的数据长度，将其赋予u.u_count
	geterror(bp);
	return;
    bad:
	u.u_error = EFAULT;
}

/*
 * Pick up the device's error number and pass it to the user;
 * if there is an error but the number is 0 set a generalized
 * code.  Actually the latter is always true because devices
 * don't yet return specific errors.
 */

/*
 * geterror()是处理错误的函数
 * 如果未设置buf.b_flags的错误标志位，则不做任何处理
 * 参数: (1) abp, 缓冲区
 */

geterror(abp)
struct buf *abp;
{
	register struct buf *bp;

	bp = abp;
	if (bp->b_flags&B_ERROR)
		if ((u.u_error = bp->b_error)==0)
			u.u_error = EIO;
}
