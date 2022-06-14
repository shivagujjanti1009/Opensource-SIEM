# Copyright (C) 2015, Wazuh Inc.
# Created by Wazuh, Inc. <info@wazuh.com>.
# This program is free software; you can redistribute it and/or modify it under the terms of GPLv2

import sys
from unittest.mock import patch, MagicMock

import pytest

with patch('wazuh.core.common.wazuh_uid'):
    with patch('wazuh.core.common.wazuh_gid'):
        sys.modules['wazuh.rbac.orm'] = MagicMock()
        import wazuh.rbac.decorators
        from wazuh.tests.util import RBAC_bypasser

        del sys.modules['wazuh.rbac.orm']
        wazuh.rbac.decorators.expose_resources = RBAC_bypasser
        from scripts import security_resources
        from wazuh.tests.test_security import db_setup # noqa


@patch('scripts.security_resources.sys.exit')
def test_signal_handler(mock_exit):
    """Check if exit is called in the `signal_handler` function."""
    security_resources.signal_handler('test', 'test')
    mock_exit.assert_called_once_with(1)


@pytest.mark.asyncio
@pytest.mark.parametrize("user_input", ["NewPassword1!", ""])
@patch("builtins.print")
@patch("yaml.safe_load", return_value={"default_users": ["testing_user"]})
@patch("scripts.security_resources.cluster_utils.forward_function")
async def test_restore_default_passwords(forward_mock, safe_load_mock, print_mock, user_input, db_setup):
    """Check if the `restore_default_passwords` uses the correct parameters when called.

    Parameters
    ----------
    user_input : str
        Mocked password.
    """
    security, _, _ = db_setup
    with patch("getpass.getpass", return_value=user_input):
        await security_resources.restore_default_passwords()
        if user_input != "":
            forward_mock.assert_called_with(security.update_user, f_kwargs={'user_id': '1', 'password': user_input},
                                            request_type="local_master")
            assert "testing_user" in print_mock.call_args[0][0]
            assert "UPDATED" in print_mock.call_args[0][0]
        else:
            forward_mock.assert_not_called()
            print_mock.assert_not_called()


@pytest.mark.asyncio
@patch("builtins.print")
@patch("getpass.getpass", return_value="NewPassword1!")
@patch("yaml.safe_load", return_value={"default_users": ["testing_user"]})
async def test_restore_default_passwords_exceptions(safe_load_mock, getpass_mock, print_mock):
    """Check the `restore_default_passwords` function behaviour when receiving exceptions."""
    exception_message = "Random exception message"
    with patch("scripts.security_resources.cluster_utils.forward_function", return_value=Exception(exception_message)):
        await security_resources.restore_default_passwords()

        assert "testing_user" in print_mock.call_args[0][0]
        assert exception_message in print_mock.call_args[0][0]


@pytest.mark.asyncio
@pytest.mark.parametrize("user_input", ["RESET", "whatever"])
@patch("builtins.print")
@patch("scripts.security_resources.cluster_utils.forward_function")
async def test_reset_rbac_database(forward_mock, print_mock, user_input, db_setup):
    """Check if the `restore_default_passwords` uses the correct parameters when called.

    Parameters
    ----------
    user_input : str
        Mocked password.
    """
    _, _, core_security = db_setup
    with patch("builtins.input", return_value=user_input):
        if user_input == "RESET":
            await security_resources.reset_rbac_database()
            forward_mock.assert_called_with(core_security.rbac_db_factory_reset, request_type="local_master")
            assert "Successfully reset RBAC database" in print_mock.call_args[0][0]
        else:
            with pytest.raises(SystemExit):
                await security_resources.reset_rbac_database()
                forward_mock.assert_not_called()
                assert "RBAC database reset aborted." in print_mock.call_args[0][0]


@pytest.mark.asyncio
@patch("builtins.print")
@patch("builtins.input", return_value="RESET")
async def test_reset_rbac_database_exceptions(input_mock, print_mock):
    """Check the `restore_default_passwords` function behaviour when receiving exceptions."""
    exception_message = "Random exception message"
    with patch("scripts.security_resources.cluster_utils.forward_function", return_value=Exception(exception_message)):
        await security_resources.reset_rbac_database()
        assert "RBAC database reset failed" in print_mock.call_args[0][0]
        assert exception_message in print_mock.call_args[0][0]


@patch("scripts.security_resources.sys.exit")
def test_get_script_arguments(exit_mock):
    """Test exit conditions for the `get_script_arguments` function."""
    with patch("scripts.security_resources.sys.argv", new=["script", "at_least_one_argument"]):
        # Valid number of script arguments
        security_resources.get_script_arguments()
        exit_mock.assert_called_with(2)

    # Invalid number of script arguments
    with patch("scripts.security_resources.sys.argv", new=["script"]):
        security_resources.get_script_arguments()
        exit_mock.assert_called_with(0)


@pytest.mark.asyncio
@patch("scripts.security_resources.sys.exit")
@patch("scripts.security_resources.sys.argv", new=["script", "at_least_one_argument"])
@patch("scripts.security_resources.restore_default_passwords")
@patch("scripts.security_resources.reset_rbac_database")
async def test_main(reset_mock, restore_mock, exit_mock):
    """Test all the possible options for the `main` function depending on user input."""
    class Arguments:
        def __init__(self, change_password=None, factory_reset=None):
            self.change_password = change_password
            self.factory_reset = factory_reset

    security_resources.args = Arguments()

    # No arguments
    await security_resources.main()

    # --change-password
    security_resources.args = Arguments(change_password=True)
    await security_resources.main()
    restore_mock.assert_called_once()
    restore_mock.reset_mock()
    reset_mock.assert_not_called()
    exit_mock.assert_called_with(0)

    # --factory-reset
    security_resources.args = Arguments(factory_reset=True)
    await security_resources.main()
    reset_mock.assert_called_once()
    reset_mock.reset_mock()
    restore_mock.assert_not_called()
    exit_mock.assert_called_with(0)
