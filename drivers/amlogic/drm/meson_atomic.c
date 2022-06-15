// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/component.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_flip_work.h>
#include <drm/drm_self_refresh_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_rect.h>
#include <drm/drm_fb_helper.h>

#include "meson_drv.h"
#include "meson_crtc.h"
#include "meson_plane.h"
#include "meson_writeback.h"

struct meson_commit_work_item {
	struct kthread_work kthread_work;
	struct work_struct *work;
};

static void commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;
	const struct drm_mode_config_helper_funcs *funcs;
	struct drm_crtc_state *new_crtc_state;
	struct drm_crtc *crtc;
	ktime_t start;
	s64 commit_time_ms;
	unsigned int i, new_self_refresh_mask = 0;

	funcs = dev->mode_config.helper_private;

	/*
	 * We're measuring the _entire_ commit, so the time will vary depending
	 * on how many fences and objects are involved. For the purposes of self
	 * refresh, this is desirable since it'll give us an idea of how
	 * congested things are. This will inform our decision on how often we
	 * should enter self refresh after idle.
	 *
	 * These times will be averaged out in the self refresh helpers to avoid
	 * overreacting over one outlier frame
	 */
	start = ktime_get();

	drm_atomic_helper_wait_for_fences(dev, old_state, false);

	drm_atomic_helper_wait_for_dependencies(old_state);

	/*
	 * We cannot safely access new_crtc_state after
	 * drm_atomic_helper_commit_hw_done() so figure out which crtc's have
	 * self-refresh active beforehand:
	 */
	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i)
		if (new_crtc_state->self_refresh_active)
			new_self_refresh_mask |= BIT(i);

	if (funcs && funcs->atomic_commit_tail)
		funcs->atomic_commit_tail(old_state);
	else
		drm_atomic_helper_commit_tail(old_state);

	commit_time_ms = ktime_ms_delta(ktime_get(), start);
	if (commit_time_ms > 0)
		drm_self_refresh_helper_update_avg_times(old_state,
						 (unsigned long)commit_time_ms,
						 new_self_refresh_mask);

	drm_atomic_helper_commit_cleanup_done(old_state);

	drm_atomic_state_put(old_state);
}

static void meson_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;
	const struct drm_mode_config_helper_funcs *funcs;
	struct drm_crtc_state *new_crtc_state;
	struct drm_crtc *crtc;
	ktime_t start;
	s64 commit_time_ms;
	unsigned int i, new_self_refresh_mask = 0;

	funcs = dev->mode_config.helper_private;

	/*
	 * We're measuring the _entire_ commit, so the time will vary depending
	 * on how many fences and objects are involved. For the purposes of self
	 * refresh, this is desirable since it'll give us an idea of how
	 * congested things are. This will inform our decision on how often we
	 * should enter self refresh after idle.
	 *
	 * These times will be averaged out in the self refresh helpers to avoid
	 * overreacting over one outlier frame
	 */
	start = ktime_get();

	drm_atomic_helper_wait_for_fences(dev, old_state, false);

	drm_atomic_helper_wait_for_dependencies(old_state);

	/*
	 * We cannot safely access new_crtc_state after
	 * drm_atomic_helper_commit_hw_done() so figure out which crtc's have
	 * self-refresh active beforehand:
	 */
	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i)
		if (new_crtc_state->self_refresh_active)
			new_self_refresh_mask |= BIT(i);

	if (funcs && funcs->atomic_commit_tail)
		funcs->atomic_commit_tail(old_state);
	else
		drm_atomic_helper_commit_tail(old_state);

	commit_time_ms = ktime_ms_delta(ktime_get(), start);
	if (commit_time_ms > 0)
		drm_self_refresh_helper_update_avg_times(old_state,
						 (unsigned long)commit_time_ms,
						 new_self_refresh_mask);

	drm_atomic_helper_commit_cleanup_done(old_state);

	drm_atomic_state_put(old_state);
}

static void meson_commit_work(struct kthread_work *work)
{
	struct meson_commit_work_item *work_item = container_of(work,
						struct meson_commit_work_item,
						kthread_work);
	struct drm_atomic_state *state = container_of(work_item->work,
						      struct drm_atomic_state,
						      commit_work);
	kfree(work_item);
	meson_commit_tail(state);
}

