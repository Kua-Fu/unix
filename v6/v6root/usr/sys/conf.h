/*
 * Used to dissect integer device code
 * into major (driver designation) and
 * minor (driver parameter) parts.
 */
struct
{
	char	d_minor;
	char	d_major;
};

/*
 * Declaration of block device
 * switch. Each entry (row) is
 * the only link between the
 * main unix code and the driver.
 * The initialization of the
 * device switches is in the
 * file conf.c.
 */

/*
 * 块设备驱动为操作块设备的程序，每一种块设备都具有对应的驱动程序，驱动程序集中了对设备的各种操作功能，同时也包含了设备的规格参数
 * 其他程序通过调用与设备对应的驱动程序，可以在无须了解设备的详情下进行操作
 *
 * 块设备驱动通过块设备驱动表bdevsw[]进行管理,bdevsw结构体包含打开、关闭、访问相应的设备的函数地址，以及指向位于b-list起始位置的devtab结构体的地址
 * 这些函数和与设备引发的中断相对应的处理函数，构成了设备驱动的实体
 * bdevsw[]中包含了与系统中所有块设备（的种类）相对应的设备驱动，系统管理者要根据系统构成编辑bdevsw[]，并对内核进行重新构筑
 *
 * 设备处理队列
 * 设备驱动拥有设备处理队列，队列头部的元素与b-list相同，为devtab结构体（bdevsw[].d_tab), 队列中包含缓冲区
 * devtab结构体的d_actf和d_actl分别指向位于头部和末尾的缓冲区，而各缓冲区的buf.av_forw指向后方的缓冲区，末尾的缓冲区的buf.av_forw指向NULL(0)
 * 设备驱动使用的缓冲区的状态为处理中(B_BUSY)，并从av-list中清除，因此，即使直接操作buf.av_forw的值，也不会和av-list发生冲突
 * 新的缓冲区被追加到队列的末尾，设备驱动从队列的头部开始处理
 *
 * 块设备的处理流程；具体细节由设备驱动的实现方法决定，这里只是比较典型的例子
 * （1）内核（主要是块设备子系统）执行访问函数
 * （2）将传递进来的缓冲区追加到设备处理队列
 * （3）操作设备的寄存器，并开始对位于设备处理队列头部的缓冲区进行输入输出处理
 * （4）处理结束后设备将引发中断，如果设备处理队列中还留有缓冲区，中断处理函数将继续进行输入输出处理
 *
 * 当系统运行时候，打开设备的函数会预先对设备进行初始化处理，当系统运行结束时候，关闭设备的函数将终止设备的运行，根据设备的种类，处理也不总是必须进行的
 *
 */

struct	bdevsw // 块设备驱动表
{
	int	(*d_open)(); // 指向打开设备的函数的指针
	int	(*d_close)(); // 指向关闭设备的函数的指针
	int	(*d_strategy)(); // 指向访问设备的函数的指针，读取和写入共用一个函数，通过缓冲区的标志位判断进行何种操作
	int	*d_tab; // 指向devtab结构体的指针
} bdevsw[];

/*
 * Nblkdev is the number of entries
 * (rows) in the block switch. It is
 * set in binit/bio.c by making
 * a pass over the switch.
 * Used in bounds checking on major
 * device numbers.
 */
int	nblkdev;

/*
 * Character device switch.
 */
struct	cdevsw
{
	int	(*d_open)();
	int	(*d_close)();
	int	(*d_read)();
	int	(*d_write)();
	int	(*d_sgtty)();
} cdevsw[];

/*
 * Number of character switch entries.
 * Set by cinit/tty.c
 */
int	nchrdev;
