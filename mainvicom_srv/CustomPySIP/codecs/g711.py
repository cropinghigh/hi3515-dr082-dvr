from abc import ABC, abstractmethod
import audioop
from .base import Decoder, Encoder
import struct


SAMPLE_RATE = 8000
SAMPLE_WIDTH = 2 # 16 bits
SAMPLES_PER_FRAME = 160


class PcmEncoder(ABC, Encoder):
    @staticmethod
    @abstractmethod 
    def _convert(data: bytes, width: int, vol) -> bytes:
        pass

    def encode(self, frame, vol) -> bytes:
        return self._convert(frame, SAMPLE_WIDTH, vol)


class PcmDecoder(ABC, Decoder):
    @staticmethod
    @abstractmethod 
    def _convert(data: bytes, width: int) -> bytes:
        pass

    def decode(self, frame, vol) -> bytes:
        return self._convert(frame, SAMPLE_WIDTH)


class PcmaEncoder(PcmEncoder):
    @staticmethod
    def _convert(data: bytes, width: int, vol) -> bytes:
        if(width != 2):
            print("WRONG WIDTH: " + str(width))
        data = bytearray(data)
        avg = 0
        for i in range(len(data)//2):
            word = data[i] + data[i+1] << 8
            avg += word
        avg = avg // (len(data)//2)
        for i in range(len(data)//2):
            word = data[i] + data[i+1] << 8
            word = (word - avg) * vol
            data[i] = word & 0x00ff
            data[i+1] = word >> 8
        return audioop.lin2alaw(data, width)


class PcmaDecoder(PcmDecoder):
    @staticmethod
    def _convert(data: bytes, width: int) -> bytes:
        return audioop.alaw2lin(data, width)


class PcmuEncoder(PcmaEncoder):
    @staticmethod
    def _convert(data: bytes, width: int, vol) -> bytes:
        if(width != 2):
            print("WRONG WIDTH: " + str(width))
        data = bytearray(data)
        avg = 0
        for i in range(len(data)//2):
            word = struct.unpack('<h', bytearray([data[2*i], data[2*i+1]]))[0]
            avg += word
        avg = avg // (len(data)//2)
        for i in range(len(data)//2):
            # print(bytearray([data[2*i], data[2*i+1]]))
            word = struct.unpack('<h', bytearray([data[2*i], data[2*i+1]]))[0]
            word = int((word - avg) * vol)
            # word = word//2
            # print(word)
            if(word >= 65535//2):
                word = 65535//2
            elif(word <= -65535//2):
                word = -65535//2
            word2 = struct.pack('<h', word)
            data[2*i] = word2[0]
            data[2*i+1] = word2[1]
        #     word = data[i] + data[i+1] << 8
        #     word = int(word * vol)
        #     data[i] = word & 0x00ff
        #     data[i+1] = (word >> 8) & 0x00ff
        return audioop.lin2ulaw(data, width)


class PcmuDecoder(PcmDecoder):
    @staticmethod
    def _convert(data: bytes, width: int) -> bytes:
        return audioop.ulaw2lin(data, width)
