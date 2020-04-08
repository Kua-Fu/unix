/*
 * The I node is the focus of all
 * file activity in unix. There is a unique
 * inode allocated for each active file,
 * each current directory, each mounted-on
 * file, text file, and the root. An inode is 'named'
 * by its dev/inumber pair. (iget/iget.c)
 * Data, from mode on, is read in
 * from permanent inode on volume.
 */

/*
 * 内核从块设备读取inode的数据并将其转换为inode[]数组元素的形式，通过操作该元素实现对inode的操作
 * inode[]元素以设备编号和inode编号进行命名
 * 所有块设备的inode由同一个inode[]管理，此外，inode[]元素通过参照计数器来记录该元素是否处于使用中的状态
 * inode[]可以起到缓存的作用，inode[]中某个元素即使不再使用也不会马上被删除，在该元素再次被其他inode使用之前，其数据仍然保存在inode[]中
 * 因此，inode[]中保存着当前正在使用的inode，以及最近曾经使用过的inode的数据，当所需的inode在inode[]中存在时候，无需再从块设备中读取
 * 此外，通过对inode[]元素进行排他处理，可以防止多个进程操作同一个文件时候，引发的冲突
 *
 */

struct	inode // 内存中的inode通过inode结构体的数组inode[]管理
{             // 代表着被读取至内存的inode的数据结构
	char	i_flag;  // 标志
	char	i_count;	/* reference count */ // 参照计数器
	int	i_dev;		/* device where inode resides */ // 设备编号
	int	i_number;	/* i number, 1-to-1 with device address */ // inode编号，0表示未使用的元素
	int	i_mode; // 状态、控制信息，低位9比特表示权限
	char	i_nlink;	/* directory entries */ // 来自目录的参照数量
	char	i_uid;		/* owner */ // 用户ID
	char	i_gid;		/* group of owner */ // 组ID
	char	i_size0;	/* most significant of size */ // 文件长度的高位8比特
	char	*i_size1;	/* least sig */ // 文件长度的低位16比特
	int	i_addr[8];	/* device addresses constituting file */ // 使用的存储区域的块编号
	int	i_lastr;	/* last logical block read (for read-ahead) */ // 在此之前读取的逻辑块的编号，用于预读取功能
} inode[NINODE]; // NINODE值为100

/* flags */
#define	ILOCK	01		/* inode is locked */ // 已加索
#define	IUPD	02		/* inode has been modified */ // 已更新
#define	IACC	04		/* inode access time to be updated */ // 已参照
#define	IMOUNT	010		/* inode is mounted on */ // 该inode[]元素为挂载点
#define	IWANT	020		/* some process waiting on lock */ // 存在等待解锁的inode[]元素的进程
#define	ITEXT	040		/* inode is pure text prototype */ // 该inode[]元素作为代码段分配给进程

/* modes */
#define	IALLOC	0100000		/* file is used */
#define	IFMT	060000		/* type of file */
#define		IFDIR	040000	/* directory */
#define		IFCHR	020000	/* character special */
#define		IFBLK	060000	/* block special, 0 is regular */
#define	ILARG	010000		/* large addressing algorithm */
#define	ISUID	04000		/* set user id on execution */
#define	ISGID	02000		/* set group id on execution */
#define ISVTX	01000		/* save swapped text even after use */
#define	IREAD	0400		/* read, write, execute permissions */
#define	IWRITE	0200
#define	IEXEC	0100
