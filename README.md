# Yet another ESP32 NAT Router Fork - with mDNS Response and UDP-NATing Support

This is yet another Fork from the great ESP32 NAT Router Project from Martin-ger.

Purpose / Functionality of this fork:

I have a Anker Solarbank 2 and a Anker Smartmeter in place which are too far away located to bring them together in one single WiFi-LAN (Broadcast-Domain).
IoT-Devices like these locate themselves through mDNS (Multicast DNS) which gets not routed through network devices. 
Using WPS-Repeaters (which extends the broadcast-domain) was not an option for me to extend the network. 
Therefore I use the ESP and told the ESP to correctly respond on mDNS-Queries on the AP and STA side to the anker devices.
Furthermore the devices exchange status information via UDP 8899.
I added support to route (NAT-ing) UDP-traffic between AP and STA to the desired destinations.
So it´s possible to have those devices in two different Wireless LANs (Broadcast Domains).

In the mDNS-Proxy configuration page you are able to modify the mDNS-Response details:

<img width="943" height="815" alt="grafik" src="https://github.com/user-attachments/assets/9b3dd98c-f8f0-4269-96d2-45a5823f2dcd" />


and

<img width="953" height="689" alt="grafik" src="https://github.com/user-attachments/assets/65398224-557b-46e1-ba60-5517a64768a5" />


You can react on different query services on each side (AP and STA) if necessary. For example if you have devices from different vendors on each side.
In general it should be possible to respond to other mDNS-Queries (from other vendors)

To get the exact details of mDNS Response it´s necessary to sniff the traffic with wireshark. - Especially for Anker to get the account_id.
To sniff the communication and get the information you have to bring both devices to AP-Side and sniff with Wireshark UDP 5353.
I made the TXT keys and values editable, so there is a chance to use it with other devices maybe...

Under mappings you can now also choose the AP interface to forward UDP-Traffic:
<img width="913" height="787" alt="grafik" src="https://github.com/user-attachments/assets/edd98965-7f19-4cd7-878b-63e4be92683a" />


Please note this only works for UDP-Traffic!

Modules-Page:

I also added a "Modules" Page where you can enable disable the desired modules at boot to safe performance when disabled.

<img width="954" height="851" alt="grafik" src="https://github.com/user-attachments/assets/4915b730-af5b-4602-8fdc-df721f1877a6" />



Code-modifications:
To tell ESP to NAT UDP-Traffic in both directions I had to modify ip4.c and ip4napt.c in the LWIP Module.
I included LWIP Module in the project to have the changes in the project. ESP-IDF v5.2.5 was used for development.
