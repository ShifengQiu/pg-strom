/*
 * strom_types.h
 *
 * Type definitions of PG-Strom
 *
 * --
 * Copyright 2011-2014 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>
 * Copyright 2014 (C) The PG-Strom Development Team
 *
 * This software is an extension of PostgreSQL; You can use, copy,
 * modify or distribute it under the terms of 'LICENSE' included
 * within this package.
 */
#ifndef STROM_TYPES_H
#define STROM_TYPES_H
#include "storage/spin.h"

/*
 * pgstrom_platform_info
 *
 * Properties of OpenCL platform being choosen. Usually, a particular
 * platform shall be choosen on starting up time according to the GUC
 * configuration (including automatic policy).
 * Note that the properties below are supported on the OpenCL 1.1 only,
 * because older drivers cannot understand newer parameter names appeared
 * in v1.2.
 */
typedef struct {
	cl_uint		pl_index;
	char	   *pl_profile;
	char	   *pl_version;
	char	   *pl_name;
	char	   *pl_vendor;
	char	   *pl_extensions;
	Size		buflen;
	char		buffer[FLEXIBLE_ARRAY_MEMBER];
} pgstrom_platform_info;

/*
 * pgstrom_device_info
 *
 * A set of OpenCL properties of a particular device. See above comments.
 */
typedef struct {
	pgstrom_platform_info *pl_info;
	cl_uint		dev_index;
	cl_uint		dev_address_bits;
	cl_bool		dev_available;
	cl_bool		dev_compiler_available;
	cl_device_fp_config	dev_double_fp_config;
	cl_bool		dev_endian_little;
	cl_bool		dev_error_correction_support;
	cl_device_exec_capabilities dev_execution_capabilities;
	char	   *dev_device_extensions;
	cl_ulong	dev_global_mem_cache_size;
	cl_device_mem_cache_type	dev_global_mem_cache_type;
	cl_uint		dev_global_mem_cacheline_size;
	cl_ulong	dev_global_mem_size;
	cl_bool		dev_host_unified_memory;
	cl_ulong	dev_local_mem_size;
	cl_device_local_mem_type	dev_local_mem_type;
	cl_uint		dev_max_clock_frequency;
	cl_uint		dev_max_compute_units;
	cl_uint		dev_max_constant_args;
	cl_ulong	dev_max_constant_buffer_size;
	cl_ulong	dev_max_mem_alloc_size;
	size_t		dev_max_parameter_size;
	cl_uint		dev_max_samplers;
	size_t		dev_max_work_group_size;
	cl_uint		dev_max_work_item_dimensions;
	size_t		dev_max_work_item_sizes[3];
	cl_uint		dev_mem_base_addr_align;
	char	   *dev_name;
	cl_uint		dev_native_vector_width_char;
	cl_uint		dev_native_vector_width_short;
	cl_uint		dev_native_vector_width_int;
	cl_uint		dev_native_vector_width_long;
	cl_uint		dev_native_vector_width_float;
	cl_uint		dev_native_vector_width_double;
	char	   *dev_opencl_c_version;
	cl_uint		dev_preferred_vector_width_char;
	cl_uint		dev_preferred_vector_width_short;
	cl_uint		dev_preferred_vector_width_int;
	cl_uint		dev_preferred_vector_width_long;
	cl_uint		dev_preferred_vector_width_float;
	cl_uint		dev_preferred_vector_width_double;
	char	   *dev_profile;
	size_t		dev_profiling_timer_resolution;
	cl_command_queue_properties	dev_queue_properties;
	cl_device_fp_config	dev_single_fp_config;
	cl_device_type	dev_type;
	char	   *dev_vendor;
	cl_uint		dev_vendor_id;
	char	   *dev_version;
	char	   *driver_version;
	Size		buflen;
	char		buffer[FLEXIBLE_ARRAY_MEMBER];
} pgstrom_device_info;

/*
 * Tag of shared memory object classes
 */
typedef enum {
	StromTag_DevProgram = 1001,
	StromTag_MsgQueue,
	StromTag_ParamBuf,
	StromTag_RowStore,
	StromTag_ColumnStore,
	StromTag_ToastBuf,
	StromTag_GpuScan,
	StromTag_GpuSort,
	StromTag_HashJoin,
	StromTag_TestMessage,
} StromTag;

/*
 * Performance monitor structure
 */
