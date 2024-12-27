#!/bin/python

from mainvicom_common import *
from mainvicom_domru import *
from mainvicom_simple import *
import sys
import asyncio
import time
import os
import signal
import string
import re
import datetime

print("Starting mainvicom srv...")

volume_downlink = "5.0"
volume_uplink = 15.0

mainvicom_interface = Domru_Icom_Interface()

ffmpeg_v_proc = -1
ffmpeg_a_proc = -1
local_a_proc = -1
ffmpeg_v_proc_dest = ""

class UdpHandler(asyncio.DatagramProtocol):
    def __init__(self, loop, mvc_ifc) -> None:
        self.transport: Optional[asyncio.DatagramTransport] = None
        self.loop = loop
        self.mvc_ifc = mvc_ifc
        super().__init__()

    def connection_made(self, transport) -> None:
        self.transport = transport

    def connection_lost(self, exc: Exception | None) -> None:
        if self.transport:
            self.transport.close()

    def error_received(self, exc: Exception) -> None:
        print("An error received: " +  str(exc))

    def send_message(self, message) -> None:
        if not self.transport:
            print("Unable to send message due to Transport closed")
            return
        self.transport.sendto(message)

    def datagram_received(self, data: bytes, addr) -> None:
        # self.data_q.put_nowait(data)
        if(self.mvc_ifc):
            self.mvc_ifc.put_audio_data(data)


def stop_proc():
    global ffmpeg_v_proc
    global ffmpeg_a_proc
    global local_a_proc
    if(ffmpeg_v_proc != -1):
        for proc in ffmpeg_v_proc:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            time.sleep(0.01)
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        ffmpeg_v_proc = -1
        print("Killed v ffmpeg")
    if(ffmpeg_a_proc != -1):
        os.killpg(os.getpgid(ffmpeg_a_proc.pid), signal.SIGTERM)
        os.killpg(os.getpgid(ffmpeg_a_proc.pid), signal.SIGTERM)
        os.killpg(os.getpgid(ffmpeg_a_proc.pid), signal.SIGTERM)
        os.killpg(os.getpgid(ffmpeg_a_proc.pid), signal.SIGTERM)
        time.sleep(0.01)
        os.killpg(os.getpgid(ffmpeg_a_proc.pid), signal.SIGKILL)
        ffmpeg_a_proc = -1
        print("Killed a ffmpeg")
    if(local_a_proc != -1):
        local_a_proc.transport.close()
        local_a_proc = -1
        print("Killed a stream")

def stop_ffmpeg_a_proc():
    global ffmpeg_a_proc
    if(ffmpeg_a_proc != -1):
        os.killpg(os.getpgid(ffmpeg_a_proc.pid), signal.SIGTERM)
        os.killpg(os.getpgid(ffmpeg_a_proc.pid), signal.SIGTERM)
        os.killpg(os.getpgid(ffmpeg_a_proc.pid), signal.SIGTERM)
        os.killpg(os.getpgid(ffmpeg_a_proc.pid), signal.SIGTERM)
        time.sleep(0.01)
        os.killpg(os.getpgid(ffmpeg_a_proc.pid), signal.SIGKILL)
        ffmpeg_a_proc = -1
        print("Killed a ffmpeg")


def start_ffmpeg_av_proc(source, dest):
    global ffmpeg_v_proc
    global ffmpeg_v_proc_dest
    if(ffmpeg_v_proc == -1):
        ffmpeg_v_proc = []
        ffmpeg_v_proc_dest = dest
        ffmpeg_v_proc_a = subprocess.Popen(["bash","-c","ffmpeg -hide_banner -loglevel error -re -probesize 65536 -analyzeduration 65536 -fflags nobuffer -flags low_delay -i " + source + " -r 6 -vcodec mpeg2video -filter:v \"scale=640x480\" -bufsize 128k -maxrate 4000k -qscale:v 2 -fflags nobuffer -flags low_delay -avioflags direct -an -f mpegts udp://" + dest + ":8101?pkt_size=1024 -vn -filter:a \"volume=" + volume_downlink + "\" -acodec pcm_s16le -ac 1 -ar 8000 -f s16le udp://127.0.0.1:8105?pkg_size=1024"], preexec_fn=os.setsid)
        ffmpeg_v_proc_b = subprocess.Popen(["bash","-c","ffmpeg -hide_banner -loglevel error -re -probesize 65536 -analyzeduration 65536 -fflags nobuffer -flags low_delay -f s16le -ac 1 -ar 8000 -i udp://127.0.0.1:8103?listen=1 -fflags nobuffer -flags low_delay -avioflags direct -acodec pcm_s16le -ac 1 -ar 8000 -f s16le udp://" + dest + ":8104?pkt_size=1024"], preexec_fn=os.setsid)
        ffmpeg_v_proc = [ffmpeg_v_proc_a, ffmpeg_v_proc_b]
        print("Started v ffmpeg")

