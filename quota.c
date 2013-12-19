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
		if (quotactl(Q_GETQUOTA, "/", getuid(), (char *)&q)) {
			uwsgi_error("master_check_quota()/quotactl()");
		}
		else {
			uwsgi_log("quota ready\n");
		}
	}
}

static int hook_setquota(char *arg) {
	size_t i,rlen = 0;
	int ret = -1;
	struct dqblk q;
	memset(&q, 0, sizeof(struct dqblk));
	char **argv = uwsgi_split_quoted(arg, strlen(arg), " \t", &rlen);
	if (rlen < 3) {
		uwsgi_log("invalid setquota syntax, must be <mountpoint> <uid> <bytes>\n");
		goto clear;
	}
	int id = atoi(argv[1]);
	if (id < 1) {
		uwsgi_log("invalid uid, you can only set a quota for unprivileged users\n");
		goto clear;
	}
	q.dqb_bhardlimit = btodb(uwsgi_n64(argv[2]));
	if (q.dqb_bhardlimit < 1) {
		uwsgi_log("invalid quota size: %llu\n", (unsigned long long) q.dqb_bhardlimit);
		goto clear;
	}
	q.dqb_valid = QIF_BLIMITS;
	if (quotactl(QCMD(Q_SETQUOTA, USRQUOTA), argv[0], id, (caddr_t) &q)) {
		uwsgi_error("hook_setquota()/quotactl()");
		goto clear;
	}
	ret = 0;
clear:
	if (argv) {
		for(i=0;i<rlen;i++) {
			free(argv[i]);
		}
		free(argv);
	}
	return ret;
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
