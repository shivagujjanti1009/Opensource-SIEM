#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
#
# Copyright (C) 2015-2021, Wazuh Inc.
# Created by Wazuh, Inc. <info@wazuh.com>.
# This program is free software; you can redistribute
# it and/or modify it under the terms of GPLv2

import csv
import logging
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))  # noqa: E501
from bucket import WazuhGCloudBucket


class GCSAccessLogs(WazuhGCloudBucket):
    """Class for getting Google Cloud Storage Access Logs logs"""
    def __init__(self, credentials_file: str, logger: logging.Logger, **kwargs):
        """Class constructor.

        Parameters
        ----------
        credentials_file : str
            Path to credentials file
        logger : logging.Logger
            Logger to use
        """
        self.db_table_name = "access_logs"
        WazuhGCloudBucket.__init__(self, credentials_file, logger, **kwargs)

    def load_information_from_file(self, msg: str):
        """Load the contents of an Access Logs blob and processes it.

        GCS Access Logs blobs will always contain the fieldnames as the first line while the remaining lines store the
        data of the log itself.
        """
        # Clean and split each line in the file
        lines = msg.replace('"', '').split("\n")

        # Get the fieldnames from the first line
        fieldnames = lines[0].split(",")
        values = lines[1:]
        tsv_file = csv.DictReader(values, fieldnames=fieldnames, delimiter=',')
        return [dict(x, source='gcp_bucket') for x in tsv_file]
