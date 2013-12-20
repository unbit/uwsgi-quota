uwsgi-quota
===========

uWSGI plugin for UNIX quota integration


This is another plugin developed for the upcoming uwsgi.it service.

It allows the customer Emperor to set quota on startup and to monitor it constantly.

The plugin exposes the `setquota:<path> <uid> <bytes>` hook and the `--alarm-quota <alarm> <path> [percentage]` option.

Generally you will call setquota as soon as possibile, so the `--hook-asap` hook will be a good candidate for the job:


```ini
[uwsgi]
plugins = quota
; enforce uid 30001 to 100MB quota
hook-asap = setquota:/dev/sda1 30001 100000000
...
```

Once the quota is configured you can map it to an alarm:

```ini
[uwsgi]
plugins = quota
; enforce uid 30001 to 100MB quota
hook-asap = setquota:/dev/sda1 30001 100000000
; create a new alarm (simly log the message, on real deployment use something else, like mails or jabber/xmpp)
alarm = overquota log:
; raise the 'overquota' alarm when the quota is over 98%
alarm-quota = overquota /dev/sda1 98
; do not raise more than one alarm for minute
alarm-freq = 60
...
```

The plugin supports the new build api, so to compile it you only need 1.9.21/2.0 uwsgi binary and run:

```sh
uwsgi --build-plugin uwsgi-quota
```
