#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libudev.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#define CMD_TOUCHPAD_ENABLE		"synclient TouchpadOff=0"
#define CMD_TOUCHPAD_DISABLE	"synclient TouchpadOff=1"

int main(void)
{
	int fd;
	int ret;
	fd_set rfds;
	struct timeval tv;
	struct udev *udev;
	struct udev_monitor *udev_mon;
	struct udev_device *event_dev;
	
	udev = udev_new();
	assert(udev);

	udev_mon = udev_monitor_new_from_netlink(udev, "udev");
	assert(udev_mon);

	ret = udev_monitor_filter_add_match_subsystem_devtype(udev_mon, "input", NULL);
	assert(ret == 0);
	
	ret = udev_monitor_enable_receiving(udev_mon);
	assert(ret >= 0);
	
	fd = udev_monitor_get_fd(udev_mon);
	assert(fd > 0);
	
	do{
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 500*1000;
		
		ret = select(fd+1, &rfds, NULL, NULL, &tv);
		if(ret < 0){
			printf("select failed!\n");
			break;
		}
	
		if(ret == 0) continue;

		event_dev = udev_monitor_receive_device(udev_mon);
		assert(event_dev);

		const char *action = udev_device_get_action(event_dev);
		const char *sysname = udev_device_get_sysname(event_dev);

		printf("<%s> %s\n", action, sysname);
		if(strncmp(sysname, "mouse", 5) == 0){
			if(strcmp(action, "add") == 0){
				printf("disable tp\n");
				system(CMD_TOUCHPAD_DISABLE);
			}else if(strcmp(action, "remove") == 0){
				printf("enable tp\n");
				system(CMD_TOUCHPAD_ENABLE);
			}
		}
/*
		printf("<%s>: vendor(%s) product(%s) node %s move\n", 
									udev_device_get_action(event_dev),
									udev_device_get_sysattr_value(event_dev, "idVendor"),
									udev_device_get_sysattr_value(event_dev, "idProduct"),
									udev_device_get_devnode(event_dev));
*/
/*		printf("%s, %s, %s, %s, %s, %s, %s\n", 
				udev_device_get_devpath(event_dev),
				udev_device_get_subsystem(event_dev),
				udev_device_get_devtype(event_dev),
				udev_device_get_syspath(event_dev),
				udev_device_get_sysname(event_dev),
				udev_device_get_sysnum(event_dev),
				udev_device_get_devnode(event_dev));
*/
//		printf("**** %s *****\n", udev_device_get_sysname(event_dev));
		udev_device_unref(event_dev);
	}while(1);

	udev_monitor_unref(udev_mon);
	udev_unref(udev);
	
	return 0;
}
	
