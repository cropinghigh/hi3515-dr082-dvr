from mainvicom_common import *

DRAPI_HOST = "https://myhome.proptech.ru"
DRAPI_USERAGENT = "myHomeErth/8 CFNetwork/1209 Darwin/20.2.0"

DRAPI_OPERATORS = DRAPI_HOST + "/public/v1/operators"
DRAPI_LOGIN = DRAPI_HOST + "/auth/v2/login/%phone"
DRAPI_LOGIN_CONFIRM = DRAPI_HOST + "/auth/v2/confirmation/%phone"
DRAPI_LOGIN_CONFIRM2 = DRAPI_HOST + "/auth/v3/auth/%phone/confirmation"

DRAPI_PROFILES = DRAPI_HOST + "/rest/v1/subscribers/profiles"
DRAPI_PLACES = DRAPI_HOST + "/rest/v3/subscriber-places"
DRAPI_NOTIFICATIONS = DRAPI_HOST + "/rest/v1/subsriberNotifications"
DRAPI_DEVICE_INSTALLATIONS = DRAPI_HOST + "/api/mh-customer-device/mobile/public/v1/customers/device-installations"
DRAPI_AVAILABLE_FEATURES = DRAPI_HOST + "/rest/v1/stomp/available-features"
DRAPI_CAMERAS = DRAPI_HOST + "/rest/v1/places/%place/cameras"
DRAPI_ACCESS_CONTROLS = DRAPI_HOST + "/rest/v1/places/%place/accesscontrols"
DRAPI_PUBLIC_CAMERAS = DRAPI_HOST + "/rest/v2/places/%place/public/cameras"
DRAPI_SIPDEVICES = DRAPI_HOST + "/rest/v1/places/%place/accesscontrols/%acid/sipdevices"
DRAPI_SCREEN_SETTINGS = DRAPI_HOST + "/api/mh-customer/mobile/v1/customers/places/6363975/settings/screens"
DRAPI_SNAPSHOT = DRAPI_HOST + "/rest/v1/places/%place/accesscontrols/%acid/videosnapshots"
DRAPI_GETSTREAM = DRAPI_HOST + "/rest/v1/forpost/cameras/%ecamid/video?LightStream=0&Format=H264"
DRAPI_OPENDOOR = DRAPI_HOST + "/rest/v1/places/%place/accesscontrols/%acid/actions"

import sys
import select
import subprocess
import os
import signal
import time
import json
import pycurl
import re
import io
import CustomPySIP
import CustomPySIP.sip_account
import traceback
import socket
import asyncio
import logging
import uuid
import queue
from CustomPySIP.utils.logger import logger
import logging

class byteFIFO:
    """ byte FIFO buffer """
    def __init__(self):
        self._buf = bytearray()
        self._running = True

    def put(self, data):
        if(self._running):
            self._buf.extend(data)

    def get(self, size):
        if(self._running):
            data = self._buf[:size]
            # The fast delete syntax
            self._buf[:size] = b''
            return data
        return []

    def peek(self, size):
        if(self._running):
            return self._buf[:size]
        return -1

    def getvalue(self):
        # peek with no copy
        if(self._running):
            return self._buf
        return -1

    def __len__(self):
        if(self._running):
            return len(self._buf)
        else:
            return 0

    def isRunning(self):
        return self._running

    def close(self):
        self._running = False

class CustomAudioStream(CustomPySIP.audio_stream.AudioStream):
    def __init__(self) -> None:
        self.input_fifo = byteFIFO()
        self.input_q: queue.Queue = queue.Queue() # Used queue.Queue instead of asyncio.Queue for it's thread-safity
        self.stream_done_future: asyncio.Future = asyncio.Future()
        self.stream_id = str(uuid.uuid4())
        pass

    def __del__(self):
        self.stream_done()

    def send(self, data):
        self.input_fifo.put(data)

    def recv(self):
        logger.log(logging.DEBUG, f"Started stream now - id ({self.stream_id})")
        while self.input_fifo.isRunning():
            if(len(self.input_fifo) >= 160*2):
                frame = self.input_fifo.get(160*2)
                self.input_q.put(frame)
            else:
                time.sleep(0.01)
        self.input_q.put(None)
        if not self.stream_done_future.done():
            self.stream_done_future.set_result("Sream Sent")

    def stream_done(self):
        self.input_fifo.close()

