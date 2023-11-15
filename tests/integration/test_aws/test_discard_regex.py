"""
Copyright (C) 2015-2023, Wazuh Inc.
Created by Wazuh, Inc. <info@wazuh.com>.
This program is free software; you can redistribute it and/or modify it under the terms of GPLv2

This module will contains all cases for the discard_regex test suite
"""

import pytest

# qa-integration-framework imports
from wazuh_testing import session_parameters
from wazuh_testing.modules.aws import event_monitor, local_internal_options  # noqa: F401

from wazuh_testing.modules.aws.db_utils import s3_db_exists, services_db_exists

# Local module imports
from .utils import ERROR_MESSAGES, TIMEOUTS
from conftest import TestConfigurator

pytestmark = [pytest.mark.server]

# Set test configurator for the module
configurator = TestConfigurator(module='discard_regex_test_module')

# --------------------------------------------- TEST_BUCKET_DISCARD_REGEX ---------------------------------------------
# Configure T1 test
configurator.configure_test(configuration_file='configuration_bucket_discard_regex.yaml',
                            cases_file='cases_bucket_discard_regex.yaml')


@pytest.mark.tier(level=0)
@pytest.mark.parametrize('configuration, metadata',
                         zip(configurator.test_configuration_template, configurator.metadata),
                         ids=configurator.cases_ids)
def test_bucket_discard_regex(
        configuration, metadata, load_wazuh_basic_configuration, set_wazuh_configuration, clean_s3_cloudtrail_db,
        configure_local_internal_options_function, truncate_monitored_files, restart_wazuh_function, file_monitoring,
):
    """
    description: Check that some bucket logs are excluded when the regex and field defined in <discard_regex>
                 match an event.

    test_phases:
        - setup:
            - Load Wazuh light configuration.
            - Apply ossec.conf configuration changes according to the configuration template and use case.
            - Apply custom settings in local_internal_options.conf.
            - Truncate wazuh logs.
            - Restart wazuh-manager service to apply configuration changes.
        - test:
            - Check in the ossec.log that a line has appeared calling the module with correct parameters.
            - Check the expected number of events were forwarded to analysisd, only logs stored in the bucket and skips
              the ones that match with regex.
            - Check the database was created and updated accordingly.
        - teardown:
            - Truncate wazuh logs.
            - Restore initial configuration, both ossec.conf and local_internal_options.conf.
            - Delete the uploaded file

    wazuh_min_version: 4.6.0

    parameters:
        - configuration:
            type: dict
            brief: Get configurations from the module.
        - metadata:
            type: dict
            brief: Get metadata from the module.
        - load_wazuh_basic_configuration:
            type: fixture
            brief: Load basic wazuh configuration.
        - set_wazuh_configuration:
            type: fixture
            brief: Apply changes to the ossec.conf configuration.
        - clean_s3_cloudtrail_db:
            type: fixture
            brief: Delete the DB file before and after the test execution.
        - configure_local_internal_options_function:
            type: fixture
            brief: Apply changes to the local_internal_options.conf configuration.
        - truncate_monitored_files:
            type: fixture
            brief: Truncate wazuh logs.
        - restart_wazuh_daemon_function:
            type: fixture
            brief: Restart the wazuh service.
        - file_monitoring:
            type: fixture
            brief: Handle the monitoring of a specified file.

    assertions:
        - Check in the log that the module was called with correct parameters.
        - Check the expected number of events were forwarded to analysisd.
        - Check the database was created and updated accordingly.

    input_description:
        - The `configuration_bucket_discard_regex` file provides the module configuration for this test.
        - The `cases_bucket_discard_regex` file provides the test cases.
    """
    bucket_name = metadata['bucket_name']
    bucket_type = metadata['bucket_type']
    only_logs_after = metadata['only_logs_after']
    discard_field = metadata['discard_field']
    discard_regex = metadata['discard_regex']
    found_logs = metadata['found_logs']
    skipped_logs = metadata['skipped_logs']
    path = metadata['path'] if 'path' in metadata else None

    pattern = fr'.*The "{discard_regex}" regex found a match in the "{discard_field}" field.' \
              ' The event will be skipped.'

    parameters = [
        'wodles/aws/aws-s3',
        '--bucket', bucket_name,
        '--aws_profile', 'qa',
        '--only_logs_after', only_logs_after,
        '--discard-field', discard_field,
        '--discard-regex', discard_regex,
        '--type', bucket_type,
        '--debug', '2'
    ]

    if path is not None:
        parameters.insert(5, path)
        parameters.insert(5, '--trail_prefix')

    # Check AWS module started
    log_monitor.start(
        timeout=session_parameters.default_timeout,
        callback=event_monitor.callback_detect_aws_module_start
    )

    assert log_monitor.callback_result is not None, ERROR_MESSAGES['failed_start']

    # Check command was called correctly
    log_monitor.start(
        timeout=session_parameters.default_timeout,
        callback=event_monitor.callback_detect_aws_module_called(parameters)
    )

    assert log_monitor.callback_result is not None, ERROR_MESSAGES['incorrect_parameters']

    log_monitor.start(
        timeout=TIMEOUTS[20],
        callback=event_monitor.callback_detect_event_processed_or_skipped(pattern),
        accumulations=found_logs + skipped_logs
    )

    assert s3_db_exists()


