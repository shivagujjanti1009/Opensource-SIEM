from engine_test.parser import Parser
from engine_test.event_format import EventFormat, Formats

class CommandFormat(EventFormat):
    def __init__(self, integration, args):
        super().__init__(integration, args)
        self.config['queue'] = Formats.COMMAND.value['queue']

    def parse_events(self, events, config):
        return self.parse_event(events, config)

    def parse_event(self, event, config):
        event_parsed = []
        event = self.parse_command(event, config)
        event = Parser.get_event_ossec_format(event, config)
        event_parsed.append(event)
        return event_parsed

    def parse_command(self, event, config):
        agent_id = Parser.get_agent_id(config['agent_id'])
        agent_name = Parser.get_agent_name(config['agent_name'])
        agent_ip = Parser.get_agent_ip(config['agent_ip'])
        origin = Parser.get_origin(config['origin'])
        queue = Parser.get_queue(config['queue'])
        header = Parser.get_header_ossec_format(queue, agent_id, agent_name, agent_ip, origin)
        header = "{}:ossec: output: '{}':".format(header, origin)
        return "{} {}".format(header, '{} '.format(header).join([line + '\n' for line in event]))

    def format_event(self, event):
        return "ossec: output: '{}':".format(event)
