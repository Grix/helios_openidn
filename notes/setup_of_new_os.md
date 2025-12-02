1. Install Armbian server/cli (or minimal but with network-manager enabled). Username laser, password pen_pineapple . Don't generate locales etc.

2. Copy files to system. 

	```
    scp .\openidn_start.sh laser@10.0.0.20:/home/laser/
	scp .\heliosdac.rules laser@10.0.0.20:/home/laser/
	scp .\openidn.service laser@10.0.0.20:/home/laser/
	scp .\helios_openidn_v0.9.3.bin laser@10.0.0.20:/home/laser/
	```
	
3. Log into system, move software to correct folder

	```
    mkdir openidn
	mv helios_openidn_v0.9.1.bin openidn/helios_openidn
	chmod +x openidn/helios_openidn
	touch openidn/settings.ini
	mkdir library
	```
	
4. Make autostart service

	```
	chmod +x openidn_start.sh
	sudo mv openidn.service /lib/systemd/system
	sudo systemctl enable openidn
	```
	
5. Setup usb stuff

	```
	sudo mv heliosdac.rules /etc/udev
	sudo ln -s /etc/udev/heliosdac.rules /etc/udev/rules.d/011-heliosdac.rules
	sudo mkdir /media/usbdrive
	```
	
6. Setup networks

	```
	sudo nmcli connection modify "Wired connection 1" ipv4.dhcp-timeout 10
	sudo nmcli connection modify "Wired connection 1" connection.autoconnect-retries 2
	sudo nmcli connection modify "Wired connection 1" connection.autoconnect-priority 2
	sudo nmcli connection modify "Wired connection 1" ipv4.may-fail no
	sudo nmcli connection modify "Wired connection 1" ipv6.method disable
	sudo nmcli connection add type ethernet ifname end0 con-name "Wired connection fallback"
	sudo nmcli connection modify "Wired connection fallback" ipv4.method link-local
	sudo nmcli connection modify "Wired connection fallback" connection.autoconnect-priority 1
	
	sudo nano /etc/udev/rules.d/05-fixMACaddress.rules
	(Add the following line: KERNEL=="end0", ACTION=="add" RUN+="fixEtherAddr %k 06" )
	(Save and exit with ctrl+O, ctrl+X)
	```
	
7. LED control permissions

	```
	sudo chown -R root:laser /sys/devices/platform/leds/leds/rockpis\:blue\:user
	sudo chmod -R ug+rw /sys/devices/platform/leds/leds/rockpis\:blue\:user
	```
	
	
	

(Todo: SPI if needed, use rk3308-spi2-spidev_WORKING.dts)
```
sudo armbian-add-overlay rk3308-spi2-spidev.dts
In /boot/armbianEnv.txt:
param_spidev_spi_bus=2
```