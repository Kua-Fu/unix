#
#include "../param.h"
#include "../inode.h"
#include "../user.h"
#include "../systm.h"
#include "../buf.h"

/*
 * Convert a pathname into a pointer to
 * an inode. Note that the inode is locked.
 *
 * func = function called to get next char of name
 *	&uchar if name is in user space
 *	&schar if name is in system space
 * flag = 0 if name is sought
 *	1 if name is to be created
 *	2 if name is to be deleted
 */

/*
 * namei()，遍历文件路径，取得与文件路径名相对应的文件或者目录的inode[]元素
 * 如果给出的路径名起始字符为'/', 则可以判断该路径为绝对路径，将根目录作为遍历的起点，否则可判断为相对路径，将当前路径作为遍历的起点
 * 从路径名的起始位置，开始逐个取得构成路径的各个元素，然后遍历各个目录对应表所包含的记录，在使用iget()取得与各个记录相对应的inode[]元素，重复上述处理
 * 遍历目录的同时检查路径是否恰当和访问权限，如果对某个目录不具备执行权限，是无法访问该目录中的数据的
 * namei()的参数中的flag代表准备对路径表示的文件进行的操作，可指定的操作包含: 寻找，生成或者删除，根据指定的操作，会相应的更新user结构体的内容
 * u.u_pdir, 只有当准备生成新的文件或者目录时候，才会设定为与对象文件的父目录相对应的inode[]元素，例如：如果准备生成名为'/home/yz/test.log'的文件
 * 则将u.u_pdir, 设定为与'/home/yz'相对应的inode[]元素
 * u.u_offset，只有当准备生成新的文件或者目录时候，才会设定为指向对象文件父目录中空记录的偏移量
 * u.u_dbuf, 文件名或者目录名，如果路径为'/home/yz/test.log'，则将u.u_dbuf设定为'test.log'
 * 很多的函数都是使用上述更新后的数据进行自身处理的，因此在执行这些函数之前，需要首先执行namei()
 * namei()通过执行schar()和uchar()取得路径名，区别是路径名保存在用户还是内核空间
 * 一般，用户程序通过系统调用对文件进行操作，此时在trap()中，将u.u_dirp设定为u.u_arg[0], u.uarg[0]是赋予系统调用的参数，被设定为路径名的地址
 * uchar()通过一边对u.u_dirp进行位移操作，一边执行fubyte()，在用户空间逐个处理构成数据的字符
 *
 * 当内核程序希望独自操作文件时候，也采取将u.u_dirp设定为路径名地址的方式，此时不会发生模式的切换，而是通过递增u.u_dirp对字符进行逐个处理
 * 参数：(1) func, &uchar或者&schar，区别在于路径名是存在于用户空间还是内核空间； (2) flag, 0: 寻找路径名所示的inode, 1: 生成路径名所示的inode, 2: 删除路径名所示的inode
 */

