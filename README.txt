dtptd - service to handle "DeskTop PassThrough" network connections from
Windows Phone 7, acting as a very lightweight PPP server

WIP, NOT FINISHED, etc etc

MVP checklist:
- [x] Starting connection with device
- [x] PPP link establishment
- [x] PPP network establishment
- [x] Networking tunnel to connect device to PC
- [ ] That weird TCP-based DNS server WMZuneComm implements

non-urgent TODOs:
- [ ] Automatic configuration of NAT on supported OS
- [ ] Networking code that doesn't rely on tunneling (aaagh)
- [ ] Multiple device support of some kind
- [ ] Could we use pppd on BSD/macOS/Linux?
- [ ] Clean up usbpcap .pcapng->.pppd tool and add to repo
- [ ] Add example packet captures to docs folder
- [ ] Windows :/

Some documentation on some stuff is in the "docs" folder.

Support for Zune or Windows Mobile 6.x (or other Windows CE variants) is not
currently implemented or planned.

Temporary usage instructions:
- Install libusb and libmtp development packages.
- Build with CMake.
- Run "nc -lvp 64695" in a spare Terminal; the device will disconnect after a
  few seconds without.
- Run "dtptd" as root with your device connected.
- macOS: "sudo route -n add -net 192.168.55.0/24 -interface utunX" with the utun
  value printed by dtptd.
- Set nftables rules or similar for NAT to get access to the outside web.
