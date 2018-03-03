#!/bin/bash

SERVICE_FILE=$(tempfile)

cat >> $SERVICE_FILE <<\EOF
#!/bin/sh
### BEGIN INIT INFO
# Provides:          <NAME>
# Required-Start:    $local_fs $network $named $time $syslog
# Required-Stop:     $local_fs $network $named $time $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Description:       <DESCRIPTION>
### END INIT INFO

SCRIPT=<COMMAND>
RUNAS=<USERNAME>

PIDFILE=/var/run/<NAME>.pid
LOGFILE=/var/log/<NAME>.log

start() {
  if [ -f "$PIDFILE" ] && kill -0 $(cat "$PIDFILE"); then
    echo 'Service already running' >&2
    return 1
  fi
  echo 'Starting service…' >&2
  local CMD="$SCRIPT &> \"$LOGFILE\" & echo \$!"
  su -c "$CMD" $RUNAS > "$PIDFILE"
  echo 'Service started' >&2
}

stop() {
  if [ ! -f "$PIDFILE" ] || ! kill -0 $(cat "$PIDFILE"); then
    echo 'Service not running' >&2
    return 1
  fi
  echo 'Stopping service…' >&2
  kill -15 $(cat "$PIDFILE") && rm -f "$PIDFILE"
  echo 'Service stopped' >&2
}

uninstall() {
  echo -n "Are you really sure you want to uninstall this service? That cannot be undone. [yes|No] "
  local SURE
  read SURE
  if [ "$SURE" = "yes" ]; then
    stop
    rm -f "$PIDFILE"
    echo "Notice: log file is not be removed: '$LOGFILE'" >&2
    update-rc.d -f <NAME> remove
    rm -fv "$0"
  fi
}

case "$1" in
  start)
    start
    ;;
  stop)
    stop
    ;;
  uninstall)
    uninstall
    ;;
  restart)
    stop
    start
    ;;
  *)
    echo "Usage: $0 {start|stop|restart|uninstall}"
esac
EOF

echo "--- Customize ---"
echo "I'll now ask you some information to customize script"
echo "Press Ctrl+C anytime to abort."
echo "Empty values are not accepted."
echo ""

prompt_token() {
  local VAL=""
  while [ "$VAL" = "" ]; do
    echo -n "${2:-$1} : "
    read VAL
    if [ "$VAL" = "" ]; then
      echo "Please provide a value"
    fi
  done
  VAL=$(printf '%q' "$VAL")
  eval $1=$VAL
  sed -i "s!<$1>!$(printf '%q' "$VAL")!g" $SERVICE_FILE
}

prompt_token 'NAME'        'Service name'
if [ -f "/etc/init.d/$NAME" ]; then
  echo "Error: service '$NAME' already exists"
  exit 1
fi

prompt_token 'DESCRIPTION' ' Description'
prompt_token 'COMMAND'     '     Command'
prompt_token 'USERNAME'    '        User'
if ! id -u "$USERNAME" &> /dev/null; then
  echo "Error: user '$USERNAME' not found"
  exit 1
fi

echo ""

echo "--- Installation ---"
if [ ! -w /etc/init.d ]; then
  echo "You don't gave me enough permissions to install service myself."
  echo "That's smart, always be really cautious with third-party shell scripts!"
  echo "You should now type those commands as superuser to install and run your service:"
  echo ""
  echo "   mv \"$SERVICE_FILE\" \"/etc/init.d/$NAME\""
  echo "   touch \"/var/log/$NAME.log\" && chown \"$USERNAME\" \"/var/log/$NAME.log\""
  echo "   update-rc.d \"$NAME\" defaults"
  echo "   service \"$NAME\" start"
else
  echo "1. mv \"$SERVICE_FILE\" \"/etc/init.d/$NAME\""
  mv -v "$SERVICE_FILE" "/etc/init.d/$NAME"
  echo "1b. chmod +x"
  chmod 755 "/etc/init.d/$NAME"
  echo "2. touch \"/var/log/$NAME.log\" && chown \"$USERNAME\" \"/var/log/$NAME.log\""
  touch "/var/log/$NAME.log" && chown "$USERNAME" "/var/log/$NAME.log"
  echo "3. update-rc.d \"$NAME\" defaults"
  update-rc.d "$NAME" defaults
  echo "4. service \"$NAME\" start"
  service "$NAME" start
fi

echo ""
echo "---Uninstall instructions ---"
echo "The service can uninstall itself:"
echo "    service \"$NAME\" uninstall"
echo "It will simply run update-rc.d -f \"$NAME\" remove && rm -f \"/etc/init.d/$NAME\""
echo ""
echo "--- Terminated ---"
