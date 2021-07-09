################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2021 Intel Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
################################################################################

import pytest
import logging
import common
import jsonschema
import mock

from config import ConfigStore
import caps

from copy import deepcopy

logging.basicConfig(level=logging.DEBUG)

LOG = logging.getLogger('config')

CONFIG = {
    "apps": [
        {
            "cores": [1],
            "id": 1,
            "name": "app 1",
            "pids": [1]
        },
        {
            "cores": [3],
            "id": 2,
            "name": "app 2",
            "pids": [2, 3]
        },
        {
            "cores": [3],
            "id": 3,
            "name": "app 3",
            "pids": [4]
        }
    ],
    "pools": [
        {
            "apps": [1],
            "cbm": 0xf0,
            "cores": [1],
            "id": 1,
            "mba": 20,
            "name": "cat&mba"
        },
        {
            "apps": [2, 3],
            "cbm": 0xf,
            "cores": [3],
            "id": 2,
            "name": "cat"
        },
        {
            "id": 3,
            "mba": 30,
            "name": "mba",
            "cores": [4]
        }
    ]
}

CONFIG_NO_MBA = {
    "apps": [
        {
            "cores": [1],
            "id": 1,
            "name": "app 1",
            "pids": [1]
        },
        {
            "cores": [3],
            "id": 2,
            "name": "app 2",
            "pids": [2, 3]
        },
        {
            "cores": [3],
            "id": 3,
            "name": "app 3",
            "pids": [4]
        }
    ],
    "pools": [
        {
            "apps": [1],
            "cbm": 0xf0,
            "cores": [1],
            "id": 1,
            "name": "cat"
        },
        {
            "apps": [2, 3],
            "cbm": 0xf,
            "cores": [3],
            "id": 2,
            "name": "cat"
        }
    ]
}


@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("pid, app", [
    (1, 1),
    (2, 2),
    (3, 2),
    (4, 3),
    (5, None),
    (None, None)
])
def test_config_pid_to_app(mock_get_config, pid, app):

    mock_get_config.return_value = CONFIG
    config_store = ConfigStore()

    assert config_store.pid_to_app(pid) == app

@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("app, pool_id", [
    (1, 1),
    (2, 2),
    (3, 2),
    (99, None),
    (None, None)
])
def test_config_app_to_pool(mock_get_config, app, pool_id):
    mock_get_config.return_value = CONFIG
    config_store = ConfigStore()

    assert config_store.app_to_pool(app) == pool_id

@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("pid, pool_id", [
    (1, 1),
    (2, 2),
    (3, 2),
    (4, 2),
    (1234, None),
    (None, None)
])
def test_config_pid_to_pool(mock_get_config, pid, pool_id):
    mock_get_config.return_value = CONFIG
    config_store = ConfigStore()

    assert config_store.pid_to_pool(pid) == pool_id


@mock.patch('common.PQOS_API.get_cores')
def test_config_default_pool(mock_get_cores):
    mock_get_cores.return_value = range(16)
    config_store = ConfigStore()
    config = CONFIG.copy()

    # just in case, remove default pool from config
    for pool in config['pools']:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # no default pool in config
    assert not config_store.is_default_pool_defined(config)

    # add default pool to config
    config_store.add_default_pool(config)
    assert config_store.is_default_pool_defined(config)

    # test that config now contains all cores (cores configured + default pool cores)
    all_cores = range(16)
    for pool in config['pools']:
        all_cores = [core for core in all_cores if core not in pool['cores']]
    assert not all_cores

    # remove default pool from config
    for pool in config['pools']:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # no default pool in config
    assert not config_store.is_default_pool_defined(config)


@mock.patch('common.PQOS_API.get_cores', mock.MagicMock(return_value=range(8)))
@mock.patch('common.PQOS_API.get_max_l3_cat_cbm', mock.MagicMock(return_value=0xDEADBEEF))
@mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
@mock.patch("caps.mba_supported", mock.MagicMock(return_value=False))
def test_config_default_pool_cat():
    config_store = ConfigStore()
    config = deepcopy(CONFIG)

    # just in case, remove default pool from config
    for pool in config['pools']:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # no default pool in config
    assert not config_store.is_default_pool_defined(config)

    # add default pool to config
    config_store.add_default_pool(config)
    assert config_store.is_default_pool_defined(config)

    pool_cbm = None

    for pool in config['pools']:
        if pool['id'] == 0:
            assert 'cbm' in pool
            assert not 'mba' in pool
            assert not 'mba_bw' in pool
            pool_cbm = pool['cbm']
            break

    assert pool_cbm == 0xDEADBEEF


