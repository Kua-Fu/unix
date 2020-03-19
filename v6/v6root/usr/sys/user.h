/*
 * The user structure.
 * One allocated per process.
 * Contains all per process data
 * that doesn't need to be referenced
 * while the process is swapped.
 * The user block is USIZE*64 bytes
 * long; resides at virtual kernel
 * loc 140000; contains the system
 * stack per user; is cross referenced
 * with the proc structure for the
 * same process.
 */
struct user
{
	int	u_rsav[2];		/* save r5,r6 when exchanging stacks */ // 进程切换时候用来保存r5和r6的当前值
	int	u_fsav[25];		/* save fp registers */
					/* rsav and fsav must be first in structure */
	char	u_segflg;		/* flag for IO; user or kernel space */ // 读写文件时候使用的标志变量
	char	u_error;		/* return error code */ // 出错时候用来保存错误代码
	char	u_uid;			/* effective user id */ // 实效用户ID
	char	u_gid;			/* effective group id */ // 实效组ID
	char	u_ruid;			/* real user id */ // 实际用户ID
	char	u_rgid;			/* real group id */ // 实际组ID
	int	u_procp;		/* pointer to proc structure */ // 此user结构体对应的数组proc[]的元素
	char	*u_base;		/* base address for IO */ // 读写文件时候用于传递参数
	char	*u_count;		/* bytes remaining for IO */ // 读写文件时候用于传递参数
	char	*u_offset[2];		/* offset in file for IO */ // 读写文件时候用于传递参数
	int	*u_cdir;		/* pointer to inode of current directory */ // 当前目录对应的数组inode[]的元素
	char	u_dbuf[DIRSIZ];		/* current pathname component */ // 供函数namei()使用的临时工作变量，用来存放文件和目录名
	char	*u_dirp;		/* current pointer to inode */ // 在读取由用户程序或者内核程序传来的文件的路径名时候使用
	struct	{			/* current directory entry */
		int	u_ino; // 存放inode的编号
		char	u_name[DIRSIZ]; // u_name存放文件或者目录名
	} u_dent; // 供函数namei()使用的临时工作变量，用来存放目录数据
	int	*u_pdir;		/* inode of parent directory of dirp */ // 供函数namei()存放对象文件和目录的父目录
	int	u_uisa[16];		/* prototype of segmentation addresses */ // 用户PAR的值
	int	u_uisd[16];		/* prototype of segmentation descriptors */ // 用户PDR的值
	int	u_ofile[NOFILE];	/* pointers to file structures of open files */ // 由进程打开的文件
	int	u_arg[5];		/* arguments to current system call */ // 用户程序向系统调用传递参数时候的使用
	int	u_tsize;		/* text size (*64) */ // 代码段的长度(单位为64字节）
	int	u_dsize;		/* data size (*64) */ // 数据区域的长度（单位为64字节）
	int	u_ssize;		/* stack size (*64) */ // 栈区域的长度（单位为64字节）
	int	u_sep;			/* flag for I and D separation */
	int	u_qsav[2];		/* label variable for quits and interrupts */ // 在系统调用处理中处理信息时候用来保存r5和r6的值
	int	u_ssav[2];		/* label variable for swapping */ // 当进程被换出至交换空间
	int	u_signal[NSIG];		/* disposition of signals */ // 用于设置收到信号后的动作
	int	u_utime;		/* this process user time */ // 用户模式下占用CPU的时间（时钟tick数)
	int	u_stime;		/* this process system time */ // 内核模式下占用CPU的时间（时钟tick数）
	int	u_cutime[2];		/* sum of childs' utimes */ // 子进程在用户模式下占用CPU的时间（时钟tick数）
	int	u_cstime[2];		/* sum of childs' stimes */ // 子进程在内核模式下占用CPU的时间(时钟tick数）
	int	*u_ar0;			/* address of users saved R0 */ // 系统调用处理中，操作用户进程的通用寄存器或者PSW时使用
	int	u_prof[4];		/* profile arguments */ // 用于统计
	char	u_intflg;		/* catch intr from sys */ // 标志变量，用于判断系统调用处理中是否发生了对信号的处理
					/* kernel stack per user
					 * extends from u + USIZE*64
					 * backward not to reach here
					 */
} u;

/* u_error codes */
#define	EFAULT	106 // 在用户空间和内核空间之间传递数据失败
#define	EPERM	1 // 当前用户不是超级用户
#define	ENOENT	2 // 指定文件不存在
#define	ESRCH	3 // 信号的目标进程不存在，或者已经结束
#define	EINTR	4 // 系统调用处理中对信号做了处理
#define	EIO	5 // IO错误
#define	ENXIO	6 // 设备编号所示设备不存在
#define	E2BIG	7 // 通过系统调用exec向待执行程序传送了超过512字节的参数
#define	ENOEXEC	8 // 无法识别待执行程序的格式（魔术数字）
#define	EBADF	9 // 试图操作未打开的文件，或者试图写入用只读模式打开的文件，或者试图读出用只写模式打开的文件
#define	ECHILD	10 // 执行系统调用wait时候，无法找到子进程
#define	EAGAIN	11 // 执行系统调用fork时候，无法在数组proc[]中找到空元素
#define	ENOMEM	12 // 试图向进程分配超过可以使用容量的内存
#define	EACCES	13 // 没有对文件或者目录的访问权限
#define	ENOTBLK	15 // 不是代表块设备的特殊文件
#define	EBUSY	16 // 执行系统调用mount, unmount时候，最为对象的挂载点仍在使用中
#define	EEXIST	17 // 执行系统调用link时候该文件已经存在
#define	EXDEV	18 // 试图对其他设备上的文件创建链接
#define	ENODEV	19 // 设备编号所示设备不存在
#define	ENOTDIR	20 // 不是目录
#define	EISDIR	21 // 试图对目录进行写入操作
#define	EINVAL	22 // 参数有误
#define	ENFILE	23 // 数组file[]溢出
#define	EMFILE	24 // 数组user.u_ofile[]溢出
#define	ENOTTY	25 // 不是代表终端的特殊文件
#define	ETXTBSY	26 // 准备加载到代码段的程序文件曾被其他进程当作数据文件使用，或者对准备加载到代码段的程序文件进行了写入操作
#define	EFBIG	27 // 文件尺寸过大
#define	ENOSPC	28 // 块设备的容量不足
#define	ESPIPE	29 // 对管道文件执行了系统调用seek
#define	EROFS	30 // 试图更新只读块设备上的文件或者目录
#define	EMLINK	31 // 文件连接数过多
#define	EPIPE	32 // 损坏的管道文件

