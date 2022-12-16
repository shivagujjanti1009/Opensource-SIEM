import sys
from os import path
from datetime import datetime

sys.path.append(path.dirname(path.realpath(__file__)))
import aws_service

sys.path.insert(0, path.dirname(path.dirname(path.abspath(__file__))))
import tools

class AWSInspector(aws_service.AWSService):
    """
    Class for getting AWS Inspector logs

    Parameters
    ----------
    access_key : str
        AWS access key id.
    secret_key : str
        AWS secret access key.
    aws_profile : str
        AWS profile.
    iam_role_arn : str
        IAM Role that will be assumed to use the service.
    only_logs_after : str
        Date after which obtain logs.
    region : str
        AWS region that will be used to fetch the events.

    Attributes
    ----------
    sent_events : int
        The number of events collected and sent to analysisd.
    """


    def __init__(self, reparse, access_key, secret_key, aws_profile,
                 iam_role_arn, only_logs_after, region, aws_log_groups=None,
                 remove_log_streams=None, discard_field=None, discard_regex=None,
                 sts_endpoint=None, service_endpoint=None, iam_role_duration=None):

        aws_service.AWSService.__init__(self, db_table_name=aws_service.DEFAULT_TABLENAME, service_name='inspector',
                                        reparse=reparse, access_key=access_key, secret_key=secret_key,
                                        aws_profile=aws_profile, iam_role_arn=iam_role_arn, only_logs_after=only_logs_after,
                                        region=region, aws_log_groups=aws_log_groups,
                                        remove_log_streams=remove_log_streams, discard_field=discard_field,
                                        discard_regex=discard_regex, sts_endpoint=sts_endpoint,
                                        service_endpoint=service_endpoint, iam_role_duration=iam_role_duration)

        # max DB records for region
        self.retain_db_records = 5
        self.sent_events = 0

    def send_describe_findings(self, arn_list: list):
        """
        Collect and send to analysisd the requested findings.

        Parameters
        ----------
        arn_list : list[str]
            The ARN of the findings that should be requested to AWS and sent to analysisd.
        """
        if len(arn_list) != 0:
            response = self.client.describe_findings(findingArns=arn_list)['findings']
            self.sent_events += len(response)
            tools.debug(f"+++ Processing {len(response)} events", 3)
            for elem in response:
                self.send_msg(self.format_message(elem))

    def get_alerts(self):
        self.init_db(self.sql_create_table.format(table_name=self.db_table_name))
        try:
            initial_date = self.get_last_log_date()
            # reparse logs if this parameter exists
            if self.reparse:
                last_scan = initial_date
            else:
                self.db_cursor.execute(self.sql_find_last_scan.format(table_name=self.db_table_name), {
                    'service_name': self.service_name,
                    'aws_account_id': self.account_id,
                    'aws_region': self.region})
                last_scan = self.db_cursor.fetchone()[0]
        except TypeError as e:
            # write initial date if DB is empty
            self.db_cursor.execute(self.sql_insert_value.format(table_name=self.db_table_name), {
                'service_name': self.service_name,
                'aws_account_id': self.account_id,
                'aws_region': self.region,
                'scan_date': initial_date})
            last_scan = initial_date

        date_last_scan = datetime.strptime(last_scan, '%Y-%m-%d %H:%M:%S.%f')
        date_scan = date_last_scan
        if self.only_logs_after:
            date_only_logs = datetime.strptime(self.only_logs_after, "%Y%m%d")
            date_scan = date_only_logs if date_only_logs > date_last_scan else date_last_scan

        # get current time (UTC)
        date_current = datetime.utcnow()
        # describe_findings only retrieves 100 results per call
        response = self.client.list_findings(maxResults=100, filter={'creationTimeRange':
                                                                         {'beginDate': date_scan,
                                                                          'endDate': date_current}})
        tools.debug(f"+++ Listing findings starting from {date_scan}", 2)
        self.send_describe_findings(response['findingArns'])
        # Iterate if there are more elements
        while 'nextToken' in response:
            response = self.client.list_findings(maxResults=100, nextToken=response['nextToken'],
                                                 filter={'creationTimeRange': {'beginDate': date_scan,
                                                                               'endDate': date_current}})
            self.send_describe_findings(response['findingArns'])

        if self.sent_events:
            tools.debug(f"+++ {self.sent_events} events collected and processed in {self.region}", 1)
        else:
            tools.debug(f'+++ There are no new events in the "{self.region}" region', 1)

        # insert last scan in DB
        self.db_cursor.execute(self.sql_insert_value.format(table_name=self.db_table_name), {
            'service_name': self.service_name,
            'aws_account_id': self.account_id,
            'aws_region': self.region,
            'scan_date': date_current})
        # DB maintenance
        self.db_cursor.execute(self.sql_db_maintenance.format(table_name=self.db_table_name), {
            'service_name': self.service_name,
            'aws_account_id': self.account_id,
            'aws_region': self.region,
            'retain_db_records': self.retain_db_records})
        # close connection with DB
        self.close_db()
