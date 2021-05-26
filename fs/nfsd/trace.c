// SPDX-License-Identifier: GPL-2.0

#define CREATE_TRACE_POINTS
#include "trace.h"

const char *cb_state2str(const int state)
{
	switch (state) {
	case NFSD4_CB_UP:
		return "UP";
	case NFSD4_CB_UNKNOWN:
		return "UNKNOWN";
	case NFSD4_CB_DOWN:
		return "DOWN";
	case NFSD4_CB_FAULT:
		return "FAULT";
	}
	return "UNDEFINED";
}
