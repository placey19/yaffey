/*
 * YAFFS: Yet another Flash File System . A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2011 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 * Note: Only YAFFS headers are LGPL, YAFFS C code is covered by GPL.
 */

#ifndef __YAFFS_GUTS_H__
#define __YAFFS_GUTS_H__

//common
#define YCHAR char
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned u32;
#ifdef WIN32
    typedef unsigned loff_t;
#elif __APPLE__
    typedef unsigned loff_t;
#else
    #include <sys/types.h>
#endif  //_WIN32

#define YAFFS_MAX_NAME_LENGTH           255
#define YAFFS_MAX_ALIAS_LENGTH          159

#define YAFFS_NOBJECT_BUCKETS           256
#define YAFFS_LOWEST_SEQUENCE_NUMBER    0x00001000

#define YAFFS_OBJECTID_ROOT             1
#define YAFFS_OBJECTID_LOSTNFOUND       2
#define YAFFS_OBJECTID_UNLINKED         3
#define YAFFS_OBJECTID_DELETED          4

enum yaffs_ecc_result {
    YAFFS_ECC_RESULT_UNKNOWN,
    YAFFS_ECC_RESULT_NO_ERROR,
    YAFFS_ECC_RESULT_FIXED,
    YAFFS_ECC_RESULT_UNFIXED
};

enum yaffs_obj_type {
    YAFFS_OBJECT_TYPE_UNKNOWN,
    YAFFS_OBJECT_TYPE_FILE,
    YAFFS_OBJECT_TYPE_SYMLINK,
    YAFFS_OBJECT_TYPE_DIRECTORY,
    YAFFS_OBJECT_TYPE_HARDLINK,
    YAFFS_OBJECT_TYPE_SPECIAL
};

#define YAFFS_OBJECT_TYPE_MAX           YAFFS_OBJECT_TYPE_SPECIAL

struct yaffs_ext_tags {
    unsigned chunk_used;	/*  Status of the chunk: used or unused */
    unsigned obj_id;	/* If 0 this is not used */
    unsigned chunk_id;	/* If 0 this is a header, else a data chunk */
    unsigned n_bytes;	/* Only valid for data chunks */

    /* The following stuff only has meaning when we read */
    enum yaffs_ecc_result ecc_result;
    unsigned block_bad;

    /* YAFFS 1 stuff */
    unsigned is_deleted;	/* The chunk is marked deleted */
    unsigned serial_number;	/* Yaffs1 2-bit serial number */

    /* YAFFS2 stuff */
    unsigned seq_number;	/* The sequence number of this block */

    /* Extra info if this is an object header (YAFFS2 only) */

    unsigned extra_available;	/* Extra info available if not zero */
    unsigned extra_parent_id;	/* The parent object */
    unsigned extra_is_shrink;	/* Is it a shrink header? */
    unsigned extra_shadows;	/* Does this shadow another object? */

    enum yaffs_obj_type extra_obj_type;	/* What object type? */

    loff_t extra_file_size;		/* Length if it is a file */
    unsigned extra_equiv_id;	/* Equivalent object for a hard link */
};

struct yaffs_obj_hdr {
    enum yaffs_obj_type type;

    /* Apply to everything  */
    int parent_obj_id;
    u16 sum_no_longer_used;	/* checksum of name. No longer used */
    char name[YAFFS_MAX_NAME_LENGTH + 1];

    /* The following apply to all object types except for hard links */
    u32 yst_mode;		/* protection */

    u32 yst_uid;
    u32 yst_gid;
    u32 yst_atime;
    u32 yst_mtime;
    u32 yst_ctime;

    /* File size applies to files only */
    u32 file_size_low;

    /* Equivalent object id applies to hard links only. */
    int equiv_id;

    /* Alias is for symlinks only. */
    YCHAR alias[YAFFS_MAX_ALIAS_LENGTH + 1];

    u32 yst_rdev;	/* stuff for block and char devices (major/min) */

    u32 win_ctime[2];
    u32 win_atime[2];
    u32 win_mtime[2];

    u32 inband_shadowed_obj_id;
    u32 inband_is_shrink;

    u32 file_size_high;
    u32 reserved[1];
    int shadows_obj;	/* This object header shadows the specified object if > 0 */

    /* is_shrink applies to object headers written when we make a hole. */
    u32 is_shrink;
};

#endif  //__YAFFS_GUTS_H__
