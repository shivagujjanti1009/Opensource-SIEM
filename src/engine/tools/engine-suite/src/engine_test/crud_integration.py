import json
from os.path import exists
from pathlib import Path
from engine_test.event_format import Formats
from engine_test.config import Config

from importlib.metadata import files

class CrudIntegration:
    def __init__(self):
        self.formats = []
        self.formats.append(Formats.AUDIT.value['name'])
        self.formats.append(Formats.COMMAND.value['name'])
        self.formats.append(Formats.FULL_COMMAND.value['name'])
        self.formats.append(Formats.EVENTCHANNEL.value['name'])
        self.formats.append(Formats.JSON.value['name'])
        self.formats.append(Formats.MACOS.value['name'])
        self.formats.append(Formats.MULTI_LINE.value['name'])
        self.formats.append(Formats.SYSLOG.value['name'])
        self.formats.append(Formats.REMOTE_SYSLOG.value['name'])

    def get_integrations(self):
        json_content = ""
        with open(Config.get_config_file()) as fp:
            try:
                json_content = json.load(fp)
            except Exception as ex:
                print('Error while reading JSON file: {}'.format(ex))
                exit(1)

        return json_content

    def get_integration(self, integration_name: str):
        integrations = self.get_integrations()
        if integration_name in integrations:
            return integrations[integration_name]
        else:
            return None

    def save_integration(self, integration_name: str, format: str, origin: str, lines: str = None):
        if not format and not integration_name:
            print('To save the integration, the integration-name and format parameters cannot be empty.')
            return False

        if self.get_integration(integration_name) != None:
            print('The integration already exists!')
            return False

        if format not in self.formats:
            print('The format is invalid!')
            return False

        try:
            with open(Config.get_config_file()) as fp:
                json_content = json.load(fp)
        except Exception as ex:
            print('Error while reading configuration file: {}'.format(ex))
            return False

        if origin and format:
            content = {
                "format": format,
                "origin": origin
                }
        else:
            content = { "format": format }

        if (Formats.MULTI_LINE.value['name'] == format):
            if (lines != None):
                content['lines'] = lines
            else:
                print("Parameter 'lines' is mandatory for multi-line format.")
                return False
        try:
            json_content[integration_name] = content
        except KeyError as ex:
            print('Error saving integration. Error: {}'.format(ex))
            return False

        try:
            with open(Config.get_config_file(), 'w') as json_file:
                json.dump(json_content, json_file, indent=4, separators=(',',': '))
        except Exception as ex:
            print('Error while writing configuration file: {}'.format(ex))
            return False

        print('Integration added successfully.')
        return True

    def delete_integration(self, integration_name: str):
        try:
            with open(Config.get_config_file()) as fp:
                json_content = json.load(fp)
        except Exception as ex:
            print('Error while reading configuration file: {}'.format(ex))
            return False

        if not integration_name:
            print('To delete the integration, the integration-name parameter cannot be empty.')
            return False

        try:
            del json_content[integration_name]
        except KeyError:
            print('Integration not found!')
            return False

        try:
            with open(Config.get_config_file(), 'w') as json_file:
                json.dump(json_content, json_file, indent=4, separators=(',',': '))

            print('Integration removed successfully.')
        except Exception as ex:
            print('Error while writing configuration file: {}'.format(ex))
            return False

        return True

    def import_integration(self, integration_path: str):
        working_path = integration_path
        path = Path(working_path)
        if path.is_dir():
            working_path = str(path.resolve())
        else:
            print(f'Error: directory does not exist ')
            return

        config_file = working_path + '/' + Config.get_config_file_name()

        if not exists(config_file):
            print(f"File '{config_file}' not found!")
            return

        try:
            with open(config_file) as fp:
                json_content = json.load(fp)
        except Exception as ex:
            print('Error while reading configuration file: {}'.format(ex))
            return False

        print(f"Importing from: '{working_path}'...\n")

        for item in json_content:
            try:
                integration_name = item
                format = json_content[item]['format']
                origin = json_content[item]['origin']
                lines = None

                message = f"Adding integration '{item}' with format '{format}'"
                if (Formats.MULTI_LINE.value['name'] == format):
                    lines = json_content[item]['lines']
                    message += f", lines '{lines}'"
                message += f" and origin '{origin}'"

                print(message)
                self.save_integration(integration_name, format, origin, lines)
            except Exception as ex:
                print(f'Error importing file: {ex}')
