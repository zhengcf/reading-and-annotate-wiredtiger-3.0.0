/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#define	WT_SYSCALL(call, ret) do {					\
	/*								\
	 * A call returning 0 indicates success; any call where		\
	 * 0 is not the only successful return must provide an		\
	 * expression evaluating to 0 in all successful	cases.		\
	 *								\
	 * XXX								\
	 * Casting the call's return to int is because CentOS 7.3.1611	\
	 * complains about syscall returning a long and the loss of	\
	 * integer precision in the assignment to ret. The cast should	\
	 * be a no-op everywhere.					\
	 */								\
	if (((ret) = (int)(call)) == 0)					\
		break;							\
	/*								\
	 * The call's error was either returned by the call or		\
	 * is in errno, and there are cases where it depends on		\
	 * the software release as to which it is (for example,		\
	 * posix_fadvise on FreeBSD and OS X). Failing calls		\
	 * must either return a non-zero error value, or -1 if		\
	 * the error value is in errno. (The WiredTiger errno		\
	 * function returns WT_ERROR if errno is 0, which isn't		\
	 * ideal but won't discard the failure.)			\
	 */								\
	if ((ret) == -1)						\
		(ret) = __wt_errno();					\
} while (0)

#define	WT_SYSCALL_RETRY(call, ret) do {				\
	int __retry;							\
	for (__retry = 0; __retry < 10; ++__retry) {			\
		WT_SYSCALL(call, ret);					\
		switch (ret) {						\
		case EAGAIN:						\
		case EBUSY:						\
		case EINTR:						\
		case EIO:						\
		case EMFILE:						\
		case ENFILE:						\
		case ENOSPC:						\
			__wt_sleep(0L, 50000L);				\
			continue;					\
		default:						\
			break;						\
		}							\
		break;							\
	}								\
} while (0)

#define	WT_TIMEDIFF_NS(end, begin)					\
	(WT_BILLION * (uint64_t)((end).tv_sec - (begin).tv_sec) +	\
	    (uint64_t)(end).tv_nsec - (uint64_t)(begin).tv_nsec)
#define	WT_TIMEDIFF_US(end, begin)					\
	(WT_TIMEDIFF_NS((end), (begin)) / WT_THOUSAND)
#define	WT_TIMEDIFF_MS(end, begin)					\
	(WT_TIMEDIFF_NS((end), (begin)) / WT_MILLION)
#define	WT_TIMEDIFF_SEC(end, begin)					\
	(WT_TIMEDIFF_NS((end), (begin)) / WT_BILLION)

#define	WT_TIMECMP(t1, t2)						\
	((t1).tv_sec < (t2).tv_sec ? -1 :				\
	     (t1).tv_sec == (t2).tv_sec ?				\
	     (t1).tv_nsec < (t2).tv_nsec ? -1 :				\
	     (t1).tv_nsec == (t2).tv_nsec ? 0 : 1 : 1)

/*
 * Macros to ensure a file handle is inserted or removed from both the main and
 * the hashed queue, used by connection-level and in-memory data structures.
 */
#define	WT_FILE_HANDLE_INSERT(h, fh, bucket) do {			\
	TAILQ_INSERT_HEAD(&(h)->fhqh, fh, q);				\
	TAILQ_INSERT_HEAD(&(h)->fhhash[bucket], fh, hashq);		\
} while (0)

#define	WT_FILE_HANDLE_REMOVE(h, fh, bucket) do {			\
	TAILQ_REMOVE(&(h)->fhqh, fh, q);				\
	TAILQ_REMOVE(&(h)->fhhash[bucket], fh, hashq);			\
} while (0)

//创建空间和赋值见__wt_open，该结构item最终是存入到WT_CONNECTION_IMPL.fhhash hash表中的
struct __wt_fh {
	/*
	 * There is a file name field in both the WT_FH and WT_FILE_HANDLE
	 * structures, which isn't ideal. There would be compromises to keeping
	 * a single copy: If it were in WT_FH, file systems could not access
	 * the name field, if it were just in the WT_FILE_HANDLE internal
	 * WiredTiger code would need to maintain a string inside a structure
	 * that is owned by the user (since we care about the content of the
	 * file name). Keeping two copies seems most reasonable.
	 */
	const char *name;			/* File name */
    //该fh处于WT_CONNECTION_IMPL的那个hash槽位，见__handle_search
	uint64_t name_hash;			/* hash of name */
	TAILQ_ENTRY(__wt_fh) q;			/* internal queue */
	TAILQ_ENTRY(__wt_fh) hashq;		/* internal hash queue */
	u_int ref;				/* reference count */

	WT_FILE_HANDLE *handle; //文件读写关闭等接口
};

#ifdef _WIN32
struct __wt_file_handle_win {
	WT_FILE_HANDLE iface;

	/*
	 * Windows specific file handle fields
	 */
	HANDLE filehandle;			/* Windows file handle */
	HANDLE filehandle_secondary;		/* Windows file handle
						   for file size changes */
	bool	 direct_io;			/* O_DIRECT configured */
};

#else

//见__posix_open_file
struct __wt_file_handle_posix {
	WT_FILE_HANDLE iface;

	/*
	 * POSIX specific file handle fields
	 */
	int	 fd;				/* POSIX file handle */

	bool	 direct_io;			/* O_DIRECT configured */
};
#endif

struct __wt_file_handle_inmem {
	WT_FILE_HANDLE iface;

	/*
	 * In memory specific file handle fields
	 */
	uint64_t name_hash;			/* hash of name */
	TAILQ_ENTRY(__wt_file_handle_inmem) q;	/* internal queue, hash queue */
	TAILQ_ENTRY(__wt_file_handle_inmem) hashq;

	WT_ITEM  buf;				/* Data */
	u_int	 ref;				/* Reference count */
};

//初始化赋值见__stdio_init
struct __wt_fstream {
	const char *name;			/* Stream name */

	FILE *fp;				/* stdio FILE stream */
	WT_FH *fh;				/* WT file handle */
	wt_off_t off;				/* Read/write offset */
	wt_off_t size;				/* File size */
	WT_ITEM  buf;				/* Data */

#define	WT_STREAM_APPEND	0x01		/* Open a stream for append */
#define	WT_STREAM_READ		0x02		/* Open a stream for read */
#define	WT_STREAM_WRITE		0x04		/* Open a stream for write */
	uint32_t flags;

	int (*close)(WT_SESSION_IMPL *, WT_FSTREAM *);
	int (*fstr_flush)(WT_SESSION_IMPL *, WT_FSTREAM *);
	int (*fstr_getline)(WT_SESSION_IMPL *, WT_FSTREAM *, WT_ITEM *);
	int (*fstr_printf)(
	    WT_SESSION_IMPL *, WT_FSTREAM *, const char *, va_list);
};
