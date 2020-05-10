// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2015-2020 Micron Technology, Inc.  All rights reserved.
 */

#include <linux/fs.h>
#include <linux/blk_types.h>
#include <linux/log2.h>
#include <linux/mm.h>

#include "mpcore_defs.h"

/*
 * ecio API functions
 */

/*
 * Error codes: all ecio fns can return one or more of:
 * -EINVAL = invalid fn args
 * -EIO = all other errors
 *
 * NOTE: the ecio error report carries more detailed error information
 */

/**
 * iov_len_and_alignment()
 *
 * Calculate the total data length of an iovec list, and set a flag if there
 * are any non-page-aligned pointers (or non-page-multiple iovecs)
 */
static u64 iov_len_and_alignment(struct iovec *iov, int iovcnt, int *alignment)
{
	u64    rval = 0;
	int    i = 0;
	int    align = 0;

	*alignment = 0;

	for (i = 0; i < iovcnt; i++) {
		rval += iov[i].iov_len;

		/* Make alignment and length problems distinguishable */
		if (!PAGE_ALIGNED((ulong)iov[i].iov_base))
			align |= 1;
		if (!PAGE_ALIGNED(iov[i].iov_len))
			align |= 2;
	}

	*alignment = align;

	return rval;
}

/**
 * ecio_object_valid()
 *
 * @mp
 * @layout
 *
 * Validate an mblock for read, write or erase.
 */
static merr_t
ecio_object_valid(
	struct mpool_descriptor    *mp,
	struct pmd_layout          *layout,
	enum obj_type_omf           type)
{
	merr_t err;

	if (ev(type != OMF_OBJ_MBLOCK && type != OMF_OBJ_MLOG))
		return merr(EINVAL);

	if (pmd_objid_type(layout->eld_objid) != type) {
		err = merr(EINVAL);
		mp_pr_err("mpool %s, object 0x%lx type %d is not %s",
			  err, mp->pds_name,
			  (ulong)layout->eld_objid,
			  pmd_objid_type(layout->eld_objid),
			  (type == OMF_OBJ_MBLOCK) ? "mblock" : "mlog");
		return err;
	}

	return 0;
}

u32
ecio_mblock_stripe_size(
	struct mpool_descriptor    *mp,
	struct pmd_layout          *layout)
{
	struct mpool_dev_info          *pd;

	pd = &mp->pds_pdv[layout->eld_ld.ol_pdh];

	return pd->pdi_parm.dpr_optiosz;
}

/**
 * ecio_mbrw_argcheck()
 *
 * @mp:      - Mpool descriptor
 * @layout:  - Layout of the mblock
 * @iov:     - iovec array
 * @iovcnt:  - iovec count
 * @boff:    - Byte offset into the layout.  Must be equal to layout->eld_mblen
 *             for write
 * @rw:      - MPOOL_OP_READ or MPOOL_OP_WRITE
 * @nbytes:  - Output: number of bytes in iov list
 *
 * Validate ecio_mblock_write() and ecio_mblock_read()
 *
 * Sets *nbytes to total data in iov
 *
 * Returns: 0 if successful, merr_t otherwise
 *
 * Note: be aware that there are checks in this function that prevent illegal
 * arguments in lower level functions (lower level functions should assert the
 * requirements but not otherwise check them)
 */
static merr_t
ecio_mbrw_argcheck(
	struct mpool_descriptor    *mp,
	struct pmd_layout          *layout,
	struct iovec               *iov,
	int                         iovcnt,
	loff_t                      boff,
	int                         rw,
	u64                        *nbytes)
{
	u64    data_len = 0;
	u64    stripe_bytes;
	u32    mblock_cap;
	int    alignment;
	merr_t err;

	err = ecio_object_valid(mp, layout, OMF_OBJ_MBLOCK);
	if (ev(err))
		return err;

	mblock_cap   = ecio_obj_get_cap_from_layout(mp, layout);
	stripe_bytes = ecio_mblock_stripe_size(mp, layout);

	data_len = iov_len_and_alignment(iov, iovcnt, &alignment);
	if (alignment) {
		err = merr(EINVAL);
		mp_pr_err("mpool %s, mblock %s IOV not page aligned (%d)",
			  err, mp->pds_name,
			  (rw == MPOOL_OP_READ) ? "read" : "write",
			  alignment);
		return err;
	}

	if (rw == MPOOL_OP_READ) {
		/* boff must be a multiple of the OS page size */
		if (!PAGE_ALIGNED(boff)) {
			err = merr(EINVAL);
			mp_pr_err("mpool %s, read offset 0x%lx is not multiple of OS page size",
				  err, mp->pds_name, (ulong) boff);
			return err;
		}

		/* Check that boff is not past end of mblock capacity.
		 */
		if (mblock_cap <= boff) {
			err = merr(EINVAL);
			mp_pr_err("mpool %s, read offset 0x%lx >= mblock capacity 0x%x",
				  err, mp->pds_name, (ulong)boff, mblock_cap);
			return err;
		}

		/* Check that the request does not extend past the data
		 * written.  Don't record an error if this appears to
		 * be an mcache readahead request.
		 *
		 * TODO: Use (data_len != MCACHE_RA_PAGES_MAX)
		 */
		if (ev(boff + data_len > layout->eld_mblen))
			return merr(EINVAL);
	} else {
		/* Write boff required to match eld_mblen */
		if (boff != layout->eld_mblen) {
			err = merr(EINVAL);
			mp_pr_err("mpool %s write boff (%ld) != eld_mblen (%d)",
				  err, mp->pds_name, (ulong)boff,
				  layout->eld_mblen);
			return err;
		}

		/* Writes must be stripe-aligned */
		if (boff % stripe_bytes) {
			err = merr(EINVAL);
			mp_pr_err("mpool %s, write not stripe-aligned, offset 0x%lx",
				  err, mp->pds_name, (ulong)boff);
			return err;
		}

		/* Check for write past end of allocated space (!) */
		if ((data_len + boff) > mblock_cap) {
			err = merr(EINVAL);
			mp_pr_err("(write): len %lu + boff %lu > mblock_cap %lu",
				  err, (ulong)data_len, (ulong)boff,
				  (ulong)mblock_cap);
			return err;
		}
	}

	/* Set the amount of data to read/write */
	*nbytes = data_len;

	return 0;
}

