#!/bin/sh

DIR=https://dl.dropboxusercontent.com/u/13881457/RedPitaya_RadioBox/Releases/RB_v0.95.01
ECO=ecosystem-0.95-4600-a765b14.zip

echo
echo "========================================================================="
echo "Upgrading current RedPitaya image to support additional RadioBox features"
echo "========================================================================="
echo

echo
echo "Step 1: syncing last RadioBox ecosystem down from server"
echo "------"
rw
wget -P /tmp -c ${DIR}/${ECO}
CURDIR=`pwd`
cd /opt/redpitaya
unzip -u -o /tmp/${ECO}
cd $CURDIR
sync

echo
echo "Step 2: preparing kernel modules"
echo "------"
rw
mv /lib/modules /lib/modules_old
ln -s /opt/redpitaya/lib/modules /lib/modules
rm /lib/modules_old
depmod -a
echo "... done."

echo
echo "Step 3: updating the dpkg catalog"
echo "------"
rw
apt-get update -y
echo "... done."

echo
echo "Step 4: upgrade outdated packages"
echo "------"
rw
apt-get upgrade -y
echo "... done."

echo
echo "Step 5: installing additionally packages"
echo "------"
rw
apt-get -y install alsaplayer-alsa alsa-tools alsa-utils dbus dbus-x11 dosfstools esound-common flac icecast2 ices2 jack-tools locate multicat pavucontrol pulseaudio pulseaudio-esound-compat pulseaudio-module-jack python-apt rsync software-properties-common speex strace tcpdump vorbis-tools x11-common x11-xkb-utils x11-xserver-utils xauth xfonts-100dpi xfonts-75dpi xfonts-base xfonts-encodings xfonts-scalable xfonts-utils xinetd xkb-data xserver-common xserver-xorg-core
echo "... done."

echo
echo "Step 6: adding apt-repositories (PPA)"
echo "------"
rw
add-apt-repository -y ppa:kamalmostafa/fldigi
echo "... done."

echo
echo "Step 7: updating the dpkg catalog"
echo "------"
rw
apt-get update -y
echo "... done."

echo
echo "Step 8: installing additionally packages (PPA)"
echo "------"
rw
apt-get -y install fldigi flwrap
echo "... done."

#echo
#echo "Step 9: adding dpkg selections and upgrading to the current Ubuntu release."
#echo "------"
#rw
#dpkg --get-selections > data/RadioBox-Upgrade_dpkg-selections-current.dat
#LC_ALL=C cat data/RadioBox-Upgrade_dpkg-selections-needed.dat data/RadioBox-Upgrade_dpkg-selections-current.dat | grep -v deinstall | sort | uniq > data/RadioBox-Upgrade_dpkg-selections-new.dat
#dpkg --set-selections < data/RadioBox-Upgrade_dpkg-selections-new.dat
#echo "... done."

#echo
#echo "Step 10: upgrade outdates packages (2)"
#echo "-------"
#rw
#apt-get upgrade -y
#echo "... done."

echo
echo "Step 11: clean-up not more needed automatic packages"
echo "-------"
rw
apt-get autoremove -y
echo "... done."

echo
echo "Step 12: clean-up packages not more needed"
echo "-------"
rw
apt-get autoclean -y
echo "... done."

echo
echo "Step 13: setting up new file links"
echo "-------"
rw
mv /etc/xinetd.conf /etc/xinetd.conf_old
mv /etc/xinetd.d    /etc/xinetd.d_old
cp -r /opt/redpitaya/etc/xinetd.conf /etc/xinetd.conf
cp -r /opt/redpitaya/etc/xinetd.d    /etc/xinetd.d
rm -rf /etc/xinetd.conf_old
rm -rf /etc/xinetd.d_old
echo "... done."

echo
echo "Step 14: setting up audio streaming"
echo "-------"
rw
addgroup --gid 115 icecast
adduser  --gecos "" --home /usr/share/icecast2 --disabled-password --disabled-login --uid 115 --gid 115 icecast2
adduser  icecast2 icecast
mv /etc/pulse    /etc/pulse_old
mv /etc/icecast2 /etc/icecast2_old
mv /etc/ices2    /etc/ices2_old 
mkdir /var/log/ices
chown -R icecast2:icecast         /var/log/ices
cp -r /opt/redpitaya/etc/pulse    /etc/pulse
cp -r /opt/redpitaya/etc/icecast2 /etc/icecast2
cp -r /opt/redpitaya/etc/ices2    /etc/ices2
chown -R icecast2:icecast /etc/icecast2 /etc/ices2
rm -rf /etc/pulse_old
rm -rf /etc/icecast2_old
rm -rf /etc/ices2_old
cp -a /opt/redpitaya/www/apps/radiobox/bin/data/RadioBox-Upgrade_etc_default_icecast2 /etc/default/icecast2

echo
echo "Step 15: setting up sound system"
echo "-------"
rw
redpitaya-ac97_stop
# renaming of pulse machine dependant files
rm -rf /root/.config/pulse
tar -C / -Jxf /opt/redpitaya/www/apps/radiobox/bin/data/RadioBox-Upgrade_root-config-pulse.tar.7z
MI=`cat /etc/machine-id`
for FILE in /root/.config/pulse/MACHINEID*; do
	echo $FILE > /tmp/tmp.txt
	NEWFILE=`sed -e s/MACHINEID/${MI}/ </tmp/tmp.txt`
	mv $FILE $NEWFILE 2>/dev/null
done
ln -s /tmp/pulse-* /root/.config/pulse/$MI-runtime
rm -f /tmp/tmp.txt
cp -a /opt/redpitaya/www/apps/radiobox/bin/data/RadioBox-Upgrade_asound.state /var/lib/alsa/asound.state
# done
redpitaya-ac97_start
alsactl restore
amixer -D pulse sset Master 100% on
amixer -D pulse sset Capture 100% on
# amixer -D hw:CARD=RedPitayaAC97 sset Master 100% on
# amixer -D hw:CARD=RedPitayaAC97 sset PCM 100% on
# amixer -D hw:CARD=RedPitayaAC97 sset Line 100% off
# amixer -D hw:CARD=RedPitayaAC97 sset Capture 100% on
pactl set-sink-volume 0 100%
pactl set-source-volume 1 100%
pactl set-source-output-volume 0 100%
alsactl store
echo "... done."

echo
echo "Step 16: update locate database"
echo "-------"
rw
updatedb
sync
ro
echo "... done."

echo
echo ">>> FINISH <<<  Congrats, the system is ready for RadioBox additional features"
echo    "Please check if current running kernel is: 4.0.0-xilinx"
echo -n "Currently running                        : "
uname -r
echo
echo "If it not matches, please reboot Red Pitaya, then restart this script again."
echo "=============================================================================="
echo

