/*
 * Text structure.
 * One allocated per pure
 * procedure on swap device.
 * Manipulated by text.c
 */
struct text
{
	int	x_daddr;	/* disk address of segment */ // 交换磁盘中的地址（1个块=512字节）
	int	x_caddr;	/* core address, if loaded */ // 读入内存时候的物理内存地址（以64字节为单位）
	int	x_size;		/* size (*64) */ // 代码段的长度（以64字节为单位）
	int	*x_iptr;	/* inode of prototype */ // 指向inode[]中对应程序执行文件的元素
	char	x_count;	/* reference count */ // 以所有进程为对象的参照计数器
	char	x_ccount;	/* number of loaded references */ // 以内存中的进程为对象的参照计数器
} text[NTEXT];