/**
 * ecio_mblock_write()
 *
 * Write a complete mblock, erasure coded per mblock layout;
 * caller MUST hold pmd_obj_*lock() on layout;
 *
 * Sets bytes written in nbytes.
 *
 * Returns: 0 if successful, merr_t otherwise
 */
merr_t
ecio_mblock_write(
	struct mpool_descriptor    *mp,
	struct pmd_layout          *layout,
	struct iovec               *iov,
	int                         iovcnt,
	u64                        *nbytes)
{
	struct mpool_dev_info  *pd;
	merr_t                  err;
	loff_t                  mboff;

	/* After this call, *nbytes is valid */
	err = ecio_mbrw_argcheck(mp, layout, iov, iovcnt, layout->eld_mblen,
				 MPOOL_OP_WRITE, nbytes);
	if (err) {
		mp_pr_debug("ecio write argcheck err", err);
		return merr(err);
	}

	if (*nbytes == 0)
		return 0;

	mboff = layout->eld_mblen;

	assert(PAGE_ALIGNED(*nbytes));
	assert(PAGE_ALIGNED(mboff));
	assert(iovcnt == (*nbytes >> PAGE_SHIFT));

	pd = &mp->pds_pdv[layout->eld_ld.ol_pdh];

	err = pd_zone_pwritev(pd, iov, iovcnt, layout->eld_ld.ol_zaddr,
			      mboff, REQ_FUA);
	if (!err)
		layout->eld_mblen += *nbytes;
	else
		mpool_pd_status_set(pd, PD_STAT_OFFLINE);

	return err;
}

/*
 * Read mblock starting at byte offset boff;
 * and transparently handles media failures if possible; boff and read length
 * must be OS page multiples;
 * caller MUST hold pmd_obj_*lock() on layout.
 *
 * Returns: 0 if successful, merr_t otherwise
 */
merr_t
ecio_mblock_read(
	struct mpool_descriptor    *mp,
	struct pmd_layout          *layout,
	struct iovec               *iov,
	int                         iovcnt,
	u64                         boff)
{
	struct mpool_dev_info  *pd;
	u64                     nbytes = 0;
	merr_t                  err;

	err = ecio_mbrw_argcheck(mp, layout, iov, iovcnt, boff,
				 MPOOL_OP_READ, &nbytes);
	if (err) {
		mp_pr_debug("ecio read argcheck", err);
		return merr(err);
	}

	if (!nbytes)
		return 0;

	assert(PAGE_ALIGNED(nbytes));
	assert(PAGE_ALIGNED(boff));
	assert(iovcnt == (nbytes >> PAGE_SHIFT));

	pd = &mp->pds_pdv[layout->eld_ld.ol_pdh];

	err = pd_zone_preadv(pd, iov, iovcnt, layout->eld_ld.ol_zaddr, boff);
	if (ev(err))
		mpool_pd_status_set(pd, PD_STAT_OFFLINE);

	return err;
}

inline u32
ecio_zonepg(
	struct mpool_descriptor    *mp,
	struct pmd_layout          *layout)
{
	return mp->pds_pdv[layout->eld_ld.ol_pdh].pdi_parm.dpr_zonepg;
}

inline u32
ecio_sectorsz(
	struct mpool_descriptor    *mp,
	struct pmd_layout          *layout)
{
	struct pd_prop *prop;

	prop = &(mp->pds_pdv[layout->eld_ld.ol_pdh].pdi_prop);

	return PD_SECTORSZ(prop);
}

u64
ecio_obj_get_cap_from_layout(
	struct mpool_descriptor    *mp,
	struct pmd_layout          *layout)
{
	enum obj_type_omf otype = pmd_objid_type(layout->eld_objid);

	switch (otype) {
	case OMF_OBJ_MBLOCK:
	case OMF_OBJ_MLOG:
		return (ecio_zonepg(mp, layout) * layout->eld_ld.ol_zcnt) <<
			PAGE_SHIFT;

	case OMF_OBJ_UNDEF:
		break;
	}

	mp_pr_warn("mpool %s objid 0x%lx, undefined object type %d",
		   mp->pds_name, (ulong)layout->eld_objid, otype);

	return 0;
}