# ----------------------------------------- TEST_CLOUDWATCH_DISCARD_REGEX_JSON ----------------------------------------
# Configure T2 test
configurator.configure_test(configuration_file='configuration_cloudwatch_discard_regex_json.yaml',
                            cases_file='cases_cloudwatch_discard_regex_json.yaml')


@pytest.mark.tier(level=0)
@pytest.mark.parametrize('configuration, metadata',
                         zip(configurator.test_configuration_template, configurator.metadata),
                         ids=configurator.cases_ids)
def test_cloudwatch_discard_regex_json(
        configuration, metadata, load_wazuh_basic_configuration, set_wazuh_configuration, clean_aws_services_db,
        configure_local_internal_options_function, truncate_monitored_files, restart_wazuh_function, file_monitoring,
):
    """
    description: Check that some CloudWatch JSON logs are excluded when the regex and field defined in <discard_regex>
                 match an event.

    test_phases:
        - setup:
            - Load Wazuh light configuration.
            - Apply ossec.conf configuration changes according to the configuration template and use case.
            - Apply custom settings in local_internal_options.conf.
            - Truncate wazuh logs.
            - Restart wazuh-manager service to apply configuration changes.
        - test:
            - Check in the ossec.log that a line has appeared calling the module with correct parameters.
            - Check the expected number of events were forwarded to analysisd, only logs stored in the bucket and skips
              the ones that match with regex.
            - Check the database was created and updated accordingly.
        - teardown:
            - Truncate wazuh logs.
            - Restore initial configuration, both ossec.conf and local_internal_options.conf.
            - Delete the uploaded file

    wazuh_min_version: 4.6.0

    parameters:
        - configuration:
            type: dict
            brief: Get configurations from the module.
        - metadata:
            type: dict
            brief: Get metadata from the module.
        - load_wazuh_basic_configuration:
            type: fixture
            brief: Load basic wazuh configuration.
        - set_wazuh_configuration:
            type: fixture
            brief: Apply changes to the ossec.conf configuration.
        - clean_aws_services_db:
            type: fixture
            brief: Delete the DB file before and after the test execution.
        - configure_local_internal_options_function:
            type: fixture
            brief: Apply changes to the local_internal_options.conf configuration.
        - truncate_monitored_files:
            type: fixture
            brief: Truncate wazuh logs.
        - restart_wazuh_daemon_function:
            type: fixture
            brief: Restart the wazuh service.
        - file_monitoring:
            type: fixture
            brief: Handle the monitoring of a specified file.

    assertions:
        - Check in the log that the module was called with correct parameters.
        - Check the expected number of events were forwarded to analysisd.
        - Check the database was created and updated accordingly.

    input_description:
        - The `configuration_cloudwatch_discard_regex` file provides the module configuration for this test.
        - The `cases_cloudwatch_discard_regex` file provides the test cases.
    """
    log_group_name = metadata.get('log_group_name')
    service_type = metadata.get('service_type')
    only_logs_after = metadata.get('only_logs_after')
    regions: str = metadata.get('regions')
    discard_field = metadata.get('discard_field', None)
    discard_regex = metadata.get('discard_regex')
    found_logs = metadata.get('found_logs')

    pattern = fr'.*The "{discard_regex}" regex found a match in the "{discard_field}" field.' \
              ' The event will be skipped.'

    parameters = [
        'wodles/aws/aws-s3',
        '--service', service_type,
        '--aws_profile', 'qa',
        '--only_logs_after', only_logs_after,
        '--regions', regions,
        '--aws_log_groups', log_group_name,
        '--discard-field', discard_field,
        '--discard-regex', discard_regex,
        '--debug', '2'
    ]

    # Check AWS module started
    log_monitor.start(
        timeout=global_parameters.default_timeout,
        callback=event_monitor.callback_detect_aws_module_start,
        error_message='The AWS module did not start as expected',
    ).result()

    # Check command was called correctly
    log_monitor.start(
        timeout=global_parameters.default_timeout,
        callback=event_monitor.callback_detect_aws_module_called(parameters),
        error_message='The AWS module was not called with the correct parameters',
    ).result()

    log_monitor.start(
        timeout=T_20,
        callback=event_monitor.callback_detect_event_processed_or_skipped(pattern),
        error_message=(
            'The AWS module did not show the correct message about discard regex or ',
            'did not process the expected amount of logs'
        ),
        accum_results=found_logs
    ).result()

    assert services_db_exists()


