#!/usr/bin/env python3
import urllib
import urllib.request
import json
import time

omega = 'omega-87db.local'
port = 80
user = 'rgb'
password = 'RGBd_*CLX123'


seq_num_max = 0x7fffffff
session_id_stub = '00000000000000000000000000000000'


class JSONRPCError(Exception):
    """Exception raised for errors in JSON-RPC.

    Attributes:
        code -- code of the error
        message -- explanation of the error
    """

    def __init__(self, code=0, message="Ok"):
        self.code = code
        self.message = message
        super().__init__(f"JSON-RPC Error ({code}): {message}")


class JSONRPCResultError(Exception):
    """Exception raised for errors in JSON-RPC.

    Attributes:
        code -- code of the error
        message -- explanation of the error
    """

    def __init__(self, code=0, message=None):
        self.code = code
        self.message = message
        msg = f"JSON-RPC Error ({code})"
        if message is not None:
            msg += f": {message}"
        super().__init__(msg)


class JSONRPCExecutor(object):
    def __init__(self, uri, user=None, password=None):
        self.uri = uri
        self.user = user
        self.password = password
        self.session_id = session_id_stub
        self.timeout = -1
        self.expires = time.time() - 1
        self.seq_num = 1

    def get_new_seq(self):
        if self.seq_num >= seq_num_max:
            self.seq_num = 1
        else:
            self.seq_num += 1
        return self.seq_num

    def login(self):
        res = self.request("call", "session", "login", {
            "username": self.user,
            "password": self.password
        }, test_session=False)
        from pprint import pprint
        pprint(res)
        if "ubus_rpc_session" in res:
            self.session_id = res["ubus_rpc_session"]
        if "expires" in res:
            self.expires = time.time() + res["expires"]
        if "timeout" in res:
            self.timeout = res["timeout"]

    def request(self, method, path, function, arguments=None, test_session=True):
        start_time = time.time()
        if test_session and self.timeout != 0 and self.expires < time.time():
            self.login()

        params = [self.session_id, path, function]
        if arguments is not None:
            params.append(arguments)
        with urllib.request.urlopen(self.uri,
                                    data=json.dumps({
                                        "jsonrpc": "2.0",
                                        "id": self.get_new_seq(),
                                        "method": method,
                                        "params": params
                                    }).encode("utf-8")) as res:
            res_j = json.load(res)
            print(f"--- took {time.time()-start_time} seconds")
            if "error" in res_j:
                raise JSONRPCError(res_j["error"]["code"], res_j["error"]["message"])
            elif "result" in res_j:
                result = res_j["result"]
                if result[0] != 0:
                    raise JSONRPCResultError(result[0], result[1] if len(result)>1 else None)
            return result[1]

    def call(self, path, function, arguments=None):
        return self.request("call", path, function, arguments)


if __name__ == "__main__":
    from pprint import pprint
    o = JSONRPCExecutor(f"http://{omega}:{port}/ubus", user=user, password=password)
    pprint(o.call("rgbdriver", "status", {}))
    pprint(o.call("rgbdriver", "status", {}))
