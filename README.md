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

<img width="943" height="815" alt="grafik" src="https://github.com/user-attachments/assets/7d7efa12-b72f-48b7-bc49-30dc0ac442c8" />

and

<img width="953" height="689" alt="grafik" src="https://github.com/user-attachments/assets/e0e203e9-54d0-463d-8092-77d57ad93f57" />

In general it should be possible to respond to other mDNS-Queries (from other vendors)

To get the exact details of mDNS Response it´s necessary to sniff the traffic with wireshark. - Especially for Anker to get the account_id.
To sniff the communication and get the information you have to bring both devices to AP-Side and sniff with Wireshark UDP 5353.
I made the TXT keys and values editable, so maybe there is a chance to use it with other devices maybe...

Under mappings you can now also choose the AP interface to forward UDP-Traffic:
<img width="913" height="787" alt="grafik" src="https://github.com/user-attachments/assets/b3692472-f952-4c0a-af4b-9f4cd7ad318b" />

Please note this only works for UDP-Traffic!

Modules-Page:

I also added a "Modules" Page where you can enable disable the desired modules at boot to safe performance when disabled.

<img width="954" height="851" alt="grafik" src="https://github.com/user-attachments/assets/8168e25f-ac65-4e80-a5db-b9099ce2bf34" />


Code-modifications:
To tell ESP to NAT UDP-Traffic in both directions I had to modify ip4.c and ip4napt.c in the LWIP Module.
I included LWIP Module in the project to have the changes in the project. ESP-IDF v5.2.5 was used for development.

USE THE MAIN BRANCH AS THIS CONTAINS MY VERSION!