@mock.patch('common.PQOS_API.get_cores', mock.MagicMock(return_value=range(8)))
@mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
@mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=False))
@mock.patch("caps.mba_bw_enabled", mock.MagicMock(return_value=False))
def test_config_default_pool_mba():
    config_store = ConfigStore()
    config = deepcopy(CONFIG)

    # just in case, remove default pool from config
    for pool in config['pools']:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # no default pool in config
    assert not config_store.is_default_pool_defined(config)

    # add default pool to config
    config_store.add_default_pool(config)
    assert config_store.is_default_pool_defined(config)

    pool_mba = None

    for pool in config['pools']:
        if pool['id'] == 0:
            assert not 'cat' in pool
            assert not 'mba_bw' in pool
            pool_mba = pool['mba']
            break

    assert pool_mba == 100


@mock.patch('common.PQOS_API.get_cores', mock.MagicMock(return_value=range(8)))
@mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
@mock.patch("caps.mba_bw_enabled", mock.MagicMock(return_value=True))
@mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=False))
def test_config_default_pool_mba_bw():
    config_store = ConfigStore()
    config = deepcopy(CONFIG)

    # just in case, remove default pool from config
    for pool in config['pools']:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # no default pool in config
    assert not config_store.is_default_pool_defined(config)

    # add default pool to config
    config_store.add_default_pool(config)
    assert config_store.is_default_pool_defined(config)

    pool_mba_bw = None

    for pool in config['pools']:
        if pool['id'] == 0:
            assert not 'mba' in pool
            assert not 'cbm' in pool
            pool_mba_bw = pool['mba_bw']
            break

    assert pool_mba_bw == 2**32 - 1


@pytest.mark.parametrize("def_pool_def", [True, False])
def test_config_recreate_default_pool(def_pool_def):
    config_store = ConfigStore()

    with mock.patch('config.ConfigStore.is_default_pool_defined', mock.MagicMock(return_value=def_pool_def)) as mock_is_def_pool_def,\
         mock.patch('config.ConfigStore.remove_default_pool') as mock_rm_def_pool,\
         mock.patch('config.ConfigStore.add_default_pool') as mock_add_def_pool:

        config_store.recreate_default_pool()

        if def_pool_def:
            mock_rm_def_pool.assert_called_once()
        else:
            mock_rm_def_pool.assert_not_called()

        mock_add_def_pool.assert_called_once()


CONFIG_POOLS = {
    "pools": [
        {
            "apps": [1],
            "cbm": 0xf0,
            "cores": [1],
            "id": 0,
            "mba": 20,
            "name": "cat&mba_0"
        },
        {
            "apps": [2, 3],
            "cbm": 0xf,
            "cores": [3],
            "id": 9,
            "name": "cat_9"
        },
        {
            "id": 31,
            "cbm": "0xf0",
            "name": "cat_31",
            "cores": [4],
        }
    ]
}


def test_config_is_default_pool_defined():
    config = deepcopy(CONFIG_POOLS)

    # FUT, default pool in config
    assert ConfigStore.is_default_pool_defined(config) == True

    # remove default pool from config
    for pool in config['pools'][:]:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # FUT, no default pool in config
    assert not ConfigStore.is_default_pool_defined(config)


def test_config_remove_default_pool():
    config = deepcopy(CONFIG_POOLS)

    # default pool in config
    assert ConfigStore.is_default_pool_defined(config) == True

    # FUT
    ConfigStore.remove_default_pool(config)

    # no default pool in config
    assert not ConfigStore.is_default_pool_defined(config)


