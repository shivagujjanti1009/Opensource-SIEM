#!/usr/bin/env bash

sed -i "s:    # policies = RBAChecker.run_testing():    policies = RBAChecker.run_testing():g" /var/ossec/framework/python/lib/python3.7/site-packages/wazuh-3.11.0-py3.7.egg/wazuh/rbac/preprocessor.py
permissions='[{"actions":["rules:read"],"resources":["rule:file:0010-rules_config.xml","rule:file:0015-ossec_rules.xml"],"effect":"allow"}]'
awk -v var="${permissions}" '{sub(/testing_policies = \[\]/, "testing_policies = " var)}1' /var/ossec/framework/python/lib/python3.7/site-packages/wazuh-3.11.0-py3.7.egg/wazuh/rbac/auth_context.py >> /var/ossec/framework/python/lib/python3.7/site-packages/wazuh-3.11.0-py3.7.egg/wazuh/rbac/auth_context1.py
cat /var/ossec/framework/python/lib/python3.7/site-packages/wazuh-3.11.0-py3.7.egg/wazuh/rbac/auth_context1.py > /var/ossec/framework/python/lib/python3.7/site-packages/wazuh-3.11.0-py3.7.egg/wazuh/rbac/auth_context.py

/var/ossec/bin/ossec-control restart
