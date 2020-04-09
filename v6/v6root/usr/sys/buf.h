/*
 * Each buffer in the pool is usually doubly linked into 2 lists:
 * the device with which it is currently associated (always)
 * and also on a list of blocks available for allocation
 * for other use (usually).
 * The latter list is kept in last-used order, and the two
 * lists are doubly linked to make it easy to remove
 * a buffer from one list when it was found by
 * looking through the other.
 * A buffer is on the available list, and is liable
 * to be reassigned to another disk block, if and only
 * if it is not marked BUSY.  When a buffer is busy, the
 * available-list pointers can be used for other purposes.
 * Most drivers use the forward ptr as a link in their I/O
 * active queue.
 * A buffer header contains all the information required
 * to perform I/O.
 * Most of the routines which manipulate these things
 * are in bio.c.
 */
struct buf
{
	int	b_flags;		/* see defines below */ // 标志变量
	struct	buf *b_forw;		/* headed by devtab of b_dev */ // 指向 b-list前方的指针
	struct	buf *b_back;		/*  "  */ // 指向 b-list后方的指针
	struct	buf *av_forw;		/* position on free list, */ // 指向 av-list前方的指针
	struct	buf *av_back;		/*     if not BUSY*/ // 指向 av-list后方的指针
	int	b_dev;			/* major+minor device name */ // 设备编号
	int	b_wcount;		/* transfer count (usu. words) */ // 读写长度，在访问设备时候以2的补数形式指定
	char	*b_addr;		/* low order core address */ // 用于放置设备数据拷贝的内存区域
	char	*b_xmem;		/* high order core address */ // 地址的扩张比特
	char	*b_blkno;		/* block # on device */ // 块编号
	char	b_error;		/* returned after I/O */ // 表示在访问设备时候发生错误
	char	*b_resid;		/* words not transferred after error */ // 用于RAW输入输出，保存因错误而无法传送的数据的长度（字节为单位）
} buf[NBUF]; // NUBF 值为15

/*
 * Each block device has a devtab, which contains private state stuff
 * and 2 list heads: the b_forw/b_back list, which is doubly linked
 * and has all the buffers currently associated with that major
 * device; and the d_actf/d_actl list, which is private to the
 * device but in fact is always used for the head and tail
 * of the I/O queue for the device.
 * Various routines in bio.c look at b_forw/b_back
 * (notice they are the same as in the buf structure)
 * but the rest is private to each device driver.
 */
struct devtab
{
	char	d_active;		/* busy flag */  // 设备处理中
	char	d_errcnt;		/* error count (for recovery) */ // 错误计数
	struct	buf *b_forw;		/* first buffer for this dev */ // 指向b-list的头部
	struct	buf *b_back;		/* last buffer for this dev */ // 指向b-list的末尾
	struct	buf *d_actf;		/* head of I/O queue */ // 指向设备处理队列的头部
	struct 	buf *d_actl;		/* tail of I/O queue */ // 指向设备处理队列的末尾
};

/*
 * This is the head of the queue of available
 * buffers-- all unused except for the 2 list heads.
 */
struct	buf bfreelist;

/*
 * These flags are kept in b_flags.
 */
#define	B_WRITE	0	/* non-read pseudo-flag */ // 进行写入处理
#define	B_READ	01	/* read when I/O occurs */ // 进行读取处理
#define	B_DONE	02	/* transaction finished */ // 此缓冲区拥有最新的数据，表示已经完成与设备的同步（读取或者写入），或者拥有比设备更新的数据（写入），设置此标志时，不会对设备进行读取数据的处理
#define	B_ERROR	04	/* transaction aborted */ // 发生错误
#define	B_BUSY	010	/* not on av_forw/back list */ // 此缓冲区处于使用中的状态，在av-list中不存在
#define	B_PHYS	020	/* Physical IO potentially using UNIBUS map */ // 表示处于RAW输入输出的状态
#define	B_MAP	040	/* This block has the UNIBUS map allocated */
#define	B_WANTED 0100	/* issue wakeup when BUSY goes off */ // 此缓冲区处于使用中的状态，第三者正等待该缓冲区被释放
#define	B_RELOC	0200	/* no longer used */
#define	B_ASYNC	0400	/* don't wait for I/O completion */ // 进行预先读取，异步写入，不等待设备处理结束，处理结束后，在被调用的iodone()中对B_BUSY进行重置
#define	B_DELWRI 01000	/* don't write till block leaves available list */ // 进行延迟写入，并非立即将数据写入设备，而是在通过getblk()对缓冲区进行再分配时候，或是在 bflush()被调用的时候将数据写入设备