@mock.patch('config.ConfigStore.get_config')
def test_config_is_any_pool_defined(mock_get_config):

    config_store = ConfigStore()
    config = deepcopy(CONFIG_POOLS)

    mock_get_config.return_value = config
    assert config_store.is_any_pool_defined() == True

    for pool in config['pools'][:]:
        print(pool)
        if not pool['id'] == 0:
            config['pools'].remove(pool)

    print(config)

    mock_get_config.return_value = config
    assert not config_store.is_any_pool_defined()


@mock.patch('config.ConfigStore.get_config')
def test_config_get_new_pool_id(mock_get_config):

    def get_max_cos_id(alloc_type):
        if 'mba' in alloc_type:
            return 9
        else:
            return 31


    with mock.patch('common.PQOS_API.get_max_cos_id', new=get_max_cos_id):
        config_store = ConfigStore()

        mock_get_config.return_value = CONFIG
        assert 9 == config_store.get_new_pool_id({"mba":10})
        assert 9 == config_store.get_new_pool_id({"mba":20, "cbm":"0xf0"})
        assert 31 == config_store.get_new_pool_id({"cbm":"0xff"})

        mock_get_config.return_value = CONFIG_POOLS
        assert 8 == config_store.get_new_pool_id({"mba":10})
        assert 8 == config_store.get_new_pool_id({"mba":20, "cbm":"0xf0"})
        assert 30 == config_store.get_new_pool_id({"cbm":"0xff"})


def test_config_reset():
    from copy import deepcopy

    with mock.patch('common.PQOS_API.get_cores') as mock_get_cores,\
         mock.patch('config.ConfigStore.load') as mock_load,\
         mock.patch('caps.mba_supported', return_value = True) as mock_mba,\
         mock.patch('caps.cat_l3_supported', return_value = True),\
         mock.patch('common.PQOS_API.get_max_l3_cat_cbm', return_value = 0xFFF),\
         mock.patch('common.PQOS_API.check_core', return_value = True),\
         mock.patch('pid_ops.is_pid_valid', return_value = True):

        mock_load.return_value = deepcopy(CONFIG)
        mock_get_cores.return_value = range(8)

        config_store = ConfigStore()
        config_store.from_file("/tmp/appqos_test.config")
        config_store.process_config()

        assert len(config_store.get_pool_attr('cores', None)) == 8
        assert config_store.get_pool_attr('cbm', 0) == 0xFFF
        assert config_store.get_pool_attr('mba', 0) == 100

        # test get_pool_attr
        assert config_store.get_pool_attr('non_exisiting_key', None) == None

        # reset mock and change return values
        # more cores this time (8 vs. 16)
        mock_get_cores.return_value = range(16)
        mock_get_cores.reset_mock()

        # use CONFIG_NO_MBA this time, as MBA is reported as not supported
        mock_load.return_value = deepcopy(CONFIG_NO_MBA)
        mock_load.reset_mock()
        mock_mba.return_value = False

        # verify that reset reloads config from file and Default pool is
        # recreated with different set of cores
        # (get_num_cores mocked to return different values)
        config_store.reset()

        mock_load.assert_called_once_with("/tmp/appqos_test.config")
        mock_get_cores.assert_called_once()

        assert len(config_store.get_pool_attr('cores', None)) == 16
        assert config_store.get_pool_attr('cbm', 0) == 0xFFF
        assert config_store.get_pool_attr('mba', 0) is None


@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("cfg, default, result", [
    ({}, True, True),
    ({}, False, False),
    ({"test": True}, True, True),
    ({"test": True}, False, False),
    ({"power_profiles_expert_mode": False}, False, False),
    ({"power_profiles_expert_mode": False}, True, False),
    ({"power_profiles_expert_mode": True}, False, True),
    ({"power_profiles_expert_mode": True}, True, True)
])
def test_get_global_attr_power_profiles_expert_mode(mock_get_config, cfg, default, result):
    mock_get_config.return_value = cfg
    config_store = ConfigStore()

    assert config_store.get_global_attr('power_profiles_expert_mode', default) == result


