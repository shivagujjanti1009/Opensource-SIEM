from engine_test.crud_integration import CrudIntegration
from engine_test.event_format import Formats
from engine_test.command import Command

class AddCommand(Command):
    def __init__(self):
        pass

    def adjust_origin(self, args):
        if (args['origin'] == None):
            swticher = {
                Formats.AUDIT.value["name"]: Formats.AUDIT.value["origin"],
                Formats.COMMAND.value["name"]: Formats.COMMAND.value["origin"],
                Formats.EVENTCHANNEL.value["name"]: Formats.EVENTCHANNEL.value["origin"],
                Formats.FULL_COMMAND.value["name"]: Formats.FULL_COMMAND.value["origin"],
                Formats.JSON.value["name"]: Formats.JSON.value["origin"],
                Formats.MACOS.value["name"]: Formats.MACOS.value["origin"],
                Formats.MULTI_LINE.value["name"]: Formats.MULTI_LINE.value["origin"],
                Formats.SYSLOG.value["name"]: Formats.SYSLOG.value["origin"],
                Formats.REMOTE_SYSLOG.value["name"]: Formats.REMOTE_SYSLOG.value["origin"]
            }
            args['origin'] = swticher.get(args['format'], None)

    def run(self, args):
        super().run(args)
        args['post_parse'](args)
        integration = CrudIntegration()
        try:
            if (args['integration_path'] != None):
                integration.import_integration(args['integration_path'])
            else:
                integration.save_integration(args['integration_name'], args['format'], args['origin'], args['lines'])
        except Exception as ex:
            print(ex)

    def configure(self, subparsers):
        parser_add = subparsers.add_parser("add", help='Add integration')

        group = parser_add.add_mutually_exclusive_group()

        group.add_argument('-n', '--integration-name', type=str, help=f'Integration test name')

        group.add_argument('-p', '--integration-path', type=str, help=f'Integration path to import')

        parser_add.add_argument('-f', '--format', help=f'Format of integration.', choices=Formats.get_formats(),
                                dest='format')

        parser_add.add_argument('-o', '--origin', help='Origin of integration.',
                                dest='origin')

        parser_add.add_argument('-l', '--lines', help='Number of lines. Only for multi-line format.',
                                dest='lines')

        parser_add.set_defaults(func=self.run, post_parse=self.adjust_origin)