static int stall_checks(struct drm_crtc *crtc, bool nonblock)
{
	struct drm_crtc_commit *commit, *stall_commit = NULL;
	bool completed = true;
	int i;
	long ret = 0;

	spin_lock(&crtc->commit_lock);
	i = 0;
	list_for_each_entry(commit, &crtc->commit_list, commit_entry) {
		if (i == 0) {
			completed = try_wait_for_completion(&commit->flip_done);
			/* Userspace is not allowed to get ahead of the previous
			 * commit with nonblocking ones.
			 */
			if (!completed && nonblock) {
				spin_unlock(&crtc->commit_lock);
				return -EBUSY;
			}
		} else if (i == 1) {
			stall_commit = drm_crtc_commit_get(commit);
			break;
		}

		i++;
	}
	spin_unlock(&crtc->commit_lock);

	if (!stall_commit)
		return 0;

	/* We don't want to let commits get ahead of cleanup work too much,
	 * stalling on 2nd previous commit means triple-buffer won't ever stall.
	 */
	ret = wait_for_completion_interruptible_timeout(&stall_commit->cleanup_done,
							10 * HZ);
	if (ret == 0)
		DRM_ERROR("[CRTC:%d:%s] cleanup_done timed out\n",
			  crtc->base.id, crtc->name);

	drm_crtc_commit_put(stall_commit);

	return ret < 0 ? ret : 0;
}

static void release_crtc_commit(struct completion *completion)
{
	struct drm_crtc_commit *commit = container_of(completion,
						      typeof(*commit),
						      flip_done);

	drm_crtc_commit_put(commit);
}

static void init_commit(struct drm_crtc_commit *commit, struct drm_crtc *crtc)
{
	init_completion(&commit->flip_done);
	init_completion(&commit->hw_done);
	init_completion(&commit->cleanup_done);
	INIT_LIST_HEAD(&commit->commit_entry);
	kref_init(&commit->ref);
	commit->crtc = crtc;
}

static struct drm_crtc_commit *
crtc_or_fake_commit(struct drm_atomic_state *state, struct drm_crtc *crtc)
{
	if (crtc) {
		struct drm_crtc_state *new_crtc_state;

		new_crtc_state = drm_atomic_get_new_crtc_state(state, crtc);

		return new_crtc_state->commit;
	}

	if (!state->fake_commit) {
		state->fake_commit = kzalloc(sizeof(*state->fake_commit), GFP_KERNEL);
		if (!state->fake_commit)
			return NULL;

		init_commit(state->fake_commit, NULL);
	}

	return state->fake_commit;
}

static int meson_drm_atomic_helper_setup_commit(struct drm_atomic_state *state,
				   bool nonblock)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct drm_connector *conn;
	struct drm_connector_state *old_conn_state, *new_conn_state;
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state, *new_plane_state;
	struct drm_crtc_commit *commit;
	int i, ret;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		commit = kzalloc(sizeof(*commit), GFP_KERNEL);
		if (!commit)
			return -ENOMEM;

		init_commit(commit, crtc);

		new_crtc_state->commit = commit;

		ret = stall_checks(crtc, nonblock);
		if (ret)
			DRM_DEBUG_ATOMIC("meson stall checks %d\n", ret);

		/* Drivers only send out events when at least either current or
		 * new CRTC state is active. Complete right away if everything
		 * stays off.
		 */
		if (!old_crtc_state->active && !new_crtc_state->active) {
			complete_all(&commit->flip_done);
			continue;
		}

		/* Legacy cursor updates are fully unsynced. */
		if (state->legacy_cursor_update) {
			complete_all(&commit->flip_done);
			continue;
		}

		if (!new_crtc_state->event) {
			commit->event = kzalloc(sizeof(*commit->event),
						GFP_KERNEL);
			if (!commit->event)
				return -ENOMEM;

			new_crtc_state->event = commit->event;
		}

		new_crtc_state->event->base.completion = &commit->flip_done;
		new_crtc_state->event->base.completion_release = release_crtc_commit;
		drm_crtc_commit_get(commit);

		commit->abort_completion = true;

		state->crtcs[i].commit = commit;
		drm_crtc_commit_get(commit);
	}

	for_each_oldnew_connector_in_state(state, conn, old_conn_state, new_conn_state, i) {
		/* Userspace is not allowed to get ahead of the previous
		 * commit with nonblocking ones.
		 */
		if (nonblock && old_conn_state->commit &&
		    !try_wait_for_completion(&old_conn_state->commit->flip_done))
			DRM_DEBUG_ATOMIC("connetor commit not done\n");

		/* Always track connectors explicitly for e.g. link retraining. */
		commit = crtc_or_fake_commit(state, new_conn_state->crtc ?: old_conn_state->crtc);
		if (!commit)
			return -ENOMEM;

		new_conn_state->commit = drm_crtc_commit_get(commit);
	}

	for_each_oldnew_plane_in_state(state, plane, old_plane_state, new_plane_state, i) {
		/* Userspace is not allowed to get ahead of the previous
		 * commit with nonblocking ones.
		 */
		if (nonblock && old_plane_state->commit &&
		    !try_wait_for_completion(&old_plane_state->commit->flip_done))
			DRM_DEBUG_ATOMIC("plane commit not done\n");

		/* Always track planes explicitly for async pageflip support. */
		commit = crtc_or_fake_commit(state, new_plane_state->crtc ?: old_plane_state->crtc);
		if (!commit)
			return -ENOMEM;

		new_plane_state->commit = drm_crtc_commit_get(commit);
	}

	return 0;
}