def start_ffmpeg_a_proc_fromvideo(source, dest):
    global ffmpeg_a_proc
    if(ffmpeg_a_proc == -1):
        ffmpeg_a_proc = 1
        ffmpeg_a_proc = subprocess.Popen(["bash","-c","ffmpeg -hide_banner -loglevel error -re  -probesize 65536 -analyzeduration 65536 -fflags nobuffer -flags low_delay -f s16le -ac 1 -ar 8000 -i udp://127.0.0.1:8105?listen=1 -fflags nobuffer -flags low_delay -avioflags direct -acodec pcm_s16le -ac 1 -ar 8000 -f s16le udp://127.0.0.1:8103?pkt_size=1024"], preexec_fn=os.setsid)
        print("Started a fromvideo ffmpeg")

def start_ffmpeg_a_proc_fromfile(source, dest):
    global ffmpeg_a_proc
    if(ffmpeg_a_proc == -1):
        ffmpeg_a_proc = 1
        ffmpeg_a_proc = subprocess.Popen(["bash","-c","ffmpeg -hide_banner -loglevel error -re -stream_loop -1 -i " + source + " -fflags nobuffer -flags low_delay -avioflags direct -acodec pcm_s16le -ac 1 -ar 8000 -f s16le udp://127.0.0.1:8103?pkt_size=1024"], preexec_fn=os.setsid)
        print("Started a fromfile ffmpeg")

async def start_local_a_proc(source, dest):
    global local_a_proc
    if(local_a_proc == -1):
        local_a_proc = 1
        loop = asyncio.get_event_loop()
        transport, protocol = await loop.create_datagram_endpoint(
            lambda: UdpHandler(loop, dest),
            local_addr=('0.0.0.0', 8102)
        )
        local_a_proc = protocol
        print("Started a stream")



async def has_data(reader: asyncio.StreamReader) -> bool:
    try:
        data = await asyncio.wait_for(reader._wait_for_data('has_data'), timeout=0.1)
        return True
    except asyncio.TimeoutError:
        return False

async def autopendel_ffmpeg():
    global ffmpeg_v_proc
    global ffmpeg_a_proc
    print("FFMPEG Autopendel started")
    while(1):
        if(ffmpeg_v_proc != -1):
            fall = False
            for proc in ffmpeg_v_proc:
                if(proc.poll() != None):
                    print("FFMPEG V PROC FALLED! RESTARTING...")
                    fall = True
            if(fall):
                for proc in ffmpeg_v_proc:
                    proc.poll()
                    if(proc.returncode == None):
                        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
                        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
                        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
                        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
                        time.sleep(0.01)
                        os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
                print("Killed ffmpeg v proc")

                r = mainvicom_interface.stream_request()
                if(r != -1):
                    ffmpeg_v_proc = -1
                    start_ffmpeg_av_proc(r, ffmpeg_v_proc_dest)
                # print("Retarting ffmpeg v procs with args: " + str(ffmpeg_v_proc[0].args) + " ; " + str(ffmpeg_v_proc[1].args))
                # ffmpeg_v_proc_a = subprocess.Popen(ffmpeg_v_proc[0].args, preexec_fn=os.setsid)
                # ffmpeg_v_proc_b = subprocess.Popen(ffmpeg_v_proc[1].args, preexec_fn=os.setsid)
                # ffmpeg_v_proc = [ffmpeg_v_proc_a, ffmpeg_v_proc_b]
                # print("Restarted v ffmpeg")
        if(ffmpeg_a_proc != -1):
            if(ffmpeg_a_proc.poll() != None):
                print("FFMPEG A PROC FALLED! RESTARTING...")

                print("Retarting ffmpeg a proc with args: " + str(ffmpeg_a_proc.args))
                ffmpeg_a_proc = subprocess.Popen(ffmpeg_a_proc.args, preexec_fn=os.setsid)
                print("Restarted a ffmpeg")
        await asyncio.sleep(0.5)

