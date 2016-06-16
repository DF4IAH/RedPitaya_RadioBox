#!/bin/bash

DIR=https://dl.dropboxusercontent.com/u/13881457/RedPitaya_RadioBox/Releases/RB_v0.95.01
ECO=ecosystem-0.95-4600-a765b14.zip

echo
echo "========================================================================="
echo "Upgrading current RedPitaya image to support additional RadioBox features"
echo
echo "This update process needs one boot cycle:"
echo "  - Stage 1 before the boot,"
echo "  - Stage 2 after  the boot."
echo "========================================================================="
echo

KERNELREL=`uname -r`

echo "Red Pitaya booted kernel release: $KERNELREL"
echo

if [ "$KERNELREL" = "3.18.0-xilinx" ]; then
	echo "Stage 1: kernel 3.18 found - preparing for new kernel from the ecosystem"

	echo
	echo "Step 101: syncing latest RadioBox ecosystem down from server"
	echo "--------"
	rw
	wget -P /tmp -c ${DIR}/${ECO}
	echo "Expanding latest RadioBox ecosystem ..."
	cd /opt/redpitaya; unzip -u -o /tmp/${ECO}
	cd /
	sync
	echo "... done."
	echo
	echo "!! Please start-up this RadioBox_Upgrade.sh script after the reboot !!"
	echo "!! AGAIN to make the final upgrades to the system.  Thank you       !!"
	echo
	echo "now rebooting ..."
	ro
	reboot
	exit

