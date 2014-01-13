#include <uwsgi.h>
#include <sys/quota.h>

/*

	Currently, only Linux is supported

*/

static struct uwsgi_quota {
	struct uwsgi_string_list *alarms;
} uquota;

static struct uwsgi_option quota_options[] = {
	{ "alarm-quota", required_argument, 0, "raise the specified alarm when user filesystem quota is higher than threshold percentage (default 90), syntax <alarm> <path> [percentage]", uwsgi_opt_add_string_list, &uquota.alarms, UWSGI_OPT_MASTER},
	UWSGI_END_OF_OPTIONS
};

static void master_check_quota() {
	if (!uquota.alarms) return;
	struct uwsgi_string_list *usl = NULL;
	uwsgi_foreach(usl, uquota.alarms) {
		if (!usl->custom && !usl->custom_ptr) {
			size_t rlen;
			char **argv = uwsgi_split_quoted(usl->value, usl->len, " \t", &rlen);
			if (!argv || rlen < 2) {
				uwsgi_log("invalid quota-alarm specified, must be in the form: <alarm> <path> [percentage]\n");
				// not very funny to exit here...
				exit(1);
			}
			usl->custom_ptr = argv;
			usl->custom = 90;
			if (rlen > 2) {
				usl->custom = atoi(argv[2]);
			}
			uwsgi_log("added quota monitor for %s (threshold: %d%)\n", argv[1], usl->custom);
		}
		struct dqblk q;
		char **argv = (char **) usl->custom_ptr;
		if (quotactl(QCMD(Q_GETQUOTA, USRQUOTA), argv[1], getuid(), (char *)&q)) {
			uwsgi_error("master_check_quota()/quotactl()");
		}
		else {
			uint64_t max_quota = dbtob(q.dqb_bhardlimit);
			if (max_quota < 1) continue;
			uint64_t current_quota = q.dqb_curspace;
			uint64_t result = 100;
			if (current_quota > 0) {
				result = (current_quota*100)/max_quota;
			}
			if (result >= usl->custom) {
				time_t now = uwsgi_now();
				if (now - usl->custom2 < 60) continue;
				char msg[1024];
				int ret = snprintf(msg, 1024, "quota of %s is higher than the threshold (%llu/%llu)", argv[1], current_quota, max_quota);
				if (ret > 0) {
					uwsgi_alarm_trigger(argv[0], msg, ret);
				}
				else {
					// fallback
					uwsgi_alarm_trigger(argv[0], "QUOTA ALARM", 11);
				}
				usl->custom2 = now;
			}
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