async def handle_client(reader, writer):
    # Get client's address
    addr = writer.get_extra_info('peername')
    print(f'Got connection from {addr}')

    await mainvicom_interface.stop_calls()
    stop_proc()

    writer.write(b"e\n") #stop all streams
    await writer.drain()

    time.sleep(0.1)

    writer.write(b"s0\n") #set status to no internet
    await writer.drain()
    print("Sending s0")

    time.sleep(0.1)

    prev_status = -1
    printables = string.printable
    printables = printables.replace("\n", "").replace("\r", "")

    keepalive_timer = datetime.datetime.now()
    keepalive_ctr = 0

    while(not writer.is_closing() and not reader.at_eof()):
        if(await has_data(reader)):
            rawdata = (await reader.readline())
            data = rawdata.decode()
            data = "".join(filter(lambda x: x in printables, data))
            if(data == "r"):
                print("r")
                if(prev_status != -1):
                    r = mainvicom_interface.stream_request()
                    if(r != -1):
                        print("Starting v stream from " + r)
                        start_ffmpeg_av_proc(r, addr[0])
                        writer.write(b"v\n") #start video stream
                        await writer.drain()
                    if(prev_status == 0):
                        #no request, loop audio from video stream
                        start_ffmpeg_a_proc_fromvideo("127.0.0.1", "127.0.0.1")
                    elif(prev_status == 1):
                        #play ringtone
                        start_ffmpeg_a_proc_fromfile("./ring.wav", "127.0.0.1")
            elif(data == "a"):
                print("a")
                if(prev_status == 1):
                    r = await mainvicom_interface.accept_call()
                    if(r == -1):
                        writer.write(b"s1\n") #set status to no requests
                        await writer.drain()
                    else:
                        print("Starting audio stream to " + r)
                        stop_ffmpeg_a_proc()
                        start_ffmpeg_a_proc_fromvideo("127.0.0.1", "127.0.0.1")
                        # start_ffmpeg_a_proc(addr[0], r)
                        await start_local_a_proc(addr[0], mainvicom_interface)
                        time.sleep(0.5)
                        writer.write(b"a\n") #start audio stream
                        await writer.drain()
            elif(data == "o"):
                print("o")
                mainvicom_interface.open_door()
            elif(data == "d"):
                print("d")
                await mainvicom_interface.stop_calls()
                if(prev_status == 1):
                    await mainvicom_interface.decline_call()
                stop_proc()
            elif(data == "p"):
                #ping
                if(keepalive_ctr < 0):
                    keepalive_ctr = 0
                keepalive_ctr += 1
            else:
                print(f"Unknown data: {rawdata} ({data})")
        new_status = await mainvicom_interface.is_call_requested()
        if(new_status != prev_status):
            if(new_status == -1):
                stop_proc()
                writer.write(b"s0\n") #set status to no internet
                await writer.drain()
                print("Sending s0")
            if(new_status == 0):
                stop_proc()
                writer.write(b"s1\n") #set status to no requests
                await writer.drain()
                print("Sending s1")
            if(new_status == 1):
                writer.write(b"s2\n") #set status to requested
                await writer.drain()
                print("Sending s2")
            prev_status = new_status
        if((datetime.datetime.now() - keepalive_timer).total_seconds() >= 5):
            writer.write(b"p\n") #ping
            keepalive_ctr
            await writer.drain()
            if(keepalive_ctr <= -5):
                print("TIMEOUT!")
                writer.close()
            keepalive_ctr -= 1
            keepalive_timer = datetime.datetime.now()

    await mainvicom_interface.stop_calls()
    stop_proc()

    # Close the connection
    print(f"Close the connection with {addr}")
    writer.close()

async def main():
    if(len(sys.argv) > 1):
        if(len(sys.argv) == 3 and sys.argv[1] == "login"):
            r = mainvicom_interface.login(sys.argv[2])
            if(r == -1):
                print("Login failed!")
            else:
                print("Login success!")
            exit()
        else:
            print("Wrong args!")
            exit()

    r = await mainvicom_interface.init_main(volume_uplink)
    if(r == -2):
        print("Login required!")
        exit(1)

    server = await asyncio.start_server(
        handle_client, '0.0.0.0', 8100
    )

    task = asyncio.create_task(autopendel_ffmpeg())

    addr = server.sockets[0].getsockname()
    print(f'Listening on {addr}')

    async with server:
        await server.serve_forever()
    
    stop_proc()
    await task

async def intr():
    print("CTRL+C CAUGHT! STOPPING!")
    stop_proc()
    

loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)
try:
    loop.run_until_complete(main())
except KeyboardInterrupt:
    loop.run_until_complete(intr())
finally:
    print('Program finished')
