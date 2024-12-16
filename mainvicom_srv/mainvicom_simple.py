from mainvicom_common import *

import sys
import select
import subprocess
import os
import signal
import time

class Simple_Icom_Interface(Icom_Interface):
    lastx = ""
    aplay_proc = -1

    def __init__(self):
        print("Simple icom interface init")

    def init_main(self):
        return 0

    def login(self, username):
        return 0 #ok

    def stop_calls(self):
        if(self.aplay_proc != -1):
            os.killpg(os.getpgid(self.aplay_proc.pid), signal.SIGTERM)
            os.killpg(os.getpgid(self.aplay_proc.pid), signal.SIGTERM)
            os.killpg(os.getpgid(self.aplay_proc.pid), signal.SIGTERM)
            os.killpg(os.getpgid(self.aplay_proc.pid), signal.SIGTERM)
            time.sleep(0.5)
            os.killpg(os.getpgid(self.aplay_proc.pid), signal.SIGKILL)
            self.aplay_proc = -1
        pass

    def stream_request(self):
        print("stream_request")
        return "./testvideo.mp4"

    def is_call_requested(self):
        if(select.select([sys.stdin], [], [], 0.01)[0] != []):
            self.lastx = input()

        if(self.lastx == "r"):
            return 1
        elif(self.lastx == "n"):
            return 0
        else:
            return -1

    def accept_call(self):
        if(self.aplay_proc == -1):
            self.aplay_proc = subprocess.Popen(["bash","-c","ffmpeg -probesize 32768 -analyzeduration 32768 -fflags nobuffer -flags low_delay -re -f s16le -ac 1 -ar 16000 -avioflags direct -i udp://:8103?listen=1 -fflags nobuffer -flags low_delay -avioflags direct -f s16le -acodec pcm_s16le - | aplay -f S16_LE -r 16000 -c 1 -"], preexec_fn=os.setsid)

        return "udp://127.0.0.1:8103?pkt_size=1024"

    def decline_call(self):
        print("decline_call")
        lastx = "n"

    def open_door(self):
        print("open_door")
