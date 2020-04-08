/*
 * Inode structure as it appears on
 * the disk. Not used by the system,
 * but by things like check, df, dump.
 */
struct	inode // 块设备中inode的数据形式
{
	int	i_mode; // 状态、控制信息，低位9比特表示权限
	char	i_nlink; // 来自目录的参照数量
	char	i_uid; // 用户ID
	char	i_gid; // 组ID
	char	i_size0; // 文件长度的高位8比特
	char	*i_size1; // 文件长度的低位16比特
	int	i_addr[8]; // 使用的存储区域的块编号
	int	i_atime[2]; // 参照时间
	int	i_mtime[2]; // 更新时间
};

/* modes */
#define	IALLOC	0100000 // 当前inode已经被分配
#define	IFMT	060000 // 调查格式时候使用
#define		IFDIR	040000 // 目录
#define		IFCHR	020000 // 字符特殊文件
#define		IFBLK	060000 // 块特殊文件
#define	ILARG	010000 // 文件长度较大，使用间接参照
#define	ISUID	04000 // SUID比特
#define	ISGID	02000 // SGID比特
#define ISVTX	01000 // sticky比特
#define	IREAD	0400 // 读取权限
#define	IWRITE	0200 // 写入权限
#define	IEXEC	0100 // 执行权限