static bool check_parallel_commit(struct drm_atomic_state *state,
		struct drm_crtc **dest_crtc)
{
	struct drm_connector_state *connector_state;
	struct drm_connector *connector;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;
	int i, num_crtcs = 0;

	/* any connector change, return false */
	for_each_new_connector_in_state(state, connector, connector_state, i)
		return false;

	for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
		if (drm_atomic_crtc_needs_modeset(crtc_state))
			return false;
		if (++num_crtcs > 1)
			return false;
		*dest_crtc = crtc;
	}

	return true;
}

int meson_atomic_commit(struct drm_device *dev,
			     struct drm_atomic_state *state,
			     bool nonblock)
{
	int ret, crtc_index;
	struct meson_commit_work_item *work_item;
	struct kthread_worker *worker;
	struct drm_crtc *dest_crtc = NULL;
	struct meson_drm *priv = dev->dev_private;
	bool is_parallel = check_parallel_commit(state, &dest_crtc);

	if (state->async_update) {
		ret = drm_atomic_helper_prepare_planes(dev, state);
		if (ret)
			return ret;

		ret = drm_atomic_helper_swap_state(state, true);
		drm_atomic_helper_async_commit(dev, state);
		meson_osd_plane_async_flush(state);
		drm_atomic_helper_cleanup_planes(dev, state);

		return 0;
	}

	ret = meson_drm_atomic_helper_setup_commit(state, nonblock);
	if (ret)
		return ret;

	ret = drm_atomic_helper_prepare_planes(dev, state);
	if (ret)
		return ret;

	if (!nonblock) {
		ret = drm_atomic_helper_wait_for_fences(dev, state, true);
		if (ret)
			goto err;
	}

	/*
	 * This is the point of no return - everything below never fails except
	 * when the hw goes bonghits. Which means we can commit the new state on
	 * the software side now.
	 */
	if (nonblock) {
		work_item = kzalloc(sizeof(*work_item), GFP_KERNEL);
		if (!work_item)
			goto err;
	}

	ret = drm_atomic_helper_swap_state(state, true);
	if (ret)
		goto err;

	/*
	 * Everything below can be run asynchronously without the need to grab
	 * any modeset locks at all under one condition: It must be guaranteed
	 * that the asynchronous work has either been cancelled (if the driver
	 * supports it, which at least requires that the framebuffers get
	 * cleaned up with drm_atomic_helper_cleanup_planes()) or completed
	 * before the new state gets committed on the software side with
	 * drm_atomic_helper_swap_state().
	 *
	 * This scheme allows new atomic state updates to be prepared and
	 * checked in parallel to the asynchronous completion of the previous
	 * update. Which is important since compositors need to figure out the
	 * composition of the next frame right after having submitted the
	 * current layout.
	 *
	 * NOTE: Commit work has multiple phases, first hardware commit, then
	 * cleanup. We want them to overlap, hence need system_unbound_wq to
	 * make sure work items don't artificially stall on each another.
	 */

	drm_atomic_state_get(state);
	if (nonblock) {
		crtc_index = 0;
		work_item->work = &state->commit_work;
		kthread_init_work(&work_item->kthread_work, meson_commit_work);
		if (is_parallel && dest_crtc)
			crtc_index = dest_crtc->index;

		worker = &priv->commit_thread[crtc_index].worker;
		kthread_queue_work(worker, &work_item->kthread_work);
	} else {
		commit_tail(state);
	}

	return 0;

err:
	drm_atomic_helper_cleanup_planes(dev, state);
	return ret;
}

void meson_atomic_helper_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_connector *conn;
	struct meson_connector *meson_conn;
	struct am_drm_writeback *drm_writeback;
	struct drm_connector_state *new_conn_state, *old_conn_state;
	int i;

	/*do  update which dont need       pipe change.*/
	for_each_oldnew_connector_in_state(old_state, conn, old_conn_state, new_conn_state, i) {
		if (conn->connector_type == DRM_MODE_CONNECTOR_WRITEBACK) {
			drm_writeback = connector_to_am_writeback(conn);
			meson_conn = &drm_writeback->base;
		} else {
			meson_conn = connector_to_meson_connector(conn);
		}

		if (meson_conn->update)
			meson_conn->update(new_conn_state, old_conn_state);
	}

	/*use */
	drm_atomic_helper_commit_tail_rpm(old_state);
}

