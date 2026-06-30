#!/usr/bin/env python
# -*- coding: UTF-8 -*-

"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2025 Huawei Technologies Co.,Ltd.

MindStudio is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:

         http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
-------------------------------------------------------------------------
"""

import os
import re
import sys
import types

PACKAGE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if PACKAGE_DIR not in sys.path:
    sys.path.insert(0, PACKAGE_DIR)

if "psutil" not in sys.modules:
    psutil_stub = types.ModuleType("psutil")
    psutil_stub.net_connections = lambda: []
    sys.modules["psutil"] = psutil_stub

if "jupyter_server" not in sys.modules:
    jupyter_server_stub = types.ModuleType("jupyter_server")
    base_stub = types.ModuleType("jupyter_server.base")
    handlers_stub = types.ModuleType("jupyter_server.base.handlers")
    utils_stub = types.ModuleType("jupyter_server.utils")

    class APIHandler:
        pass

    handlers_stub.APIHandler = APIHandler
    utils_stub.url_path_join = lambda *parts: "/".join(str(part).strip("/") for part in parts if part != "")
    sys.modules["jupyter_server"] = jupyter_server_stub
    sys.modules["jupyter_server.base"] = base_stub
    sys.modules["jupyter_server.base.handlers"] = handlers_stub
    sys.modules["jupyter_server.utils"] = utils_stub

if "tornado" not in sys.modules:
    tornado_stub = types.ModuleType("tornado")
    web_stub = types.ModuleType("tornado.web")

    def authenticated(func):
        return func

    class StaticFileHandler:
        def prepare(self):
            return None

    web_stub.authenticated = authenticated
    web_stub.StaticFileHandler = StaticFileHandler
    tornado_stub.web = web_stub
    sys.modules["tornado"] = tornado_stub
    sys.modules["tornado.web"] = web_stub


def get_host_pattern():
    from mindstudio_insight_jupyterlab.handlers import HOST_PATTERN

    return re.compile(HOST_PATTERN)


def test_host_pattern_matches_domain_ipv4_and_ipv6_hosts():
    pattern = get_host_pattern()

    assert pattern.fullmatch("localhost")
    assert pattern.fullmatch("localhost:9583")
    assert pattern.fullmatch("example.com")
    assert pattern.fullmatch("example.com:9583")
    assert pattern.fullmatch("sub-domain.example.com")
    assert pattern.fullmatch("sub-domain.example.com:9583")
    assert pattern.fullmatch("127.0.0.1")
    assert pattern.fullmatch("127.0.0.1:9583")
    assert pattern.fullmatch("2605:340:cd51:4900:376f:a7d8:5c02:fb7")
    assert pattern.fullmatch("[2605:340:cd51:4900:376f:a7d8:5c02:fb7]")
    assert pattern.fullmatch("[2605:340:cd51:4900:376f:a7d8:5c02:fb7]:9583")


def test_host_pattern_rejects_invalid_and_excessively_long_hosts():
    pattern = get_host_pattern()

    assert not pattern.fullmatch("")
    assert not pattern.fullmatch("bad host")
    assert not pattern.fullmatch("example.com:abc")
    assert not pattern.fullmatch("[2605:340:cd51:4900:376f:a7d8:5c02:fb7")
    assert not pattern.fullmatch(":" * 45 + ":9583")
    assert not pattern.fullmatch("a" * 256)
