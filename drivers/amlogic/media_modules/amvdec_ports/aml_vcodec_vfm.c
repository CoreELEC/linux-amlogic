#include "aml_vcodec_vfm.h"
#include "aml_vcodec_vfq.h"
#include "aml_vcodec_util.h"
#include <media/v4l2-mem2mem.h>

#define RECEIVER_NAME	"v4l2-video"
#define PROVIDER_NAME	"v4l2-video"

static struct vframe_s *vdec_vf_peek(void *op_arg)
{
	struct vcodec_vfm_s *vfm = (struct vcodec_vfm_s *)op_arg;

	return vfq_peek(&vfm->vf_que);
}

static struct vframe_s *vdec_vf_get(void *op_arg)
{
	struct vcodec_vfm_s *vfm = (struct vcodec_vfm_s *)op_arg;

	return vfq_pop(&vfm->vf_que);
}

static void vdec_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vcodec_vfm_s *vfm = (struct vcodec_vfm_s *)op_arg;

	vf_put(vf, vfm->recv_name);
	vf_notify_provider(vfm->recv_name, VFRAME_EVENT_RECEIVER_PUT, NULL);
}

static int vdec_event_cb(int type, void *data, void *private_data)
{

	if (type & VFRAME_EVENT_RECEIVER_PUT) {
	} else if (type & VFRAME_EVENT_RECEIVER_GET) {
	} else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT) {
	}
	return 0;
}

static int vdec_vf_states(struct vframe_states *states, void *op_arg)
{
	struct vcodec_vfm_s *vfm = (struct vcodec_vfm_s *)op_arg;

	states->vf_pool_size	= POOL_SIZE;
	states->buf_recycle_num	= 0;
	states->buf_free_num	= POOL_SIZE - vfq_level(&vfm->vf_que);
	states->buf_avail_num	= vfq_level(&vfm->vf_que);

	return 0;
}

void video_vf_put(struct vdec_fb *fb)
{
	struct vframe_provider_s *vfp = vf_get_provider(RECEIVER_NAME);
	struct vframe_s *vf = (struct vframe_s *)fb->vf_handle;

	aml_v4l2_debug(4, "%s() [%d], vfp: %p, vf: %p, cnt: %d\n",
		__FUNCTION__, __LINE__, vfp, vf, atomic_read(&vf->use_cnt));

	if (vfp && vf && atomic_dec_and_test(&vf->use_cnt))
		vf_put(vf, RECEIVER_NAME);
}

static const struct vframe_operations_s vf_provider = {
	.peek		= vdec_vf_peek,
	.get		= vdec_vf_get,
	.put		= vdec_vf_put,
	.event_cb	= vdec_event_cb,
	.vf_states	= vdec_vf_states,
};

static int video_receiver_event_fun(int type, void *data, void *private_data)
{
	int ret = 0;
	struct vframe_states states;
	struct vcodec_vfm_s *vfm = (struct vcodec_vfm_s *)private_data;

	aml_v4l2_debug(4, "%s() [%d], type: %d, vfm: %p\n",
		__FUNCTION__, __LINE__, type, vfm);

	switch (type) {
	case VFRAME_EVENT_PROVIDER_UNREG: {
		if (vf_get_receiver(vfm->prov_name)) {
			aml_v4l2_debug(4, "%s() [%d] unreg %s provider.\n",
				__FUNCTION__, __LINE__, vfm->prov_name);
			vf_unreg_provider(&vfm->vf_prov);
		}

		vfq_init(&vfm->vf_que, POOL_SIZE + 1, &vfm->pool[0]);

		break;
	}

	case VFRAME_EVENT_PROVIDER_START: {
		if (vf_get_receiver(vfm->prov_name)) {
			aml_v4l2_debug(4, "%s() [%d] reg %s provider.\n",
				__FUNCTION__, __LINE__, vfm->prov_name);
			vf_provider_init(&vfm->vf_prov, vfm->prov_name,
				&vf_provider, vfm);
			vf_reg_provider(&vfm->vf_prov);
			vf_notify_receiver(vfm->prov_name,
				VFRAME_EVENT_PROVIDER_START, NULL);
		}

		vfq_init(&vfm->vf_que, POOL_SIZE + 1, &vfm->pool[0]);

		break;
	}

	case VFRAME_EVENT_PROVIDER_QUREY_STATE: {
		vdec_vf_states(&states, vfm);
		if (states.buf_avail_num > 0)
			ret = RECEIVER_ACTIVE;
		break;
	}

	case VFRAME_EVENT_PROVIDER_VFRAME_READY: {
		if (vfq_level(&vfm->vf_que) > POOL_SIZE - 1)
			ret = -1;

		if (!vf_peek(vfm->recv_name))
			ret = -1;

		vfm->vf = vf_get(vfm->recv_name);
		if (!vfm->vf)
			ret = -1;

		vfq_push(&vfm->vf_que, vfm->vf);

		/*vf_notify_receiver(vfm->prov_name,
			VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);*/

		/* schedule capture work. */
		vdec_device_vf_run(vfm->ctx);
		aml_v4l2_debug(1, "%s() [%d] VFRAME_EVENT_PROVIDER_VFRAME_READY.\n",
			__FUNCTION__, __LINE__);

		break;
	}

	default:
		aml_v4l2_debug(4, "the vf event is %d .", type);
	}

	return ret;
}

static const struct vframe_receiver_op_s vf_receiver = {
	.event_cb	= video_receiver_event_fun
};

struct vframe_s *peek_video_frame(struct vcodec_vfm_s *vfm)
{
	return vfq_peek(&vfm->vf_que);
}

struct vframe_s *get_video_frame(struct vcodec_vfm_s *vfm)
{
	return vfq_pop(&vfm->vf_que);
}

int vcodec_vfm_init(struct vcodec_vfm_s *vfm)
{
	memcpy(vfm->recv_name, RECEIVER_NAME, sizeof(RECEIVER_NAME));
	memcpy(vfm->prov_name, PROVIDER_NAME, sizeof(PROVIDER_NAME));

	vf_receiver_init(&vfm->vf_recv, vfm->recv_name, &vf_receiver, vfm);
	vf_reg_receiver(&vfm->vf_recv);

	return 0;
}

void vcodec_vfm_release(struct vcodec_vfm_s *vfm)
{
	vf_unreg_receiver(&vfm->vf_recv);
	vf_unreg_provider(&vfm->vf_prov);
}

