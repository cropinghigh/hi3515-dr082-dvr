
class Icom_Interface:
    def __init__(self):
        print("Default icom interface init")

    async def init_main(self, vol):
        print("Default icom interface init_main")
        return 0 #ok
        return -2 #login required

    def login(self, username):
        print("Default icom interface login")
        return 0 #ok
        return -1 #failed

    async def stop_calls(self):
        print("Default icom interface stop_calls")

    def stream_request(self):
        print("Default icom interface stream_request")
        return -1 #not allowed
        return "rtsp://gowno.mocha/123" #ok, returning link to the stream for ffmpeg

    async def is_call_requested(self):
        print("Default icom interface is_call_requested")
        return 0 #no requests
        return 1 #requested
        return -1 #network error

    async def accept_call(self):
        print("Default icom interface accept_call")
        return -1 #failed
        return "rtsp://127.0.0.1:2228" #ok, returning stream link of audio from home to street

    def put_audio_data(self, data):
        print("Default icom interface put_audio_data")

    async def decline_call(self):
        print("Default icom interface decline_call")

    def open_door(self):
        print("Default icom interface open_door")
    