@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("cfg, default, result", [
    ({}, True, True),
    ({}, False, False),
    ({"test": True}, True, True),
    ({"test": True}, False, False),
    ({"power_profiles_verify": False}, False, False),
    ({"power_profiles_verify": False}, True, False),
    ({"power_profiles_verify": True}, False, True),
    ({"power_profiles_verify": True}, True, True)
])
def test_get_global_attr_power_profiles_verify(mock_get_config, cfg, default, result):
    mock_get_config.return_value = cfg
    config_store = ConfigStore()

    assert config_store.get_global_attr('power_profiles_verify', default) == result


class TestConfigValidate:

    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_pool_invalid_core(self):
        def check_core(core):
            return core != 3

        data = {
            "pools": [
                {
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "valid"
                },
                {
                    "cbm": 0xf,
                    "cores": [3],
                    "id": 8,
                    "name": "invalid"
                }
            ]
        }

        with mock.patch('common.PQOS_API.check_core', new=check_core):
            with pytest.raises(ValueError, match="Invalid core 3"):
                ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_pool_duplicate_core(self):
        data = {
            "pools": [
                {
                    "cbm": 0xf0,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                },
                {
                    "cbm": 0xf,
                    "id": 10,
                    "cores": [3],
                    "name": "pool 1"
                }
            ]
        }

        with pytest.raises(ValueError, match="already assigned to another pool"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_pool_same_ids(self):
        data = {
            "pools": [
                {
                    "cbm": 0xf0,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                },
                {
                    "cbm": 0xf,
                    "id": 1,
                    "cores": [3],
                    "name": "pool 2"
                }
            ]
        }

        with pytest.raises(ValueError, match="Pool 1, multiple pools with same id"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_pool_invalid_app(self):
        data = {
            "pools": [
                {
                    "apps": [1, 3],
                    "cbm": 0xf0,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ],
            "apps": [
                {
                    "cores": [3],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                }
            ]
        }

        with pytest.raises(KeyError, match="does not exist"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_pool_invalid_cbm(self):
        data = {
            "pools": [
                {
                    "apps": [],
                    "cbm": 0x5,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        }

        with pytest.raises(ValueError, match="not contiguous"):
            ConfigStore.validate(data)

        data['pools'][0]['cbm'] = 0
        with pytest.raises(ValueError, match="not contiguous"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=False))
    def test_pool_cat_not_supported(self):
        data = {
            "pools": [
                {
                    "apps": [],
                    "cbm": 0x4,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        }

        with pytest.raises(ValueError, match="CAT is not supported"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=False))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    def test_pool_cat_not_supported_mba(self):
        data = {
            "pools": [
                {
                    "apps": [],
                    "cbm": 0x4,
                    "mba": 100,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        }

        with pytest.raises(ValueError, match="CAT is not supported"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    def test_pool_invalid_mba(self):
        data = {
            "pools": [
                {
                    "mba": 101,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        }

        with pytest.raises(jsonschema.exceptions.ValidationError, match="Failed validating 'maximum' in schema"):
            ConfigStore.validate(data)

        data['pools'][0]['mba'] = 0
        with pytest.raises(jsonschema.exceptions.ValidationError, match="Failed validating 'minimum' in schema"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=False))
    def test_pool_mba_not_supported(self):
        data = {
            "pools": [
                {
                    "mba": 50,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        }

        with pytest.raises(ValueError, match="MBA is not supported"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_bw_supported", mock.MagicMock(return_value=False))
    def test_pool_mba_bw_not_supported(self):
        data = {
            "pools": [
                {
                    "mba_bw": 5000,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        }

        with pytest.raises(ValueError, match="MBA BW is not enabled/supported"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=False))
    def test_pool_mba_not_supported_cat(self):
        data = {
            "pools": [
                {
                    "cbm": 0xf,
                    "mba": 50,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        }

        with pytest.raises(ValueError, match="MBA is not supported"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_bw_supported", mock.MagicMock(return_value=False))
    def test_pool_mba_bw_not_supported_cat(self):
        data = {
            "pools": [
                {
                    "cbm": 0xf,
                    "mba_bw": 5000,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        }

        with pytest.raises(ValueError, match="MBA BW is not enabled/supported"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_bw_supported", mock.MagicMock(return_value=True))
    def test_pool_mba_mba_bw_enabled(self):
        data = {
            "rdt_iface": {"interface": "os"},
            "mba_ctrl": {"enabled": True},
            "pools": [
                {
                    "cbm": 0xf,
                    "mba": 50,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                },
                {
                    "cbm": 0xf,
                    "mba": 70,
                    "cores": [2],
                    "id": 2,
                    "name": "pool 2"
                }
            ]
        }

        with pytest.raises(ValueError, match="MBA % is not enabled. Disable MBA BW and try again"):
            ConfigStore.validate(data)


    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_invalid_core(self):
        def check_core(core):
            return core != 3

        data = {
            "pools": [
                {
                    "apps": [1],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                }
            ],
            "apps": [
                {
                    "cores": [3],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                }
            ]
        }

        with mock.patch('common.PQOS_API.check_core', new=check_core):
            with pytest.raises(ValueError, match="Invalid core 3"):
                ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_core_does_not_match_pool(self):
        data = {
            "pools": [
                {
                    "apps": [1],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                }
            ],
            "apps": [
                {
                    "cores": [3,4,5],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                }
            ]
        }

        with pytest.raises(ValueError, match="App 1, cores {3, 4, 5} does not match Pool 1"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_without_pool(self):

        data = {
            "pools": [
                {
                    "apps": [1],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                },
                {
                    "cbm": 0xf0,
                    "cores": [2],
                    "id": 2,
                    "name": "pool 2"
                }
            ],
            "apps": [
                {
                    "cores": [1],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                },
                {
                    "cores": [1],
                    "id": 2,
                    "name": "app 2",
                    "pids": [2]
                }
            ]
        }

        with pytest.raises(ValueError, match="not assigned to any pool"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_without_pool(self):
        data = {
            "pools": [
                {
                    "apps": [1],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                },
                {
                    "apps": [1, 2],
                    "cbm": 0xf0,
                    "cores": [2],
                    "id": 2,
                    "name": "pool 2"
                }
            ],
            "apps": [
                {
                    "cores": [1],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                },
                {
                    "cores": [2],
                    "id": 2,
                    "name": "app 2",
                    "pids": [2]
                }
            ]
        }

        with pytest.raises(ValueError, match="App 1, Assigned to more than one pool"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_same_ids(self):
        data = {
            "pools": [
                {
                    "apps": [1],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                },
            ],
            "apps": [
                {
                    "cores": [1],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                },
                {
                    "cores": [1],
                    "id": 1,
                    "name": "app 2",
                    "pids": [1]
                }
            ]
        }

        with pytest.raises(ValueError, match="App 1, multiple apps with same id"):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_same_pid(self):
        data = {
            "pools": [
                {
                    "apps": [1, 2],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                },
            ],
            "apps": [
                {
                    "cores": [1],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                },
                {
                    "cores": [1],
                    "id": 2,
                    "name": "app 2",
                    "pids": [1]
                }
            ]
        }

        with pytest.raises(ValueError, match=r"App 2, PIDs \{1} already assigned to another App."):
            ConfigStore.validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_invalid_pid(self):
        data = {
            "pools": [
                {
                    "apps": [1, 2],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                },
            ],
            "apps": [
                {
                    "cores": [1],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                },
                {
                    "cores": [1],
                    "id": 2,
                    "name": "app 2",
                    "pids": [99999]
                }
            ]
        }

        with pytest.raises(ValueError, match="App 2, PID 99999 is not valid"):
            ConfigStore.validate(data)


    def test_power_profile_expert_mode_invalid(self):
        data = {
            "pools": [],
            "apps": [],
            "power_profiles_expert_mode": None
        }

        with pytest.raises(jsonschema.exceptions.ValidationError, match="None is not of type 'boolean'"):
            ConfigStore.validate(data)

        data['power_profiles_expert_mode'] = 1
        with pytest.raises(jsonschema.exceptions.ValidationError, match="1 is not of type 'boolean'"):
            ConfigStore.validate(data)

        data['power_profiles_expert_mode'] = []
        with pytest.raises(jsonschema.exceptions.ValidationError, match="\\[\\] is not of type 'boolean'"):
            ConfigStore.validate(data)


    def test_power_profile_verify_invalid(self):
        data = {
            "pools": [],
            "apps": [],
            "power_profiles_verify": None
        }

        with pytest.raises(jsonschema.exceptions.ValidationError, match="None is not of type 'boolean'"):
            ConfigStore.validate(data)

        data['power_profiles_verify'] = 1
        with pytest.raises(jsonschema.exceptions.ValidationError, match="1 is not of type 'boolean'"):
            ConfigStore.validate(data)

        data['power_profiles_verify'] = []
        with pytest.raises(jsonschema.exceptions.ValidationError, match="\\[\\] is not of type 'boolean'"):
            ConfigStore.validate(data)


    def test_rdt_iface_invalid(self):
        data = {
            "pools": [],
            "apps": [],
            "rdt_iface": "os"
        }

        with pytest.raises(jsonschema.exceptions.ValidationError, match="'os' is not of type 'object'"):
            ConfigStore.validate(data)

        data['rdt_iface'] = {}
        with pytest.raises(jsonschema.exceptions.ValidationError, match="'interface' is a required property"):
            ConfigStore.validate(data)

        data['rdt_iface']['interface'] = None
        with pytest.raises(jsonschema.exceptions.ValidationError, match="None is not of type 'string'"):
            ConfigStore.validate(data)

        data['rdt_iface']['interface'] = 2
        with pytest.raises(jsonschema.exceptions.ValidationError, match="2 is not of type 'string'"):
            ConfigStore.validate(data)

        data['rdt_iface']['interface'] = "test_string"
        with pytest.raises(jsonschema.exceptions.ValidationError, match="'test_string' is not one of \\['msr', 'os'\\]"):
            ConfigStore.validate(data)


    def test_mba_ctrl_invalid(self):
        data = {
            "pools": [],
            "apps": [],
            "mba_ctrl": True
        }

        with pytest.raises(jsonschema.exceptions.ValidationError, match="True is not of type 'object'"):
            ConfigStore.validate(data)

        data['mba_ctrl'] = {}
        with pytest.raises(jsonschema.exceptions.ValidationError, match="'enabled' is a required property"):
            ConfigStore.validate(data)

        data['mba_ctrl']['enabled'] = None
        with pytest.raises(jsonschema.exceptions.ValidationError, match="None is not of type 'boolean'"):
            ConfigStore.validate(data)

        data['mba_ctrl']['enabled'] = 2
        with pytest.raises(jsonschema.exceptions.ValidationError, match="2 is not of type 'boolean'"):
            ConfigStore.validate(data)

        data['mba_ctrl']['enabled'] = "test_string"
        with pytest.raises(jsonschema.exceptions.ValidationError, match="'test_string' is not of type 'boolean'"):
            ConfigStore.validate(data)

        data['mba_ctrl']['enabled'] = True
        with pytest.raises(ValueError, match="MBA CTRL requires RDT OS interface"):
            ConfigStore.validate(data)

        data['rdt_iface'] = {"interface": "msr"}
        with pytest.raises(ValueError, match="MBA CTRL requires RDT OS interface"):
            ConfigStore.validate(data)


@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("cfg, result", [
    ({}, "msr"),
    ({"rdt_iface": {"interface": "msr"}}, "msr"),
    ({"rdt_iface": {"interface": "msr_test"}}, "msr_test"),
    ({"rdt_iface": {"interface": "os_test"}}, "os_test"),
    ({"rdt_iface": {"interface": "os"}}, "os")
])
def test_get_rdt_iface(mock_get_config, cfg, result):
    mock_get_config.return_value = cfg
    config_store = ConfigStore()

    assert config_store.get_rdt_iface() == result


@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("cfg, result", [
    ({}, False),
    ({"mba_ctrl": {"enabled": True}}, True),
    ({"mba_ctrl": {"enabled": False}}, False)
])
def test_get_mba_ctrl_enabled(mock_get_config, cfg, result):
    mock_get_config.return_value = cfg
    config_store = ConfigStore()

    assert config_store.get_mba_ctrl_enabled() == result