# ------------------------------------- TEST_CLOUDWATCH_DISCARD_REGEX_SIMPLE_TEXT -------------------------------------
# Configure T3 test
configurator.configure_test(configuration_file='configuration_cloudwatch_discard_regex_simple_text.yaml',
                            cases_file='cases_cloudwatch_discard_regex_simple_text.yaml')


@pytest.mark.tier(level=0)
@pytest.mark.parametrize('configuration, metadata',
                         zip(configurator.test_configuration_template, configurator.metadata),
                         ids=configurator.cases_ids)
def test_cloudwatch_discard_regex_simple_text(
        configuration, metadata, load_wazuh_basic_configuration, set_wazuh_configuration, clean_aws_services_db,
        configure_local_internal_options_function, truncate_monitored_files, restart_wazuh_function, file_monitoring,
):
    """
    description: Check that some CloudWatch simple text logs are excluded when the regex defined in <discard_regex>
                 matches an event.

    test_phases:
        - setup:
            - Load Wazuh light configuration.
            - Apply ossec.conf configuration changes according to the configuration template and use case.
            - Apply custom settings in local_internal_options.conf.
            - Truncate wazuh logs.
            - Restart wazuh-manager service to apply configuration changes.
        - test:
            - Check in the ossec.log that a line has appeared calling the module with correct parameters.
            - Check the expected number of events were forwarded to analysisd, only logs stored in the bucket and skips
              the ones that match with regex.
            - Check the database was created and updated accordingly.
        - teardown:
            - Truncate wazuh logs.
            - Restore initial configuration, both ossec.conf and local_internal_options.conf.
            - Delete the uploaded file

    wazuh_min_version: 4.6.0

    parameters:
        - configuration:
            type: dict
            brief: Get configurations from the module.
        - metadata:
            type: dict
            brief: Get metadata from the module.
        - load_wazuh_basic_configuration:
            type: fixture
            brief: Load basic wazuh configuration.
        - set_wazuh_configuration:
            type: fixture
            brief: Apply changes to the ossec.conf configuration.
        - clean_aws_services_db:
            type: fixture
            brief: Delete the DB file before and after the test execution.
        - configure_local_internal_options_function:
            type: fixture
            brief: Apply changes to the local_internal_options.conf configuration.
        - truncate_monitored_files:
            type: fixture
            brief: Truncate wazuh logs.
        - restart_wazuh_daemon_function:
            type: fixture
            brief: Restart the wazuh service.
        - file_monitoring:
            type: fixture
            brief: Handle the monitoring of a specified file.

    assertions:
        - Check in the log that the module was called with correct parameters.
        - Check the expected number of events were forwarded to analysisd.
        - Check the database was created and updated accordingly.

    input_description:
        - The `configuration_cloudwatch_discard_regex_simple_text` file provides
        the module configuration for this test.
        - The `cases_cloudwatch_discard_regex_simple_text` file provides the test cases.
    """
    log_group_name = metadata.get('log_group_name')
    service_type = metadata.get('service_type')
    only_logs_after = metadata.get('only_logs_after')
    regions: str = metadata.get('regions')
    discard_regex = metadata.get('discard_regex')
    found_logs = metadata.get('found_logs')

    pattern = fr'.*The "{discard_regex}" regex found a match. The event will be skipped.'

    parameters = [
        'wodles/aws/aws-s3',
        '--service', service_type,
        '--aws_profile', 'qa',
        '--only_logs_after', only_logs_after,
        '--regions', regions,
        '--aws_log_groups', log_group_name,
        '--discard-regex', discard_regex,
        '--debug', '2'
    ]

    # Check AWS module started
    log_monitor.start(
        timeout=global_parameters.default_timeout,
        callback=event_monitor.callback_detect_aws_module_start,
        error_message='The AWS module did not start as expected',
    ).result()

    # Check command was called correctly
    log_monitor.start(
        timeout=global_parameters.default_timeout,
        callback=event_monitor.callback_detect_aws_module_called(parameters),
        error_message='The AWS module was not called with the correct parameters',
    ).result()

    log_monitor.start(
        timeout=T_20,
        callback=event_monitor.callback_detect_event_processed_or_skipped(pattern),
        error_message=(
            'The AWS module did not show the correct message about discard regex or ',
            'did not process the expected amount of logs'
        ),
        accum_results=found_logs
    ).result()

    assert services_db_exists()