else
	echo "Stage 2: kernel 4.0 found - upgrading the image to include the icecast2 streaming server"

	echo
	echo "Step 201: preparing the kernel modules"
	echo "--------"
	rw
	mv /lib/modules /lib/modules_old 2>/dev/null
	ln -s /opt/redpitaya/lib/modules /lib/modules
	rm /lib/modules_old 2>/dev/null
	depmod -a
	echo "... done."

	echo
	echo "Step 202: updating the dpkg catalog"
	echo "--------"
	rw
	apt-get update -y
	echo "... done."

	echo
	echo "Step 203: upgrade outdated packages"
	echo "--------"
	rw
	apt-get upgrade -y
	echo "... done."

	echo
	echo "Step 204: installing additionally packages"
	echo "--------"
	rw
	apt-get -y install alsaplayer-alsa alsa-tools alsa-utils dbus dbus-x11 dosfstools esound-common flac icecast2 ices2 jack-tools locate multicat pavucontrol pulseaudio pulseaudio-esound-compat pulseaudio-module-jack python-apt rsync software-properties-common speex strace tcpdump vorbis-tools x11-common x11-xkb-utils x11-xserver-utils xauth xfonts-100dpi xfonts-75dpi xfonts-base xfonts-encodings xfonts-scalable xfonts-utils xinetd xkb-data xserver-common xserver-xorg-core
	echo "... done."

	echo
	echo "Step 205: installing current fldigi and flwrap"
	echo "--------"
	rw
	add-apt-repository -y ppa:kamalmostafa/fldigi
	apt-get update -y
	apt-get -y install fldigi flwrap
	echo "... done."

	echo
	echo "Step 206: adding dpkg selections and upgrading to the current Ubuntu release."
	echo "--------"
	rw
	#dpkg --get-selections > data/RadioBox-Upgrade_dpkg-selections-current.dat
	#LC_ALL=C cat data/RadioBox-Upgrade_dpkg-selections-needed.dat data/RadioBox-Upgrade_dpkg-selections-current.dat | grep -v deinstall | sort | uniq > data/RadioBox-Upgrade_dpkg-selections-new.dat
	#dpkg --set-selections < data/RadioBox-Upgrade_dpkg-selections-new.dat
	echo "... done."

	echo
	echo "Step 207: upgrade outdates packages (2)"
	echo "--------"
	rw
	#apt-get upgrade autoremove autoclean -y
	echo "... done."

	echo
	echo "Step 208: setting up new file links"
	echo "--------"
	rw
	/etc/init.d/xinetd stop
	mv /etc/xinetd.conf /etc/xinetd.conf_old 2>/dev/null
	mv /etc/xinetd.d    /etc/xinetd.d_old 2>/dev/null
	cp -r /opt/redpitaya/etc/xinetd.conf /etc/xinetd.conf
	cp -r /opt/redpitaya/etc/xinetd.d    /etc/xinetd.d
	rm -rf /etc/xinetd.conf_old 2>/dev/null
	rm -rf /etc/xinetd.d_old 2>/dev/null
	/etc/init.d/xinetd start
	echo "... done."

	echo
	echo "Step 209: setting up audio streaming"
	echo "--------"
	rw
	/etc/init.d/icecast2 stop
	addgroup --gid 115 icecast
	adduser  --gecos "" --home /usr/share/icecast2 --disabled-password --disabled-login --uid 115 --gid 115 icecast2
	adduser  icecast2 icecast
	mv /etc/pulse    /etc/pulse_old 2>/dev/null
	mv /etc/icecast2 /etc/icecast2_old 2>/dev/null
	mv /etc/ices2    /etc/ices2_old 2>/dev/null
	mkdir /var/log/ices
	chown -R icecast2:icecast         /var/log/ices
	cp -r /opt/redpitaya/etc/pulse    /etc/pulse
	cp -r /opt/redpitaya/etc/icecast2 /etc/icecast2
	cp -r /opt/redpitaya/etc/ices2    /etc/ices2
	chown -R icecast2:icecast /etc/icecast2 /etc/ices2
	rm -rf /etc/pulse_old 2>/dev/null
	rm -rf /etc/icecast2_old 2>/dev/null
	rm -rf /etc/ices2_old 2>/dev/null
	cp -a /opt/redpitaya/www/apps/radiobox/bin/data/RadioBox-Upgrade_etc_default_icecast2 /etc/default/icecast2
	/etc/init.d/icecast2 start
	echo "... done."

	echo
	echo "Step 210: setting up sound system"
	echo "--------"
	rw
	redpitaya-ac97_stop 2>/dev/null
	rm -rf /tmp/pulse* 2>/dev/null
	# renaming of pulse machine dependant files ...
	rm -rf /root/.config/pulse 2>/dev/null
	tar -C / -Jxf /opt/redpitaya/www/apps/radiobox/bin/data/RadioBox-Upgrade_root-config-pulse.tar.7z
	MI=`cat /etc/machine-id`
	for FILE in /root/.config/pulse/MACHINEID*; do
		echo $FILE > /tmp/tmp.txt
		NEWFILE=`sed -e s/MACHINEID/${MI}/ </tmp/tmp.txt`
		mv $FILE $NEWFILE 2>/dev/null
	done
	redpitaya-ac97_start
	ln -s /tmp/pulse-* /root/.config/pulse/$MI-runtime
	rm -f /tmp/tmp.txt
	cp -a /opt/redpitaya/www/apps/radiobox/bin/data/RadioBox-Upgrade_asound.state /var/lib/alsa/asound.state
	# ... is completed
	alsactl restore
	amixer -D pulse sset Master 100% on
	amixer -D pulse sset Capture 100% on
	amixer -D hw:CARD=RedPitayaAC97 sset Master 100% on
	amixer -D hw:CARD=RedPitayaAC97 sset PCM 100% on
	amixer -D hw:CARD=RedPitayaAC97 sset Line 100% off
	amixer -D hw:CARD=RedPitayaAC97 sset Capture 100% on
	pactl set-sink-volume 0 100%
	pactl set-source-volume 0 100%
	pactl set-source-volume 1 100%
	pactl set-source-output-volume 0 100%
	pactl set-source-mute 0 0
	pactl set-source-mute 1 0
	alsactl store
	sync
	echo "... done."

	echo
	echo "Step 211: update locate database"
	echo "--------"
	rw
	updatedb
	sync
	ro
	echo "... done."

	echo
	echo
	echo ">>> FINISH <<<  Congrats, the Red Pitaya RadioBox upgrade has completed successfully."
	echo
	echo "New features added:"
	echo "    - streaming from an icecast2 server on port 8000"
	echo "    - fldigi  for amateur radio digital modes"
	echo
fi

