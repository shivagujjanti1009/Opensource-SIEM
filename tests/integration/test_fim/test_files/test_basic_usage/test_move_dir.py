import sys
import pytest

from pathlib import Path

from wazuh_testing.constants.paths.logs import WAZUH_LOG_PATH
from wazuh_testing.constants.platforms import WINDOWS
from wazuh_testing.modules.agentd.configuration import AGENTD_DEBUG, AGENTD_WINDOWS_DEBUG
from wazuh_testing.modules.fim.patterns import ADDED_EVENT, DELETED_EVENT
from wazuh_testing.modules.fim.utils import get_fim_event_data
from wazuh_testing.modules.monitord.configuration import MONITORD_ROTATE_LOG
from wazuh_testing.modules.syscheck.configuration import SYSCHECK_DEBUG
from wazuh_testing.tools.monitors.file_monitor import FileMonitor
from wazuh_testing.utils import file
from wazuh_testing.utils.callbacks import generate_callback
from wazuh_testing.utils.configuration import get_test_cases_data, load_configuration_template

from . import TEST_CASES_PATH, CONFIGS_PATH


# Pytest marks to run on any service type on linux or windows.
pytestmark = [pytest.mark.linux, pytest.mark.tier(level=0)]

# Test metadata, configuration and ids.
cases_path = Path(TEST_CASES_PATH, 'cases_move_dir.yaml')
config_path = Path(CONFIGS_PATH, 'configuration_basic.yaml')
test_configuration, test_metadata, cases_ids = get_test_cases_data(cases_path)
test_configuration = load_configuration_template(config_path, test_configuration, test_metadata)

# Set configurations required by the fixtures.
daemons_handler_configuration = {'all_daemons': True}
local_internal_options = {SYSCHECK_DEBUG: 2, AGENTD_DEBUG: 2, MONITORD_ROTATE_LOG: 0}
if sys.platform == WINDOWS: local_internal_options.update({AGENTD_WINDOWS_DEBUG: 2})


@pytest.mark.parametrize('test_configuration, test_metadata', zip(test_configuration, test_metadata), ids=cases_ids)
def test_move_dir(test_configuration, test_metadata, set_wazuh_configuration, configure_local_internal_options,
                  truncate_monitored_files, folder_to_monitor, daemons_handler, start_monitoring):
    # Arrange
    wazuh_log_monitor = FileMonitor(WAZUH_LOG_PATH)
    fim_mode = test_metadata.get('fim_mode')
    folder_to_move = test_metadata.get('folder_to_move')
    folder_moved_path = test_metadata.get('folder_moved_path')

    # Act
    file.create_folder(folder_to_move)
    file.write_file(Path(folder_to_move, 'newfile'), 'test')
    wazuh_log_monitor.start(generate_callback(ADDED_EVENT))

    # Assert
    file.move(folder_to_move, folder_moved_path)
    wazuh_log_monitor.start(generate_callback(DELETED_EVENT))
    assert wazuh_log_monitor.callback_result
    assert get_fim_event_data(wazuh_log_monitor.callback_result)['mode'] == fim_mode

    wazuh_log_monitor.start(generate_callback(ADDED_EVENT))
    assert wazuh_log_monitor.callback_result
