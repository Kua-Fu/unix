#
/*
 */

#include "../param.h"
#include "../user.h"
#include "../filsys.h"
#include "../file.h"
#include "../conf.h"
#include "../inode.h"
#include "../reg.h"

/*
 * Convert a user supplied
 * file descriptor into a pointer
 * to a file structure.
 * Only task is to check range
 * of the descriptor.
 */
getf(f)
{
	register *fp, rf;

	rf = f;
	if(rf<0 || rf>=NOFILE)
		goto bad;
	fp = u.u_ofile[rf];
	if(fp != NULL)
		return(fp);
bad:
	u.u_error = EBADF;
	return(NULL);
}

/*
 * Internal form of close.
 * Decrement reference count on
 * file structure and call closei
 * on last closef.
 * Also make sure the pipe protocol
 * does not constipate.
 */
closef(fp)
int *fp;
{
	register *rfp, *ip;

	rfp = fp;
	if(rfp->f_flag&FPIPE) {
		ip = rfp->f_inode;
		ip->i_mode =& ~(IREAD|IWRITE);
		wakeup(ip+1);
		wakeup(ip+2);
	}
	if(rfp->f_count <= 1)
		closei(rfp->f_inode, rfp->f_flag&FWRITE);
	rfp->f_count--;
}

/*
 * Decrement reference count on an
 * inode due to the removal of a
 * referencing file structure.
 * On the last closei, switchout
 * to the close entry point of special
 * device handler.
 * Note that the handler gets called
 * on every open and only on the last
 * close.
 */
closei(ip, rw)
int *ip;
{
	register *rip;
	register dev, maj;

	rip = ip;
	dev = rip->i_addr[0];
	maj = rip->i_addr[0].d_major;
	if(rip->i_count <= 1)
	switch(rip->i_mode&IFMT) {

	case IFCHR:
		(*cdevsw[maj].d_close)(dev, rw);
		break;

	case IFBLK:
		(*bdevsw[maj].d_close)(dev, rw);
	}
	iput(rip);
}

/*
 * openi called to allow handler
 * of special files to initialize and
 * validate before actual IO.
 * Called on all sorts of opens
 * and also on mount.
 */
openi(ip, rw)
int *ip;
{
	register *rip;
	register dev, maj;

	rip = ip;
	dev = rip->i_addr[0];
	maj = rip->i_addr[0].d_major;
	switch(rip->i_mode&IFMT) {

	case IFCHR:
		if(maj >= nchrdev)
			goto bad;
		(*cdevsw[maj].d_open)(dev, rw);
		break;

	case IFBLK:
		if(maj >= nblkdev)
			goto bad;
		(*bdevsw[maj].d_open)(dev, rw);
	}
	return;

bad:
	u.u_error = ENXIO;
}

/*
 * Check mode permission on inode pointer.
 * Mode is READ, WRITE or EXEC.
 * In the case of WRITE, the
 * read-only status of the file
 * system is checked.
 * Also in WRITE, prototype text
 * segments cannot be written.
 * The mode is shifted to select
 * the owner/group/other fields.
 * The super user is granted all
 * permissions except for EXEC where
 * at least one of the EXEC bits must
 * be on.
 */

/*
 * access()用来检查文件的权限设定，
 * 将由参数指定的mode(读写执行)与inode[]元素的i_mode的低位9bit进行比较，确认是否具有相应的权限
 * inode.i_mode的低位9bit表示文件的权限设定，从高位开始以3bit为1组，分别表示文件拥有者、组用户、其他用户具有的权限
 * 如果是写入，那么不能将文件系统本身设置为只读状态，此外，作为代码段被使用的文件不能被写入
 * 超级用户(user.u_uid为0）一定会有权限，但是如果没有为文件拥有者、组用户、其他用户的其中之一设定执行权限，那么即使是超级用户，也无法执行该文件
 * 如果具有权限则返回0，否则返回1，权限不足时候，u.u_error被设置为错误代码
 * 参数: (1) aip, inode[]元素， (2) mode, 处理种类：读写执行
 *
 */

access(aip, mode)
int *aip;
{
	register *ip, m;

	ip = aip;
	m = mode;
	if(m == IWRITE) { // 检查是否具有写入权限时候的处理，利用getfs()取得超级块，检查是否为只读状态，同时确认该文件不是作为代码段的文件
		if(getfs(ip->i_dev)->s_ronly != 0) {
			u.u_error = EROFS;
			return(1);
		}
		if(ip->i_flag & ITEXT) {
			u.u_error = ETXTBSY;
			return(1);
		}
	}
	if(u.u_uid == 0) { // 超级用户的处理
		if(m == IEXEC && (ip->i_mode & 
			(IEXEC | (IEXEC>>3) | (IEXEC>>6))) == 0)
				goto bad;
		return(0);
	}
	if(u.u_uid != ip->i_uid) { // 根据权限检查对象的inode[]元素与当前用户的所属关系，调整参数mode的比特位置，对inode[]元素的i_mode与参数mode进行与(&)运算
		m =>> 3;
		if(u.u_gid != ip->i_gid)
			m =>> 3;
	}
	if((ip->i_mode&m) != 0) // 如果结果不为0，则可以确定用户具有访问权限
		return(0);

bad:
	u.u_error = EACCES;
	return(1);
}

/*
 * Look up a pathname and test if
 * the resultant inode is owned by the
 * current user.
 * If not, try for super-user.
 * If permission is granted,
 * return inode pointer.
 */
owner()
{
	register struct inode *ip;
	extern uchar();

	if ((ip = namei(uchar, 0)) == NULL)
		return(NULL);
	if(u.u_uid == ip->i_uid)
		return(ip);
	if (suser())
		return(ip);
	iput(ip);
	return(NULL);
}

/*
 * Test if the current user is the
 * super user.
 */
suser()
{

	if(u.u_uid == 0)
		return(1);
	u.u_error = EPERM;
	return(0);
}

/*
 * Allocate a user file descriptor.
 */
ufalloc()
{
	register i;

	for (i=0; i<NOFILE; i++)
		if (u.u_ofile[i] == NULL) {
			u.u_ar0[R0] = i;
			return(i);
		}
	u.u_error = EMFILE;
	return(-1);
}

/*
 * Allocate a user file descriptor
 * and a file structure.
 * Initialize the descriptor
 * to point at the file structure.
 *
 * no file -- if there are no available
 * 	file structures.
 */
falloc()
{
	register struct file *fp;
	register i;

	if ((i = ufalloc()) < 0)
		return(NULL);
	for (fp = &file[0]; fp < &file[NFILE]; fp++)
		if (fp->f_count==0) {
			u.u_ofile[i] = fp;
			fp->f_count++;
			fp->f_offset[0] = 0;
			fp->f_offset[1] = 0;
			return(fp);
		}
	printf("no file\n");
	u.u_error = ENFILE;
	return(NULL);
}