namei(func, flag)
int (*func)();
{
	register struct inode *dp;
	register c;
	register char *cp;
	int eo, *bp;

	/*
	 * If name starts with '/' start from
	 * root; otherwise start from current dir.
	 */

	dp = u.u_cdir;
	if((c=(*func)()) == '/') // rootdir 表示与根目录相对应的inode[]元素，在系统启动的时候，设定
		dp = rootdir;        // 如果路径名以'/'开始，则将dp设定为与根目录相对应的inode[]元素，否则将其设定为u.u_cdir（当前目录), dp将作为遍历的起点
	iget(dp->i_dev, dp->i_number); // 执行iget()并等待解锁inode[]元素，然后确认相关的文件系统已经被挂载，递增参照计数器，在对inode[]元素加锁
	while(c == '/') // 如果路径名中的指针指向'/'， 则忽略该字符，移动指针使其指向'/'的下一个字符，并将字符赋予c, 结束循环时候，如果路径名为'///home/yz'，则c的值为'h'，如果路径名为'foo/var', 则c的值为f
		c = (*func)();
	if(c == '\0' && flag != 0) { // 当路径名为' '（空字符），'/' 或者 '///'时候，产生错误，根目录是无法生成或者删除的
		u.u_error = ENOENT;
		goto out;
	}

cloop: // 从u.u_dirp指向的元素中取出一个元素，之后将对其进行各种处理，此时dp指向与最后处理的要素相对应的inode[]元素
	/*
	 * Here dp contains pointer
	 * to last component matched.
	 */

	if(u.u_error) // 如果发生了错误，则跳转到out, u.u_error中可能容纳由iget()等生成的错误代码
		goto out;
	if(c == '\0')  // 如果已经到路径名的末尾，则返回dp,例如：路径名为'/home/yz/test.log'时候，此处dp的值为与'test.log'相对应的inode[]元素
		return(dp);

	/*
	 * If there is another component,
	 * dp must be a directory and
	 * must have x permission.
	 */

	if((dp->i_mode&IFMT) != IFDIR) { // 如果dp不为目录，则将进行错误处理，例如: 路径名为'/home/yz/test.log'， 若dp的值为与'yz'相对应的inode[]的元素
		u.u_error = ENOTDIR;
		goto out;
	}
	if(access(dp, IEXEC)) // 如果对dp不具备执行权限则进行错误处理，假设路径名为'/home/yz/test.log'， 如果对目录'yz'不具备执行权限，则将引发错误
		goto out;

	/*
	 * Gather up name into
	 * users' dir buffer.
	 */

	cp = &u.u_dbuf[0]; // u.u_dbuf容纳着dp指向的元素的下一个元素名, 例如：路径名为'/home/yz/test.log'，且dp指向与'/home/' 相对应的inode[]元素时候，u.u_dbuf的值是'yz'
	while(c!='/' && c!='\0' && u.u_error==0) { // 由于u.u_dbuf只能容纳DIRSIZ(14)个字符，之后的字符将被忽略，如果不到14个字符，则剩余的字符将用NULL(0)填充
		if(cp < &u.u_dbuf[DIRSIZ])
			*cp++ = c;
		c = (*func)();
	}
	while(cp < &u.u_dbuf[DIRSIZ])
		*cp++ = '\0';
	while(c == '/') // 忽略重复的'/'字符
		c = (*func)();
	if(u.u_error) // 如果设定了u.u_error，则进行错误处理，u.u_error中可能容纳由uchar()等生成的错误代码
		goto out; // 例如: 路径名为'/home/yz/test.log'， 且dp指向与'home'相对应的inode[]元素时候，此时u.u_dbuf的值为'yz\0\0\0...', 而u.u_dirp指向'test.log'头部的't'

	/*
	 * Set up to search a directory.
	 */

	u.u_offset[1] = 0; // 由于dp指向与目录相对应的inode[]元素，此后将从dp代表的目录的对应表中寻找与u.u_dbuf容纳的元素名相对应的记录
	u.u_offset[0] = 0; // 因此，在此处首先进行初始化设定，u.u_count表示目录对应表中的记录数，因为目录的文件长度等于记录数*16, 所以将文件长度除以16(DIRSIZ+2)即可得到记录数
	u.u_segflg = 1;
	eo = 0;
	u.u_count = ldiv(dp->i_size1, DIRSIZ+2);
	bp = NULL;

eloop: // 检查目录对应表中一条记录的处理

	/*
	 * If at the end of the directory,
	 * the search failed. Report what
	 * is appropriate as per flag.
	 */

	if(u.u_count == 0) { // 当检查完所有记录，也没有找到与u.u_dbuf容纳的元素名对应的记录时候的处理，每处理一条记录后将递减u.u_count
		if(bp != NULL) // 如果bp有块设备缓冲区，则释放该缓冲区
			brelse(bp);
		if(flag==1 && c=='\0') { // 如果flag为1，（试图生成文件或者目录), 且u.u_dirp指向路径末尾时候的处理，假设路径名为'/home/yz/test.log'， 且dp指向与'yz'相对应的inode[]元素,
			if(access(dp, IWRITE)) // u.u_dbuf的值为'test.log\0\0...', 而u.u_dirp指向路径末尾('test.log'之后), 此时的状态表示试图在目录'/home/yz'中生成名为'test.log'的文件或者目录
				goto out;          // 如果dp不具备写入权限，则将引发错误
			u.u_pdir = dp;   // 将u.u_pdir设定为dp, 此处的u.u_pdir将使用在别的函数中
			if(eo) // 如果eo的值不为0， 则将u.u_offset[1]设为由eo减去16字节后的值，因为eo指向位于dp代表的目录对应表中空记录之后的记录，减去16(一条记录的长度)后，将指向空记录的起始位置
				u.u_offset[1] = eo-DIRSIZ-2; else // 如果eo的值为0， 即当目录对应表中不存在空记录时候，设置dp的inode更新标志位
				dp->i_flag =| IUPD;
			return(NULL);
		}
		u.u_error = ENOENT; // 如果在目录的记录中没有找到对象记录，将引发ENOENT错误
		goto out;
	}

	/*
	 * If offset is on a block boundary,
	 * read the next directory block.
	 * Release previous if it exists.
	 */

	if((u.u_offset[1]&0777) == 0) { // 当u.u_offset[1]指向块(512字节）的边界时候的处理 ，在首次遍历某个目录的记录时候，一定会执行此处理
		if(bp != NULL)              // 如果bp已经持有块设备缓冲区，则释放
			brelse(bp);
		bp = bread(dp->i_dev, // 读取下一个块
			bmap(dp, ldiv(u.u_offset[1], 512)));
	}

	/*
	 * Note first empty directory slot
	 * in eo for possible creat.
	 * String compare the directory entry
	 * and the current component.
	 * If they do not match, go back to eloop.
	 */

	bcopy(bp->b_addr+(u.u_offset[1]&0777), &u.u_dent, (DIRSIZ+2)/2); // 从块设备缓冲区向u.u_dent复制对应表中的一条记录，u.u_dent.u_ino表示inode编号, u.u_dent.u_name表示文件或者目录名
	u.u_offset[1] =+ DIRSIZ+2; // 将u.u_offset[1]与16（一条记录的长度）相加，每处理一条记录，将u.u_count的值减去1
	u.u_count--;
	if(u.u_dent.u_ino == 0) { // 如果eo的值为0，则将eo设定为u.u_offset[1]，使eo指向当前目录对应表中空记录之后的一条记录，返回至eloop检查下一条记录
		if(eo == 0)
			eo = u.u_offset[1];
		goto eloop;
	}
	for(cp = &u.u_dbuf[0]; cp < &u.u_dbuf[DIRSIZ]; cp++) // 将u.u_dbuf中保存的字符串与u.u_dent.u_name中保存的字符串进行比较，如果不一致，则返回至eloop检查下一条记录
		if(*cp != cp[u.u_dent.u_name - u.u_dbuf]) // u.u_dent.u_name - u.u_dbuf用于计算u.u_dent.u_name和u.u_dbuf地址之差，加上*cp（指向u.u_dbuf中的第x个字符）后
			goto eloop;                           // 即可指向u.u_dent.u_name中的第x个字符，如果字符串一致，则表示在dp代表的目录中找到了与u.u_dbuf相对应的记录

	/*
	 * Here a component matched in a directory.
	 * If there is more pathname, go back to
	 * cloop, otherwise return.
	 */

	if(bp != NULL) // 如果bp已经有块设备缓冲区，则将其释放
		brelse(bp);
	if(flag==2 && c=='\0') { // 如果准备删除文件或者目录，且已经到达路径名的末尾，则检查是否具有对该文件或目录的父目录的写入权限，如果具有权限，则返回dp
		if(access(dp, IWRITE)) // 调用namei()函数，通过清除父目录对应表中相应记录的inode编号，达到删除文件或者目录的目的
			goto out;
		return(dp);
	}
	bp = dp->i_dev; // 释放dp指向的inode[]元素，将在目录对应表中找到的记录相对应的inode[]元素赋予dp
	iput(dp);
	dp = iget(bp, u.u_dent.u_ino);
	if(dp == NULL)
		return(NULL);
	goto cloop;

out:
	iput(dp); // 用来递减inode[]元素的参照计数器的值，当参照计数器的值为0时，将inode[]元素的内容写回块设备
	return(NULL);
}

/*
 * Return the next character from the
 * kernel string pointed at by dirp.
 */
schar()
{

	return(*u.u_dirp++ & 0377);
}

/*
 * Return the next character from the
 * user string pointed at by dirp.
 */
uchar()
{
	register c;

	c = fubyte(u.u_dirp++);
	if(c == -1)
		u.u_error = EFAULT;
	return(c);
}
