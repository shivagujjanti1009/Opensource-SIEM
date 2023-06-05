import sys
import os
from unittest.mock import patch
import pytest
import json

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), '.'))
import aws_utils as utils

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), '..'))
import wazuh_integration

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', 'subscribers'))
import sqsqueue

SAMPLE_RAW_MESSAGE = {'Messages': [{'MessageId': 'string', 'ReceiptHandle': 'string', 'MD5OfBody': 'string',
                                    'Body': '{"detail": {"bucket": {"name": "example-bucket"},"object": {"key": '
                                            '"example-key"}}}', 'Attributes': {'string': 'string'}}]}
SAMPLE_BODY = json.loads(SAMPLE_RAW_MESSAGE['Messages'][0]['Body'])
SAMPLE_MESSAGE = {"handle": SAMPLE_RAW_MESSAGE['Messages'][0]['ReceiptHandle']}
SAMPLE_URL = "sqs-test-url.com"


@patch('wazuh_integration.WazuhIntegration.get_sts_client')
@patch('wazuh_integration.WazuhIntegration.get_client')
@patch('sqsqueue.AWSSLSubscriberBucket.__init__', return_value=None)
@patch('sqsqueue.AWSSQSQueue._get_sqs_url')
@patch('wazuh_integration.WazuhIntegration.__init__', side_effet=wazuh_integration.WazuhIntegration.__init__)
def test_aws_sqs_queue_initializes_properly(mock_wazuh_integration, mock_get_sqs_url,
                                            mock_bucket_init, mock_client, mock_sts_client):
    """Test if the instances of AWSSQSQueue are created properly."""
    mock_sts_client.return_value = mock_client
    kwargs = utils.get_aws_sqs_queue_parameters(name=utils.TEST_SQS_NAME,
                                                external_id=utils.TEST_EXTERNAL_ID,
                                                service_endpoint=utils.TEST_SERVICE_ENDPOINT,
                                                sts_endpoint=utils.TEST_STS_ENDPOINT,
                                                iam_role_arn=utils.TEST_IAM_ROLE_ARN,
                                                iam_role_duration=utils.TEST_IAM_ROLE_DURATION)
    integration = sqsqueue.AWSSQSQueue(**kwargs)
    mock_wazuh_integration.assert_called_with(integration, service_name='sqs',
                                              access_key=None, secret_key=None,
                                              iam_role_arn=kwargs["iam_role_arn"], external_id=kwargs["external_id"],
                                              sts_endpoint=kwargs["sts_endpoint"],
                                              iam_role_duration=kwargs["iam_role_duration"],
                                              profile=None)

    assert integration.sqs_name == kwargs['name']
    assert integration.iam_role_arn == kwargs['iam_role_arn']
    mock_get_sqs_url.assert_called_once()
    mock_bucket_init.assert_called_once()
    mock_sts_client.assert_called_with(access_key=None, secret_key=None)
    mock_client.get_caller_identity.assert_called_once()


@patch('sqsqueue.AWSSQSQueue._get_sqs_url', return_value=SAMPLE_URL)
@patch('wazuh_integration.WazuhIntegration.get_sts_client')
@patch('wazuh_integration.WazuhIntegration.__init__', side_effect=wazuh_integration.WazuhIntegration.__init__)
def test_aws_sqs_queue_delete_message(mock_wazuh_integration, mock_sts_client, mock_get_url):
    """Test 'delete_message' method sends the given message to SQS."""

    instance = utils.get_mocked_aws_sqs_queue()
    instance.delete_message(SAMPLE_MESSAGE)

    instance.client.delete_message.assert_called_with(QueueUrl=SAMPLE_URL, ReceiptHandle=SAMPLE_MESSAGE["handle"])


@patch('wazuh_integration.WazuhIntegration.get_sts_client')
@patch('wazuh_integration.WazuhIntegration.__init__', side_effect=wazuh_integration.WazuhIntegration.__init__)
def test_aws_sqs_queue_delete_message_handles_exception_when_deleting_message(mock_wazuh_integration, mock_sts_client):
    """Test 'delete_message' handles exceptions raised when trying to delete a message from SQS."""
    instance = utils.get_mocked_aws_sqs_queue()
    instance.client.delete_message.side_effect = Exception

    with pytest.raises(SystemExit) as e:
        instance.delete_message(SAMPLE_MESSAGE)
    assert e.value.code == utils.UNABLE_TO_FETCH_DELETE_FROM_QUEUE


