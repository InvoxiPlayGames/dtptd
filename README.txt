dtptd - service to handle "DeskTop PassThrough" network connections from
Windows Phone 7, acting as a very lightweight PPP server

WIP, NOT FINISHED, etc etc

MVP checklist:
- [x] Starting connection with device
- [x] PPP link establishment
- [ ] PPP network establishment
- [ ] Port mapping to connect to device / connect device to PC
- [ ] That weird TCP-based DNS server WMZuneComm implements

non-urgent TODOs:
- [ ] Could we use pppd on BSD/macOS/Linux?
- [ ] Clean up usbpcap .pcapng->.pppd tool and add to repo
- [ ] Add example packet captures to docs folder
- [ ] Windows :/

Some documentation on some stuff is in the "docs" folder.

Support for Zune or Windows Mobile 6.x (or other Windows CE variants) is not
currently implemented or planned.
