#!/usr/bin/env python3
"""
MFMS Protocol Test Client

Usage:
    python test_client.py [--host HOST] [--port PORT] COMMAND [ARGS...]

Commands:
    snapshot        Request status snapshot
    motion ROBOT CMD  Send motion request to robot
    upload FILE [ROBOT]  Upload file (LUA if ROBOT specified, else QT log)
    log DATE        Request server log for date (YYYY-MM-DD)
"""

import argparse
import socket
import struct
import json
import os
import sys
import time
from typing import Optional, Tuple, BinaryIO

# Protocol constants
MAGIC = b'MFMS'
VERSION = 1
HEADER_SIZE = 24
MAX_PAYLOAD = 32 * 1024 * 1024

# Message types
MSG_STATUS_SNAPSHOT_REQ = 0x0001
MSG_MOTION_REQ = 0x0002
MSG_LUA_UPLOAD_BEGIN = 0x0010
MSG_LUA_UPLOAD_CHUNK = 0x0011
MSG_LUA_UPLOAD_END = 0x0012
MSG_QT_LOG_BEGIN = 0x0020
MSG_QT_LOG_CHUNK = 0x0021
MSG_QT_LOG_END = 0x0022
MSG_SERVER_LOG_REQ = 0x0030

MSG_STATUS_SNAPSHOT_RESP = 0x8001
MSG_MOTION_RESP = 0x8002
MSG_LUA_FORWARD_RESULT = 0x8012
MSG_SERVER_LOG_CHUNK = 0x8030
MSG_SERVER_LOG_END = 0x8031
MSG_ACK = 0xFF00
MSG_ERROR = 0xFFFF


def pack_header(msg_type: int, request_id: int, payload_len: int, flags: int = 0) -> bytes:
    """Pack a frame header."""
    header = bytearray(HEADER_SIZE)
    header[0:4] = MAGIC
    struct.pack_into('<H', header, 4, VERSION)
    struct.pack_into('<H', header, 6, msg_type)
    struct.pack_into('<I', header, 8, flags)
    struct.pack_into('<I', header, 12, request_id)
    struct.pack_into('<I', header, 16, payload_len)
    struct.pack_into('<I', header, 20, 0)  # CRC placeholder
    return bytes(header)


def unpack_header(data: bytes) -> Tuple[int, int, int, int, int, int]:
    """Unpack a frame header. Returns (version, msg_type, flags, request_id, payload_len, crc)."""
    if len(data) < HEADER_SIZE:
        raise ValueError("Header too short")
    if data[0:4] != MAGIC:
        raise ValueError(f"Invalid magic: {data[0:4]}")

    version = struct.unpack_from('<H', data, 4)[0]
    msg_type = struct.unpack_from('<H', data, 6)[0]
    flags = struct.unpack_from('<I', data, 8)[0]
    request_id = struct.unpack_from('<I', data, 12)[0]
    payload_len = struct.unpack_from('<I', data, 16)[0]
    crc = struct.unpack_from('<I', data, 20)[0]

    return version, msg_type, flags, request_id, payload_len, crc


def pack_string(s: str) -> bytes:
    """Pack a length-prefixed string."""
    encoded = s.encode('utf-8')
    return struct.pack('<H', len(encoded)) + encoded


def unpack_string(data: bytes, offset: int) -> Tuple[str, int]:
    """Unpack a length-prefixed string. Returns (string, new_offset)."""
    length = struct.unpack_from('<H', data, offset)[0]
    s = data[offset + 2:offset + 2 + length].decode('utf-8')
    return s, offset + 2 + length