class Domru_Icom_Interface(Icom_Interface):
    auth_data = {}
    operators_data = {}
    sip_account = -1
    sip_status = -1
    current_call = -1
    current_uplink = -1

    def do_login_request(self, url, postdata = ""):
        buff = io.BytesIO()
        c = pycurl.Curl()
        c.setopt(c.URL, url)
        c.setopt(c.POST, (len(postdata) > 0))
        c.setopt(c.ENCODING, "gzip")
        headers = [
            "user-agent: " + DRAPI_USERAGENT,
            "accept-encoding: gzip",
        ]
        if(len(postdata) > 0):
            c.setopt(c.POSTFIELDS, postdata)
            headers.append("content-type: application/json; charset=UTF-8")
            headers.append("content-length: " + str(len(postdata)))
        c.setopt(pycurl.HTTPHEADER, headers)
        c.setopt(c.WRITEDATA, buff)

        c.perform()
        if(c.getinfo(c.RESPONSE_CODE) != 200 and c.getinfo(c.RESPONSE_CODE) != 300):
            print("BAD RESPONSE FOR " + url + ": ", c.getinfo(c.RESPONSE_CODE))
            c.close()
            return -1
        c.close()
        resp = buff.getvalue().decode('utf-8')
        return resp

    def do_request(self, url, postdata = ""):
        buff = io.BytesIO()
        c = pycurl.Curl()
        c.setopt(c.URL, url)
        c.setopt(c.POST, (len(postdata) > 0))
        c.setopt(c.ENCODING, "gzip")
        headers = [
            "user-agent: " + DRAPI_USERAGENT,
            "accept-encoding: gzip",
            "operator: " + str(self.auth_data["operatorId"]),
            "authorization: " + str(self.auth_data["tokenType"]) + " " + str(self.auth_data["accessToken"])
        ]
        if(len(postdata) > 0):
            c.setopt(c.POSTFIELDS, postdata)
            headers.append("content-type: application/json; charset=UTF-8")
            headers.append("content-length: " + str(len(postdata)))
        c.setopt(pycurl.HTTPHEADER, headers)
        c.setopt(c.WRITEDATA, buff)

        c.perform()
        if(c.getinfo(c.RESPONSE_CODE) != 200 and c.getinfo(c.RESPONSE_CODE) != 300):
            print("BAD RESPONSE FOR " + url + ": ", c.getinfo(c.RESPONSE_CODE))
            c.close()
            return -1
        c.close()
        resp = buff.getvalue().decode('utf-8')
        return resp

    def __init__(self):
        resp = self.do_login_request(DRAPI_OPERATORS)
        if(resp == -1):
            print("OPERATOR LIST FETCH ERROR!")
        try:
            resp_json = json.loads(resp)
            if(len(resp_json) < 1 or len(resp_json['data']) < 1):
                print("Wrong response:" + str(resp_json))
            else:
                for operator in resp_json['data']:
                    self.operators_data[operator['id']] = operator
        except json.decoder.JSONDecodeError:
            print("OPERATOR LIST DECODE ERROR!")
        print("Domru icom interface init")

    async def init_main(self, vol):
        self.out_volume = vol
        try:
            with open("domru_auth.json", 'r') as f:
                self.auth_data = json.load(f)
        except FileNotFoundError:
            print("No auth data file! Plz login")
        if("accessToken" not in self.auth_data):
            return -2

        resp = self.do_request(DRAPI_PROFILES)
        if(resp == -1):
            return -2
        try:
            resp_json = json.loads(resp)
        except json.decoder.JSONDecodeError:
            resp_json = []
        if(len(resp_json) < 1 or len(resp_json["data"]) < 1 or "allowAddPhone" not in resp_json["data"]):
            return -2
        print("Allow add phone: " + str(resp_json["data"]["allowAddPhone"]))
        print("Call selected place only: " + str(resp_json["data"]["callSelectedPlaceOnly"]))
        print("Check phone for svc activation: " + str(resp_json["data"]["checkPhoneForSvcActivation"]))
        print("Push user id: " + str(resp_json["data"]["pushUserId"]))
        print("Subscriber id: " + str(resp_json["data"]["subscriber"]["id"]))
        print("Subscriber name: " + str(resp_json["data"]["subscriber"]["name"]))
        print("Subscriber phone id: " + str(resp_json["data"]["subscriberPhones"][0]["id"]))
        print("Subscriber phone: " + str(resp_json["data"]["subscriberPhones"][0]["number"]))
        print("Subscriber phone valid: " + str(resp_json["data"]["subscriberPhones"][0]["numberValid"]))

        resp = self.do_request(DRAPI_PLACES)
        if(resp == -1):
            return -2
        try:
            resp_json = json.loads(resp)
        except json.decoder.JSONDecodeError:
            resp_json = []
        if(len(resp_json) < 1 or len(resp_json["data"]) < 1 or "blocked" not in resp_json["data"][0]):
            return -2
        print("Blocked: " + str(resp_json["data"][0]["blocked"]))
        print("Guard call out: " + str(resp_json["data"][0]["guardCallOut"]))
        print("Places ID: " + str(resp_json["data"][0]["id"]))
        print("Place location: " + str(resp_json["data"][0]["place"]["address"]["visibleAddress"]))
        print("Place ID: " + str(resp_json["data"][0]["place"]["id"]))
        print("Provider: " + str(resp_json["data"][0]["provider"]))
        print("Subscriber type: " + str(resp_json["data"][0]["subscriberType"]))
        self.auth_data["placeId"] = str(resp_json["data"][0]["place"]["id"])

        resp = self.do_request(DRAPI_ACCESS_CONTROLS.replace("%place", self.auth_data["placeId"]))
        if(resp == -1):
            return -2
        try:
            resp_json = json.loads(resp)
        except json.decoder.JSONDecodeError:
            resp_json = []
        if(len(resp_json) < 1 or len(resp_json["data"]) < 1 or "allowCallMobile" not in resp_json["data"][0]):
            return -2

        if(not resp_json["data"][0]["allowCallMobile"]):
            print("No allowCallMobile!")
            return -2
        if(not resp_json["data"][0]["allowOpen"]):
            print("No allowOpen!")
            return -2
        if(not resp_json["data"][0]["allowVideo"]):
            print("No allowVideo!")
            return -2
        print("Ext.cam.id: " + str(resp_json["data"][0]["externalCameraId"]))
        print("Forpost group id: " + str(resp_json["data"][0]["forpostGroupId"]))
        print("Access control id: " + str(resp_json["data"][0]["id"]))
        print("Access control name: " + str(resp_json["data"][0]["name"]))
        if(resp_json["data"][0]["openMethod"] != "ACCESS_CONTROL"):
            print("Unknown openmethod: " + str(resp_json["data"][0]["openMethod"]))
            return -2
        if(resp_json["data"][0]["type"] != "SIP"):
            print("Unknown type: " + str(resp_json["data"][0]["type"]))
            return -2
        self.auth_data["extCamId"] = str(resp_json["data"][0]["externalCameraId"])
        self.auth_data["accCtlId"] = str(resp_json["data"][0]["id"])

        resp = self.do_request(DRAPI_SIPDEVICES.replace("%place", self.auth_data["placeId"]).replace("%acid", self.auth_data["accCtlId"]), "{ \"installationId\": \"AFluffyCutie~\" }")
        if(resp == -1):
            return -2
        try:
            resp_json = json.loads(resp)
        except json.decoder.JSONDecodeError:
            resp_json = []
        if(len(resp_json) < 1 or len(resp_json["data"]) < 1 or "id" not in resp_json["data"]):
            return -2
        self.auth_data["sip"] = resp_json["data"]

        await self.is_call_requested()
        await self.is_call_requested()
        return 0

    def login(self, username):
        resp = self.do_login_request(DRAPI_LOGIN.replace("%phone", username))
        if(resp == -1):
            return -1
        try:
            resp_json = json.loads(resp)
        except json.decoder.JSONDecodeError:
            resp_json = []
        if(len(resp_json) < 1 or len(resp_json[0]) < 1 or "operatorId" not in resp_json[0]):
            return -1
        print("OperatorID: " + str(resp_json[0]["operatorId"]) + "(" + self.operators_data[resp_json[0]["operatorId"]]["dispName"] + ")")
        print("SubscriberID: " + str(resp_json[0]["subscriberId"]))
        print("AccountID: " + str(resp_json[0]["accountId"]))
        print("PlaceID: " + str(resp_json[0]["placeId"]))
        print("Address: " + str(resp_json[0]["address"]))
        print("ProfileID: " + str(resp_json[0]["profileId"]))
        cont = input("Is this correct? [y/n] ")
        if(cont != "y" and cont != "Y"):
            return -1

        resp = self.do_login_request(DRAPI_LOGIN_CONFIRM.replace("%phone", username), json.dumps(resp_json[0]))
        if(resp == -1):
            return -1
        print("SMS was sent to the number")
        code = input("Enter the code: ")
        req_json = {
            "accountId": str(resp_json[0]["accountId"]),
            "confirm1": str(code),
            "confirm2": str(code),
            "login": str(username),
            "operatorId": int(resp_json[0]["operatorId"]),
            "profileId": str(resp_json[0]["profileId"]),
            "subscriberId": str(resp_json[0]["subscriberId"])
        }
        resp = self.do_login_request(DRAPI_LOGIN_CONFIRM2.replace("%phone", username), json.dumps(req_json))
        if(resp == -1):
            return -1
        try:
            resp_json = json.loads(resp)
        except json.decoder.JSONDecodeError:
            resp_json = []
        if(len(resp_json) < 1 or "operatorId" not in resp_json):
            return -1
        print("OperatorName: " + str(resp_json["operatorName"]))
        print("Token type: " + str(resp_json["tokenType"]))
        self.auth_data = resp_json
        with open('domru_auth.json', 'w') as f:
            json.dump(self.auth_data, f)

        return 0 #ok

    def stream_request(self):
        resp = self.do_request(DRAPI_GETSTREAM.replace("%ecamid", self.auth_data["extCamId"]))
        if(resp == -1):
            return -1
        try:
            resp_json = json.loads(resp)
        except json.decoder.JSONDecodeError:
            resp_json = []
        if(len(resp_json) < 1 or len(resp_json["data"]) < 1 or "Error" not in resp_json["data"]):
            return -1
        if(resp_json["data"]["Error"]):
            print("Stream error: " + str(resp_json["data"]["Error"]) + " " + str(resp_json["data"]["ErrorCode"]) + " " + str(resp_json["data"]["Status"]))
            return -1

        return resp_json["data"]["URL"]

    async def stop_calls(self):
        if(self.sip_status == 1 and self.current_call != -1 and self.current_call._rtp_session):
            await self.current_call.call_handler.hangup()
            print("STOPPED!")
            self.sip_status = 0
            return "ok"
        pass

    async def custom_on_call_answered(self, state: CustomPySIP.filters.CallState):
        if(self.current_call != -1):
            if state is CustomPySIP.filters.CallState.ANSWERED:
                # set-up RTP connections
                if not self.current_call.dialogue.local_session_info:
                    CustomPySIP.utils.logger.logger.log(logging.CRITICAL, "No local session info defined")
                    await self.current_call.stop()
                    return
                elif not self.current_call.dialogue.remote_session_info:
                    CustomPySIP.utils.logger.logger.log(logging.CRITICAL, "No remote session info defined")
                    await self.current_call.stop()
                    return

                local_sdp = self.current_call.dialogue.local_session_info
                remote_sdp = self.current_call.dialogue.remote_session_info
                print("Starting RTP session")
                # print("RTP data: " + str(local_sdp) + str(remote_sdp))
                # print("RTP map: " + str(remote_sdp.rtpmap))
                # print("RTP local ip: " + str(local_sdp.ip_address))
                # print("RTP local port: " + str(local_sdp.port))
                # print("RTP remote ip: " + str(remote_sdp.ip_address))
                # print("RTP remote port: " + str(remote_sdp.port))
                # print("RTP ssrc: " + str(local_sdp.ssrc))
                self.current_call._rtp_session = CustomPySIP.rtp_handler.RTPClient(
                    remote_sdp.rtpmap,
                    local_sdp.ip_address,
                    local_sdp.port,
                    remote_sdp.ip_address,
                    remote_sdp.port,
                    CustomPySIP.rtp_handler.TransmitType.SENDRECV,
                    local_sdp.ssrc,
                    self.out_volume,
                    self.current_call._callbacks,
                )

                _rtp_task = asyncio.create_task(self.current_call._rtp_session._start())
                self.current_call._rtp_session._rtp_task = _rtp_task
                self.current_call._register_callback("dtmf_handler", self.current_call._dtmf_handler.dtmf_callback)
                CustomPySIP.utils.logger.logger.log(logging.DEBUG, "Done spawned _rtp_task in the background")

    async def custom_on_call_hanged(self, reason):
        if(self.current_call != -1):
            print("Call hanged")
            if(self.current_uplink != -1):
                self.current_uplink.stream_done()
            # if(self.current_call != -1 and self.current_call._rtp_session):
                # await self.current_call._rtp_session._stop()
            time.sleep(0.01)
            self.current_call = -1
            self.current_uplink = -1
            self.sip_status = 0
            # print("Call hanged finished")

    async def custom_handle_incoming_call(self, call: CustomPySIP.sip_call.SipCall):
        print("CALL!")
        if(self.current_call == -1):
            for cb in call._get_callbacks("state_changed_cb"):
                call._remove_callback("state_changed_cb", cb)
            call._register_callback("state_changed_cb", self.custom_on_call_answered)
            call._register_callback("hanged_up_cb", self.custom_on_call_hanged)
            self.current_call = call
            self.sip_status = 1
        else:
            await call.busy()
            print("BUSY!")

    async def is_call_requested(self):
        if(self.sip_account == -1):
            try:
                ipaddr = socket.gethostbyname(self.auth_data["sip"]["realm"])
                print("Connecting to SIP server at " + self.auth_data["sip"]["realm"] + "...")
                self.sip_account = CustomPySIP.sip_account.SipAccount(
                    self.auth_data["sip"]["login"],
                    self.auth_data["sip"]["password"],
                    self.auth_data["sip"]["realm"],
                    connection_type = "UDP"
                )
            except ConnectionError:
                print("Failed to connect to SIP server")
                self.sip_account = -1
                time.sleep(3.0)
        else:
            if(not self.sip_account._SipAccount__sip_client or not self.sip_account._SipAccount__sip_client.registered.is_set()):
                await self.sip_account.register()
                @self.sip_account.on_incoming_call
                async def handle_incoming_call(call: CustomPySIP.sip_call.SipCall):
                    await self.custom_handle_incoming_call(call)

                if(not self.sip_account._SipAccount__sip_client.registered.is_set()):
                    print("Registration failed!")
                    self.sip_status = -1
                else:
                    print("Registered!")
                    self.sip_status = 0
        return self.sip_status

    def put_audio_data(self, data):
        if(self.current_call != -1 and self.current_uplink != -1):
            self.current_uplink.send(data)

    async def accept_call(self):
        if(self.sip_status == 1 and self.current_call != -1):
            await self.current_call.accept()
            self.current_uplink = CustomAudioStream()
            coroutine = asyncio.to_thread(self.current_uplink.recv)
            asyncio.create_task(coroutine)
            await self.current_call.call_handler.audio_queue.put(("audio", self.current_uplink))
            print("ACCEPT!")
            self.sip_status = 1
            return "ok"
        else:
            return -1

    async def decline_call(self):
        if(self.sip_status == 1 and self.current_call != -1):
            await self.current_call.reject()
            print("DECLINE!")
            self.sip_status = 0
            return "ok"
        else:
            return -1

    def open_door(self):
        resp = self.do_request(DRAPI_OPENDOOR.replace("%place", self.auth_data["placeId"]).replace("%acid", self.auth_data["accCtlId"]), "{\"name\": \"accessControlOpen\"}")
        if(resp == -1):
            return -1
        try:
            resp_json = json.loads(resp)
        except json.decoder.JSONDecodeError:
            resp_json = []
        if(len(resp_json) < 1 or len(resp_json["data"]) < 1 or "errorCode" not in resp_json["data"]):
            return -1
        if(not resp_json["data"]["status"]):
            print("Door open error: " + str(resp_json["data"]["status"]) + " " + str(resp_json["data"]["errorCode"]) + " " + str(resp_json["data"]["errorMessage"]))

