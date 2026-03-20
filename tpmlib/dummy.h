/*
 * Copyright (c) 2019 Apertus Solutions, LLC
 *
 * Author(s):
 *      Daniel P. Smith <dpsmith@apertussolutions.com>
 *
 * The definitions in this header are extracted from the Trusted Computing
 * Group's "TPM Main Specification", Parts 1-3.
 *
 */

#ifndef _DUMMY_H
#define _DUMMY_H

#ifdef LINUX_USERSPACE

#include <stdint.h>
#include <sys/types.h>

#define u8 uint8_t

#endif

#include "tpm.h"

u8 dummy_tpm_init(struct tpm *t);

#endif
