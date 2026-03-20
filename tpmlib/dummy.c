/*
 * Copyright (c) 2019 Apertus Solutions, LLC
 *
 * Author(s):
 *      Daniel P. Smith <dpsmith@apertussolutions.com>
 */

#ifdef LINUX_KERNEL

#include <linux/types.h>

#endif

#include "tpm.h"
#include "tpmbuff.h"
#include "crb.h"
#include "tpm_common.h"

static u8 dummy_request_locality(u8 l)
{
	return TPM_NO_LOCALITY;
}

static void dummy_relinquish_locality(void)
{
}

static size_t dummy_send(__attribute__((unused)) struct tpmbuff *buf)
{
	return 0;
}

static size_t dummy_recv(__attribute__((unused)) enum tpm_family family,
		         __attribute__((unused)) struct tpmbuff *buf)
{
	return 0;
}

u8 dummy_tpm_init(struct tpm *t)
{
	t->ops.request_locality = dummy_request_locality;
	t->ops.relinquish_locality = dummy_relinquish_locality;
	t->ops.send = dummy_send;
	t->ops.recv = dummy_recv;

	return 1;
}
