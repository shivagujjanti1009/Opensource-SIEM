'''
copyright: Copyright (C) 2015-2024, Wazuh Inc.

           Created by Wazuh, Inc. <info@wazuh.com>.

           This program is free software; you can redistribute it and/or modify it under the terms of GPLv2

type: integration

brief: The 'wazuh-logcollector' daemon monitors configured files and commands for new log messages.
       Specifically, these tests will check if the Wazuh component (agent or manager) starts when
       the 'location' tag is set in the configuration, and the Wazuh API returns the same values for
       the configured 'localfile' section.
       Log data collection is the real-time process of making sense out of the records generated by
       servers or devices. This component can receive logs through text files or Windows event logs.
       It can also directly receive logs via remote syslog which is useful for firewalls and
       other such devices.
       This test suite will check the 'journald' log format, which is a system service that collects and stores
       logging data. The 'journald' log format is used by the 'systemd-journald' API.

tier: 0

modules:
    - logcollector

components:
    - agent

daemons:
    - wazuh-logcollector
    - wazuh-apid

os_platform:
    - linux

os_version:
    - Arch Linux
    - Amazon Linux 2
    - CentOS 8
    - Debian Buster
    - Red Hat 8
    - Ubuntu Focal
    - Ubuntu Bionic

references:
    - https://documentation.wazuh.com/current/user-manual/capabilities/log-data-collection/index.html
    - https://documentation.wazuh.com/current/user-manual/reference/ossec-conf/localfile.html

tags:
    - logcollector
'''


import pytest, sys, os
import tempfile

from pathlib import Path

from wazuh_testing.constants.paths.logs import WAZUH_LOG_PATH
from wazuh_testing.constants.platforms import WINDOWS, MACOS, LINUX
from wazuh_testing.modules.agentd import configuration as agentd_configuration
from wazuh_testing.modules.logcollector import patterns
from wazuh_testing.modules.logcollector import utils
from wazuh_testing.modules.logcollector import configuration as logcollector_configuration
from wazuh_testing.tools.monitors import file_monitor
from wazuh_testing.utils import callbacks, configuration

from . import TEST_CASES_PATH, CONFIGURATIONS_PATH


LOG_COLLECTOR_GLOBAL_TIMEOUT = 40

def build_tc_config(tc_conf_list):
    '''
    Build the configuration for each test case.

    Args:
        tc_conf_list (list): List of test case configurations.

    Returns:
        list: List of configurations for each test case.
    '''

    config_list = []  # List of configurations for each test case

    # Build the configuration for each test case
    for tc_config in tc_conf_list:
        sections = []
        for i, elements in enumerate(tc_config, start=1):
            if i == 1:
                section = {
                    "section": "localfile",
                    "elements": elements
                }
            else:
                section = {
                    "section": "localfile",
                    "attributes": [{"unique_id": str(i)}],
                    "elements": elements
                }
            sections.append(section)  

        config_list.append({"sections": sections})

    return config_list

# Marks
pytestmark = [pytest.mark.agent, pytest.mark.linux, pytest.mark.tier(level=0)]

# Configuration
journald_case_path = Path(TEST_CASES_PATH, 'cases_basic_configuration_journald.yaml')

test_configuration, test_metadata, test_cases_ids = configuration.get_test_cases_data(journald_case_path)
test_configuration = build_tc_config(test_configuration)

daemon_debug = logcollector_configuration.LOGCOLLECTOR_DEBUG


# Test daemons to restart.
daemons_handler_configuration = {'all_daemons': True}

local_internal_options = {daemon_debug: '1'}


# Test function.
@pytest.mark.parametrize('test_configuration, test_metadata', zip(test_configuration, test_metadata), ids=test_cases_ids)
def test_configuration_location(test_configuration, test_metadata, truncate_monitored_files, configure_local_internal_options,
                                set_wazuh_configuration, daemons_handler, wait_for_logcollector_start):
    '''
    description: Check if the 'wazuh-logcollector' daemon starts properly when the 'location' tag is used.
                 For this purpose, the test will configure the logcollector to monitor a 'syslog' directory
                 and use a pathname with special characteristics. Finally, the test will verify that the
                 Wazuh component is started by checking its process, and the Wazuh API returns the same
                 values for the 'localfile' section that the configured one.

    wazuh_min_version: 4.2.0

    parameters:
        - test_configuration:
            type: data
            brief: Configuration used in the test.
        - test_metadata:
            type: data
            brief: Configuration cases.
        - truncate_monitored_files:
            type: fixture
            brief: Reset the 'ossec.log' file and start a new monitor.
        - set_wazuh_configuration:
            type: fixture
            brief: Configure a custom environment for testing.
        - daemons_handler:
            type: fixture
            brief: Handler of Wazuh daemons.
        - wait_for_logcollector_start:
            type: fixture
            brief: Wait for the logcollector startup log.

    assertions:
        - Verify that the Wazuh component (agent or manager) can start when the 'location' tag is used.
        - Verify that the Wazuh API returns the same value for the 'localfile' section as the configured one.
        - Verify that the expected event is present in the log file.

    input_description: A configuration template (test_basic_configuration_location) is contained in an external
                       YAML file (wazuh_basic_configuration.yaml). That template is combined with different
                       test cases defined in the module. Those include configuration settings for
                       the 'wazuh-logcollector' daemon.

    expected_output:
        - Boolean values to indicate the state of the Wazuh component.
        - Did not receive the expected "ERROR: Could not EvtSubscribe() for ... which returned ..." event.

    tags:
        - invalid_settings
    '''

    if test_metadata['validate_config']:
        utils.check_logcollector_socket()
        # Check if test_metadata['log_format'] is not 'macos' or 'journald', this return a modify configuration
        if test_metadata['log_format'] not in ['macos', 'journald']:
            utils.validate_test_config_with_module_config(test_configuration=test_configuration)
