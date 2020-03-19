/*
 * One structure allocated per active
 * process. It contains all data needed
 * about the process while the
 * process may be swapped out.
 * Other per process data (user.h)
 * is swapped with the process.
 */
struct	proc
{
	char	p_stat; // 状态，等于NULL时候意味着proc[]数组中该元素为空
	char	p_flag; // 标志变量
	char	p_pri;		/* priority, negative is high */ // 执行优先级，数值越小优先级越高，下次被执行的可能性也就越大
	char	p_sig;		/* signal number sent to this process */ // 接收到的信号
	char	p_uid;		/* user id, used to direct tty signals */ // 用户ID（整数）
	char	p_time;		/* resident time for scheduling */ // 在内存或者交换空间内存在的时间（秒）
	char	p_cpu;		/* cpu usage for scheduling */ // 占用CPU的累计时间（时钟tick数）
	char	p_nice;		/* nice for scheduling */ // 用来降低执行优先级的补正系数
	int	p_ttyp;		/* controlling tty */ // 正在操作进程的终端
	int	p_pid;		/* unique process id */ // 进程ID
	int	p_ppid;		/* process id of parent */ // 父进程ID
	int	p_addr;		/* address of swappable image */ // 数据段的物理地址（单位为64字节）
	int	p_size;		/* size of swappable image (*64 bytes) */ // 数据段的长度（单位为64字节）
	int	p_wchan;	/* event process is awaiting */ // 使进程进入休眠状态的原因
	int	*p_textp;	/* pointer to text structure */ // 使用的代码段(text segment)
} proc[NPROC];

/* stat codes */
#define	SSLEEP	1		/* sleeping on high priority */ // 高优先级休眠状态，执行优先级为负值
#define	SWAIT	2		/* sleeping on low priority */ // 低优先级休眠状态，执行优先级为0或者正值
#define	SRUN	3		/* running */ // 可执行状态
#define	SIDL	4		/* intermediate state in process creation */ // 进程生成中
#define	SZOMB	5		/* intermediate state in process termination */ // 僵尸状态
#define	SSTOP	6		/* process being traced */ // 等待被跟踪(trace)

/* flag codes */
#define	SLOAD	01		/* in core */ // 进程图像处于内存中（未被换出到交换空间）
#define	SSYS	02		/* scheduling process */  // 系统进程，不会被交换出交互空间，在UNIX v6中只有proc[0]是系统进程
#define	SLOCK	04		/* process cannot be swapped */ // 进程调度锁，不允许进程图像被换出到交换空间
#define	SSWAP	010		/* process is being swapped out */ // 进程图像已经被交换到交换空间
#define	STRC	020		/* process is being traced */ // 处于被跟踪状态
#define	SWTED	040		/* another tracing flag */ // 在被跟踪时候使用
