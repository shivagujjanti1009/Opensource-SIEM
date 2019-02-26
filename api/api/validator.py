# Copyright (C) 2015-2019, Wazuh Inc.
# Created by Wazuh, Inc. <info@wazuh.com>.
# This program is a free software; you can redistribute it and/or modify it under the terms of GPLv2


import json
from xml.etree import ElementTree as ET
import re


regex_dict = {'numbers': r'^\d+$',
              'array_numbers': r'^\d+(,\d+)*$',
              'names': r'^[\w\-\.]+$',
              'array_names': r'^[\w\-\.]+(,[\w\-\.]+)*$',
              'paths': r'^[\w\-\.\\\/:]+$',
              'dates': r'^\d{8}$',
              'ips': r'^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)(?:\/(?:[0-9]|[1-2][0-9]|3[0-2])){0,1}$|^any$|^ANY$',
              'alphanumeric_param': r'^[\w,\-\.\+\s\:]+$',
              'sort_param': r'^[\w_\-\,\s\+\.]+$',
              'search_param': r'^[^;\|&\^*>]+$',
              'select_param': r'^[\w\,\.]+$',
              'ranges': r'[\d]+$|^[\d]{1,2}\-[\d]{1,2}$',
              'hashes': r'^[\da-fA-F]{32}(?:[\da-fA-F]{8})?$|(?:[\da-fA-F]{32})?$',
              'ossec_key': r'[a-zA-Z0-9]+$',
              'timeframe_type': r'^(\d{1,}[d|h|m|s]?){1}$',
              'empty_boolean': r'^$|(^true$|^false$)',
              'yes_no_boolean': r'^yes$|^no$',
              'boolean': r'^true$|^false$',
              'query_param': r'[\w ]+$',
              'type_format': r'^xml$|^json$',
              'relative_paths': r'(^etc\/ossec.conf$)|((^etc\/rules\/|^etc\/decoders\/)[\w\-\/]+\.{1}xml$|(^etc\/lists\/)[\w\-\.\/]+)$'
             }


def check_params(parameters, filters):
    """
    Function to check multiple parameters
    :param parameters: Dictionary with parameters to be checked
    :param filters: Dictionary with filters for checking parameters
    :return: True if parameters are OK, False otherwise
    """
    for key in parameters.keys():
        if key not in filters.keys() or not check_exp(parameters[key], filters[key]):
            return False

    return True


def check_exp(exp, regex_name):
    """
    Function to check if an expression matches a regex
    :param exp: Expression to check
    :param regex_name: Name of regular expression to do the matching
    :return: True if expression is matched, False otherwise
    """
    return True if re.match(regex_dict[regex_name], exp) else False


def check_path(relative_path):
    """
    Function to check if a relative path is allowed (for uploading files)
    :param relative_path: XML string to check
    :return: True if XML is OK, False otherwise
    """
    if './' in relative_path or '../' in relative_path:
        return False

    return check_exp(relative_path, 'relative_paths')


def check_xml(xml_string):
    """
    Function to check if a XML string is right
    :param xml_string: XML string to check
    :return: True if XML is OK, False otherwise
    """
    try:
        ET.fromstring(xml_string)
    except ET.ParseError:
        return False
    
    return True


def check_cdb_list(cdb_list):
    """
    Function to check if a CDB list is well formed
    :param cdb_list: CDB list to check
    :return: True if CDB list is OK, False otherwise
    """
    cdb_list_splitted = cdb_list.split('\n')
    regex = re.compile(r'^#?[\w\s-]+:{1}(#?[\w\s-]+|)$')
    line = 1

    for elem in cdb_list_splitted:
        if not regex.match(elem):
            return False
        line += 1
    
    return True


def allowed_fields(filters):
    """
    Returns a list with allowed fields
    :param filters: Dictionary with valid filters
    :return: List with allowed filters
    """
    return [field for field in filters.keys()]

