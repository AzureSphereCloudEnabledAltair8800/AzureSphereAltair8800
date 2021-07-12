#!/usr/bin/env python
# encoding: utf-8
import os
from pathlib import Path
import struct
import paho.mqtt.client as mqtt
import time
from random import randint, seed
from random import random
import sys
import getopt

mqtt_broker_url = "<REPLACE_WITH_YOUR_MQTT_BROKER_URL>"

vdiskReadMqttTopic = "altair/+/vdisk/read"
vdiskWriteMqttTopic = "altair/+/vdisk/write"
response = ""

channels = dict()
disk_dirty = dict()


def on_connect(client, userdata, flags, rc):
    print("Connected with result code: %s" % rc)
    client.subscribe([(vdiskReadMqttTopic, 0), (vdiskWriteMqttTopic, 0)])


def on_disconnect(client, userdata, rc):
    print("Disconnected with result code: %s" % rc)


def on_message(client, userdata, msg):
    global channels, disk_dirty

    # get channel number from the topic
    channel_id = msg.topic.split('/')[1]
    current_channel = channels.get(channel_id)

    # If new channel then load up disk image into dictionary for the channel
    if current_channel is None:
        print("New channel for {channel_id}".format(channel_id=channel_id))
        channels[channel_id] = None
        disk_dirty[channel_id] = False

        disk = str(channel_id) + '.dsk'

        if not Path(disk).is_file():
            disk = 'Empty.dsk'

            if not Path(disk).is_file():
                print("disk not found")
                exit()

        # disk = 'cpm63k.dsk'

        fp = open(disk, "rb")
        data = fp.read()
        channels[channel_id] = bytearray(data)
        fp.close()

    if (msg.topic.endswith("read")):
        s = struct.Struct('I')

        unpacked_data = s.unpack(msg.payload)
        sectorOffset = unpacked_data[0]

        response_topic = msg.topic[:-4] + "data"
        sector = channels[channel_id][sectorOffset:sectorOffset + 137]

        # the first 4 bytes are the requested sector number
        out_packet = struct.pack('I137s', sectorOffset, sector)

        client.publish(response_topic, out_packet)      # 4 + 137 == sector size)

        return

    if (msg.topic.endswith("write")):
        unpacked_data = struct.unpack("I", msg.payload[:4])
        sectorOffset = unpacked_data[0]
        chunk = msg.payload[4:]

        # print("Write Offset {offset} for channel {channel_id}".format(offset = sectorOffset, channel_id = channel_id))
        channels[channel_id][sectorOffset: sectorOffset + len(chunk)] = chunk
        disk_dirty[channel_id] = True


def save_disk():
    global channels, disk_dirty
    for channel_id in disk_dirty:
        if disk_dirty[channel_id]:
            print("Backing up disk for channel id: " + str(channel_id))
            fp = open(str(channel_id) + '.dsk', "wb")
            data = fp.write(channels[channel_id])
            fp.close()
            disk_dirty[channel_id] = False


def memoryCRC(diskData):
    memCRC = 0
    for val in diskData:
        memCRC = memCRC+val
        memCRC = memCRC & 0xffff
    print('memCRC : ', hex(memCRC))


def main(argv):
    global mqtt_broker_url, vdiskReadMqttTopic, vdiskWriteMqttTopic

    shared_mode = False
    channel_set = False
    broker_set = False
    channel_id = None

    try:
        opts, args = getopt.getopt(argv, "hsc:b:", ["channel=", "broker="])
    except getopt.GetoptError:
        print('test.py -c <channel> -s')
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print('PyDiskSvr.py -b <mqtt broker url> -c <channel> -s <shared mode>')
            sys.exit()
        elif opt in ("-b", "--broker"):
            mqtt_broker_url = arg
            broker_set = True
        elif opt in ("-c", "--channel"):
            channel_id = arg.strip()
            channel_set = True
        elif opt in ("-s", "--shared"):
            shared_mode = True

    if not broker_set:
        print("ERROR: Missing MQTT Broker URL")
        sys.exit()

    if channel_set == True and shared_mode == True:
        print("ERROR: You cannot set a Channel ID AND enable shared mode together")
        sys.exit()

    if channel_set == False and shared_mode == False:
        print("ERROR: You must specify either Channel ID or Shared Mode")
        sys.exit()

    if channel_set and not channel_id.isnumeric():
        print("ERROR: Channel ID must be numeric")
        sys.exit()

    if channel_set and len(channel_id) > 8:
        print("ERROR: Channel ID must be less than 9 chanacters")
        sys.exit()

    if channel_set:
        vdiskReadMqttTopic = "altair/{channel_id}/vdisk/read".format(channel_id=channel_id)
        vdiskWriteMqttTopic = "altair/{channel_id}/vdisk/write".format(channel_id=channel_id)

    print("MQTT Broker URL: {mqtt_broker_url}".format(mqtt_broker_url=mqtt_broker_url))
    print("Channel ID: {channel_id}".format(channel_id=channel_id))
    print("Shared Mode: {shared_mode} ".format(shared_mode=shared_mode))
    print("Read Topic: ", vdiskReadMqttTopic)
    print("Write topic: ", vdiskWriteMqttTopic)


if __name__ == "__main__":
    main(sys.argv[1:])

    print("Altair Virtual Disk Server")

    seed(1)

    mqtt_client_id = 'altrairvdisk{random_number}'.format(
        random_number=randint(10000, 99999))

    client = mqtt.Client(mqtt_client_id, mqtt.MQTTv311)

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    client.connect(mqtt_broker_url)

    client.loop_start()

    while True:  # sleep forever
        time.sleep(30)
        save_disk()
