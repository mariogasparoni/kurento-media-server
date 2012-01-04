#include <kms-core.h>
#include <rtp/kms-rtp.h>

#define KMS_RTP_RECEIVER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), KMS_TYPE_RTP_RECEIVER, KmsRtpReceiverPriv))

#define LOCK(obj) (g_mutex_lock(KMS_RTP_RECEIVER(obj)->priv->mutex))
#define UNLOCK(obj) (g_mutex_unlock(KMS_RTP_RECEIVER(obj)->priv->mutex))

struct _KmsRtpReceiverPriv {
	GMutex *mutex;

	KmsSdpMedia *local_spec;

	GstElement *udpsrc;

	gint audio_port;
	gint video_port;
};

enum {
	PROP_0,

	PROP_LOCAL_SPEC,
};

G_DEFINE_TYPE(KmsRtpReceiver, kms_rtp_receiver, KMS_TYPE_MEDIA_HANDLER_SRC)

static void
dispose_local_spec(KmsRtpReceiver *self) {
	if (self->priv->local_spec != NULL) {
		g_object_unref(self->priv->local_spec);
		self->priv->local_spec = NULL;
	}
}

void
kms_rtp_receiver_terminate(KmsRtpReceiver *self) {
	LOCK(self);
	if (self->priv->udpsrc) {
		gst_element_send_event(self->priv->udpsrc,
						gst_event_new_flush_start());
		self->priv->udpsrc = NULL;
	}
	UNLOCK(self);
}

