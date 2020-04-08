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
