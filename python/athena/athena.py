#! /usr/bin/python
import struct
from threading import Thread
import sys
import os
import socket
import urllib
import time
import json

# Athena Protobuf import (autogenerated)
import queryMessage_pb2

class Athena(object):

    def __init__(self):
        self.dataNotUsed = []
        self.conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.connected = False

    def connect(self, host='localhost', port=55555):
        self.conn.connect((host, port))
        self.connected = True

    def disconnect(self):
        self.conn.close()
        self.connected = False

    # Recieves a json struct as a string
    def query(self, query_JSON, img_array = []):

        if not self.connected:
            return "NOT CONNECTED"

        quer = queryMessage_pb2.queryMessage()
        # quer has .json and .blob
        quer.json = query_JSON

        for im in img_array:
            quer.blobs.extend(im)

        # Serialize with protobuf and send
        # start = time.time()
        data = quer.SerializeToString();
        # start = time.time()
        sent_len = struct.pack('@I', len(data)) # send size first
        self.conn.send( sent_len )
        self.conn.send(data)
        # end = time.time()
        # print "ATcomm[ms]:" + str((end - start)*1000)

        # time.sleep(1)

        # Recieve response
        recv_len = self.conn.recv(4)
        recv_len = struct.unpack('@I', recv_len)[0]
        response = ''
        while len(response) < recv_len:
            packet = self.conn.recv(recv_len - len(response))
            if not packet:
                return None
            response += packet

        querRes = queryMessage_pb2.queryMessage()
        querRes.ParseFromString(response)

        img_array = []
        for b in querRes.blobs:
            img_array.append(b)

        return (querRes.json, img_array)

    # Recieves json object
    def queryJSONObj(self, json_obj, img_array = []):

        parsed = json.loads(json_obj)
        str_query = json.dumps(parsed, indent=4, sort_keys=False)
        return self.query(str_query, img_array)
