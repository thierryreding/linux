/* SPDX-License-Identifier: (GPL-2.0 OR MIT) */

#ifndef DT_BINDINGS_MEMORY_TEGRA234_MC_H
#define DT_BINDINGS_MEMORY_TEGRA234_MC_H

/* special clients */
#define TEGRA234_SID_INVALID		0x00
#define TEGRA234_SID_PASSTHROUGH	0x7f

/* NISO0 stream IDs */
#define TEGRA234_SID_MGBE	0x06
#define TEGRA234_SID_MGBE_VF1	0x49
#define TEGRA234_SID_MGBE_VF2	0x4a
#define TEGRA234_SID_MGBE_VF3	0x4b

/* NISO1 stream IDs */
#define TEGRA234_SID_SDMMC4	0x02
#define TEGRA234_SID_EQOS	0x03
#define TEGRA234_SID_BPMP	0x10

/*
 * memory client IDs
 */

/* MGBE0 read client */
#define TEGRA234_MEMORY_CLIENT_MGBEARD 0x58
/* MGBEB read client */
#define TEGRA234_MEMORY_CLIENT_MGBEBRD 0x59
/* MGBEC read client */
#define TEGRA234_MEMORY_CLIENT_MGBECRD 0x5a
/* MGBED read client */
#define TEGRA234_MEMORY_CLIENT_MGBEDRD 0x5b
/* MGBE0 write client */
#define TEGRA234_MEMORY_CLIENT_MGBEAWR 0x5c
/* MGBEB write client */
#define TEGRA234_MEMORY_CLIENT_MGBEBWR 0x5f
/* MGBEC write client */
#define TEGRA234_MEMORY_CLIENT_MGBECWR 0x61
/* sdmmcd memory read client */
#define TEGRA234_MEMORY_CLIENT_SDMMCRAB 0x63
/* MGBED write client */
#define TEGRA234_MEMORY_CLIENT_MGBEDWR 0x65
/* sdmmcd memory write client */
#define TEGRA234_MEMORY_CLIENT_SDMMCWAB 0x67
/* EQOS read client */
#define TEGRA234_MEMORY_CLIENT_EQOSR 0x8e
/* EQOS write client */
#define TEGRA234_MEMORY_CLIENT_EQOSW 0x8f
/* BPMP read client */
#define TEGRA234_MEMORY_CLIENT_BPMPR 0x93
/* BPMP write client */
#define TEGRA234_MEMORY_CLIENT_BPMPW 0x94
/* BPMPDMA read client */
#define TEGRA234_MEMORY_CLIENT_BPMPDMAR 0x95
/* BPMPDMA write client */
#define TEGRA234_MEMORY_CLIENT_BPMPDMAW 0x96

#endif
