/**
 * @file syscalls.c
 * @brief newlib 系统调用桩（bare-metal，无文件系统）
 *
 * 作用：
 *   1. 消除链接时 libnosys.a 打印的
 *      "_close / _lseek / _read / _write is not implemented and will always fail" 提示；
 *   2. 为后续把 printf 重定向到串口预留位置（见 _write）。
 *
 * 背景：
 *   newlib 的 stdio 内部（libc_a-stdio.o）会引用 _close_r / _lseek_r / _read_r / _write_r，
 *   进而引用 _close / _lseek / _read / _write，并把 libnosys.a 里的空桩链进来。
 *   这些代码在本工程会被 --gc-sections 丢弃，但链接器处理符号时仍会打印提示；
 *   这里给出自己的实现后，libnosys 的空桩不再被引用，提示自然消失。
 *
 * 注意：本文件需要加入工程才会参与编译（EIDE：Core 文件夹右键 → Add File）。
 */
#include <sys/stat.h>
#include <errno.h>

/* close：本工程没有文件系统，直接返回失败 */
int _close(int file)
{
	(void)file;
	errno = ENOSYS;
	return -1;
}

/* lseek：不支持定位，返回 0（newlib 视作“从流起始位置开始”） */
int _lseek(int file, int offset, int whence)
{
	(void)file;
	(void)offset;
	(void)whence;
	return 0;
}

/* read：本工程没有输入设备 */
int _read(int file, char *ptr, int len)
{
	(void)file;
	(void)ptr;
	(void)len;
	errno = ENOSYS;
	return -1;
}

/* write：printf / puts 最终会走到这里。返回 len 表示“视为写成功（内容丢弃）”
 * TODO：若要输出到串口，在这里调用 HAL_UART_Transmit(...) 之后 return len; */
int _write(int file, char *ptr, int len)
{
	(void)file;
	(void)ptr;
	return len;
}

/* fstat / isatty：newlib 用来判断标准流是否为字符设备，配合 printf 使用 */
int _fstat(int file, struct stat *st)
{
	(void)file;
	st->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int file)
{
	(void)file;
	return 1;
}
