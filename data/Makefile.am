conf_DATA = rtcom-eventlogger.conf
confdir = $(sysconfdir)/osso-backup/applications

backup_SCRIPTS = rtcom-eventlogger-backup.sh
backupdir = $(sysconfdir)/osso-backup/pre-backup.d

postbackup_SCRIPTS = rtcom-eventlogger-postbackup.sh
postbackupdir = $(sysconfdir)/osso-backup/post-backup.d

restore_SCRIPTS = rtcom-eventlogger-restore.sh
restoredir = $(sysconfdir)/osso-backup/restore.d/always

cud_SCRIPTS = rtcom-eventlogger-clean.sh
cuddir = $(sysconfdir)/osso-cud-scripts

EXTRA_DIST = \
	$(conf_DATA) \
	$(cud_SCRIPTS) \
	rtcom-eventlogger.supp

MAINTAINERCLEANFILES = Makefile.in