typedef struct {
	cl_bool		enabled;
	cl_ulong	time_to_load;	/* time to load data from heap/cache/subplan */
	cl_ulong	time_in_sendq;	/* waiting time in the server mqueue */
	cl_ulong	time_kern_build;/* time to build opencl kernel */
	cl_ulong	time_dma_send;	/* time to send host=>device data */
	cl_ulong	time_kern_exec;	/* time to execute kernel */
	cl_ulong	time_dma_recv;	/* time to receive device=>host data */
	cl_ulong	time_in_recvq;	/* waiting time in the response mqueue */
	struct timeval	tv;	/* result of gettimeofday(2) when enqueued */
} pgstrom_perfmon;

#define timeval_diff(tv1,tv2)						\
	(((tv2)->tv_sec - (tv1)->tv_sec) * 1000000L +	\
	 ((tv2)->tv_usec - (tv1)->tv_usec))

/*
 * pgstrom_queue
 *
 * A message queue allocated on shared memory, to send messages to/from
 * OpenCL background server. A message queue is constructed with refcnt=1,
 * then its reference counter shall be incremented for each message enqueue
 * to be returned
 */
typedef struct {
	StromTag		stag;
	dlist_node		chain;	/* link to free queues list in mqueue.c */
	pid_t			owner;
	int				refcnt;
	pthread_mutex_t	lock;
	pthread_cond_t	cond;
	dlist_head		qhead;
	bool			closed;
} pgstrom_queue;

typedef struct pgstrom_message {
	StromTag		stag;
	slock_t			lock;	/* protection for reference counter */
	cl_int			refcnt;
	cl_int			errcode;
	dlist_node		chain;
	pgstrom_queue  *respq;	/* mqueue for response message */
	void	(*cb_process)(struct pgstrom_message *message);
	void	(*cb_release)(struct pgstrom_message *message);
	pgstrom_perfmon	pfm;
} pgstrom_message;

/*
 * Kernel Param/Const buffer
 */
typedef struct {
	StromTag		stag;
	slock_t			lock;
	int				refcnt;
	kern_parambuf	kern;
} pgstrom_parambuf;

/*
 * Row-format data store
 */
typedef struct {
	StromTag		stag;
	dlist_node		chain;
	kern_column_store *kcs_head;	/* template of in-kernel column store */
	kern_row_store	kern;
} pgstrom_row_store;

/*
 * NOTE: shmem.c put a magic number to detect shared memory usage overrun.
 * So, we have a little adjustment for this padding.
 */
#define ROWSTORE_DEFAULT_SIZE	(8 * 1024 * 1024 - sizeof(cl_uint))

/*
 * Column-format data store
 */
typedef struct {
	StromTag		stag;
	dlist_node		chain;
	dlist_head		toast;	/* list of toast buffers */
	kern_column_store kern;
} pgstrom_column_store;

typedef struct {
	StromTag		stag;
	dlist_node		chain;
	cl_uint			usage;
	kern_toastbuf	kern;
} pgstrom_toastbuf;

/*
 * Type declarations for code generator
 */
#define DEVINFO_IS_NEGATIVE			0x0001
#define DEVTYPE_IS_VARLENA			0x0002
#define DEVTYPE_IS_BUILTIN			0x0004
#define DEVFUNC_NEEDS_TIMELIB		0x0008
#define DEVFUNC_NEEDS_TEXTLIB		0x0010
#define DEVFUNC_NEEDS_NUMERICLIB	0x0020
#define DEVFUNC_INCL_FLAGS			0x0038
#define DEVKERNEL_NEEDS_DEBUG		0x0100
#define DEVKERNEL_NEEDS_GPUSCAN		0x0200
#define DEVKERNEL_NEEDS_GPUSORT		0x0400
#define DEVKERNEL_NEEDS_HASHJOIN	0x0800

struct devtype_info;
struct devfunc_info;

typedef struct devtype_info {
	Oid			type_oid;
	uint32		type_flags;
	char	   *type_name;	/* name of device type; same of SQL's type */
	char	   *type_base;	/* base name of this type (like varlena) */
	char	   *type_decl;
	struct devfunc_info *type_is_null_fn;
	struct devfunc_info	*type_is_not_null_fn;
} devtype_info;

typedef struct devfunc_info {
	int32		func_flags;
	Oid			func_namespace;
	Oid		   *func_argtypes;
	const char *func_name;	/* name of device function; same of SQL's func */
	List	   *func_args;	/* list of devtype_info */
	devtype_info *func_rettype;
	const char *func_decl;	/* declaration of function */
} devfunc_info;

#endif	/* STROM_TYPES_H */
