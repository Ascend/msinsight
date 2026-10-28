#!/usr/bin/env python
# -*- coding: UTF-8 -*-

"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2026 Huawei Technologies Co.,Ltd.

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

from contextlib import ExitStack
from dataclasses import FrozenInstanceError
import os
from unittest.mock import patch

import pytest
from traitlets.config import Config, LoggingConfigurable
from traitlets.config.loader import KVArgParseConfigLoader

from mindstudio_insight_jupyterlab import config as config_module
from mindstudio_insight_jupyterlab.config import (
    InsightFileSelectorConfig,
    MSInsight,
    load_insight_file_selector_config,
)


UNSET = object()


@pytest.fixture(autouse=True)
def clear_directory_environment(monkeypatch):
    monkeypatch.delenv('MINDSTUDIO_INSIGHT_INITIAL_DIR', raising=False)
    monkeypatch.delenv('MINDSTUDIO_INSIGHT_EVENT_DIR', raising=False)


def make_server(root, value=UNSET, cli=UNSET):
    config = Config() if value is UNSET else Config({'MSInsight': {'initial_dir': value}})
    if cli is not UNSET:
        config.merge(KVArgParseConfigLoader(['--MSInsight.initial_dir=' + cli]).load_config())
    app = LoggingConfigurable(config=config)
    app.root_dir = str(root)
    return app


def assert_invalid(snapshot):
    assert snapshot.default_browse_dir == ''
    assert snapshot.error.startswith('Invalid Insight file selector configuration:')
    assert 'MSInsight.initial_dir' in snapshot.error


def test_public_configuration_name_and_default(tmp_path):
    traits = MSInsight.class_traits(config=True)
    assert 'initial_dir' in traits
    assert 'event_dir' not in traits
    assert load_insight_file_selector_config(make_server(tmp_path)) == InsightFileSelectorConfig()


@pytest.mark.parametrize(
    'environment,configured,cli,expected',
    [
        (None, UNSET, UNSET, ''),
        ('environment', UNSET, UNSET, 'environment'),
        ('environment', 'file', UNSET, 'file'),
        ('environment', 'file', 'cli', 'cli'),
        ('environment', '', UNSET, ''),
        ('environment', 'file', '', ''),
        ('environment', '', 'cli', 'cli'),
    ],
    ids=['default', 'environment', 'file-over-env', 'cli-over-file', 'empty-file', 'empty-cli', 'cli-over-empty-file'],
)
def test_configuration_priority_and_explicit_empty(tmp_path, monkeypatch, environment, configured, cli, expected):
    if environment is not None:
        monkeypatch.setenv('MINDSTUDIO_INSIGHT_INITIAL_DIR', environment)
    result = load_insight_file_selector_config(make_server(tmp_path, configured, cli))
    assert result.error is None
    assert result.default_browse_dir == (str(tmp_path / expected) if expected else '')


def test_legacy_environment_variable_is_not_an_alias(tmp_path, monkeypatch):
    monkeypatch.setenv('MINDSTUDIO_INSIGHT_EVENT_DIR', '/legacy')
    assert load_insight_file_selector_config(make_server(tmp_path)) == InsightFileSelectorConfig()


@pytest.mark.parametrize('value', ['data', 'runs/../data', 'run with spaces', chr(0x5206) + chr(0x6790)])
def test_relative_directory_uses_server_root_not_process_cwd(tmp_path, value):
    root = tmp_path / 'server-root'
    result = load_insight_file_selector_config(make_server(root, value))
    assert result == InsightFileSelectorConfig(os.path.abspath(os.path.join(str(root), value)))


def test_absolute_directory_does_not_use_server_root(tmp_path):
    value = str(tmp_path / 'data')
    assert load_insight_file_selector_config(make_server(tmp_path / 'other', value)) == InsightFileSelectorConfig(value)


def test_home_directory_is_expanded(tmp_path, monkeypatch):
    home = tmp_path / 'home'
    monkeypatch.setenv('HOME', str(home))
    monkeypatch.setenv('USERPROFILE', str(home))
    result = load_insight_file_selector_config(make_server(tmp_path / 'other', '~/data'))
    assert result == InsightFileSelectorConfig(str(home / 'data'))


def test_empty_directory_skips_path_processing(tmp_path):
    server = make_server(tmp_path, '')
    with (
        patch.object(config_module.os.path, 'expanduser', side_effect=AssertionError('Unexpected expansion')),
        patch.object(config_module.os, 'fsencode', side_effect=AssertionError('Unexpected encoding')),
    ):
        assert load_insight_file_selector_config(server) == InsightFileSelectorConfig()


