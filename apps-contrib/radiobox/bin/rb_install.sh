#!/bin/sh

cd /tmp

echo "Stopping nginx ..."
/opt/www/apps/radiobox/bin/nginx_stop.sh

echo "Changing mode to RW ..."
rw

echo "Removing radiobox directory ..."
rm -rf /opt/www/apps/radiobox

echo "Unpacking of radiobox*.zip file ..."
cd /opt/www/apps
unzip /tmp/radiobox*.zip >/dev/null
cd /tmp

echo "Removing radiobox*.zip file ..."
rm -f /tmp/radiobox-*.zip

echo "Syncing ..."
sync

echo "Changing mode to RO ..."
(sleep 1; ro)&

echo "Starting nginx ..."
/opt/www/apps/radiobox/bin/nginx_start.sh

echo "done."
