#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libudev.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>

#define CMD_TOUCHPAD_ENABLE "xinput set-prop `xinput list | grep -i touchpad | cut -f 2 | grep -oE '[[:digit:]]+'` \"Device Enabled\" 1"
#define CMD_TOUCHPAD_DISABLE "xinput set-prop `xinput list | grep -i touchpad | cut -f 2 | grep -oE '[[:digit:]]+'` \"Device Enabled\" 0"

#define LOGNAME	"TouchpadSwitch"

const char *targ_name = "mouse";

static void daemonize(void)
{
	int ret;
	pid_t pid;
	struct rlimit rl;
	struct sigaction sa;

	/* clear file create mask */
	umask(0);
	
	/* Get maximum number of file descriptors */
	ret = getrlimit(RLIMIT_NOFILE, &rl);
	assert(ret >= 0);

	/* create a daemon, only keep the child*/
	pid = fork();
	if(pid < 0){
		printf("fork: %s\n", strerror(errno));
		exit(0);
	}else if(pid > 0) exit(0);	//parent out

	/* create a new session */	
	ret = setsid();
	assert(ret >= 0);
	
	/* Ensure furure opens won'y allocate controle TTYs */
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	
	ret = sigaction(SIGHUP, &sa, NULL);
	assert(ret >= 0);
	ret = sigaction(SIGCHLD, &sa, NULL);
	assert(ret >= 0);
	
	/* fork, detach from process group leader */
	pid = fork();
	if(pid < 0){
		printf("fork: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}else if(pid > 0) exit(EXIT_SUCCESS);	//parent out


	/*
	 * Change the current working directory to the root so
	 * we won’t prevent file systems from being unmounted.
	 */
	ret = chdir("/");
	assert(ret >= 0);

	/* Close all open file descriptors */
	if(rl.rlim_max == RLIM_INFINITY)
		rl.rlim_max = 1024;
	for(int i = 0; i < rl.rlim_max; i++)
		close(i);

	/* Attach file descriptors 0, 1, and 2 to /dev/null */
	int fd0 = open("/dev/null", O_RDWR);	
	int fd1 = dup(0);
	int fd2 = dup(0);
	
	/* Initial the log file */
	openlog(LOGNAME, LOG_CONS, LOG_DAEMON);
	if (fd0 != 0 || fd1 != 1 || fd2 != 2){
		syslog(LOG_ERR, "unexpected file descriptors %d %d %d",
						fd0, fd1, fd2);
		exit(EXIT_FAILURE);
	}
}

#ifdef DAEMONIZE
	#define LOGMSG(msg, args...)	syslog(LOG_NOTICE, msg, ##args);
	#define LOGERR(msg, args...) 	syslog(LOG_ERR, msg, ##args);
	#define LOGSTART(msg, args...)	syslog(LOG_WARNING, msg, ##args);
#else 
	#define LOGMSG(msg, args...)	do{}while(0);
	#define LOGERR(msg, args...) 	fprintf(stderr, msg, ##args);
	#define LOGSTART(msg, args...)	fprintf(stdout, msg, ##args);
#endif

static void tpsd_init_check(struct udev *udev)
{
	int tps_off = 0;
	struct udev_enumerate *enumerate;
    struct udev_list_entry *devices;
    struct udev_list_entry *dev_list_entry;
    struct udev_device *dev;
    struct udev_device *parent_dev;

	enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(dev_list_entry, devices){
		const char *path;
		char name[128] = {0};
		
		path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);
        parent_dev = udev_device_get_parent_with_subsystem_devtype(dev, "input", 0);
        if(parent_dev){
            udev_device_unref(parent_dev);
            continue;
        }
		
		snprintf(name, sizeof(name), "%s",
					udev_device_get_sysattr_value(dev, "name"));
		
		udev_device_unref(dev);

		if(strstr(name, "Mouse")){
			tps_off = 1;
			break;
		}
		
		memset(name, 0, sizeof(name));
	}

	udev_enumerate_unref(enumerate);

	if(tps_off){
		LOGMSG("first check: detect mouse in used, disable touchpad");
		system(CMD_TOUCHPAD_DISABLE);
	}else{
		LOGMSG("first check: no mouse in used, enable touchpad");
		system(CMD_TOUCHPAD_ENABLE);
	}

}

int main(void)
{
	int fd;
	int ret;
	fd_set rfds;
	struct timeval tv;
	struct udev *udev;
	struct udev_monitor *udev_mon;
	struct udev_device *event_dev;

#ifdef DAEMONIZE
	daemonize();
#endif

	udev = udev_new();
	if(!udev){
		LOGERR("udev_new failed!");
		exit(EXIT_FAILURE);
	}

	LOGSTART("-- Touchpad switch daemon START! --");

	tpsd_init_check(udev);

	udev_mon = udev_monitor_new_from_netlink(udev, "udev");
	if(!udev_mon){
		LOGERR("udev new link failed!");
		goto out1;
	}

	if(udev_monitor_filter_add_match_subsystem_devtype(udev_mon, "input", NULL) != 0){
		LOGERR("udev add filter failed!");
		goto out;
	}
	
	if(udev_monitor_enable_receiving(udev_mon) < 0){
		LOGERR("udev enable recv failed!");
		goto out;
	}
	
	fd = udev_monitor_get_fd(udev_mon);
	if(fd <= 0){
		LOGERR("udev get monitor fd failed!");
		goto out;
	}

	do{
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 500*1000;
		
		ret = select(fd+1, &rfds, NULL, NULL, &tv);
		if(ret < 0){
			LOGERR("Select failed!");
			break;
		}
	
		if(ret == 0) continue;

		/* cacch device event */
		event_dev = udev_monitor_receive_device(udev_mon);
		if(!event_dev){
			LOGERR("get event udev failed1");
			break;
		}

		const char *action = udev_device_get_action(event_dev);
		const char *sysname = udev_device_get_sysname(event_dev);

		if(strncmp(sysname, targ_name, strlen(targ_name)) == 0){
			if(strcmp(action, "add") == 0){
				syslog (LOG_NOTICE, "detect mouse add, disable touchpad");
				system(CMD_TOUCHPAD_DISABLE);
			}else if(strcmp(action, "remove") == 0){
				syslog (LOG_NOTICE, "detect mouse remove, enable touchpad");
				system(CMD_TOUCHPAD_ENABLE);
			}
		}

		udev_device_unref(event_dev);
	}while(1);

out:
	udev_monitor_unref(udev_mon);
out1:
	udev_unref(udev);
	
	return 0;
}
	