static void
set_property (GObject *object, guint property_id, const GValue *value,
							GParamSpec *pspec) {
	KmsRtpReceiver *self = KMS_RTP_RECEIVER(object);

	switch (property_id) {
		case PROP_LOCAL_SPEC:
			LOCK(self);
			dispose_local_spec(self);
			self->priv->local_spec = g_value_dup_object(value);
			UNLOCK(self);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}

static void
get_property(GObject *object, guint property_id, GValue *value,
							GParamSpec *pspec) {
	KmsRtpReceiver *self = KMS_RTP_RECEIVER(object);

	switch (property_id) {
		case PROP_LOCAL_SPEC:
			LOCK(self);
			g_value_set_object(value, self->priv->local_spec);
			UNLOCK(self);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}

static GSList*
get_pay_list(KmsSdpMedia *media){
	GSList *list = NULL;
	GValueArray *payloads;
	gint i;

	g_object_get(media, "payloads", &payloads, NULL);

	for (i = 0; i < payloads->n_values; i++) {
		KmsSdpPayload *aux;

		aux = g_value_get_object(g_value_array_get_nth(payloads, i));
		list = g_slist_prepend(list, aux);
	}

	return list;
}

static gint
compare_pay_pt(gconstpointer ppay, gconstpointer ppt) {
	KmsSdpPayload *pay = (gpointer) ppay;
	guint pt;

	g_object_get(pay, "payload", &pt, NULL);

	return pt - (*(int *)ppt);
}

static GstCaps*
get_caps_for_pt(KmsRtpReceiver *self, guint pt) {
	GValueArray *medias;
	KmsSdpMedia *audio = NULL;
	KmsSdpMedia *video = NULL;
	GSList *payloads = NULL;
	GSList *l;
	gint i;

	LOCK(self);
	g_object_get(self->priv->local_spec, "medias", &medias, NULL);
	for (i = 0; i < medias->n_values; i++) {
		KmsSdpMedia *aux;
		KmsMediaType type;

		aux = g_value_get_object(g_value_array_get_nth(medias, i));
		g_object_get(aux, "type", &type, NULL);
		switch (type) {
		case KMS_MEDIA_TYPE_AUDIO:
			payloads = g_slist_concat(payloads,
							get_pay_list(audio));
			break;
		case KMS_MEDIA_TYPE_VIDEO:
			payloads = g_slist_concat(payloads,
							get_pay_list(video));
			break;
		default:
			/* No action */
			break;
		}
	}

	l = g_slist_find_custom(payloads, &pt, compare_pay_pt);

	if (l != NULL) {
		/* TODO: Convert payload to  */
		g_print("PT: found\n");
	}
	UNLOCK(self);

	g_slist_free(payloads);
	return NULL;
}

static GstCaps*
request_pt_map(GstElement *demux, guint pt, gpointer self) {
	GstCaps *caps;

	caps = get_caps_for_pt(self, pt);

	if (caps != NULL && gst_caps_is_fixed(caps)) {
		return caps;
	}

	if (caps != NULL)
		gst_caps_unref(caps);

	return NULL;
}

static void
new_payload_type(GstElement *demux, guint pt, GstPad *pad, gpointer user_data) {
	GstCaps *caps;
	gchar *caps_str;

	KMS_DEBUG;
	caps = gst_pad_get_caps(pad);
	caps_str = gst_caps_to_string(caps);
	g_print("%s\n", caps_str);
	g_free(caps_str);

	/* TODO: Add elements to process media correctly */

	gst_caps_unref(caps);
}

static void
create_media_src(KmsRtpReceiver *self, KmsMediaType type) {
	GstElement *udpsrc, *ptdemux;
	GstCaps *caps;
	gchar *udpsrc_name;
	gint *port;

	switch (type) {
	case KMS_MEDIA_TYPE_AUDIO:
		udpsrc_name = "udpsrc_audio";
		port = &(self->priv->audio_port);
		break;
	case KMS_MEDIA_TYPE_VIDEO:
		udpsrc_name = "udpsrc_video";
		port = &(self->priv->video_port);
		break;
	default:
		return;
	}

	caps = gst_caps_from_string("application/x-rtp");
	udpsrc = gst_element_factory_make("udpsrc", udpsrc_name);
	g_object_set(udpsrc, "port", 0, NULL);
	g_object_set(udpsrc, "caps", caps, NULL);
	gst_caps_unref(caps);

	ptdemux = gst_element_factory_make("gstrtpptdemux", NULL);

	gst_bin_add_many(GST_BIN(self), udpsrc, ptdemux, NULL);
	gst_element_link(udpsrc, ptdemux);

	gst_element_set_state(udpsrc, GST_STATE_PLAYING);
	gst_element_set_state(ptdemux, GST_STATE_PLAYING);

	g_object_get(udpsrc, "port", port, NULL);

	self->priv->udpsrc = udpsrc;

	g_object_connect(ptdemux, "signal::request-pt-map", request_pt_map,
								self, NULL);
	g_object_connect(ptdemux, "signal::new-payload-type", new_payload_type,
								self, NULL);
}

static void
constructed(GObject *object) {
	KmsRtpReceiver *self = KMS_RTP_RECEIVER(object);
	GValueArray *medias;
	gint i;

	G_OBJECT_CLASS(kms_rtp_receiver_parent_class)->constructed(object);

	create_media_src(self, KMS_MEDIA_TYPE_AUDIO);
	create_media_src(self, KMS_MEDIA_TYPE_VIDEO);

	g_object_get(self->priv->local_spec, "medias", &medias, NULL);

	for (i = 0; i < medias->n_values; i++) {
		GValue *val;
		KmsSdpMedia *media;
		KmsMediaType type;

		val = g_value_array_get_nth(medias, i);
		media = g_value_get_object(val);

		g_object_get(media, "type", &type, NULL);
		switch (type) {
		case KMS_MEDIA_TYPE_AUDIO:
			g_object_set(media, "port", self->priv->audio_port, NULL);
			break;
		case KMS_MEDIA_TYPE_VIDEO:
			g_object_set(media, "port", self->priv->video_port, NULL);
			break;
		default:
			break;
		}
	}

	g_value_array_free(medias);
}

static void
dispose(GObject *object) {
	KmsRtpReceiver *self = KMS_RTP_RECEIVER(object);

	kms_rtp_receiver_terminate(self);
	LOCK(self);
	dispose_local_spec(self);
	UNLOCK(self);

	/* Chain up to the parent class */
	G_OBJECT_CLASS(kms_rtp_receiver_parent_class)->dispose(object);
}

static void
finalize(GObject *object) {
	KmsRtpReceiver *self = KMS_RTP_RECEIVER(object);

	g_mutex_free(self->priv->mutex);

	/* Chain up to the parent class */
	G_OBJECT_CLASS(kms_rtp_receiver_parent_class)->finalize(object);
}

static void
kms_rtp_receiver_class_init(KmsRtpReceiverClass *klass) {
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(KmsRtpReceiverPriv));

	object_class->finalize = finalize;
	object_class->dispose = dispose;
	object_class->set_property = set_property;
	object_class->get_property = get_property;
	object_class->constructed = constructed;

	pspec = g_param_spec_object("local-spec", "Local Session Spec",
					"Local Session Spec",
					KMS_TYPE_SDP_SESSION,
					G_PARAM_CONSTRUCT |
					G_PARAM_READWRITE);

	g_object_class_install_property(object_class, PROP_LOCAL_SPEC, pspec);
}

static void
kms_rtp_receiver_init(KmsRtpReceiver *self) {
	self->priv = KMS_RTP_RECEIVER_GET_PRIVATE(self);

	self->priv->mutex = g_mutex_new();
	self->priv->local_spec = NULL;
}