# ------------------------------------------- TEST_INSPECTOR_DISCARD_REGEX --------------------------------------------
# Configure T4 test
configurator.configure_test(configuration_file='configuration_inspector_discard_regex.yaml',
                            cases_file='cases_inspector_discard_regex.yaml')


@pytest.mark.tier(level=0)
@pytest.mark.parametrize('configuration, metadata',
                         zip(configurator.test_configuration_template, configurator.metadata),
                         ids=configurator.cases_ids)
def test_inspector_discard_regex(
        configuration, metadata, load_wazuh_basic_configuration, set_wazuh_configuration, clean_aws_services_db,
        configure_local_internal_options_function, truncate_monitored_files, restart_wazuh_function, file_monitoring,
):
    """
    description: Check that some Inspector logs are excluded when the regex and field defined in <discard_regex>
                 match an event.

    test_phases:
        - setup:
            - Load Wazuh light configuration.
            - Apply ossec.conf configuration changes according to the configuration template and use case.
            - Apply custom settings in local_internal_options.conf.
            - Truncate wazuh logs.
            - Restart wazuh-manager service to apply configuration changes.
        - test:
            - Check in the ossec.log that a line has appeared calling the module with correct parameters.
            - Check the expected number of events were forwarded to analysisd, only logs stored in the bucket and skips
              the ones that match with regex.
            - Check the database was created and updated accordingly.
        - teardown:
            - Truncate wazuh logs.
            - Restore initial configuration, both ossec.conf and local_internal_options.conf.
            - Delete the uploaded file

    wazuh_min_version: 4.6.0

    parameters:
        - configuration:
            type: dict
            brief: Get configurations from the module.
        - metadata:
            type: dict
            brief: Get metadata from the module.
        - load_wazuh_basic_configuration:
            type: fixture
            brief: Load basic wazuh configuration.
        - set_wazuh_configuration:
            type: fixture
            brief: Apply changes to the ossec.conf configuration.
        - clean_aws_services_db:
            type: fixture
            brief: Delete the DB file before and after the test execution.
        - configure_local_internal_options_function:
            type: fixture
            brief: Apply changes to the local_internal_options.conf configuration.
        - truncate_monitored_files:
            type: fixture
            brief: Truncate wazuh logs.
        - restart_wazuh_daemon_function:
            type: fixture
            brief: Restart the wazuh service.
        - file_monitoring:
            type: fixture
            brief: Handle the monitoring of a specified file.

    assertions:
        - Check in the log that the module was called with correct parameters.
        - Check the expected number of events were forwarded to analysisd.
        - Check the database was created and updated accordingly.

    input_description:
        - The `configuration_inspector_discard_regex` file provides the module configuration for this test.
        - The `cases_inspector_discard_regex` file provides the test cases.
    """
    service_type = metadata.get('service_type')
    only_logs_after = metadata.get('only_logs_after')
    regions: str = metadata.get('regions')
    discard_field = metadata.get('discard_field', '')
    discard_regex = metadata.get('discard_regex')
    found_logs = metadata.get('found_logs')

    pattern = fr'.*The "{discard_regex}" regex found a match in the "{discard_field}" field.' \
              ' The event will be skipped.'

    parameters = [
        'wodles/aws/aws-s3',
        '--service', service_type,
        '--aws_profile', 'qa',
        '--only_logs_after', only_logs_after,
        '--regions', regions,
        '--discard-field', discard_field,
        '--discard-regex', discard_regex,
        '--debug', '2'
    ]

    # Check AWS module started
    log_monitor.start(
        timeout=global_parameters.default_timeout,
        callback=event_monitor.callback_detect_aws_module_start,
        error_message='The AWS module did not start as expected',
    ).result()

    # Check command was called correctly
    log_monitor.start(
        timeout=global_parameters.default_timeout,
        callback=event_monitor.callback_detect_aws_module_called(parameters),
        error_message='The AWS module was not called with the correct parameters',
    ).result()

    log_monitor.start(
        timeout=T_20,
        callback=event_monitor.callback_detect_event_processed_or_skipped(pattern),
        error_message=(
            'The AWS module did not show the correct message about discard regex or ',
            'did not process the expected amount of logs'
        ),
        accum_results=found_logs
    ).result()

    assert services_db_exists()