def test_configuration_does_not_inspect_or_create_the_directory(tmp_path):
    target = tmp_path / 'missing' / 'data'
    server = make_server(tmp_path, str(target))
    with ExitStack() as stack:
        for name in ('stat', 'lstat', 'access', 'scandir', 'listdir', 'mkdir', 'makedirs'):
            stack.enter_context(
                patch.object(config_module.os, name, side_effect=AssertionError('Unexpected filesystem access'))
            )
        stack.enter_context(
            patch.object(config_module.os.path, 'realpath', side_effect=AssertionError('Unexpected resolution'))
        )
        result = load_insight_file_selector_config(server)
    assert result == InsightFileSelectorConfig(str(target))
    assert not target.exists()


@pytest.mark.parametrize('value', [None, 123, False, [], {}])
def test_invalid_trait_type_is_saved_as_configuration_error(tmp_path, value):
    assert_invalid(load_insight_file_selector_config(make_server(tmp_path, value)))


@pytest.mark.parametrize('codepoint', [0, 9, 10, 13, 31, 127])
def test_control_characters_are_rejected(tmp_path, codepoint):
    assert_invalid(load_insight_file_selector_config(make_server(tmp_path, 'bad' + chr(codepoint) + 'path')))


@pytest.mark.parametrize('byte_length', [4095, 4096, 4097])
@pytest.mark.parametrize('character', ['a', chr(0x5206)], ids=['ascii', 'multibyte'])
def test_path_limit_counts_filesystem_bytes(tmp_path, byte_length, character):
    prefix = os.path.abspath(os.sep)
    count, padding = divmod(byte_length - len(os.fsencode(prefix)), len(os.fsencode(character)))
    value = prefix + character * count + 'a' * padding
    assert len(os.fsencode(value)) == byte_length
    result = load_insight_file_selector_config(make_server(tmp_path, value))
    if byte_length < 4096:
        assert result == InsightFileSelectorConfig(value)
    else:
        assert_invalid(result)
        assert '4096 filesystem-encoded bytes' in result.error


def test_overlong_raw_path_is_rejected_before_normalization(tmp_path):
    value = os.path.abspath(os.sep) + './' * 4096 + 'data'
    server = make_server(tmp_path, value)
    with patch.object(config_module.os.path, 'expanduser', side_effect=AssertionError('Unexpected expansion')):
        assert_invalid(load_insight_file_selector_config(server))


@pytest.mark.parametrize('source', ['root_dir', 'home'])
@pytest.mark.parametrize('suffix', ['\n', '\x7f', 'a' * 4096], ids=['newline', 'delete', 'too-long'])
def test_expanded_path_is_validated_again(tmp_path, monkeypatch, source, suffix):
    base = str(tmp_path) + suffix
    if source == 'home':
        monkeypatch.setenv('HOME', base)
        monkeypatch.setenv('USERPROFILE', base)
        server = make_server(tmp_path, '~/data')
    else:
        server = make_server(base, 'data')
    assert_invalid(load_insight_file_selector_config(server))


def test_unresolved_home_is_a_configuration_error(tmp_path):
    with patch.object(config_module.os.path, 'expanduser', return_value='~missing-user/data'):
        assert_invalid(load_insight_file_selector_config(make_server(tmp_path, '~missing-user/data')))


def test_path_processing_os_error_does_not_escape(tmp_path):
    with patch.object(config_module.os.path, 'expanduser', side_effect=OSError('Unavailable home')):
        assert_invalid(load_insight_file_selector_config(make_server(tmp_path, '~/data')))


def test_path_encoding_error_does_not_escape(tmp_path):
    error = UnicodeEncodeError('utf-8', '\ud800', 0, 1, 'invalid character')
    with patch.object(config_module.os, 'fsencode', side_effect=error):
        assert_invalid(load_insight_file_selector_config(make_server(tmp_path, 'data')))


def test_snapshots_are_immutable_and_isolated(tmp_path, monkeypatch):
    monkeypatch.setenv('MINDSTUDIO_INSIGHT_INITIAL_DIR', 'first')
    first = load_insight_file_selector_config(make_server(tmp_path))
    monkeypatch.setenv('MINDSTUDIO_INSIGHT_INITIAL_DIR', 'second')
    second = load_insight_file_selector_config(make_server(tmp_path))
    assert first.default_browse_dir == str(tmp_path / 'first')
    assert second.default_browse_dir == str(tmp_path / 'second')
    with pytest.raises(FrozenInstanceError):
        first.default_browse_dir = 'changed'
