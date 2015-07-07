#ifndef DRM_TEGRA_DP_H
#define DRM_TEGRA_DP_H

#include <drm/drm_dp_helper.h>

#define DP_TRAIN_VOLTAGE_SWING_LEVEL(x) ((x) << 0)
#define DP_TRAIN_PRE_EMPHASIS_LEVEL(x) ((x) << 3)
#define DP_LANE_POST_CURSOR(i, x) (((x) & 0x3) << (((i) & 1) << 2))

/**
 * struct drm_dp_link_train_set - link training settings
 * @voltage_swing: per-lane voltage swing
 * @pre_emphasis: per-lane pre-emphasis
 * @post_cursor: per-lane post-cursor
 */
struct drm_dp_link_train_set {
	unsigned int voltage_swing[4];
	unsigned int pre_emphasis[4];
	unsigned int post_cursor[4];
};

/**
 * struct drm_dp_link_train - link training state information
 * @request: currently requested settings
 * @adjust: adjustments requested by sink
 * @pattern: currently requested training pattern
 * @clock_recovered: flag to track if clock recovery has completed
 * @channel_equalized: flag to track if channel equalization has completed
 */
struct drm_dp_link_train {
	struct drm_dp_link_train_set request;
	struct drm_dp_link_train_set adjust;

	unsigned int pattern;

	bool clock_recovered;
	bool channel_equalized;
};

void drm_dp_link_train_init(struct drm_dp_link_train *train);

struct drm_dp_link_ops {
	int (*apply_training)(struct drm_dp_link *link);
	int (*configure)(struct drm_dp_link *link);
};

int __drm_dp_link_configure(struct drm_dp_link *link);
int drm_dp_link_train(struct drm_dp_link *link);

#endif
