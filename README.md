hhod
====

Home Heartbeat Online Daemon

hhod is a Linux TCP gateway daemon for the FTDI Interface for the Eaton Home heartbeat base station.

hhod stands for homeheartbeat online daemon

## How setup up the Linux FTDI Serial Driver
You will need to have your hh USB device plugged in
Copy udev rules file `10-home-heartbeat.rules`  to `/etc/udev/rules.d/10-home-heartbeat.rules`
Confirm device is loaded `ls /dev/ttyUSB*`

## How to build

    make



## How to Start
```
    
    ./hhod

```

## How to connect 

connect using netcat
 
    nc localhost 1098


## Commands
Enter ? to see standard list of commands

Additionally you can enter

    raw=on 
to see an unformatted state table

    raw=off 
will display formatted state table with the following fields
    STATE_RECORD_ID,
    DEVICE_NAME,
    DEVICE_TYPE,
    DEVICE_STATE,
    DEVICE_STATE_TIMER,
    ALIVE_UPDATE_TIMER,
    DEVICE_ALERTS, 
    DEVICE_CONFIGURATION,
    MAC_ADDRESS

This software is not approved, supported, was was written without permission from EATON.
The use of the trademark is also used without permission.

All information was obtained from the following sites
  * http://buzzdavidson.com/
  * http://www.kolinahr.com/
  * http://bloominglabs.org/index.php/Eaton_Home_Heartbeat

Some source code was used from the mochad project
http://sourceforge.net/projects/mochad/

as well as from stackoverflow
http://stackoverflow.com/
