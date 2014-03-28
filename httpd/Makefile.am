
all-local: mod_logreader.la

mod_logreader.la: mod_logreader.c
	apxs -c mod_logreader.c


install-exec-local:
	apxs -i mod_logreader.la

