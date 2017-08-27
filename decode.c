/*
 * This file is part of hhod.
 *
 * hhod is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * hhod is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * For a copy of the GNU General Public License
 * see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include "global.h"
#include "decode.h"

/***************************************************************************************************
*Output field format:
* STATE="F1,F2,F3,F4,F5,F6,F7,F8,F9,F10,F11,F12,F13,F14,F15,F16,F17"
* STATE="02,00,0040,0002,00,12,00,00,0301,12,0000,00,FF,00000000,00,000D6F0000011367,Home Key"
*/
#define	MAX_FIELD  			(17) // NUMBER OF FIELDS IN STATE TABLE

#define STATE_RECORD_ID      (0) //number associated with each device as it's associated with the base station
#define ZIGBEE_BINDING_ID    (1) //value is an index into the ZigBee Binding Table
#define DEVICE_CAPABILITIES  (2)
#define DEVICE_TYPE          (3)
#define DEVICE_STATE         (4) //contains the last reported state of the device.
#define DEVICE_STATE_TIMER   (5) //elapsed time since the device state has changed
#define DEVICE_ALERTS        (6)
#define DEVICE_NAME_INDEX    (7)
#define DEVICE_CONFIGURATION (8)
#define ALIVE_UPDATE_TIMER   (9) //last seen timer
#define UPDATE_FLAGS         (10)
//#define UNDEFINED            (11)
#define DEVICE_PARAMETER     (12) //user selectable configuration value: Power, Reminder, and Motion.
//#define UNDEFINED            (13)
#define PENDING_UPDATE_TIMER (14) //elapsed time since the user changed the device configuration parameter defined in Field 12
#define MAC_ADDRESS          (15)
#define DEVICE_NAME          (16)

//Field 3 Device Type
static const struct DeviceConst DevType[] = {
	{ "0000", "Unknown Device" },
	{ "0001", "Base Station" },
	{ "0002", "Home Key" },
	{ "0003", "Open/Closed" },
	{ "0004", "Power Sensor" },
	{ "0005", "Water Sensor" },
	{ "0006", "Reminder Sensor" },
	{ "0007", "Attention Sensor" },
	{ "0008", "Water Valve" },
	{ "0009", "Range Extender" },
	{ "0010", "Modem" },
	{ "0017", "Motion Sensor" },
	{ "0018", "Tilt Sensor" },
};

//Field 4 Device State
static const struct DeviceConst DevState[] = {
	{ "00", "none" },
	{ "01", "closed" },
	{ "02", "open" },
	{ "03", "okay" },
};



//Field 6 Device Alerts
static const struct DeviceConst DevAlert[] = {
	{ "00", "normal" },
	{ "01", "alarm" },
	{ "02", "offline" },
	{ "04", "battery low" },
	{ "05", "alert 05"},
	{ "06", "alert 06"},
	{ "07", "alert 07"},
	{ "08", "battery charging" },
	{ "09", "alert 09"},
	{ "0A", "alert 0A"},
	{ "0B", "alert 0B"},
	{ "0C", "alert 0C"},
	{ "0D", "alert 0D"},
	{ "FF", "waiting for update" }
};



// Field 7 Device Name Index - This is an index into a predefined list of available names specific
 //to the device type, also Field 17 contains the complete name retrieved out of the index.

//Field 8 Device Configuration
static const struct DeviceConst DevConfig[] = {
	{ "0000", "none" },
	{ "0001", "alarm1" },
	{ "0002", "alarm2" },
	{ "0003", "alarm3" },
	{ "0004", "alarm4" },
	{ "0100", "callme1" },
	{ "0200", "callme2" },
	{ "0300", "callme3" },
	{ "0400", "callme4" },
	{ "0101", "alarm1|callme1" },
	{ "0102", "alarm2|callme1" },
	{ "0103", "alarm3|callme1" },
	{ "0104", "alarm4|callme1" },
	{ "0201", "alarm1|callme2" },
	{ "0202", "alarm2|callme2" },
	{ "0203", "alarm3|callme2" },
	{ "0204", "alarm4|callme2" },
	{ "0301", "alarm1|callme3" },
	{ "0302", "alarm2|callme3" },
	{ "0303", "alarm3|callme3" },
	{ "0304", "alarm4|callme3" },
	{ "0401", "alarm1|callme4" },
	{ "0402", "alarm2|callme4" },
	{ "0403", "alarm3|callme4" },
	{ "0404", "alarm4|callme4" },
	{ "0501", "alarm4|callme4" },
};