class MfmsClient:
    """MFMS protocol client."""

    def __init__(self, host: str = 'localhost', port: int = 9090):
        self.host = host
        self.port = port
        self.sock: Optional[socket.socket] = None
        self.request_id = 1

    def connect(self):
        """Connect to server."""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        print(f"Connected to {self.host}:{self.port}")

    def close(self):
        """Close connection."""
        if self.sock:
            self.sock.close()
            self.sock = None

    def send_frame(self, msg_type: int, payload: bytes = b'') -> int:
        """Send a frame. Returns request_id."""
        req_id = self.request_id
        self.request_id += 1

        header = pack_header(msg_type, req_id, len(payload))
        self.sock.sendall(header + payload)
        return req_id

    def recv_frame(self) -> Tuple[int, int, bytes]:
        """Receive a frame. Returns (msg_type, request_id, payload)."""
        header = self._recv_exact(HEADER_SIZE)
        version, msg_type, flags, request_id, payload_len, crc = unpack_header(header)

        if payload_len > MAX_PAYLOAD:
            raise ValueError(f"Payload too large: {payload_len}")

        payload = self._recv_exact(payload_len) if payload_len > 0 else b''
        return msg_type, request_id, payload

    def _recv_exact(self, n: int) -> bytes:
        """Receive exactly n bytes."""
        data = bytearray()
        while len(data) < n:
            chunk = self.sock.recv(n - len(data))
            if not chunk:
                raise ConnectionError("Connection closed")
            data.extend(chunk)
        return bytes(data)

    def request_snapshot(self) -> dict:
        """Request status snapshot."""
        req_id = self.send_frame(MSG_STATUS_SNAPSHOT_REQ)
        msg_type, resp_id, payload = self.recv_frame()

        if msg_type == MSG_ERROR:
            code = struct.unpack_from('<H', payload, 0)[0]
            detail, _ = unpack_string(payload, 2)
            raise RuntimeError(f"Error {code}: {detail}")

        if msg_type != MSG_STATUS_SNAPSHOT_RESP:
            raise RuntimeError(f"Unexpected response type: {msg_type:#x}")

        return json.loads(payload.decode('utf-8'))

    def send_motion(self, robot_id: str, command: str, timeout_ms: int = 5000) -> dict:
        """Send motion request."""
        payload = pack_string(robot_id) + pack_string(command) + struct.pack('<I', timeout_ms)
        req_id = self.send_frame(MSG_MOTION_REQ, payload)

        msg_type, resp_id, resp_payload = self.recv_frame()

        if msg_type == MSG_ERROR:
            code = struct.unpack_from('<H', resp_payload, 0)[0]
            detail, _ = unpack_string(resp_payload, 2)
            raise RuntimeError(f"Error {code}: {detail}")

        if msg_type != MSG_MOTION_RESP:
            raise RuntimeError(f"Unexpected response type: {msg_type:#x}")

        # Parse response
        offset = 0
        resp_robot_id, offset = unpack_string(resp_payload, offset)
        success = resp_payload[offset] != 0
        offset += 1
        error_code = struct.unpack_from('<I', resp_payload, offset)[0]
        offset += 4
        result, _ = unpack_string(resp_payload, offset)

        return {
            'robot_id': resp_robot_id,
            'success': success,
            'error_code': error_code,
            'result': result
        }

    def upload_file(self, filepath: str, target_robot: Optional[str] = None) -> bool:
        """Upload a file. If target_robot is set, uploads as LUA, else as QT log."""
        filename = os.path.basename(filepath)
        file_size = os.path.getsize(filepath)
        chunk_size = 65536
        upload_id = int(time.time() * 1000) & 0xFFFFFFFFFFFFFFFF

        # Choose message types
        if target_robot:
            begin_type, chunk_type, end_type = MSG_LUA_UPLOAD_BEGIN, MSG_LUA_UPLOAD_CHUNK, MSG_LUA_UPLOAD_END
        else:
            begin_type, chunk_type, end_type = MSG_QT_LOG_BEGIN, MSG_QT_LOG_CHUNK, MSG_QT_LOG_END
            target_robot = ''

        # Send BEGIN
        begin_payload = (
            struct.pack('<Q', upload_id) +
            struct.pack('<I', file_size) +
            struct.pack('<I', chunk_size) +
            struct.pack('<I', 0) +  # CRC placeholder
            pack_string(filename) +
            pack_string(target_robot)
        )
        req_id = self.send_frame(begin_type, begin_payload)

        msg_type, _, payload = self.recv_frame()
        if msg_type == MSG_ERROR:
            code = struct.unpack_from('<H', payload, 0)[0]
            detail, _ = unpack_string(payload, 2)
            raise RuntimeError(f"Upload begin error {code}: {detail}")

        if msg_type != MSG_ACK:
            raise RuntimeError(f"Unexpected response: {msg_type:#x}")

        print(f"Upload started: {filename} ({file_size} bytes)")

        # Send CHUNKS
        with open(filepath, 'rb') as f:
            offset = 0
            while offset < file_size:
                chunk_data = f.read(chunk_size)
                chunk_payload = (
                    struct.pack('<Q', upload_id) +
                    struct.pack('<I', offset) +
                    struct.pack('<I', 0) +  # CRC placeholder
                    struct.pack('<I', len(chunk_data)) +
                    chunk_data
                )
                self.send_frame(chunk_type, chunk_payload)

                msg_type, _, payload = self.recv_frame()
                if msg_type == MSG_ERROR:
                    code = struct.unpack_from('<H', payload, 0)[0]
                    detail, _ = unpack_string(payload, 2)
                    raise RuntimeError(f"Upload chunk error {code}: {detail}")

                offset += len(chunk_data)
                print(f"  Uploaded {offset}/{file_size} bytes")

        # Send END
        end_payload = (
            struct.pack('<Q', upload_id) +
            struct.pack('<I', file_size) +
            struct.pack('<I', 0)  # CRC placeholder
        )
        self.send_frame(end_type, end_payload)

        msg_type, _, payload = self.recv_frame()
        if msg_type == MSG_ERROR:
            code = struct.unpack_from('<H', payload, 0)[0]
            detail, _ = unpack_string(payload, 2)
            raise RuntimeError(f"Upload end error {code}: {detail}")

        print(f"Upload complete: {filename}")
        return True

    def request_server_log(self, date: str, offset: int = 0, max_size: int = 0) -> str:
        """Request server log."""
        payload = pack_string(date) + struct.pack('<II', offset, max_size)
        self.send_frame(MSG_SERVER_LOG_REQ, payload)

        log_content = b''
        while True:
            msg_type, _, payload = self.recv_frame()

            if msg_type == MSG_ERROR:
                code = struct.unpack_from('<H', payload, 0)[0]
                detail, _ = unpack_string(payload, 2)
                raise RuntimeError(f"Log request error {code}: {detail}")

            if msg_type == MSG_SERVER_LOG_CHUNK:
                log_content += payload
            elif msg_type == MSG_SERVER_LOG_END:
                break
            else:
                raise RuntimeError(f"Unexpected response: {msg_type:#x}")

        return log_content.decode('utf-8')