@patch('sqsqueue.AWSSQSQueue._get_sqs_url', return_value=SAMPLE_URL)
@patch('wazuh_integration.WazuhIntegration.get_sts_client')
@patch('wazuh_integration.WazuhIntegration.__init__', side_effect=wazuh_integration.WazuhIntegration.__init__)
def test_aws_sqs_queue_fetch_messages(mock_wazuh_integration, mock_sts_client, mock_get_url):
    """Test 'fetch_messages' method retrieves one or more messages from the specified queue."""
    instance = utils.get_mocked_aws_sqs_queue()
    mock_receive = instance.client.receive_message
    mock_receive.return_value = SAMPLE_RAW_MESSAGE

    raw_messages = instance.fetch_messages()
    mock_receive.assert_called_with(QueueUrl=instance.sqs_url, AttributeNames=['All'],
                                    MaxNumberOfMessages=10, MessageAttributeNames=['All'],
                                    WaitTimeSeconds=20)
    assert type(raw_messages) is dict


@patch('wazuh_integration.WazuhIntegration.get_sts_client')
@patch('wazuh_integration.WazuhIntegration.__init__', side_effect=wazuh_integration.WazuhIntegration.__init__)
def test_aws_sqs_queue_fetch_messages_handles_exception_when_getting_messages(mock_wazuh_integration, mock_sts_client):
    """Test 'fetch_messages' handles exceptions raised when trying to retrieve messages from SQS."""
    instance = utils.get_mocked_aws_sqs_queue()

    mock_receive = instance.client.receive_message
    mock_receive.side_effect = Exception

    with pytest.raises(SystemExit) as e:
        instance.fetch_messages()
    assert e.value.code == utils.UNABLE_TO_FETCH_DELETE_FROM_QUEUE


@patch('sqsqueue.AWSSQSQueue.fetch_messages', return_value=SAMPLE_RAW_MESSAGE)
@patch('wazuh_integration.WazuhIntegration.get_sts_client')
@patch('wazuh_integration.WazuhIntegration.__init__', side_effect=wazuh_integration.WazuhIntegration.__init__)
def test_aws_sqs_queue_get_messages(mock_wazuh_integration, mock_sts_client, mock_fetch):
    """Test 'get_messages' method returns parsed messages."""
    instance = utils.get_mocked_aws_sqs_queue()
    messages = instance.get_messages()

    assert messages == [{"parquet_path": SAMPLE_BODY["detail"]["object"]["key"],
                         "bucket_path": SAMPLE_BODY["detail"]["bucket"]["name"],
                         "handle": SAMPLE_MESSAGE["handle"]}]


@patch('wazuh_integration.WazuhIntegration.get_sts_client')
@patch('wazuh_integration.WazuhIntegration.__init__', side_effect=wazuh_integration.WazuhIntegration.__init__)
def test_aws_sqs_queue_sync_events(mock_wazuh_integration, mock_sts_client):
    """Test 'sync_events' method gets messages from the SQS queue, sends them to AnalysisD
    and deletes from the queue until it is empty."""
    instance = utils.get_mocked_aws_sqs_queue()

    with patch('sqsqueue.AWSSQSQueue.get_messages',
               side_effect=[[{"parquet_path": SAMPLE_BODY["detail"]["object"]["key"],
                              "bucket_path": SAMPLE_BODY["detail"]["bucket"]["name"],
                              "handle": SAMPLE_MESSAGE["handle"]}], []]),\
            patch('sqsqueue.AWSSQSQueue.delete_message') as mock_delete, \
            patch('slsubscriberbucket.AWSSLSubscriberBucket.process_file') as mock_process:
        instance.sync_events()

        instance.asl_bucket_handler.process_file.assert_called()
        mock_delete.assert_called()
