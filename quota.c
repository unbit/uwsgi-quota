#include <uwsgi.h>
#include <sys/quota.h>

/*

	Currently, only Linux is supported

*/

static struct uwsgi_quota {
	struct uwsgi_string_list *alarms;
} uquota;

static struct uwsgi_option quota_options[] = {
	{ "alarm-quota", required_argument, 0, "add an alarm when user filesystem quota is higher than the specified percentage, syntax <path> <percentage>", uwsgi_opt_add_string_list, &uquota.alarms, UWSGI_OPT_MASTER},
	UWSGI_END_OF_OPTIONS
};

static void master_check_quota() {
	if (!uquota.alarms) return;
	struct uwsgi_string_list *usl = NULL;
	uwsgi_foreach(usl, uquota.alarms) {
		uwsgi_log("check quota: %s\n", usl->value);
		struct dqblk q;
		if (quotactl("/", Q_GETQUOTA, getuid(), (char *)&q)) {
			uwsgi_error("master_check_quota()/quotactl()");
		}
		else {
			uwsgi_log("quota ready\n");
		}
	}
}

static int hook_setquota(char *arg) {
	struct dqblk q;
	memset(&q, 0, sizeof(struct dqblk));
	//q.dqb_bhardlimit = btodb(100);
	//dbtob(
	return 0;
}

static void quota_register_features() {
	uwsgi_register_hook("setquota", hook_setquota);
}

struct uwsgi_plugin quota_plugin = {
	.name = "quota",
	.options = quota_options,
	.on_load = quota_register_features,
	.master_cycle = master_check_quota,
};