def main():
    parser = argparse.ArgumentParser(description='MFMS Protocol Test Client')
    parser.add_argument('--host', default='localhost', help='Server host')
    parser.add_argument('--port', type=int, default=9090, help='Server port')
    parser.add_argument('command', choices=['snapshot', 'motion', 'upload', 'log'])
    parser.add_argument('args', nargs='*', help='Command arguments')

    args = parser.parse_args()

    client = MfmsClient(args.host, args.port)
    try:
        client.connect()

        if args.command == 'snapshot':
            result = client.request_snapshot()
            print(json.dumps(result, indent=2))

        elif args.command == 'motion':
            if len(args.args) < 2:
                print("Usage: motion ROBOT_ID COMMAND_JSON")
                sys.exit(1)
            robot_id = args.args[0]
            command = args.args[1]
            result = client.send_motion(robot_id, command)
            print(json.dumps(result, indent=2))

        elif args.command == 'upload':
            if len(args.args) < 1:
                print("Usage: upload FILE [ROBOT_ID]")
                sys.exit(1)
            filepath = args.args[0]
            robot_id = args.args[1] if len(args.args) > 1 else None
            client.upload_file(filepath, robot_id)

        elif args.command == 'log':
            if len(args.args) < 1:
                print("Usage: log DATE")
                sys.exit(1)
            date = args.args[0]
            content = client.request_server_log(date)
            print(content)

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
    finally:
        client.close()


if __name__ == '__main__':
    main()
