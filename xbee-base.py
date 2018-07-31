#!/usr/bin/env python3
import os, sys, time, argparse, pprint
import msgpack
import serial
import datetime
from digi.xbee.devices import XBeeDevice

description = """
base station for sensor nodes with xbees.
tries to be column/field name agnostic.
msgpack output format is pretty much just passthrough, ndjson and csv add a timestamp in nanos.
telegraf, and ndjson add the node id, the address of the xbee to a field called "tags", while csv adds these as extra columns.
csv also outputs a header before each metric.
"""


parser = argparse.ArgumentParser(description=description)
parser.add_argument("xbee", help='xbee serial device (usually "/dev/ttyUSB1" or "COM1")')
parser.add_argument("-o", "--output-format",type=str,choices=["csv", "ndjson", "msgpack", "telegraf"], help="output format, if used telegraf tcp must be available on tcp://:8186")
parser.add_argument("-a", "--arduino", help="dont use xbee, just read from serial 9600", action="store_true")
args = parser.parse_args()

if args.output_format == "telegraf":
    import telegraf
    telegraf_client = telegraf.HttpClient(host='localhost', port=8186)
if args.output_format == "ndjson":
    import json

def dispatch(data, address=None, timestamp=None):
    if timestamp is None:
        timestamp = time.time()
    parsed = msgpack.unpackb(data)
    node = parsed.pop(b"node", None)
    sensor_type = parsed.pop(b"stype", None)
    if type(node) is bytes:
        node = node.decode()
    if type(sensor_type) is bytes:
        sensor_type = sensor_type.decode()

    decoded = {k.decode(): v for k,v in parsed.items()}
    for k,v in decoded.items():
        if type(v) is bytes:
            decoded[k] = v.decode()

    tags = {"node": node, "stype": sensor_type}
    tags["address"] = address
    sys.stderr.write("{}: recieved packet from {}-{}\n".format(datetime.datetime.now().isoformat(), node, sensor_type))
    sys.stderr.write(pprint.pformat(decoded)+"\n")
    if args.output_format == "msgpack":
        os.write(sys.stdout.fileno(), data.data)
        return
    if args.output_format == "ndjson":
        tags['timestamp'] = str(int(timestamp*1000000000))
        decoded['tags'] = tags
        sys.stdout.write(json.dumps(decoded)+'\n')
        return
    if args.output_format == "csv":
        tags['timestamp'] = str(int(timestamp*1000000000))
        line = ",".join([str(decoded[k]) for k in sorted(decoded.keys())])

        sys.stdout.write("node,stype,"+",".join(sorted(decoded.keys())))
        sys.stdout.write("\n")
        sys.stdout.write(line)
        sys.stdout.write("\n")
        return
    if args.output_format == "telegraf":
        telegraf_client.metric("sensors", decoded, tags=tags)
        return

def main():
    device = None
    while True:
        try:
            if device is None:
                sys.stderr.write("recreating device...\n")
                if args.arduino:
                    device = serial.Serial(args.xbee, 9600)
                else:
                    try:
                        device = XBeeDevice(args.xbee, 9600)
                        device.open()
                    except Exception as e:
                        device = None
                        sys.stderr.write(str(e)+"\n")
                        time.sleep(10)
                        continue
                time.sleep(1)
            address = None
            if args.arduino:
                data = device.readline().strip()
                timestamp = time.time()
            else:
                data = device.read_data()
                if data is None:
                    time.sleep(0.1)
                    continue

                address = str(data.remote_device.get_64bit_addr())
                timestamp = data.timestamp
                data = data.data


            dispatch(data, address=address, timestamp=timestamp)
        except KeyboardInterrupt:
            if device:
                device.close()
                device = None
            sys.exit(0)
        except Exception as e:
            sys.stderr.write(str(e)+"\n")
            time.sleep(10)
            try:
                device.close()
            except:
                pass
            device = None


if __name__ == "__main__":
    main()