//******************************************************************************************************

char *devTypeLookup(char *device)
{
	int i;
	for (i = 0; DevType[i].devcode; i++)
	{
		if (strcmp(device, DevType[i].devcode) == 0)
			return DevType[i].name;
	}
	return DevType[0].name;
}


char *devStateLookup(char *device)
{
	int i;
	for (i = 0; DevState[i].devcode; i++)
	{
		if (strcmp(device, DevState[i].devcode) == 0)
			return DevState[i].name;
	}
	return DevState[0].name;
}

char *devAlertLookup(char *device)
{
	int i;
	for (i = 0; DevAlert[i].devcode; i++)
	{
		if (strcmp(device, DevAlert[i].devcode) == 0)
			return DevAlert[i].name;
	}
	return DevAlert[0].name;
}

char *devConfigLookup(char *device)
{
	int i;
	for (i = 0; DevConfig[i].devcode; i++)
	{
		if (strcmp(device, DevConfig[i].devcode) == 0)
			return DevConfig[i].name;
	}
	return device;
}

char *clean_device_name(char *name)
{
	char *p = name;
	while (*p && *p != '\n' && *p != '\r' && *p != '\"')
		p++;
	*p = '\0';
	return name;
}

//return time in seconds
int decodeTimer(char *timer)
{
	int value;
	if (1 == sscanf(timer, "%x", &value)) {
		if (value < 64){
			return value; //seconds
		}
		else if (value < 128){
			return (value - 64) * 60;  //minutes
		}
		else if (value < 160){
			return (value - 128) * 3600; //hours
		}
		else {
			return (value - 160) * 3600 * 24; //days
		}
	}
	return 0;
}


int decode_state(char *aLine)
{
	if (raw_data) {
		sockprintf(-1, aLine);
		return 1;
	}

	char *command;
	command = strtok(aLine, "\"");
	if (command) {
		if (strcmp(command, "STATE=") == 0) {


			char* fields[MAX_FIELD];
			char* p = NULL;
			int i = 0;

			p = strtok(NULL, ",");
			while (p && i < MAX_FIELD)
			{
				fields[i] = malloc(strlen(p) + 1);
				strcpy(fields[i], p);
				i++;
				p = strtok(NULL, ",");
			}

			int z = 0;

			if (i >= 16)
			{
				char *name;
				char *mac;
				if (i <= DEVICE_NAME)
				{
					//base station and modem do not have names
					name = devTypeLookup(fields[DEVICE_TYPE]);
					mac = "0\0";
				}
				else
				{
					name = clean_device_name(fields[DEVICE_NAME]);
					mac = fields[MAC_ADDRESS];
				}

				sockprintf(-1, "%s,%s,%s,%s,%d,%d,%s,%s,%s\n",
					fields[STATE_RECORD_ID],
					name,
					devTypeLookup(fields[DEVICE_TYPE]),
					devStateLookup(fields[DEVICE_STATE]),
					decodeTimer(fields[DEVICE_STATE_TIMER]),
					decodeTimer(fields[ALIVE_UPDATE_TIMER]),
					devAlertLookup(fields[DEVICE_ALERTS]),
					fields[DEVICE_CONFIGURATION],
					mac
					);
			}


			//cleanup
			for (z = 0; z < i; z++)
			{
				if (fields[z])
					free(fields[z]);
			}
			return 1;
		}
		else
		{
			sockprintf(-1, aLine);
			return 1;
		}
	}
	return 0;
}
