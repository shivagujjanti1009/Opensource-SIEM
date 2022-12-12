import sys
from os import path
from datetime import datetime

sys.path.insert(0, path.dirname(path.dirname(path.abspath(__file__))))
from wazuh_integration import WazuhIntegration


class AWSService(WazuhIntegration):
    """
    Class for getting AWS Services logs from API calls
    :param access_key: AWS access key id
    :param secret_key: AWS secret access key
    :param profile: AWS profile
    :param iam_role_arn: IAM Role
    :param only_logs_after: Date after which obtain logs.
    :param region: Region of service
    """

    def __init__(self, reparse, access_key, secret_key, aws_profile, iam_role_arn,
                 service_name, only_logs_after, region, aws_log_groups=None, remove_log_streams=None,
                 discard_field=None, discard_regex=None, sts_endpoint=None, service_endpoint=None,
                 iam_role_duration=None):
        # DB name
        self.db_name = 'aws_services'
        # table name
        self.db_table_name = 'aws_services'
        self.reparse = reparse

        WazuhIntegration.__init__(self, access_key=access_key, secret_key=secret_key,
                                  aws_profile=aws_profile, iam_role_arn=iam_role_arn,
                                  service_name=service_name, region=region, discard_field=discard_field,
                                  discard_regex=discard_regex, sts_endpoint=sts_endpoint,
                                  service_endpoint=service_endpoint, iam_role_duration=iam_role_duration)

        # get sts client (necessary for getting account ID)
        self.sts_client = self.get_sts_client(access_key, secret_key, aws_profile)
        # get account ID
        self.account_id = self.sts_client.get_caller_identity().get('Account')
        self.only_logs_after = only_logs_after

        # SQL queries for services
        self.sql_create_table = """
            CREATE TABLE {table_name} (
                    service_name 'text' NOT NULL,
                    aws_account_id 'text' NOT NULL,
                    aws_region 'text' NOT NULL,
                    scan_date 'text' NOT NULL,
                    PRIMARY KEY (service_name, aws_account_id, aws_region, scan_date));"""

        self.sql_insert_value = """
            INSERT INTO {table_name} (
                service_name,
                aws_account_id,
                aws_region,
                scan_date)
            VALUES (
                :service_name,
                :aws_account_id,
                :aws_region,
                :scan_date);"""

        self.sql_find_last_scan = """
            SELECT
                scan_date
            FROM
                {table_name}
            WHERE
                service_name=:service_name AND
                aws_account_id=:aws_account_id AND
                aws_region=:aws_region
            ORDER BY
                scan_date DESC
            LIMIT 1;"""

        self.sql_db_maintenance = """
            DELETE FROM {table_name}
            WHERE
                service_name=:service_name AND
                aws_account_id=:aws_account_id AND
                aws_region=:aws_region AND
                rowid NOT IN (SELECT ROWID
                    FROM
                        {table_name}
                    WHERE
                        service_name=:service_name AND
                        aws_account_id=:aws_account_id AND
                        aws_region=:aws_region
                    ORDER BY
                        scan_date DESC
                    LIMIT :retain_db_records);"""

    def get_last_log_date(self):
        date = self.only_logs_after if self.only_logs_after is not None else self.default_date.strftime('%Y%m%d')
        return f'{date[0:4]}-{date[4:6]}-{date[6:8]} 00:00:00.0'

    def format_message(self, msg):
        # rename service field to source
        if 'service' in msg:
            msg['source'] = msg['service'].lower()
            del msg['service']
        # cast createdAt
        if 'createdAt' in msg:
            msg['createdAt'] = datetime.strftime(msg['createdAt'],
                                                 '%Y-%m-%dT%H:%M:%SZ')
        # cast updatedAt
        if 'updatedAt' in msg:
            msg['updatedAt'] = datetime.strftime(msg['updatedAt'],
                                                 '%Y-%m-%dT%H:%M:%SZ')

        return {'integration': 'aws', 'aws': msg}
